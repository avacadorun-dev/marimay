#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/build_config.h"
#include "ruda/base/utf8.h"
#include "ruda/generated/diagnostic_codes.h"

#define RUDA_STRINGIFY_INNER(value) #value
#define RUDA_STRINGIFY(value) RUDA_STRINGIFY_INNER(value)

typedef struct RudaTaskHelp {
    const char *name;
    const char *summary;
    const char *usage;
    const char *examples;
    bool implemented;
} RudaTaskHelp;

static const RudaTaskHelp ruda_tasks[] = {
    {"add", "Add a package dependency.", "add module[@version]", "ruda add ruda-lang.example/postgres@1.4", false},
    {"audit", "Audit dependencies, capabilities, unsafe code, and provenance.", "audit", "ruda audit", false},
    {"bench", "Run first-party benchmarks.", "bench [filter]", "ruda bench --compare base.json", false},
    {"build", "Build a native executable by default.", "build [target] [options]", "ruda build --release", true},
    {"check", "Run the complete frontend and MIR verifier.", "check [target] [--all-targets]", "ruda check --all-targets", true},
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
    {"run", "Run with the bytecode VM by default.", "run [target] [-- arguments]", "ruda run .", true},
    {"test", "Run built-in tests with the VM by default.", "test [target] [--engine vm|native|both]", "ruda test --engine both", true},
    {"update", "Update locked package versions.", "update [module]", "ruda update", false},
    {"vendor", "Vendor immutable locked dependencies.", "vendor", "ruda vendor", false},
    {"version", "Show compiler and runtime build identity.", "version [--json]", "ruda version --json", true},
    {"vet", "Run first-party correctness checks.", "vet [target] [options]", "ruda vet --strict", false},
    {"why", "Explain why a dependency is present.", "why module", "ruda why ruda-lang.example/crypto", false},
};

