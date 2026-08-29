#include "cli_internal.h"
#include "cli_release.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/build_config.h"
#include "mmay/generated/diagnostic_codes.h"

static const char *const mmay_global_options[] = {
    "--check",
    "--all-targets",
    "--allow-read",
    "--allow-write",
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

static bool mmay_staged_command(const char *name) {
    const MarimayCliTaskHelp *task = mmay_cli_release_find_task(name);
    return task != NULL && !task->implemented;
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
    const char *suggestion = mmay_cli_release_suggest_task(command);
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
    if (suggestion != NULL && strcmp(suggestion, option) == 0) {
        suggestion = NULL;
    }
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
    options->allow_read_count = 0;
    options->allow_write_count = 0;
    options->program_arguments = NULL;
    options->program_argument_start = 0;
    options->program_argument_count = 0;
    options->quiet = false;
    options->verbose = false;

    for (index = 1; index < argument_count; index += 1) {
        const char *argument = arguments[index];
        const char *value = NULL;
        if (strcmp(argument, "--") == 0 && options->command != NULL
            && (strcmp(options->command, "run") == 0
                || strcmp(options->command, "test") == 0)) {
            options->program_argument_start = (size_t)index + 1;
            options->program_argument_count = (size_t)argument_count
                - options->program_argument_start;
            options->program_arguments = arguments
                + options->program_argument_start;
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
            || ((strcmp(argument, "--allow-read") == 0
                    || strcmp(argument, "--allow-write") == 0)
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
            } else if (strcmp(argument, "--allow-read") == 0
                || strcmp(argument, "--allow-write") == 0) {
                const char **roots = strcmp(argument, "--allow-read") == 0
                    ? options->allow_read : options->allow_write;
                size_t *count = strcmp(argument, "--allow-read") == 0
                    ? &options->allow_read_count : &options->allow_write_count;
                if (value[0] == '\0' || strlen(value) > MMAY_FS_MAX_PATH_BYTES
                    || *count >= MMAY_CLI_MAX_FS_ROOTS) {
                    return mmay_option_value_error(
                        error_writer, options->diagnostic_style,
                        MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE, (size_t)index,
                        "filesystem capability root is empty, too long, or exceeds the 64-root limit"
                    );
                }
                roots[*count] = value;
                (strcmp(argument, "--allow-read") == 0
                    ? options->allow_read_index
                    : options->allow_write_index)[*count] = (size_t)index;
                *count += 1;
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
        } else if ((mmay_starts_with(argument, "--allow-read=")
                || mmay_starts_with(argument, "--allow-write="))
            && options->command != NULL
            && (strcmp(options->command, "run") == 0
                || strcmp(options->command, "build") == 0
                || strcmp(options->command, "test") == 0)) {
            bool read = mmay_starts_with(argument, "--allow-read=");
            size_t prefix = strlen(read ? "--allow-read=" : "--allow-write=");
            const char *root = argument + prefix;
            const char **roots = read
                ? options->allow_read : options->allow_write;
            size_t *count = read
                ? &options->allow_read_count : &options->allow_write_count;
            if (root[0] == '\0' || strlen(root) > MMAY_FS_MAX_PATH_BYTES
                || *count >= MMAY_CLI_MAX_FS_ROOTS) {
                return mmay_option_value_error(
                    error_writer, options->diagnostic_style,
                    MMAY_DIAGNOSTIC_INVALID_OPTION_VALUE, (size_t)index,
                    "filesystem capability root is empty, too long, or exceeds the 64-root limit"
                );
            }
            roots[*count] = root;
            (read ? options->allow_read_index
                : options->allow_write_index)[*count] = (size_t)index;
            *count += 1;
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
        } else if (argument[0] == '-' && options->command != NULL
            && mmay_staged_command(options->command)) {
            continue;
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
        } else if (mmay_staged_command(options->command)) {
            continue;
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
    const MarimayCliTaskHelp *task = NULL;
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
        return mmay_cli_release_write_top_help(standard_output)
            ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
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
            return mmay_cli_release_write_top_help(standard_output)
                ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
        }
        task = mmay_cli_release_find_task(options.help_target);
        if (task == NULL) {
            return mmay_unknown_command(
                standard_error,
                options.diagnostic_style,
                options.help_target,
                options.help_target_index
            );
        }
        return mmay_cli_release_write_task_help(standard_output, task)
            ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
    }

    task = mmay_cli_release_find_task(options.command);
    if (task == NULL) {
        return mmay_unknown_command(
            standard_error,
            options.diagnostic_style,
            options.command,
            options.command_index
        );
    }
    if (options.command_help) {
        return mmay_cli_release_write_task_help(standard_output, task)
            ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
    }
    if (!task->implemented) {
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
    if (options.allow_read_count != 0 || options.allow_write_count != 0) {
        int capability_status = mmay_cli_validate_fs_capabilities(
            session, &options, standard_error
        );
        if (capability_status != MMAY_EXIT_SUCCESS) return capability_status;
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
        return mmay_cli_release_write_version(
            standard_output,
            options.version_json
        )
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
            session, &options, standard_input, standard_output, standard_error
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
    return MMAY_EXIT_INTERNAL;
}
