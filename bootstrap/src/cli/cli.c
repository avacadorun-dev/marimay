#include "ruda/cli.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/build_config.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/stream.h"
#include "ruda/base/utf8.h"
#include "ruda/base/vector.h"
#include "ruda/diagnostic.h"
#include "ruda/format/formatter.h"
#include "ruda/generated/diagnostic_codes.h"
#include "ruda/lex/lexer.h"
#include "ruda/manifest/manifest.h"
#include "ruda/parse/parser.h"
#include "ruda/platform/directory.h"
#include "ruda/platform/file.h"
#include "ruda/source/source_manager.h"

#define RUDA_STRINGIFY_INNER(value) #value
#define RUDA_STRINGIFY(value) RUDA_STRINGIFY_INNER(value)

typedef struct RudaTaskHelp {
    const char *name;
    const char *summary;
    const char *usage;
    const char *examples;
    bool implemented;
} RudaTaskHelp;

typedef struct RudaCliOptions {
    RudaDiagnosticStyle diagnostic_style;
    const char *command;
    size_t command_index;
    const char *help_target;
    size_t help_target_index;
    const char *target;
    size_t target_index;
    bool top_level_help;
    bool command_help;
    bool version_json;
    size_t version_json_index;
    bool format_check;
    size_t format_check_index;
    bool format_diff;
    size_t format_diff_index;
    bool format_stdin;
    size_t format_stdin_index;
    bool quiet;
    bool verbose;
} RudaCliOptions;

static const RudaTaskHelp ruda_tasks[] = {
    {"add", "Add a package dependency.", "add module[@version]", "ruda add ruda-lang.example/postgres@1.4", false},
    {"audit", "Audit dependencies, capabilities, unsafe code, and provenance.", "audit", "ruda audit", false},
    {"bench", "Run first-party benchmarks.", "bench [filter]", "ruda bench --compare base.json", false},
    {"build", "Build a native executable by default.", "build [target] [options]", "ruda build --release", false},
    {"check", "Run the complete frontend and MIR verifier.", "check [target] [options]", "ruda check --all-targets", false},
    {"doc", "Build or check package documentation.", "doc [options]", "ruda doc --check", false},
    {"exec", "Verify and execute a portable .rudab bundle.", "exec bundle.rudab [options]", "ruda exec app.rudab", false},
    {"explain", "Explain a diagnostic or inferred compiler behavior.", "explain code|topic", "ruda explain suspension loadDashboard", false},
    {"fix", "Apply compiler-authored safe fixes.", "fix [target] [options]", "ruda fix", false},
    {"fmt", "Canonically format Ruda source and manifests.", "fmt [target] [options]", "ruda fmt --check", true},
    {"fuzz", "Run a first-party fuzz target.", "fuzz target [options]", "ruda fuzz \"decoder never panics\"", false},
    {"generate", "Run explicitly configured external generation.", "generate [target]", "ruda generate", false},
    {"help", "Show task-oriented command help.", "help [command]", "ruda help build", true},
    {"init", "Create a Ruda module.", "init [path]", "ruda init ./orders", false},
    {"list", "List package dependencies.", "list", "ruda list", false},
    {"outdated", "Report available dependency updates.", "outdated", "ruda outdated", false},
    {"remove", "Remove a package dependency.", "remove module", "ruda remove ruda-lang.example/postgres", false},
    {"run", "Run with the bytecode VM by default.", "run [target] [-- arguments]", "ruda run .", false},
    {"test", "Run built-in tests with the VM by default.", "test [filter|target] [options]", "ruda test --engine both", false},
    {"update", "Update locked package versions.", "update [module]", "ruda update", false},
    {"vendor", "Vendor immutable locked dependencies.", "vendor", "ruda vendor", false},
    {"version", "Show compiler and runtime build identity.", "version [--json]", "ruda version --json", true},
    {"vet", "Run first-party correctness checks.", "vet [target] [options]", "ruda vet --strict", false},
    {"why", "Explain why a dependency is present.", "why module", "ruda why ruda-lang.example/crypto", false},
};

static const char *const ruda_global_options[] = {
    "--check",
    "--color",
    "--diagnostic",
    "--diff",
    "--help",
    "--quiet",
    "--stdin",
    "--verbose",
    "--version",
};

static bool ruda_starts_with(const char *text, const char *prefix) {
    size_t prefix_length = strlen(prefix);
    return strncmp(text, prefix, prefix_length) == 0;
}

static size_t ruda_minimum3(size_t first, size_t second, size_t third) {
    size_t result = first < second ? first : second;
    return result < third ? result : third;
}

static size_t ruda_edit_distance(const char *left, const char *right) {
    enum { RUDA_SUGGESTION_LIMIT = 64 };
    size_t previous[RUDA_SUGGESTION_LIMIT + 1];
    size_t current[RUDA_SUGGESTION_LIMIT + 1];
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    size_t left_index = 0;
    size_t right_index = 0;

    if (left_length > RUDA_SUGGESTION_LIMIT || right_length > RUDA_SUGGESTION_LIMIT) {
        return RUDA_SUGGESTION_LIMIT;
    }
    for (right_index = 0; right_index <= right_length; right_index += 1) {
        previous[right_index] = right_index;
    }
    for (left_index = 1; left_index <= left_length; left_index += 1) {
        current[0] = left_index;
        for (right_index = 1; right_index <= right_length; right_index += 1) {
            size_t substitution = previous[right_index - 1]
                + (left[left_index - 1] == right[right_index - 1] ? 0U : 1U);
            current[right_index] = ruda_minimum3(
                previous[right_index] + 1,
                current[right_index - 1] + 1,
                substitution
            );
        }
        for (right_index = 0; right_index <= right_length; right_index += 1) {
            previous[right_index] = current[right_index];
        }
    }
    return previous[right_length];
}

static const RudaTaskHelp *ruda_find_task(const char *name) {
    size_t index = 0;
    for (index = 0; index < sizeof(ruda_tasks) / sizeof(ruda_tasks[0]); index += 1) {
        if (strcmp(ruda_tasks[index].name, name) == 0) {
            return &ruda_tasks[index];
        }
    }
    return NULL;
}

static const char *ruda_suggest_task(const char *name) {
    const char *suggestion = NULL;
    size_t best = 4;
    size_t index = 0;
    for (index = 0; index < sizeof(ruda_tasks) / sizeof(ruda_tasks[0]); index += 1) {
        size_t distance = ruda_edit_distance(name, ruda_tasks[index].name);
        if (distance < best) {
            best = distance;
            suggestion = ruda_tasks[index].name;
        }
    }
    return suggestion;
}

static const char *ruda_suggest_option(const char *name) {
    const char *suggestion = NULL;
    size_t best = 4;
    size_t index = 0;
    for (index = 0; index < sizeof(ruda_global_options) / sizeof(ruda_global_options[0]); index += 1) {
        size_t distance = ruda_edit_distance(name, ruda_global_options[index]);
        if (distance < best) {
            best = distance;
            suggestion = ruda_global_options[index];
        }
    }
    if (ruda_edit_distance(name, "--json") < best) {
        suggestion = "--json";
    }
    return suggestion;
}

static void ruda_display_argument(const char *argument, char *output, size_t capacity) {
    const unsigned char *cursor = (const unsigned char *)argument;
    size_t remaining = strlen(argument);
    size_t length = 0;
    if (capacity == 0) {
        return;
    }
    while (remaining != 0 && length + 1 < capacity) {
        unsigned char byte = *cursor;
        if (byte == '\n' || byte == '\r' || byte == '\t' || byte == '\\' || byte == '\'') {
            if (length + 2 >= capacity) {
                break;
            }
            output[length] = '\\';
            length += 1;
            output[length] = byte == '\n' ? 'n' : byte == '\r' ? 'r' : byte == '\t' ? 't' : (char)byte;
            length += 1;
            cursor += 1;
            remaining -= 1;
            continue;
        } else if (byte < 0x20U || byte == 0x7fU) {
            if (length + 1 >= capacity) {
                break;
            }
            output[length] = '?';
            length += 1;
            cursor += 1;
            remaining -= 1;
            continue;
        } else if (byte < 0x80U) {
            output[length] = (char)byte;
            length += 1;
            cursor += 1;
            remaining -= 1;
            continue;
        } else {
            RudaUtf8Scalar scalar;
            if (ruda_utf8_decode(cursor, remaining, &scalar)) {
                if (scalar.width > capacity - length - 1) {
                    break;
                }
                memcpy(output + length, cursor, scalar.width);
                length += scalar.width;
                cursor += scalar.width;
                remaining -= scalar.width;
                continue;
            }
            output[length] = '?';
            length += 1;
            cursor += 1;
            remaining -= 1;
        }
    }
    output[length] = '\0';
}

