#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/build_config.h"
#include "mmay/generated/diagnostic_codes.h"

#define MMAY_STRINGIFY_INNER(value) #value
#define MMAY_STRINGIFY(value) MMAY_STRINGIFY_INNER(value)

typedef struct MarimayTaskHelp {
    const char *name;
    const char *summary;
    const char *usage;
    const char *examples;
    bool implemented;
} MarimayTaskHelp;

static const MarimayTaskHelp mmay_tasks[] = {
    {"add", "Add a package dependency.", "add module[@version]", "mmay add marimay.example/postgres@1.4", false},
    {"audit", "Audit dependencies, capabilities, unsafe code, and provenance.", "audit", "mmay audit", false},
    {"bench", "Run first-party benchmarks.", "bench [filter]", "mmay bench --compare base.json", false},
    {"build", "Build a native executable by default.", "build [target] [--max-heap-bytes bytes] [options]", "mmay build --max-heap-bytes 67108864 --release", true},
    {"check", "Run the complete frontend and MIR verifier.", "check [target] [--all-targets]", "mmay check --all-targets", true},
    {"doc", "Build or check package documentation.", "doc [options]", "mmay doc --check", false},
    {"exec", "Verify and execute a portable .myb bundle.", "exec bundle.myb [options]", "mmay exec app.myb", false},
    {"explain", "Explain a diagnostic or inferred compiler behavior.", "explain code|topic", "mmay explain suspension loadDashboard", false},
    {"fix", "Apply compiler-authored safe fixes.", "fix [target] [options]", "mmay fix", false},
    {"fmt", "Canonically format Marimay source and manifests.", "fmt [target] [options]", "mmay fmt --check", true},
    {"fuzz", "Run a first-party fuzz target.", "fuzz target [options]", "mmay fuzz \"decoder never panics\"", false},
    {"generate", "Run explicitly configured external generation.", "generate [target]", "mmay generate", false},
    {"help", "Show task-oriented command help.", "help [command]", "mmay help build", true},
    {"init", "Create a Marimay module.", "init [path]", "mmay init ./orders", false},
    {"list", "List package dependencies.", "list", "mmay list", false},
    {"outdated", "Report available dependency updates.", "outdated", "mmay outdated", false},
    {"remove", "Remove a package dependency.", "remove module", "mmay remove marimay.example/postgres", false},
    {"run", "Run with the bytecode VM by default.", "run [target] [--max-heap-bytes bytes] [-- arguments]", "mmay run . --max-heap-bytes 67108864", true},
    {"test", "Run built-in tests with the VM by default.", "test [target] [--engine vm|native|both] [--max-heap-bytes bytes]", "mmay test --engine both --max-heap-bytes 67108864", true},
    {"update", "Update locked package versions.", "update [module]", "mmay update", false},
    {"vendor", "Vendor immutable locked dependencies.", "vendor", "mmay vendor", false},
    {"version", "Show compiler and runtime build identity.", "version [--json]", "mmay version --json", true},
    {"vet", "Run first-party correctness checks.", "vet [target] [options]", "mmay vet --strict", false},
    {"why", "Explain why a dependency is present.", "why module", "mmay why marimay.example/crypto", false},
};

static const char *const mmay_global_options[] = {
    "--check",
    "--all-targets",
    "--color",
    "--diagnostic",
    "--diff",
    "--engine",
    "--help",
    "--max-heap-bytes",
    "--output",
    "--quiet",
    "--release",
    "--emit",
    "--stdin",
    "--verbose",
    "--version",
};

static bool mmay_starts_with(const char *text, const char *prefix) {
    size_t prefix_length = strlen(prefix);
    return strncmp(text, prefix, prefix_length) == 0;
}

static size_t mmay_minimum3(size_t first, size_t second, size_t third) {
    size_t result = first < second ? first : second;
    return result < third ? result : third;
}