static const char *const ruda_global_options[] = {
    "--check",
    "--all-targets",
    "--color",
    "--diagnostic",
    "--diff",
    "--engine",
    "--help",
    "--output",
    "--quiet",
    "--release",
    "--emit",
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

void ruda_display_argument(const char *argument, char *output, size_t capacity) {
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
    options->check_all_targets = false;
    options->check_all_targets_index = 0;
    options->run_engine = "vm";
    options->run_engine_index = 0;
    options->build_output = NULL;
    options->build_output_index = 0;
    options->build_emit = NULL;
    options->build_emit_index = 0;
    options->build_release = false;
    options->build_release_index = 0;
    options->program_argument_start = 0;
    options->program_argument_count = 0;
    options->quiet = false;
    options->verbose = false;

    for (index = 1; index < argument_count; index += 1) {
        const char *argument = arguments[index];
        const char *value = NULL;
        if (strcmp(argument, "--") == 0 && options->command != NULL
            && strcmp(options->command, "run") == 0) {
            options->program_argument_start = (size_t)index + 1;
            options->program_argument_count = (size_t)argument_count
                - options->program_argument_start;
            break;
        } else if (strcmp(argument, "--diagnostic") == 0
            || strcmp(argument, "--color") == 0
            || (strcmp(argument, "--engine") == 0
                && options->command != NULL
                && (strcmp(options->command, "run") == 0
                    || strcmp(options->command, "build") == 0
                    || strcmp(options->command, "test") == 0))
            || ((strcmp(argument, "--output") == 0
                    || strcmp(argument, "--emit") == 0)
                && options->command != NULL
                && strcmp(options->command, "build") == 0)) {
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
            } else if (strcmp(argument, "--color") == 0
                && !ruda_valid_color(value)) {
                return ruda_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                    (size_t)index,
                    "--color expects auto, always, or never"
                );
            } else if (strcmp(argument, "--engine") == 0) {
                bool valid = strcmp(options->command, "test") == 0
                    ? strcmp(value, "vm") == 0
                        || strcmp(value, "native") == 0
                        || strcmp(value, "both") == 0
                    : strcmp(value, strcmp(options->command, "run") == 0
                        ? "vm" : "native") == 0;
                if (!valid) {
                    return ruda_option_value_error(
                        error_writer, options->diagnostic_style,
                        RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                        (size_t)index,
                        strcmp(options->command, "test") == 0
                            ? "ruda test --engine expects vm, native, or both"
                            : strcmp(options->command, "run") == 0
                            ? "checkpoint 9 ruda run supports --engine vm"
                            : "checkpoint 10 ruda build supports --engine native"
                    );
                }
                options->run_engine = value;
                options->run_engine_index = (size_t)index;
            } else if (strcmp(argument, "--output") == 0) {
                options->build_output = value;
                options->build_output_index = (size_t)index;
            } else if (strcmp(argument, "--emit") == 0) {
                if (strcmp(value, "c") != 0) {
                    return ruda_option_value_error(
                        error_writer, options->diagnostic_style,
                        RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                        (size_t)index,
                        "checkpoint 10 ruda build supports --emit c"
                    );
                }
                options->build_emit = value;
                options->build_emit_index = (size_t)index;
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
        } else if (ruda_starts_with(argument, "--engine=")
            && options->command != NULL
            && (strcmp(options->command, "run") == 0
                || strcmp(options->command, "build") == 0
                || strcmp(options->command, "test") == 0)) {
            value = argument + strlen("--engine=");
            {
                bool valid = strcmp(options->command, "test") == 0
                    ? strcmp(value, "vm") == 0
                        || strcmp(value, "native") == 0
                        || strcmp(value, "both") == 0
                    : strcmp(value, strcmp(options->command, "run") == 0
                        ? "vm" : "native") == 0;
                if (!valid) {
                    return ruda_option_value_error(
                        error_writer, options->diagnostic_style,
                        RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                        (size_t)index,
                        strcmp(options->command, "test") == 0
                            ? "ruda test --engine expects vm, native, or both"
                            : strcmp(options->command, "run") == 0
                            ? "checkpoint 9 ruda run supports --engine vm"
                            : "checkpoint 10 ruda build supports --engine native"
                    );
                }
            }
            options->run_engine = value;
            options->run_engine_index = (size_t)index;
        } else if (ruda_starts_with(argument, "--output=")
            && options->command != NULL
            && strcmp(options->command, "build") == 0) {
            options->build_output = argument + strlen("--output=");
            options->build_output_index = (size_t)index;
        } else if (ruda_starts_with(argument, "--emit=")
            && options->command != NULL
            && strcmp(options->command, "build") == 0) {
            value = argument + strlen("--emit=");
            if (strcmp(value, "c") != 0) {
                return ruda_option_value_error(
                    error_writer, options->diagnostic_style,
                    RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE,
                    (size_t)index,
                    "checkpoint 10 ruda build supports --emit c"
                );
            }
            options->build_emit = value;
            options->build_emit_index = (size_t)index;
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
        } else if (options->command != NULL
            && strcmp(options->command, "check") == 0
            && strcmp(argument, "--all-targets") == 0) {
            options->check_all_targets = true;
            options->check_all_targets_index = (size_t)index;
        } else if (options->command != NULL
            && strcmp(options->command, "build") == 0
            && strcmp(argument, "--release") == 0) {
            options->build_release = true;
            options->build_release_index = (size_t)index;
        } else if (options->command != NULL
            && strcmp(options->command, "test") == 0
            && (strcmp(argument, "--watch") == 0
                || strcmp(argument, "--no-cache") == 0
                || strcmp(argument, "--filter") == 0
                || strcmp(argument, "--exact") == 0
                || ruda_starts_with(argument, "--filter=")
                || ruda_starts_with(argument, "--exact="))) {
            return ruda_option_value_error(
                error_writer, options->diagnostic_style,
                RUDA_DIAGNOSTIC_INVALID_OPTION_VALUE, (size_t)index,
                "checkpoint 11 ruda test does not yet support filters, watch, or cache options"
            );
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
        } else if ((strcmp(options->command, "fmt") == 0
                || strcmp(options->command, "check") == 0
                || strcmp(options->command, "run") == 0
                || strcmp(options->command, "build") == 0
                || strcmp(options->command, "test") == 0)
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

int ruda_cli_plain_error(
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
    if (strcmp(task->name, "check") == 0) {
        return ruda_cli_check(
            session,
            &options,
            standard_output,
            standard_error
        );
    }
    if (strcmp(task->name, "run") == 0) {
        return ruda_cli_run_program(
            session, &options, standard_output, standard_error
        );
    }
    if (strcmp(task->name, "build") == 0) {
        return ruda_cli_build_program(
            session, &options, standard_output, standard_error
        );
    }
    if (strcmp(task->name, "test") == 0) {
        return ruda_cli_test_program(
            session, &options, standard_output, standard_error
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