static int ruda_cli_diagnostic(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const char *code,
    size_t argument_index,
    const char *message
) {
    RudaDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    diagnostic.message = message;
    diagnostic.has_command_argument = true;
    diagnostic.command_argument = argument_index;
    if (!ruda_diagnostic_render(error_writer, style, &diagnostic)) {
        return RUDA_EXIT_INTERNAL;
    }
    return RUDA_EXIT_USAGE;
}

static int ruda_unknown_command(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const char *command,
    size_t argument_index
) {
    char display[128];
    char message[320];
    const char *suggestion = ruda_suggest_task(command);
    int length = 0;
    ruda_display_argument(command, display, sizeof(display));
    if (suggestion != NULL) {
        length = snprintf(
            message,
            sizeof(message),
            "unknown command '%s'; did you mean '%s'?",
            display,
            suggestion
        );
    } else {
        length = snprintf(message, sizeof(message), "unknown command '%s'", display);
    }
    if (length < 0 || (size_t)length >= sizeof(message)) {
        return RUDA_EXIT_INTERNAL;
    }
    return ruda_cli_diagnostic(
        error_writer,
        style,
        RUDA_DIAGNOSTIC_UNKNOWN_COMMAND,
        argument_index,
        message
    );
}

static int ruda_unknown_option(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const char *option,
    size_t argument_index
) {
    char display[128];
    char message[320];
    const char *suggestion = ruda_suggest_option(option);
    int length = 0;
    ruda_display_argument(option, display, sizeof(display));
    if (suggestion != NULL) {
        length = snprintf(
            message,
            sizeof(message),
            "unknown option '%s'; did you mean '%s'?",
            display,
            suggestion
        );
    } else {
        length = snprintf(message, sizeof(message), "unknown option '%s'", display);
    }
    if (length < 0 || (size_t)length >= sizeof(message)) {
        return RUDA_EXIT_INTERNAL;
    }
    return ruda_cli_diagnostic(
        error_writer,
        style,
        RUDA_DIAGNOSTIC_UNKNOWN_OPTION,
        argument_index,
        message
    );
}

static bool ruda_valid_diagnostic_style(const char *value, RudaDiagnosticStyle *style) {
    if (strcmp(value, "human") == 0) {
        *style = RUDA_DIAGNOSTIC_HUMAN;
        return true;
    }
    if (strcmp(value, "short") == 0) {
        *style = RUDA_DIAGNOSTIC_SHORT;
        return true;
    }
    if (strcmp(value, "json") == 0) {
        *style = RUDA_DIAGNOSTIC_JSON;
        return true;
    }
    return false;
}

static bool ruda_valid_color(const char *value) {
    return strcmp(value, "auto") == 0
        || strcmp(value, "always") == 0
        || strcmp(value, "never") == 0;
}

static int ruda_option_value_error(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const char *code,
    size_t argument_index,
    const char *message
) {
    return ruda_cli_diagnostic(error_writer, style, code, argument_index, message);
}

static int ruda_parse_options(
    int argument_count,
    const char *const arguments[],
    RudaWriter error_writer,
    RudaCliOptions *options
) {
    int index = 0;
    options->diagnostic_style = RUDA_DIAGNOSTIC_HUMAN;
    options->command = NULL;
    options->command_index = 0;
    options->help_target = NULL;
    options->help_target_index = 0;
    options->target = NULL;
    options->target_index = 0;
    options->top_level_help = false;
    options->command_help = false;
    options->version_json = false;
    options->version_json_index = 0;
    options->format_check = false;
    options->format_check_index = 0;
    options->format_diff = false;
    options->format_diff_index = 0;
    options->format_stdin = false;
    options->format_stdin_index = 0;
    options->quiet = false;
    options->verbose = false;

    for (index = 1; index < argument_count; index += 1) {
        const char *argument = arguments[index];
        const char *value = NULL;
        if (strcmp(argument, "--diagnostic") == 0 || strcmp(argument, "--color") == 0) {
            if (index + 1 >= argument_count) {
                char message[160];
                int length = snprintf(message, sizeof(message), "option '%s' requires a value", argument);
                if (length < 0 || (size_t)length >= sizeof(message)) {
                    return RUDA_EXIT_INTERNAL;
                }
                return ruda_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    RUDA_DIAGNOSTIC_MISSING_OPTION_VALUE,
                    (size_t)index,
                    message
                );
            }
            index += 1;
            value = arguments[index];
            if (strcmp(argument, "--diagnostic") == 0) {
                if (!ruda_valid_diagnostic_style(value, &options->diagnostic_style)) {
                    return ruda_option_value_error(
                        error_writer,
                        options->diagnostic_style,
                        RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                        (size_t)index,
                        "--diagnostic expects human, short, or json"
                    );
                }
            } else if (!ruda_valid_color(value)) {
                return ruda_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                    (size_t)index,
                    "--color expects auto, always, or never"
                );
            }
        } else if (ruda_starts_with(argument, "--diagnostic=")) {
            value = argument + strlen("--diagnostic=");
            if (!ruda_valid_diagnostic_style(value, &options->diagnostic_style)) {
                return ruda_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                    (size_t)index,
                    "--diagnostic expects human, short, or json"
                );
            }
        } else if (ruda_starts_with(argument, "--color=")) {
            value = argument + strlen("--color=");
            if (!ruda_valid_color(value)) {
                return ruda_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                    (size_t)index,
                    "--color expects auto, always, or never"
                );
            }
        } else if (strcmp(argument, "--quiet") == 0) {
            options->quiet = true;
        } else if (strcmp(argument, "--verbose") == 0) {
            options->verbose = true;
        } else if (strcmp(argument, "--help") == 0) {
            if (options->command == NULL) {
                options->top_level_help = true;
            } else {
                options->command_help = true;
            }
        } else if (strcmp(argument, "--version") == 0
            && options->command == NULL) {
            options->command = "version";
            options->command_index = (size_t)index;
        } else if (strcmp(argument, "--json") == 0) {
            options->version_json = true;
            options->version_json_index = (size_t)index;
        } else if (options->command != NULL
            && strcmp(options->command, "fmt") == 0
            && strcmp(argument, "--check") == 0) {
            options->format_check = true;
            options->format_check_index = (size_t)index;
        } else if (options->command != NULL
            && strcmp(options->command, "fmt") == 0
            && strcmp(argument, "--diff") == 0) {
            options->format_diff = true;
            options->format_diff_index = (size_t)index;
        } else if (options->command != NULL
            && strcmp(options->command, "fmt") == 0
            && strcmp(argument, "--stdin") == 0) {
            options->format_stdin = true;
            options->format_stdin_index = (size_t)index;
        } else if (argument[0] == '-') {
            return ruda_unknown_option(
                error_writer,
                options->diagnostic_style,
                argument,
                (size_t)index
            );
        } else if (options->command == NULL) {
            options->command = argument;
            options->command_index = (size_t)index;
        } else if (strcmp(options->command, "help") == 0 && options->help_target == NULL) {
            options->help_target = argument;
            options->help_target_index = (size_t)index;
        } else if (strcmp(options->command, "fmt") == 0
            && options->target == NULL) {
            options->target = argument;
            options->target_index = (size_t)index;
        } else {
            char display[128];
            char message[192];
            int length = 0;
            ruda_display_argument(argument, display, sizeof(display));
            length = snprintf(message, sizeof(message), "unexpected argument '%s'", display);
            if (length < 0 || (size_t)length >= sizeof(message)) {
                return RUDA_EXIT_INTERNAL;
            }
            return ruda_option_value_error(
                error_writer,
                options->diagnostic_style,
                RUDA_DIAGNOSTIC_UNEXPECTED_ARGUMENT,
                (size_t)index,
                message
            );
        }
    }
    return RUDA_EXIT_SUCCESS;
}