static size_t mmay_edit_distance(const char *left, const char *right) {
    enum { MMAY_SUGGESTION_LIMIT = 64 };
    size_t previous[MMAY_SUGGESTION_LIMIT + 1];
    size_t current[MMAY_SUGGESTION_LIMIT + 1];
    size_t left_length = strlen(left);
    size_t right_length = strlen(right);
    size_t left_index = 0;
    size_t right_index = 0;

    if (left_length > MMAY_SUGGESTION_LIMIT || right_length > MMAY_SUGGESTION_LIMIT) {
        return MMAY_SUGGESTION_LIMIT;
    }
    for (right_index = 0; right_index <= right_length; right_index += 1) {
        previous[right_index] = right_index;
    }
    for (left_index = 1; left_index <= left_length; left_index += 1) {
        current[0] = left_index;
        for (right_index = 1; right_index <= right_length; right_index += 1) {
            size_t substitution = previous[right_index - 1]
                + (left[left_index - 1] == right[right_index - 1] ? 0U : 1U);
            current[right_index] = mmay_minimum3(
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

static const MarimayTaskHelp *mmay_find_task(const char *name) {
    size_t index = 0;
    for (index = 0; index < sizeof(mmay_tasks) / sizeof(mmay_tasks[0]); index += 1) {
        if (strcmp(mmay_tasks[index].name, name) == 0) {
            return &mmay_tasks[index];
        }
    }
    return NULL;
}

static const char *mmay_suggest_task(const char *name) {
    const char *suggestion = NULL;
    size_t best = 4;
    size_t index = 0;
    for (index = 0; index < sizeof(mmay_tasks) / sizeof(mmay_tasks[0]); index += 1) {
        size_t distance = mmay_edit_distance(name, mmay_tasks[index].name);
        if (distance < best) {
            best = distance;
            suggestion = mmay_tasks[index].name;
        }
    }
    return suggestion;
}

static const char *mmay_suggest_option(const char *name) {
    const char *suggestion = NULL;
    size_t best = 4;
    size_t index = 0;
    for (index = 0; index < sizeof(mmay_global_options) / sizeof(mmay_global_options[0]); index += 1) {
        size_t distance = mmay_edit_distance(name, mmay_global_options[index]);
        if (distance < best) {
            best = distance;
            suggestion = mmay_global_options[index];
        }
    }
    if (mmay_edit_distance(name, "--json") < best) {
        suggestion = "--json";
    }
    return suggestion;
}

static int mmay_cli_diagnostic(
    MarimayWriter error_writer,
    MarimayDiagnosticStyle style,
    const char *code,
    size_t argument_index,
    const char *message
) {
    MarimayDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = MMAY_SEVERITY_ERROR;
    diagnostic.message = message;
    diagnostic.has_command_argument = true;
    diagnostic.command_argument = argument_index;
    if (!mmay_diagnostic_render(error_writer, style, &diagnostic)) {
        return MMAY_EXIT_INTERNAL;
    }
    return MMAY_EXIT_USAGE;
}

static int mmay_unknown_command(
    MarimayWriter error_writer,
    MarimayDiagnosticStyle style,
    const char *command,
    size_t argument_index
) {
    char display[128];
    char message[320];
    const char *suggestion = mmay_suggest_task(command);
    int length = 0;
    mmay_display_argument(command, display, sizeof(display));
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
        return MMAY_EXIT_INTERNAL;
    }
    return mmay_cli_diagnostic(
        error_writer,
        style,
        MMAY_DIAGNOSTIC_UNKNOWN_COMMAND,
        argument_index,
        message
    );
}

static int mmay_unknown_option(
    MarimayWriter error_writer,
    MarimayDiagnosticStyle style,
    const char *option,
    size_t argument_index
) {
    char display[128];
    char message[320];
    const char *suggestion = mmay_suggest_option(option);
    int length = 0;
    mmay_display_argument(option, display, sizeof(display));
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
        return MMAY_EXIT_INTERNAL;
    }
    return mmay_cli_diagnostic(
        error_writer,
        style,
        MMAY_DIAGNOSTIC_UNKNOWN_OPTION,
        argument_index,
        message
    );
}

static bool mmay_valid_diagnostic_style(const char *value, MarimayDiagnosticStyle *style) {
    if (strcmp(value, "human") == 0) {
        *style = MMAY_DIAGNOSTIC_HUMAN;
        return true;
    }
    if (strcmp(value, "short") == 0) {
        *style = MMAY_DIAGNOSTIC_SHORT;
        return true;
    }
    if (strcmp(value, "json") == 0) {
        *style = MMAY_DIAGNOSTIC_JSON;
        return true;
    }
    return false;
}

static bool mmay_valid_color(const char *value) {
    return strcmp(value, "auto") == 0
        || strcmp(value, "always") == 0
        || strcmp(value, "never") == 0;
}

int mmay_cli_option_value_error(
    MarimayWriter error_writer,
    MarimayDiagnosticStyle style,
    const char *code,
    size_t argument_index,
    const char *message
) {
    return mmay_cli_diagnostic(error_writer, style, code, argument_index, message);
}

#define mmay_option_value_error mmay_cli_option_value_error

static int mmay_parse_options(
    int argument_count,
    const char *const arguments[],
    MarimayWriter error_writer,
    MarimayCliOptions *options
) {
    int index = 0;
    options->tool_executable_path = NULL;
    options->diagnostic_style = MMAY_DIAGNOSTIC_HUMAN;
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
    mmay_cli_heap_limit_default(options);
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
            || (strcmp(argument, "--max-heap-bytes") == 0
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
                    return MMAY_EXIT_INTERNAL;
                }
                return mmay_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    MMAY_DIAGNOSTIC_MISSING_OPTION_VALUE,
                    (size_t)index,
                    message
                );
            }
            index += 1;
            value = arguments[index];
            if (strcmp(argument, "--diagnostic") == 0) {
                if (!mmay_valid_diagnostic_style(value, &options->diagnostic_style)) {
                    return mmay_option_value_error(
                        error_writer,
                        options->diagnostic_style,
                        MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
                        (size_t)index,
                        "--diagnostic expects human, short, or json"
                    );
                }
            } else if (strcmp(argument, "--color") == 0
                && !mmay_valid_color(value)) {
                return mmay_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
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
                    return mmay_option_value_error(
                        error_writer, options->diagnostic_style,
                        MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
                        (size_t)index,
                        strcmp(options->command, "test") == 0
                            ? "mmay test --engine expects vm, native, or both"
                            : strcmp(options->command, "run") == 0
                            ? "checkpoint 9 mmay run supports --engine vm"
                            : "checkpoint 10 mmay build supports --engine native"
                    );
                }
                options->run_engine = value;
                options->run_engine_index = (size_t)index;
            } else if (strcmp(argument, "--output") == 0) {
                options->build_output = value;
                options->build_output_index = (size_t)index;
            } else if (strcmp(argument, "--emit") == 0) {
                if (strcmp(value, "c") != 0) {
                    return mmay_option_value_error(
                        error_writer, options->diagnostic_style,
                        MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
                        (size_t)index,
                        "checkpoint 10 mmay build supports --emit c"
                    );
                }
                options->build_emit = value;
                options->build_emit_index = (size_t)index;
            } else if (strcmp(argument, "--max-heap-bytes") == 0) {
                int heap_result = mmay_cli_heap_limit_parse(
                    options, value, (size_t)index - 1, (size_t)index,
                    error_writer
                );
                if (heap_result != MMAY_EXIT_SUCCESS) return heap_result;
            }
        } else if (mmay_starts_with(argument, "--diagnostic=")) {
            value = argument + strlen("--diagnostic=");
            if (!mmay_valid_diagnostic_style(value, &options->diagnostic_style)) {
                return mmay_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
                    (size_t)index,
                    "--diagnostic expects human, short, or json"
                );
            }
        } else if (mmay_starts_with(argument, "--color=")) {
            value = argument + strlen("--color=");
            if (!mmay_valid_color(value)) {
                return mmay_option_value_error(
                    error_writer,
                    options->diagnostic_style,
                    MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
                    (size_t)index,
                    "--color expects auto, always, or never"
                );
            }
        } else if (mmay_starts_with(argument, "--engine=")
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
                    return mmay_option_value_error(
                        error_writer, options->diagnostic_style,
                        MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
                        (size_t)index,
                        strcmp(options->command, "test") == 0
                            ? "mmay test --engine expects vm, native, or both"
                            : strcmp(options->command, "run") == 0
                            ? "checkpoint 9 mmay run supports --engine vm"
                            : "checkpoint 10 mmay build supports --engine native"
                    );
                }
            }
            options->run_engine = value;
            options->run_engine_index = (size_t)index;
        } else if (mmay_starts_with(argument, "--max-heap-bytes=")
            && options->command != NULL
            && (strcmp(options->command, "run") == 0
                || strcmp(options->command, "build") == 0
                || strcmp(options->command, "test") == 0)) {
            int heap_result = mmay_cli_heap_limit_parse(
                options, argument + strlen("--max-heap-bytes="),
                (size_t)index, (size_t)index, error_writer
            );
            if (heap_result != MMAY_EXIT_SUCCESS) return heap_result;
        } else if (mmay_starts_with(argument, "--output=")
            && options->command != NULL
            && strcmp(options->command, "build") == 0) {
            options->build_output = argument + strlen("--output=");
            options->build_output_index = (size_t)index;
        } else if (mmay_starts_with(argument, "--emit=")
            && options->command != NULL
            && strcmp(options->command, "build") == 0) {
            value = argument + strlen("--emit=");
            if (strcmp(value, "c") != 0) {
                return mmay_option_value_error(
                    error_writer, options->diagnostic_style,
                    MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
                    (size_t)index,
                    "checkpoint 10 mmay build supports --emit c"
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
                || mmay_starts_with(argument, "--filter=")
                || mmay_starts_with(argument, "--exact="))) {
            return mmay_option_value_error(
                error_writer, options->diagnostic_style,
                MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE, (size_t)index,
                "checkpoint 11 mmay test does not yet support filters, watch, or cache options"
            );
        } else if (argument[0] == '-') {
            return mmay_unknown_option(
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
            mmay_display_argument(argument, display, sizeof(display));
            length = snprintf(message, sizeof(message), "unexpected argument '%s'", display);
            if (length < 0 || (size_t)length >= sizeof(message)) {
                return MMAY_EXIT_INTERNAL;
            }
            return mmay_option_value_error(
                error_writer,
                options->diagnostic_style,
                MMAY_DIAGNOSTIC_UNEXPECTED_ARGUMENT,
                (size_t)index,
                message
            );
        }
    }
    return MMAY_EXIT_SUCCESS;
}

