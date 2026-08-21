#ifndef RUDA_CLI_INTERNAL_H
#define RUDA_CLI_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/owned_bytes.h"
#include "ruda/base/stream.h"
#include "ruda/base/vector.h"
#include "ruda/cli.h"
#include "ruda/diagnostic.h"
#include "ruda/manifest/manifest.h"
#include "ruda/platform/file.h"

typedef struct RudaCliOptions {
    const char *tool_executable_path;
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
    bool check_all_targets;
    size_t check_all_targets_index;
    const char *run_engine;
    size_t run_engine_index;
    size_t max_heap_bytes;
    size_t max_heap_bytes_index;
    bool max_heap_bytes_seen;
    const char *build_output;
    size_t build_output_index;
    const char *build_emit;
    size_t build_emit_index;
    bool build_release;
    size_t build_release_index;
    size_t program_argument_start;
    size_t program_argument_count;
    bool quiet;
    bool verbose;
} RudaCliOptions;

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

typedef struct RudaCliFormatPath { RudaOwnedBytes path; } RudaCliFormatPath;
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

int ruda_cli_plain_error(RudaWriter error_writer, RudaDiagnosticStyle style,
    const char *code, const char *message, int exit_code);
int ruda_cli_option_value_error(RudaWriter error_writer,
    RudaDiagnosticStyle style, const char *code, size_t argument_index,
    const char *message);
void ruda_cli_heap_limit_default(RudaCliOptions *options);
int ruda_cli_heap_limit_parse(RudaCliOptions *options, const char *value,
    size_t option_index, size_t value_index, RudaWriter error_writer);
void ruda_display_argument(const char *argument, char *output, size_t capacity);
bool ruda_cli_manifest_kind(const char *path, RudaManifestKind *kind);
bool ruda_cli_source_path(const char *path);
bool ruda_cli_format_file_path(const char *path);
int ruda_cli_format_stdin(RudaSession *session, const RudaCliOptions *options,
    RudaReadStream standard_input, RudaWriter standard_output,
    RudaWriter standard_error);
int ruda_cli_format_file(RudaSession *session, const RudaCliOptions *options,
    RudaWriter standard_output, RudaWriter standard_error);
int ruda_cli_format_target(RudaSession *session, const RudaCliOptions *options,
    RudaWriter standard_output, RudaWriter standard_error);
int ruda_cli_check(RudaSession *session, const RudaCliOptions *options,
    RudaWriter standard_output, RudaWriter standard_error);
int ruda_cli_run_program(RudaSession *session, const RudaCliOptions *options,
    RudaWriter standard_output, RudaWriter standard_error);
int ruda_cli_build_program(RudaSession *session, const RudaCliOptions *options,
    RudaWriter standard_output, RudaWriter standard_error);
int ruda_cli_test_program(RudaSession *session, const RudaCliOptions *options,
    RudaWriter standard_output, RudaWriter standard_error);

#endif