static bool ruda_write_top_help(RudaWriter writer) {
    static const char help[] =
        "Ruda toolchain (Stage 0 bootstrap)\n"
        "\n"
        "Usage: ruda <command> [options]\n"
        "\n"
        "Everyday workflow:\n"
        "  fmt       Canonically format source and manifests\n"
        "  check     Check source and verified typed MIR\n"
        "  test      Run built-in tests (VM by default)\n"
        "  run       Run a program (VM by default)\n"
        "  build     Build an executable (native by default)\n"
        "\n"
        "Packages and maintenance:\n"
        "  init add remove update list why outdated vendor audit\n"
        "  doc vet fix fuzz bench generate\n"
        "\n"
        "Inspection:\n"
        "  version   Show compiler and runtime build identity\n"
        "  explain   Explain diagnostics and inferred behavior\n"
        "  help      Show help for a command\n"
        "\n"
        "Global options:\n"
        "  --help\n"
        "  --version\n"
        "  --color auto|always|never\n"
        "  --diagnostic human|short|json\n"
        "  --quiet\n"
        "  --verbose\n"
        "\n"
        "Run 'ruda help <command>' for task-oriented examples.\n";
    return ruda_writer_write_cstr(writer, help);
}

static bool ruda_write_task_help(RudaWriter writer, const RudaTaskHelp *task) {
    if (!ruda_writer_write_cstr(writer, "Usage: ruda ")
        || !ruda_writer_write_cstr(writer, task->usage)
        || !ruda_writer_write_cstr(writer, "\n\n")
        || !ruda_writer_write_cstr(writer, task->summary)
        || !ruda_writer_write_cstr(writer, "\n\nExample:\n  ")
        || !ruda_writer_write_cstr(writer, task->examples)
        || !ruda_writer_write_cstr(writer, "\n")) {
        return false;
    }
    if (!task->implemented) {
        return ruda_writer_write_cstr(
            writer,
            "\nThis command is specified but not implemented in the current bootstrap slice.\n"
        );
    }
    return true;
}

static bool ruda_write_version(RudaWriter writer, bool json) {
    if (json) {
        static const char version_json[] =
            "{\"schema\":\"ruda.version/1\","
            "\"compiler\":\"" RUDA_COMPILER_VERSION "\","
            "\"runtime\":\"" RUDA_RUNTIME_VERSION "\","
            "\"language\":\"" RUDA_LANGUAGE_VERSION "\","
            "\"bytecode\":\"" RUDA_BYTECODE_VERSION "\","
            "\"standardLibrary\":\"" RUDA_STANDARD_LIBRARY_VERSION "\","
            "\"bootstrapStage\":" RUDA_STRINGIFY(RUDA_BOOTSTRAP_STAGE) ","
            "\"host\":{\"system\":\"" RUDA_HOST_SYSTEM "\","
            "\"processor\":\"" RUDA_HOST_PROCESSOR "\"},"
            "\"targets\":[\"native-host\"]}\n";
        return ruda_writer_write_cstr(writer, version_json);
    }
    return ruda_writer_write_cstr(writer, "Ruda compiler ")
        && ruda_writer_write_cstr(writer, RUDA_COMPILER_VERSION)
        && ruda_writer_write_cstr(writer, "\nruntime ")
        && ruda_writer_write_cstr(writer, RUDA_RUNTIME_VERSION)
        && ruda_writer_write_cstr(writer, "\nlanguage ")
        && ruda_writer_write_cstr(writer, RUDA_LANGUAGE_VERSION)
        && ruda_writer_write_cstr(writer, "\nbytecode ")
        && ruda_writer_write_cstr(writer, RUDA_BYTECODE_VERSION)
        && ruda_writer_write_cstr(writer, "\nstandard-library ")
        && ruda_writer_write_cstr(writer, RUDA_STANDARD_LIBRARY_VERSION)
        && ruda_writer_write_cstr(writer, "\nbootstrap-stage ")
        && ruda_writer_write_cstr(writer, RUDA_STRINGIFY(RUDA_BOOTSTRAP_STAGE))
        && ruda_writer_write_cstr(writer, "\nhost ")
        && ruda_writer_write_cstr(writer, RUDA_HOST_SYSTEM)
        && ruda_writer_write_cstr(writer, "/")
        && ruda_writer_write_cstr(writer, RUDA_HOST_PROCESSOR)
        && ruda_writer_write_cstr(writer, "\ntargets native-host\n");
}

static int ruda_cli_plain_error(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const char *code,
    const char *message,
    int exit_code
) {
    RudaDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    diagnostic.message = message;
    return ruda_diagnostic_render(error_writer, style, &diagnostic)
        ? exit_code
        : RUDA_EXIT_INTERNAL;
}

static bool ruda_cli_source_error(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const RudaSourceSnapshot *source,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    RudaDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    diagnostic.message = message;
    diagnostic.source = source;
    diagnostic.source_start = start;
    diagnostic.source_end = end;
    return ruda_diagnostic_render(error_writer, style, &diagnostic);
}

static bool ruda_cli_render_frontend_diagnostics(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const RudaLexResult *lexed,
    const RudaParseResult *parsed
) {
    size_t lex_index = 0;
    size_t parse_index = 0;
    while (lex_index < lexed->diagnostics.length
        || parse_index < parsed->diagnostics.length) {
        const RudaLexDiagnostic *lex_diagnostic = lex_index
                < lexed->diagnostics.length
            ? ruda_lex_diagnostic(lexed, lex_index)
            : NULL;
        const RudaParseDiagnostic *parse_diagnostic = parse_index
                < parsed->diagnostics.length
            ? ruda_parse_diagnostic(parsed, parse_index)
            : NULL;
        bool use_lex = parse_diagnostic == NULL
            || (lex_diagnostic != NULL
                && (lex_diagnostic->start < parse_diagnostic->start
                    || (lex_diagnostic->start == parse_diagnostic->start
                        && strcmp(
                            lex_diagnostic->code,
                            parse_diagnostic->code
                        ) <= 0)));
        if ((use_lex && lex_diagnostic == NULL)
            || (!use_lex && parse_diagnostic == NULL)) {
            return false;
        }
        if (use_lex) {
            if (!ruda_cli_source_error(
                    error_writer,
                    style,
                    lexed->snapshot,
                    lex_diagnostic->code,
                    lex_diagnostic->message,
                    lex_diagnostic->start,
                    lex_diagnostic->end
                )) {
                return false;
            }
            lex_index += 1;
        } else {
            if (!ruda_cli_source_error(
                    error_writer,
                    style,
                    lexed->snapshot,
                    parse_diagnostic->code,
                    parse_diagnostic->message,
                    parse_diagnostic->start,
                    parse_diagnostic->end
                )) {
                return false;
            }
            parse_index += 1;
        }
    }
    if (lexed->suppressed_diagnostics != 0
        || parsed->suppressed_diagnostics != 0) {
        char message[160];
        size_t suppressed = 0;
        int length = 0;
        if (!ruda_size_add(
                lexed->suppressed_diagnostics,
                parsed->suppressed_diagnostics,
                &suppressed
            )) {
            return false;
        }
        length = snprintf(
            message,
            sizeof(message),
            "%zu additional frontend diagnostics were suppressed by the configured limit",
            suppressed
        );
        if (length < 0 || (size_t)length >= sizeof(message)
            || !ruda_cli_source_error(
                error_writer,
                style,
                lexed->snapshot,
                RUDA_DIAGNOSTIC_FORMAT_LIMIT,
                message,
                ruda_source_length(lexed->snapshot),
                ruda_source_length(lexed->snapshot)
            )) {
            return false;
        }
    }
    return true;
}

typedef enum RudaCliFormatAction {
    RUDA_CLI_FORMAT_STDOUT,
    RUDA_CLI_FORMAT_CHECK,
    RUDA_CLI_FORMAT_DIFF,
    RUDA_CLI_FORMAT_WRITE
} RudaCliFormatAction;

typedef enum RudaCliFormatTargetStatus {
    RUDA_CLI_FORMAT_TARGET_OK,
    RUDA_CLI_FORMAT_TARGET_INPUT_ERROR,
    RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED,
    RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED
} RudaCliFormatTargetStatus;

typedef struct RudaCliFormatPath {
    RudaOwnedBytes path;
} RudaCliFormatPath;

typedef struct RudaCliFormatDirectory {
    RudaOwnedBytes path;
    size_t depth;
} RudaCliFormatDirectory;

enum {
    RUDA_CLI_FORMAT_MAX_PATH_BYTES = 32768,
    RUDA_CLI_FORMAT_MAX_NAME_BYTES = 4096,
    RUDA_CLI_FORMAT_MAX_DEPTH = 256,
    RUDA_CLI_FORMAT_MAX_DIRECTORIES = 100000,
    RUDA_CLI_FORMAT_MAX_ENTRIES = 1000000,
    RUDA_CLI_FORMAT_MAX_FILES = 100000
};