static bool mmay_write_top_help(MarimayWriter writer) {
    static const char help[] =
        "Marimay toolchain (Stage 0 bootstrap)\n"
        "\n"
        "Usage: mmay <command> [options]\n"
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
        "Run 'mmay help <command>' for task-oriented examples.\n";
    return mmay_writer_write_cstr(writer, help);
}

static bool mmay_write_task_help(MarimayWriter writer, const MarimayTaskHelp *task) {
    if (!mmay_writer_write_cstr(writer, "Usage: mmay ")
        || !mmay_writer_write_cstr(writer, task->usage)
        || !mmay_writer_write_cstr(writer, "\n\n")
        || !mmay_writer_write_cstr(writer, task->summary)
        || !mmay_writer_write_cstr(writer, "\n\nExample:\n  ")
        || !mmay_writer_write_cstr(writer, task->examples)
        || !mmay_writer_write_cstr(writer, "\n")) {
        return false;
    }
    if (!task->implemented) {
        return mmay_writer_write_cstr(
            writer,
            "\nThis command is specified but not implemented in the current bootstrap slice.\n"
        );
    }
    return true;
}

static bool mmay_write_version(MarimayWriter writer, bool json) {
    if (json) {
        static const char version_json[] =
            "{\"schema\":\"marimay.version/1\","
            "\"compiler\":\"" MMAY_COMPILER_VERSION "\","
            "\"runtime\":\"" MMAY_RUNTIME_VERSION "\","
            "\"language\":\"" MMAY_LANGUAGE_VERSION "\","
            "\"bytecode\":\"" MMAY_BYTECODE_VERSION "\","
            "\"standardLibrary\":\"" MMAY_STANDARD_LIBRARY_VERSION "\","
            "\"bootstrapStage\":" MMAY_STRINGIFY(MMAY_BOOTSTRAP_STAGE) ","
            "\"host\":{\"system\":\"" MMAY_HOST_SYSTEM "\","
            "\"processor\":\"" MMAY_HOST_PROCESSOR "\"},"
            "\"targets\":[\"native-host\"]}\n";
        return mmay_writer_write_cstr(writer, version_json);
    }
    return mmay_writer_write_cstr(writer, "Marimay compiler ")
        && mmay_writer_write_cstr(writer, MMAY_COMPILER_VERSION)
        && mmay_writer_write_cstr(writer, "\nruntime ")
        && mmay_writer_write_cstr(writer, MMAY_RUNTIME_VERSION)
        && mmay_writer_write_cstr(writer, "\nlanguage ")
        && mmay_writer_write_cstr(writer, MMAY_LANGUAGE_VERSION)
        && mmay_writer_write_cstr(writer, "\nbytecode ")
        && mmay_writer_write_cstr(writer, MMAY_BYTECODE_VERSION)
        && mmay_writer_write_cstr(writer, "\nstandard-library ")
        && mmay_writer_write_cstr(writer, MMAY_STANDARD_LIBRARY_VERSION)
        && mmay_writer_write_cstr(writer, "\nbootstrap-stage ")
        && mmay_writer_write_cstr(writer, MMAY_STRINGIFY(MMAY_BOOTSTRAP_STAGE))
        && mmay_writer_write_cstr(writer, "\nhost ")
        && mmay_writer_write_cstr(writer, MMAY_HOST_SYSTEM)
        && mmay_writer_write_cstr(writer, "/")
        && mmay_writer_write_cstr(writer, MMAY_HOST_PROCESSOR)
        && mmay_writer_write_cstr(writer, "\ntargets native-host\n");
}