static bool ruda_cli_manifest_kind(
    const char *path,
    RudaManifestKind *kind
) {
    const char *base = path;
    const char *cursor = path;
    if (path == NULL || kind == NULL) {
        return false;
    }
    while (*cursor != '\0') {
        if (*cursor == '/' || *cursor == '\\') {
            base = cursor + 1;
        }
        cursor += 1;
    }
    if (strcmp(base, "ruda.mod") == 0) {
        *kind = RUDA_MANIFEST_MODULE;
        return true;
    }
    if (strcmp(base, "ruda.work") == 0) {
        *kind = RUDA_MANIFEST_WORKSPACE;
        return true;
    }
    return false;
}

static const char *ruda_cli_path_base(const char *path) {
    const char *base = path;
    const char *cursor = path;
    while (*cursor != '\0') {
        if (*cursor == '/' || *cursor == '\\') {
            base = cursor + 1;
        }
        cursor += 1;
    }
    return base;
}

static bool ruda_cli_source_path(const char *path) {
    const char *base = ruda_cli_path_base(path);
    size_t length = strlen(base);
    return length >= 5 && memcmp(base + length - 5, ".ruda", 5) == 0;
}

static bool ruda_cli_format_file_path(const char *path) {
    RudaManifestKind kind;
    return ruda_cli_source_path(path) || ruda_cli_manifest_kind(path, &kind);
}

static bool ruda_cli_write_size(RudaWriter writer, size_t value) {
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%zu", value);
    return length >= 0 && (size_t)length < sizeof(buffer)
        && ruda_writer_write(writer, buffer, (size_t)length);
}

static size_t ruda_cli_diff_line_count(
    const unsigned char *bytes,
    size_t length
) {
    size_t cursor = 0;
    size_t count = 0;
    while (cursor < length) {
        count += 1;
        while (cursor < length && bytes[cursor] != '\r'
            && bytes[cursor] != '\n') {
            cursor += 1;
        }
        if (cursor < length && bytes[cursor] == '\r'
            && cursor + 1 < length && bytes[cursor + 1] == '\n') {
            cursor += 2;
        } else if (cursor < length) {
            cursor += 1;
        }
    }
    return count;
}

static bool ruda_cli_write_diff_lines(
    RudaWriter writer,
    const char *prefix,
    const unsigned char *bytes,
    size_t length
) {
    size_t cursor = 0;
    while (cursor < length) {
        size_t start = cursor;
        bool newline = false;
        while (cursor < length && bytes[cursor] != '\r'
            && bytes[cursor] != '\n') {
            cursor += 1;
        }
        if (!ruda_writer_write_cstr(writer, prefix)
            || !ruda_writer_write(
                writer,
                (const char *)bytes + start,
                cursor - start
            )
            || !ruda_writer_write_cstr(writer, "\n")) {
            return false;
        }
        if (cursor < length && bytes[cursor] == '\r'
            && cursor + 1 < length && bytes[cursor + 1] == '\n') {
            cursor += 2;
            newline = true;
        } else if (cursor < length) {
            cursor += 1;
            newline = true;
        }
        if (!newline
            && !ruda_writer_write_cstr(
                writer,
                "\\ No newline at end of file\n"
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_cli_write_format_diff(
    RudaWriter writer,
    const char *path,
    const unsigned char *original,
    size_t original_length,
    const unsigned char *formatted,
    size_t formatted_length
) {
    char display[128];
    size_t original_lines = ruda_cli_diff_line_count(original, original_length);
    size_t formatted_lines = ruda_cli_diff_line_count(formatted, formatted_length);
    ruda_display_argument(path, display, sizeof(display));
    return ruda_writer_write_cstr(writer, "--- ")
        && ruda_writer_write_cstr(writer, display)
        && ruda_writer_write_cstr(writer, "\n+++ ")
        && ruda_writer_write_cstr(writer, display)
        && ruda_writer_write_cstr(writer, "\n@@ -")
        && ruda_cli_write_size(writer, original_lines == 0 ? 0 : 1)
        && ruda_writer_write_cstr(writer, ",")
        && ruda_cli_write_size(writer, original_lines)
        && ruda_writer_write_cstr(writer, " +")
        && ruda_cli_write_size(writer, formatted_lines == 0 ? 0 : 1)
        && ruda_writer_write_cstr(writer, ",")
        && ruda_cli_write_size(writer, formatted_lines)
        && ruda_writer_write_cstr(writer, " @@\n")
        && ruda_cli_write_diff_lines(
            writer,
            "-",
            original,
            original_length
        )
        && ruda_cli_write_diff_lines(
            writer,
            "+",
            formatted,
            formatted_length
        );
}

static RudaFileStatus ruda_cli_replace_formatted_file(
    RudaAllocator allocator,
    const char *path,
    const unsigned char *bytes,
    size_t length
) {
    RudaVector temporary;
    size_t base_length = strlen(path);
    size_t attempt = 0;
    RudaFileStatus status = RUDA_FILE_ALREADY_EXISTS;
    if (!ruda_vector_init(&temporary, allocator, sizeof(char))
        || !ruda_vector_append(&temporary, path, base_length)) {
        return RUDA_FILE_ALLOCATION_FAILED;
    }
    for (attempt = 0; attempt < 128; attempt += 1) {
        char suffix[32];
        int suffix_length = snprintf(
            suffix,
            sizeof(suffix),
            ".ruda-fmt-%03zu.tmp",
            attempt
        );
        temporary.length = base_length;
        if (suffix_length < 0 || (size_t)suffix_length >= sizeof(suffix)
            || !ruda_vector_append(
                &temporary,
                suffix,
                (size_t)suffix_length
            )) {
            status = RUDA_FILE_ALLOCATION_FAILED;
            break;
        }
        status = ruda_file_replace_atomic(
            allocator,
            temporary.data,
            temporary.length,
            path,
            base_length,
            bytes,
            length
        );
        if (status != RUDA_FILE_ALREADY_EXISTS) {
            break;
        }
    }
    ruda_vector_dispose(&temporary);
    return status;
}

static int ruda_cli_finish_format(
    RudaSession *session,
    const RudaCliOptions *options,
    const char *path,
    const RudaOwnedBytes *input,
    const RudaSourceSnapshot *source,
    const unsigned char *formatted,
    size_t formatted_length,
    bool changed,
    RudaCliFormatAction action,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    if (action == RUDA_CLI_FORMAT_STDOUT) {
        if (!ruda_writer_write(
                standard_output,
                (const char *)formatted,
                formatted_length
            )) {
            return ruda_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                RUDA_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                "cannot write canonical formatter output",
                RUDA_EXIT_SOURCE_FAILURE
            );
        }
    } else if (action == RUDA_CLI_FORMAT_CHECK && changed) {
        return ruda_cli_source_error(
            standard_error,
            options->diagnostic_style,
            source,
            RUDA_DIAGNOSTIC_FORMAT_CHECK_FAILED,
            "file does not match canonical formatting",
            0,
            ruda_source_length(source)
        ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
    } else if (action == RUDA_CLI_FORMAT_DIFF && changed) {
        if (!ruda_cli_write_format_diff(
                standard_output,
                path,
                input->data,
                input->length,
                formatted,
                formatted_length
            )) {
            return ruda_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                RUDA_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                "cannot write canonical formatter diff",
                RUDA_EXIT_SOURCE_FAILURE
            );
        }
    } else if (action == RUDA_CLI_FORMAT_WRITE && changed) {
        RudaFileStatus file_status = ruda_cli_replace_formatted_file(
            session->allocator,
            path,
            formatted,
            formatted_length
        );
        if (file_status != RUDA_FILE_OK) {
            return ruda_cli_plain_error(
                standard_error,
                options->diagnostic_style,
                RUDA_DIAGNOSTIC_FORMAT_OUTPUT_IO,
                file_status == RUDA_FILE_ALLOCATION_FAILED
                    ? "cannot allocate transactional formatter output"
                    : "cannot transactionally replace formatted file",
                file_status == RUDA_FILE_ALLOCATION_FAILED
                    ? RUDA_EXIT_INTERNAL
                    : RUDA_EXIT_SOURCE_FAILURE
            );
        }
    }
    return RUDA_EXIT_SUCCESS;
}

static int ruda_cli_format_source_loaded(
    RudaSession *session,
    const RudaCliOptions *options,
    const char *path,
    const RudaOwnedBytes *input,
    RudaCliFormatAction action,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaSourceManager sources;
    const RudaSourceSnapshot *source = NULL;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaFormatResult formatted;
    RudaLexStatus lex_status;
    RudaParseStatus parse_status;
    RudaFormatStatus format_status;
    int result = RUDA_EXIT_INTERNAL;
    bool sources_initialized = false;
    memset(&sources, 0, sizeof(sources));
    memset(&lexed, 0, sizeof(lexed));
    memset(&parsed, 0, sizeof(parsed));
    memset(&formatted, 0, sizeof(formatted));
    sources_initialized = ruda_source_manager_init(&sources, session->allocator);
    if (!sources_initialized
        || ruda_source_manager_add(
            &sources,
            path,
            strlen(path),
            input->data,
            input->length,
            &source
        ) != RUDA_SOURCE_OK) {
        goto cleanup;
    }
    lex_status = ruda_lex_snapshot(
        source,
        session->allocator,
        ruda_lex_default_options(),
        &lexed
    );
    if (lex_status == RUDA_LEX_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter lexer exhausted a defensive limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (lex_status != RUDA_LEX_OK) {
        goto cleanup;
    }
    parse_status = ruda_parse_file(
        &lexed,
        session->allocator,
        ruda_parse_default_options(),
        &parsed
    );
    if (parse_status == RUDA_PARSE_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter parser exhausted a defensive limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (parse_status != RUDA_PARSE_OK) {
        goto cleanup;
    }
    if (lexed.diagnostics.length != 0 || parsed.diagnostics.length != 0
        || lexed.suppressed_diagnostics != 0
        || parsed.suppressed_diagnostics != 0) {
        result = ruda_cli_render_frontend_diagnostics(
            standard_error,
            options->diagnostic_style,
            &lexed,
            &parsed
        ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
        goto cleanup;
    }
    format_status = ruda_format_source(
        &lexed,
        &parsed,
        session->allocator,
        ruda_format_default_options(),
        &formatted
    );
    if (format_status == RUDA_FORMAT_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "formatter exhausted a defensive document or output limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (format_status != RUDA_FORMAT_OK) {
        goto cleanup;
    }
    result = ruda_cli_finish_format(
        session,
        options,
        path,
        input,
        source,
        formatted.output.data,
        formatted.output.length,
        formatted.changed,
        action,
        standard_output,
        standard_error
    );

cleanup:
    ruda_format_result_dispose(&formatted);
    ruda_parse_result_dispose(&parsed);
    ruda_lex_result_dispose(&lexed);
    if (sources_initialized) {
        ruda_source_manager_dispose(&sources);
    }
    return result;
}

static bool ruda_cli_render_manifest_diagnostics(
    RudaWriter error_writer,
    RudaDiagnosticStyle style,
    const RudaManifestParseResult *parsed
) {
    size_t index = 0;
    for (index = 0; index < parsed->diagnostics.length; index += 1) {
        const RudaManifestDiagnostic *diagnostic = ruda_manifest_diagnostic(
            parsed,
            index
        );
        if (diagnostic == NULL
            || !ruda_cli_source_error(
                error_writer,
                style,
                parsed->snapshot,
                diagnostic->code,
                diagnostic->message,
                diagnostic->start,
                diagnostic->end
            )) {
            return false;
        }
    }
    if (parsed->suppressed_diagnostics != 0) {
        char message[160];
        int length = snprintf(
            message,
            sizeof(message),
            "%zu additional manifest diagnostics were suppressed by the configured limit",
            parsed->suppressed_diagnostics
        );
        if (length < 0 || (size_t)length >= sizeof(message)
            || !ruda_cli_source_error(
                error_writer,
                style,
                parsed->snapshot,
                RUDA_DIAGNOSTIC_FORMAT_LIMIT,
                message,
                ruda_source_length(parsed->snapshot),
                ruda_source_length(parsed->snapshot)
            )) {
            return false;
        }
    }
    return true;
}

static int ruda_cli_format_manifest_loaded(
    RudaSession *session,
    const RudaCliOptions *options,
    const char *path,
    const RudaOwnedBytes *input,
    RudaManifestKind kind,
    RudaCliFormatAction action,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaSourceManager sources;
    const RudaSourceSnapshot *source = NULL;
    RudaManifestParseResult parsed;
    RudaManifestFormatResult formatted;
    RudaManifestParseStatus parse_status;
    RudaManifestFormatStatus format_status;
    int result = RUDA_EXIT_INTERNAL;
    bool sources_initialized = false;
    memset(&sources, 0, sizeof(sources));
    memset(&parsed, 0, sizeof(parsed));
    memset(&formatted, 0, sizeof(formatted));
    sources_initialized = ruda_source_manager_init(&sources, session->allocator);
    if (!sources_initialized
        || ruda_source_manager_add(
            &sources,
            path,
            strlen(path),
            input->data,
            input->length,
            &source
        ) != RUDA_SOURCE_OK) {
        goto cleanup;
    }
    parse_status = ruda_manifest_parse(
        source,
        kind,
        session->allocator,
        ruda_manifest_parse_default_options(),
        &parsed
    );
    if (parse_status == RUDA_MANIFEST_PARSE_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "manifest parser exhausted a defensive entry limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (parse_status != RUDA_MANIFEST_PARSE_OK) {
        goto cleanup;
    }
    if (parsed.diagnostics.length != 0
        || parsed.suppressed_diagnostics != 0) {
        result = ruda_cli_render_manifest_diagnostics(
            standard_error,
            options->diagnostic_style,
            &parsed
        ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
        goto cleanup;
    }
    format_status = ruda_manifest_format(
        &parsed,
        session->allocator,
        ruda_manifest_format_default_options(),
        &formatted
    );
    if (format_status == RUDA_MANIFEST_FORMAT_LIMIT_REACHED) {
        result = ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_LIMIT,
            "manifest formatter exhausted a defensive decoded or output limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
        goto cleanup;
    }
    if (format_status != RUDA_MANIFEST_FORMAT_OK) {
        goto cleanup;
    }
    result = ruda_cli_finish_format(
        session,
        options,
        path,
        input,
        source,
        formatted.output.data,
        formatted.output.length,
        formatted.changed,
        action,
        standard_output,
        standard_error
    );

cleanup:
    ruda_manifest_format_result_dispose(&formatted);
    ruda_manifest_parse_result_dispose(&parsed);
    if (sources_initialized) {
        ruda_source_manager_dispose(&sources);
    }
    return result;
}

static int ruda_cli_format_loaded(
    RudaSession *session,
    const RudaCliOptions *options,
    const char *path,
    const RudaOwnedBytes *input,
    RudaCliFormatAction action,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaManifestKind kind;
    if (ruda_cli_manifest_kind(path, &kind)) {
        return ruda_cli_format_manifest_loaded(
            session,
            options,
            path,
            input,
            kind,
            action,
            standard_output,
            standard_error
        );
    }
    return ruda_cli_format_source_loaded(
        session,
        options,
        path,
        input,
        action,
        standard_output,
        standard_error
    );
}

static int ruda_cli_format_stdin(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaReadStream standard_input,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    enum { RUDA_FORMAT_INPUT_MAX_BYTES = 64 * 1024 * 1024 };
    RudaOwnedBytes input = {0};
    RudaReadAllStatus read_status;
    int result = RUDA_EXIT_INTERNAL;
    const char *path = options->target == NULL ? "<stdin>" : options->target;
    if (standard_input.read == NULL) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_FORMAT_INPUT_IO,
            "cannot read formatter standard input",
            RUDA_EXIT_SOURCE_FAILURE
        );
    }
    read_status = ruda_read_all(
        standard_input,
        session->allocator,
        RUDA_FORMAT_INPUT_MAX_BYTES,
        &input
    );
    if (read_status != RUDA_READ_ALL_OK) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            read_status == RUDA_READ_ALL_LIMIT_EXCEEDED
                ? RUDA_DIAGNOSTIC_FORMAT_LIMIT
                : RUDA_DIAGNOSTIC_FORMAT_INPUT_IO,
            read_status == RUDA_READ_ALL_LIMIT_EXCEEDED
                ? "formatter input exceeds the 64 MiB stdin limit"
                : (read_status == RUDA_READ_ALL_ALLOCATION_FAILED
                    ? "cannot allocate formatter standard input"
                    : "cannot read formatter standard input"),
            read_status == RUDA_READ_ALL_ALLOCATION_FAILED
                ? RUDA_EXIT_INTERNAL
                : RUDA_EXIT_SOURCE_FAILURE
        );
    }
    result = ruda_cli_format_loaded(
        session,
        options,
        path,
        &input,
        RUDA_CLI_FORMAT_STDOUT,
        standard_output,
        standard_error
    );
    ruda_owned_bytes_dispose(&input);
    return result;
}

static int ruda_cli_format_file(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    enum { RUDA_FORMAT_INPUT_MAX_BYTES = 64 * 1024 * 1024 };
    RudaOwnedBytes input = {0};
    RudaFileStatus file_status = ruda_file_read_all(
        session->allocator,
        options->target,
        strlen(options->target),
        RUDA_FORMAT_INPUT_MAX_BYTES,
        &input
    );
    RudaCliFormatAction action = options->format_check
        ? RUDA_CLI_FORMAT_CHECK
        : (options->format_diff ? RUDA_CLI_FORMAT_DIFF : RUDA_CLI_FORMAT_WRITE);
    int result = RUDA_EXIT_INTERNAL;
    if (file_status != RUDA_FILE_OK) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            file_status == RUDA_FILE_LIMIT_EXCEEDED
                ? RUDA_DIAGNOSTIC_FORMAT_LIMIT
                : RUDA_DIAGNOSTIC_FORMAT_INPUT_IO,
            file_status == RUDA_FILE_LIMIT_EXCEEDED
                ? "formatter input exceeds the 64 MiB file limit"
                : (file_status == RUDA_FILE_ALLOCATION_FAILED
                    ? "cannot allocate formatter file input"
                    : "cannot read formatter file input"),
            file_status == RUDA_FILE_ALLOCATION_FAILED
                ? RUDA_EXIT_INTERNAL
                : RUDA_EXIT_SOURCE_FAILURE
        );
    }
    result = ruda_cli_format_loaded(
        session,
        options,
        options->target,
        &input,
        action,
        standard_output,
        standard_error
    );
    ruda_owned_bytes_dispose(&input);
    return result;
}

static RudaCliFormatTargetStatus ruda_cli_format_target_file_status(
    RudaFileStatus status
) {
    if (status == RUDA_FILE_OK) {
        return RUDA_CLI_FORMAT_TARGET_OK;
    }
    if (status == RUDA_FILE_ALLOCATION_FAILED) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    if (status == RUDA_FILE_LIMIT_EXCEEDED) {
        return RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    return RUDA_CLI_FORMAT_TARGET_INPUT_ERROR;
}

static bool ruda_cli_path_equal(
    const RudaOwnedBytes *left,
    const RudaOwnedBytes *right
) {
    return left->length == right->length
        && memcmp(left->data, right->data, left->length) == 0;
}

static bool ruda_cli_path_copy(
    RudaOwnedBytes *output,
    RudaAllocator allocator,
    const char *path,
    size_t length
) {
    if (length > RUDA_CLI_FORMAT_MAX_PATH_BYTES) {
        return false;
    }
    return ruda_owned_bytes_copy(output, allocator, path, length);
}

static RudaCliFormatTargetStatus ruda_cli_path_join(
    RudaAllocator allocator,
    const RudaOwnedBytes *parent,
    const char *name,
    size_t name_length,
    RudaOwnedBytes *output
) {
    bool separator = parent->length != 0
        && parent->data[parent->length - 1] != '/'
        && parent->data[parent->length - 1] != '\\';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (!ruda_size_add(parent->length, separator ? 1 : 0, &length)
        || !ruda_size_add(length, name_length, &length)
        || length > RUDA_CLI_FORMAT_MAX_PATH_BYTES
        || !ruda_size_add(length, 1, &allocation_size)) {
        return RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    bytes = ruda_allocate(&allocator, allocation_size);
    if (bytes == NULL) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    memcpy(bytes, parent->data, parent->length);
    if (separator) {
        bytes[parent->length] = '/';
    }
    memcpy(
        bytes + parent->length + (separator ? 1 : 0),
        name,
        name_length
    );
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return RUDA_CLI_FORMAT_TARGET_OK;
}

static bool ruda_cli_path_is_root(const RudaOwnedBytes *path) {
    const unsigned char *bytes = path->data;
    size_t first_share_separator = SIZE_MAX;
    size_t index = 0;
    if (path->length == 1 && bytes[0] == '/') {
        return true;
    }
    if (path->length == 3
        && ((bytes[0] >= 'A' && bytes[0] <= 'Z')
            || (bytes[0] >= 'a' && bytes[0] <= 'z'))
        && bytes[1] == ':' && bytes[2] == '/') {
        return true;
    }
    if (path->length < 5 || bytes[0] != '/' || bytes[1] != '/') {
        return false;
    }
    for (index = 2; index < path->length; index += 1) {
        if (bytes[index] == '/') {
            first_share_separator = index;
            break;
        }
    }
    return first_share_separator != SIZE_MAX
        && memchr(
            bytes + first_share_separator + 1,
            '/',
            path->length - first_share_separator - 1
        ) == NULL;
}

static bool ruda_cli_path_parent(RudaOwnedBytes *path) {
    size_t length = path->length;
    size_t separator = SIZE_MAX;
    if (ruda_cli_path_is_root(path)) {
        return false;
    }
    while (length > 1 && path->data[length - 1] == '/') {
        length -= 1;
    }
    while (length != 0) {
        if (path->data[length - 1] == '/') {
            separator = length - 1;
            break;
        }
        length -= 1;
    }
    if (separator == SIZE_MAX) {
        return false;
    }
    if (separator == 0) {
        path->length = 1;
    } else if (separator == 2 && path->data[1] == ':') {
        path->length = 3;
    } else {
        path->length = separator;
    }
    path->data[path->length] = 0;
    return true;
}

static RudaCliFormatTargetStatus ruda_cli_format_target_add_owned(
    RudaVector *targets,
    RudaOwnedBytes *path
) {
    RudaCliFormatPath target;
    if (targets->length >= RUDA_CLI_FORMAT_MAX_FILES) {
        return RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    memset(&target, 0, sizeof(target));
    target.path = *path;
    if (!ruda_vector_push(targets, &target)) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    memset(path, 0, sizeof(*path));
    return RUDA_CLI_FORMAT_TARGET_OK;
}

static RudaCliFormatTargetStatus ruda_cli_format_target_add_copy(
    RudaVector *targets,
    RudaAllocator allocator,
    const char *path,
    size_t length
) {
    RudaOwnedBytes owned = {0};
    RudaCliFormatTargetStatus status;
    if (length > RUDA_CLI_FORMAT_MAX_PATH_BYTES) {
        return RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
    }
    if (!ruda_owned_bytes_copy(&owned, allocator, path, length)) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    status = ruda_cli_format_target_add_owned(targets, &owned);
    ruda_owned_bytes_dispose(&owned);
    return status;
}

static void ruda_cli_format_targets_dispose(RudaVector *targets) {
    size_t index = 0;
    for (index = 0; index < targets->length; index += 1) {
        RudaCliFormatPath *target = ruda_vector_at(targets, index);
        ruda_owned_bytes_dispose(&target->path);
    }
    ruda_vector_dispose(targets);
}

static int ruda_cli_format_path_compare(const void *left, const void *right) {
    const RudaCliFormatPath *first = left;
    const RudaCliFormatPath *second = right;
    size_t common = first->path.length < second->path.length
        ? first->path.length
        : second->path.length;
    int comparison = common == 0
        ? 0
        : memcmp(first->path.data, second->path.data, common);
    if (comparison != 0) {
        return comparison;
    }
    return first->path.length < second->path.length
        ? -1
        : (first->path.length > second->path.length ? 1 : 0);
}

static void ruda_cli_format_targets_sort_unique(RudaVector *targets) {
    size_t read_index = 0;
    size_t write_index = 0;
    if (targets->length > 1) {
        qsort(
            targets->data,
            targets->length,
            sizeof(RudaCliFormatPath),
            ruda_cli_format_path_compare
        );
    }
    for (read_index = 0; read_index < targets->length; read_index += 1) {
        RudaCliFormatPath *current = ruda_vector_at(targets, read_index);
        RudaCliFormatPath *previous = write_index == 0
            ? NULL
            : ruda_vector_at(targets, write_index - 1);
        if (previous != NULL && ruda_cli_path_equal(
                &previous->path,
                &current->path
            )) {
            ruda_owned_bytes_dispose(&current->path);
            continue;
        }
        if (write_index != read_index) {
            RudaCliFormatPath *destination = ruda_vector_at(targets, write_index);
            *destination = *current;
            memset(current, 0, sizeof(*current));
        }
        write_index += 1;
    }
    targets->length = write_index;
}

static bool ruda_cli_format_ignored_directory(const RudaOwnedBytes *name) {
    return (name->length == 4 && memcmp(name->data, ".git", 4) == 0)
        || (name->length == 5 && memcmp(name->data, ".ruda", 5) == 0);
}

static RudaCliFormatTargetStatus ruda_cli_collect_format_tree(
    RudaAllocator allocator,
    const RudaOwnedBytes *root,
    RudaVector *targets
) {
    RudaVector directories;
    RudaCliFormatDirectory initial;
    RudaCliFormatTargetStatus status = RUDA_CLI_FORMAT_TARGET_OK;
    size_t directory_count = 0;
    size_t entry_count = 0;
    if (!ruda_vector_init(
            &directories,
            allocator,
            sizeof(RudaCliFormatDirectory)
        )) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    memset(&initial, 0, sizeof(initial));
    if (!ruda_cli_path_copy(
            &initial.path,
            allocator,
            (const char *)root->data,
            root->length
        ) || !ruda_vector_push(&directories, &initial)) {
        ruda_owned_bytes_dispose(&initial.path);
        ruda_vector_dispose(&directories);
        return root->length > RUDA_CLI_FORMAT_MAX_PATH_BYTES
            ? RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED
            : RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    while (directories.length != 0
        && status == RUDA_CLI_FORMAT_TARGET_OK) {
        RudaCliFormatDirectory directory = *(
            RudaCliFormatDirectory *
        )ruda_vector_at(&directories, directories.length - 1);
        RudaDirectoryIterator iterator;
        RudaFileStatus file_status;
        bool root_directory = ruda_cli_path_equal(&directory.path, root);
        directories.length -= 1;
        directory_count += 1;
        if (directory_count > RUDA_CLI_FORMAT_MAX_DIRECTORIES) {
            ruda_owned_bytes_dispose(&directory.path);
            status = RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
            break;
        }
        file_status = ruda_directory_open(
            allocator,
            (const char *)directory.path.data,
            directory.path.length,
            &iterator
        );
        status = ruda_cli_format_target_file_status(file_status);
        if (status != RUDA_CLI_FORMAT_TARGET_OK) {
            ruda_owned_bytes_dispose(&directory.path);
            break;
        }
        for (;;) {
            RudaOwnedBytes name = {0};
            RudaOwnedBytes child = {0};
            RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
            bool end = false;
            file_status = ruda_directory_next(
                &iterator,
                allocator,
                RUDA_CLI_FORMAT_MAX_NAME_BYTES,
                &name,
                &kind,
                &end
            );
            status = ruda_cli_format_target_file_status(file_status);
            if (status != RUDA_CLI_FORMAT_TARGET_OK || end) {
                ruda_owned_bytes_dispose(&name);
                break;
            }
            entry_count += 1;
            if (entry_count > RUDA_CLI_FORMAT_MAX_ENTRIES) {
                ruda_owned_bytes_dispose(&name);
                status = RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
                break;
            }
            status = ruda_cli_path_join(
                allocator,
                &directory.path,
                (const char *)name.data,
                name.length,
                &child
            );
            if (status != RUDA_CLI_FORMAT_TARGET_OK) {
                ruda_owned_bytes_dispose(&name);
                break;
            }
            if (kind == RUDA_DIRECTORY_ENTRY_REGULAR
                && (ruda_cli_source_path((const char *)child.data)
                    || (root_directory
                        && ruda_cli_manifest_kind(
                            (const char *)child.data,
                            &(RudaManifestKind){0}
                        )))) {
                status = ruda_cli_format_target_add_owned(targets, &child);
            } else if (kind == RUDA_DIRECTORY_ENTRY_DIRECTORY
                && !ruda_cli_format_ignored_directory(&name)) {
                RudaOwnedBytes manifest = {0};
                RudaDirectoryEntryKind manifest_kind = RUDA_DIRECTORY_ENTRY_MISSING;
                status = ruda_cli_path_join(
                    allocator,
                    &child,
                    "ruda.mod",
                    strlen("ruda.mod"),
                    &manifest
                );
                if (status == RUDA_CLI_FORMAT_TARGET_OK) {
                    file_status = ruda_directory_kind(
                        allocator,
                        (const char *)manifest.data,
                        manifest.length,
                        &manifest_kind
                    );
                    status = ruda_cli_format_target_file_status(file_status);
                }
                ruda_owned_bytes_dispose(&manifest);
                if (status == RUDA_CLI_FORMAT_TARGET_OK
                    && manifest_kind != RUDA_DIRECTORY_ENTRY_REGULAR) {
                    RudaCliFormatDirectory nested;
                    memset(&nested, 0, sizeof(nested));
                    if (directory.depth >= RUDA_CLI_FORMAT_MAX_DEPTH
                        || directories.length
                            >= RUDA_CLI_FORMAT_MAX_DIRECTORIES) {
                        status = RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED;
                    } else {
                        nested.path = child;
                        nested.depth = directory.depth + 1;
                        if (!ruda_vector_push(&directories, &nested)) {
                            status = RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
                        } else {
                            memset(&child, 0, sizeof(child));
                        }
                    }
                }
            }
            ruda_owned_bytes_dispose(&child);
            ruda_owned_bytes_dispose(&name);
            if (status != RUDA_CLI_FORMAT_TARGET_OK) {
                break;
            }
        }
        file_status = ruda_directory_close(&iterator);
        if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            status = ruda_cli_format_target_file_status(file_status);
        }
        ruda_owned_bytes_dispose(&directory.path);
    }
    while (directories.length != 0) {
        RudaCliFormatDirectory *directory = ruda_vector_at(
            &directories,
            directories.length - 1
        );
        ruda_owned_bytes_dispose(&directory->path);
        directories.length -= 1;
    }
    ruda_vector_dispose(&directories);
    return status;
}

static RudaCliFormatTargetStatus ruda_cli_discover_format_roots(
    RudaAllocator allocator,
    RudaOwnedBytes *module_root,
    RudaOwnedBytes *workspace_manifest
) {
    RudaOwnedBytes cursor = {0};
    RudaCliFormatTargetStatus status;
    RudaFileStatus file_status = ruda_directory_current(
        allocator,
        RUDA_CLI_FORMAT_MAX_PATH_BYTES,
        &cursor
    );
    memset(module_root, 0, sizeof(*module_root));
    memset(workspace_manifest, 0, sizeof(*workspace_manifest));
    status = ruda_cli_format_target_file_status(file_status);
    while (status == RUDA_CLI_FORMAT_TARGET_OK) {
        RudaOwnedBytes candidate = {0};
        RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
        status = ruda_cli_path_join(
            allocator,
            &cursor,
            "ruda.mod",
            strlen("ruda.mod"),
            &candidate
        );
        if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            file_status = ruda_directory_kind(
                allocator,
                (const char *)candidate.data,
                candidate.length,
                &kind
            );
            status = ruda_cli_format_target_file_status(file_status);
        }
        ruda_owned_bytes_dispose(&candidate);
        if (status != RUDA_CLI_FORMAT_TARGET_OK) {
            break;
        }
        if (kind == RUDA_DIRECTORY_ENTRY_REGULAR) {
            *module_root = cursor;
            memset(&cursor, 0, sizeof(cursor));
            break;
        }
        if (!ruda_cli_path_parent(&cursor)) {
            status = RUDA_CLI_FORMAT_TARGET_INPUT_ERROR;
            break;
        }
    }
    ruda_owned_bytes_dispose(&cursor);
    if (status != RUDA_CLI_FORMAT_TARGET_OK) {
        return status;
    }
    if (!ruda_owned_bytes_copy(
            &cursor,
            allocator,
            module_root->data,
            module_root->length
        )) {
        return RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
    }
    for (;;) {
        RudaOwnedBytes candidate = {0};
        RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
        status = ruda_cli_path_join(
            allocator,
            &cursor,
            "ruda.work",
            strlen("ruda.work"),
            &candidate
        );
        if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            file_status = ruda_directory_kind(
                allocator,
                (const char *)candidate.data,
                candidate.length,
                &kind
            );
            status = ruda_cli_format_target_file_status(file_status);
        }
        if (status != RUDA_CLI_FORMAT_TARGET_OK) {
            ruda_owned_bytes_dispose(&candidate);
            break;
        }
        if (kind == RUDA_DIRECTORY_ENTRY_REGULAR) {
            *workspace_manifest = candidate;
            memset(&candidate, 0, sizeof(candidate));
            ruda_owned_bytes_dispose(&candidate);
            break;
        }
        ruda_owned_bytes_dispose(&candidate);
        if (!ruda_cli_path_parent(&cursor)) {
            break;
        }
    }
    ruda_owned_bytes_dispose(&cursor);
    return status;
}

static int ruda_cli_format_target_status_error(
    const RudaCliOptions *options,
    RudaCliFormatTargetStatus status,
    RudaWriter standard_error
) {
    if (status == RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED) {
        return RUDA_EXIT_INTERNAL;
    }
    return ruda_cli_plain_error(
        standard_error,
        options->diagnostic_style,
        status == RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED
            ? RUDA_DIAGNOSTIC_FORMAT_LIMIT
            : RUDA_DIAGNOSTIC_FORMAT_INPUT_IO,
        status == RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED
            ? "formatter target discovery exhausted a defensive path or tree limit"
            : "cannot discover or traverse the selected formatter target",
        RUDA_EXIT_SOURCE_FAILURE
    );
}

static int ruda_cli_format_selected_files(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaVector *targets,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    size_t index = 0;
    int aggregate = RUDA_EXIT_SUCCESS;
    for (index = 0; index < targets->length; index += 1) {
        const RudaCliFormatPath *target = ruda_vector_at_const(targets, index);
        RudaCliOptions file_options = *options;
        int result;
        file_options.target = (const char *)target->path.data;
        result = ruda_cli_format_file(
            session,
            &file_options,
            standard_output,
            standard_error
        );
        if (result == RUDA_EXIT_INTERNAL) {
            return result;
        }
        if (result != RUDA_EXIT_SUCCESS) {
            aggregate = result;
        }
    }
    return aggregate;
}

static int ruda_cli_format_target(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaVector targets;
    RudaCliFormatTargetStatus status = RUDA_CLI_FORMAT_TARGET_OK;
    RudaOwnedBytes module_root = {0};
    RudaOwnedBytes workspace_manifest = {0};
    if (!ruda_vector_init(
            &targets,
            session->allocator,
            sizeof(RudaCliFormatPath)
        )) {
        return RUDA_EXIT_INTERNAL;
    }
    if (options->target == NULL) {
        status = ruda_cli_discover_format_roots(
            session->allocator,
            &module_root,
            &workspace_manifest
        );
        if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            status = ruda_cli_collect_format_tree(
                session->allocator,
                &module_root,
                &targets
            );
        }
        if (status == RUDA_CLI_FORMAT_TARGET_OK
            && workspace_manifest.data != NULL) {
            status = ruda_cli_format_target_add_owned(
                &targets,
                &workspace_manifest
            );
        }
    } else {
        RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
        RudaFileStatus file_status = ruda_directory_kind(
            session->allocator,
            options->target,
            strlen(options->target),
            &kind
        );
        status = ruda_cli_format_target_file_status(file_status);
        if (status == RUDA_CLI_FORMAT_TARGET_OK
            && kind == RUDA_DIRECTORY_ENTRY_REGULAR
            && ruda_cli_format_file_path(options->target)) {
            status = ruda_cli_format_target_add_copy(
                &targets,
                session->allocator,
                options->target,
                strlen(options->target)
            );
        } else if (status == RUDA_CLI_FORMAT_TARGET_OK
            && kind == RUDA_DIRECTORY_ENTRY_DIRECTORY) {
            if (!ruda_cli_path_copy(
                    &module_root,
                    session->allocator,
                    options->target,
                    strlen(options->target)
                )) {
                status = strlen(options->target)
                        > RUDA_CLI_FORMAT_MAX_PATH_BYTES
                    ? RUDA_CLI_FORMAT_TARGET_LIMIT_REACHED
                    : RUDA_CLI_FORMAT_TARGET_ALLOCATION_FAILED;
            } else {
                status = ruda_cli_collect_format_tree(
                    session->allocator,
                    &module_root,
                    &targets
                );
            }
        } else if (status == RUDA_CLI_FORMAT_TARGET_OK) {
            status = RUDA_CLI_FORMAT_TARGET_INPUT_ERROR;
        }
    }
    ruda_owned_bytes_dispose(&workspace_manifest);
    ruda_owned_bytes_dispose(&module_root);
    if (status != RUDA_CLI_FORMAT_TARGET_OK) {
        int result = ruda_cli_format_target_status_error(
            options,
            status,
            standard_error
        );
        ruda_cli_format_targets_dispose(&targets);
        return result;
    }
    ruda_cli_format_targets_sort_unique(&targets);
    {
        int result = ruda_cli_format_selected_files(
            session,
            options,
            &targets,
            standard_output,
            standard_error
        );
        ruda_cli_format_targets_dispose(&targets);
        return result;
    }
}

int ruda_cli_run(
    RudaSession *session,
    int argument_count,
    const char *const arguments[],
    RudaReadStream standard_input,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaCliOptions options;
    const RudaTaskHelp *task = NULL;
    int parse_result = 0;

    if (session == NULL || session->allocator.allocate == NULL
        || argument_count < 1 || arguments == NULL) {
        return RUDA_EXIT_INTERNAL;
    }
    parse_result = ruda_parse_options(argument_count, arguments, standard_error, &options);
    if (parse_result != RUDA_EXIT_SUCCESS) {
        return parse_result;
    }
    if (options.top_level_help || options.command == NULL) {
        return ruda_write_top_help(standard_output) ? RUDA_EXIT_SUCCESS : RUDA_EXIT_INTERNAL;
    }
    if (strcmp(options.command, "help") == 0) {
        if (options.version_json) {
            return ruda_unknown_option(
                standard_error,
                options.diagnostic_style,
                "--json",
                options.version_json_index
            );
        }
        if (options.help_target == NULL) {
            return ruda_write_top_help(standard_output) ? RUDA_EXIT_SUCCESS : RUDA_EXIT_INTERNAL;
        }
        task = ruda_find_task(options.help_target);
        if (task == NULL) {
            return ruda_unknown_command(
                standard_error,
                options.diagnostic_style,
                options.help_target,
                options.help_target_index
            );
        }
        return ruda_write_task_help(standard_output, task) ? RUDA_EXIT_SUCCESS : RUDA_EXIT_INTERNAL;
    }

    task = ruda_find_task(options.command);
    if (task == NULL) {
        return ruda_unknown_command(
            standard_error,
            options.diagnostic_style,
            options.command,
            options.command_index
        );
    }
    if (options.command_help) {
        return ruda_write_task_help(standard_output, task) ? RUDA_EXIT_SUCCESS : RUDA_EXIT_INTERNAL;
    }
    if (options.version_json && strcmp(task->name, "version") != 0) {
        return ruda_unknown_option(
            standard_error,
            options.diagnostic_style,
            "--json",
            options.version_json_index
        );
    }
    if (strcmp(task->name, "version") == 0) {
        return ruda_write_version(standard_output, options.version_json)
            ? RUDA_EXIT_SUCCESS
            : RUDA_EXIT_INTERNAL;
    }
    if (strcmp(task->name, "fmt") == 0 && options.format_stdin) {
        if (options.format_check || options.format_diff) {
            size_t argument_index = options.format_check
                ? options.format_check_index
                : options.format_diff_index;
            return ruda_option_value_error(
                standard_error,
                options.diagnostic_style,
                RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                argument_index,
                "--stdin cannot be combined with --check or --diff"
            );
        }
        return ruda_cli_format_stdin(
            session,
            &options,
            standard_input,
            standard_output,
            standard_error
        );
    }
    if (strcmp(task->name, "fmt") == 0
        && options.format_check && options.format_diff) {
        return ruda_option_value_error(
            standard_error,
            options.diagnostic_style,
            RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
            options.format_diff_index,
            "--check and --diff are mutually exclusive"
        );
    }
    if (strcmp(task->name, "fmt") == 0) {
        return ruda_cli_format_target(
            session,
            &options,
            standard_output,
            standard_error
        );
    }
    {
        char message[192];
        int length = snprintf(
            message,
            sizeof(message),
            "command '%s' is not implemented in bootstrap stage %d",
            task->name,
            RUDA_BOOTSTRAP_STAGE
        );
        if (length < 0 || (size_t)length >= sizeof(message)) {
            return RUDA_EXIT_INTERNAL;
        }
        return ruda_cli_diagnostic(
            standard_error,
            options.diagnostic_style,
            RUDA_DIAGNOSTIC_UNAVAILABLE_COMMAND,
            options.command_index,
            message
        );
    }
}