int mmay_cli_plain_error(
    MarimayWriter error_writer,
    MarimayDiagnosticStyle style,
    const char *code,
    const char *message,
    int exit_code
) {
    MarimayDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = MMAY_SEVERITY_ERROR;
    diagnostic.message = message;
    return mmay_diagnostic_render(error_writer, style, &diagnostic)
        ? exit_code
        : MMAY_EXIT_INTERNAL;
}

int mmay_cli_run(
    MarimaySession *session,
    int argument_count,
    const char *const arguments[],
    MarimayReadStream standard_input,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayCliOptions options;
    const MarimayTaskHelp *task = NULL;
    int parse_result = 0;

    if (session == NULL || session->allocator.allocate == NULL
        || argument_count < 1 || arguments == NULL) {
        return MMAY_EXIT_INTERNAL;
    }
    parse_result = mmay_parse_options(argument_count, arguments, standard_error, &options);
    if (parse_result != MMAY_EXIT_SUCCESS) {
        return parse_result;
    }
    options.tool_executable_path = arguments[0];
    if (options.top_level_help || options.command == NULL) {
        return mmay_write_top_help(standard_output) ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
    }
    if (strcmp(options.command, "help") == 0) {
        if (options.version_json) {
            return mmay_unknown_option(
                standard_error,
                options.diagnostic_style,
                "--json",
                options.version_json_index
            );
        }
        if (options.help_target == NULL) {
            return mmay_write_top_help(standard_output) ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
        }
        task = mmay_find_task(options.help_target);
        if (task == NULL) {
            return mmay_unknown_command(
                standard_error,
                options.diagnostic_style,
                options.help_target,
                options.help_target_index
            );
        }
        return mmay_write_task_help(standard_output, task) ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
    }

    task = mmay_find_task(options.command);
    if (task == NULL) {
        return mmay_unknown_command(
            standard_error,
            options.diagnostic_style,
            options.command,
            options.command_index
        );
    }
    if (options.command_help) {
        return mmay_write_task_help(standard_output, task) ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
    }
    if (options.version_json && strcmp(task->name, "version") != 0) {
        return mmay_unknown_option(
            standard_error,
            options.diagnostic_style,
            "--json",
            options.version_json_index
        );
    }
    if (strcmp(task->name, "version") == 0) {
        return mmay_write_version(standard_output, options.version_json)
            ? MMAY_EXIT_SUCCESS
            : MMAY_EXIT_INTERNAL;
    }
    if (strcmp(task->name, "fmt") == 0 && options.format_stdin) {
        if (options.format_check || options.format_diff) {
            size_t argument_index = options.format_check
                ? options.format_check_index
                : options.format_diff_index;
            return mmay_option_value_error(
                standard_error,
                options.diagnostic_style,
                MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
                argument_index,
                "--stdin cannot be combined with --check or --diff"
            );
        }
        return mmay_cli_format_stdin(
            session,
            &options,
            standard_input,
            standard_output,
            standard_error
        );
    }
    if (strcmp(task->name, "fmt") == 0
        && options.format_check && options.format_diff) {
        return mmay_option_value_error(
            standard_error,
            options.diagnostic_style,
            MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE,
            options.format_diff_index,
            "--check and --diff are mutually exclusive"
        );
    }
    if (strcmp(task->name, "fmt") == 0) {
        return mmay_cli_format_target(
            session,
            &options,
            standard_output,
            standard_error
        );
    }
    if (strcmp(task->name, "check") == 0) {
        return mmay_cli_check(
            session,
            &options,
            standard_output,
            standard_error
        );
    }
    if (strcmp(task->name, "run") == 0) {
        return mmay_cli_run_program(
            session, &options, standard_output, standard_error
        );
    }
    if (strcmp(task->name, "build") == 0) {
        return mmay_cli_build_program(
            session, &options, standard_output, standard_error
        );
    }
    if (strcmp(task->name, "test") == 0) {
        return mmay_cli_test_program(
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
            MMAY_BOOTSTRAP_STAGE
        );
        if (length < 0 || (size_t)length >= sizeof(message)) {
            return MMAY_EXIT_INTERNAL;
        }
        return mmay_cli_diagnostic(
            standard_error,
            options.diagnostic_style,
            MMAY_DIAGNOSTIC_UNAVAILABLE_COMMAND,
            options.command_index,
            message
        );
    }
}
