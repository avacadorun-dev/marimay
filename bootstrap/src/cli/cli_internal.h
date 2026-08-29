#ifndef MMAY_CLI_INTERNAL_H
#define MMAY_CLI_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/owned_bytes.h"
#include "mmay/base/stream.h"
#include "mmay/base/vector.h"
#include "mmay/check/service_types.h"
#include "mmay/cli.h"
#include "mmay/diagnostic.h"
#include "mmay/manifest/manifest.h"
#include "mmay/platform/file.h"
#include "mmay/runtime/fs.h"

#define MMAY_CLI_MAX_FS_ROOTS 64U

typedef struct MarimayCliOptions {
    const char *tool_executable_path;
    MarimayDiagnosticStyle diagnostic_style;
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
    const char *allow_read[MMAY_CLI_MAX_FS_ROOTS];
    size_t allow_read_index[MMAY_CLI_MAX_FS_ROOTS];
    size_t allow_read_count;
    const char *allow_write[MMAY_CLI_MAX_FS_ROOTS];
    size_t allow_write_index[MMAY_CLI_MAX_FS_ROOTS];
    size_t allow_write_count;
    const char *const *program_arguments;
    size_t program_argument_start;
    size_t program_argument_count;
    bool quiet;
    bool verbose;
} MarimayCliOptions;

typedef enum MarimayCliFormatAction {
    MMAY_CLI_FORMAT_STDOUT,
    MMAY_CLI_FORMAT_CHECK,
    MMAY_CLI_FORMAT_DIFF,
    MMAY_CLI_FORMAT_WRITE
} MarimayCliFormatAction;

typedef enum MarimayCliFormatTargetStatus {
    MMAY_CLI_FORMAT_TARGET_OK,
    MMAY_CLI_FORMAT_TARGET_INPUT_ERROR,
    MMAY_CLI_FORMAT_TARGET_LIMIT_REACHED,
    MMAY_CLI_FORMAT_TARGET_ALLOCATION_FAILED
} MarimayCliFormatTargetStatus;

typedef struct MarimayCliFormatPath { MarimayOwnedBytes path; } MarimayCliFormatPath;
typedef struct MarimayCliFormatDirectory {
    MarimayOwnedBytes path;
    size_t depth;
} MarimayCliFormatDirectory;

enum {
    MMAY_CLI_FORMAT_MAX_PATH_BYTES = 32768,
    MMAY_CLI_FORMAT_MAX_NAME_BYTES = 4096,
    MMAY_CLI_FORMAT_MAX_DEPTH = 256,
    MMAY_CLI_FORMAT_MAX_DIRECTORIES = 100000,
    MMAY_CLI_FORMAT_MAX_ENTRIES = 1000000,
    MMAY_CLI_FORMAT_MAX_FILES = 100000
};

int mmay_cli_plain_error(MarimayWriter error_writer, MarimayDiagnosticStyle style,
    const char *code, const char *message, int exit_code);
int mmay_cli_option_value_error(MarimayWriter error_writer,
    MarimayDiagnosticStyle style, const char *code, size_t argument_index,
    const char *message);
void mmay_cli_heap_limit_default(MarimayCliOptions *options);
int mmay_cli_heap_limit_parse(MarimayCliOptions *options, const char *value,
    size_t option_index, size_t value_index, MarimayWriter error_writer);
void mmay_display_argument(const char *argument, char *output, size_t capacity);
bool mmay_cli_manifest_kind(const char *path, MarimayManifestKind *kind);
bool mmay_cli_source_path(const char *path);
bool mmay_cli_format_file_path(const char *path);
int mmay_cli_format_stdin(MarimaySession *session, const MarimayCliOptions *options,
    MarimayReadStream standard_input, MarimayWriter standard_output,
    MarimayWriter standard_error);
int mmay_cli_format_file(MarimaySession *session, const MarimayCliOptions *options,
    MarimayWriter standard_output, MarimayWriter standard_error);
int mmay_cli_format_target(MarimaySession *session, const MarimayCliOptions *options,
    MarimayWriter standard_output, MarimayWriter standard_error);
int mmay_cli_check(MarimaySession *session, const MarimayCliOptions *options,
    MarimayWriter standard_output, MarimayWriter standard_error);
int mmay_cli_run_program(MarimaySession *session, const MarimayCliOptions *options,
    MarimayReadStream standard_input, MarimayWriter standard_output,
    MarimayWriter standard_error);
int mmay_cli_build_program(MarimaySession *session, const MarimayCliOptions *options,
    MarimayWriter standard_output, MarimayWriter standard_error);
int mmay_cli_test_program(MarimaySession *session, const MarimayCliOptions *options,
    MarimayWriter standard_output, MarimayWriter standard_error);
bool mmay_cli_optimize_checked_mir(MarimayCompilerResult *compiler);

typedef struct MarimayCliFsCapabilities {
    MarimayFsOptions options;
    MarimayFsPathView read_roots[MMAY_CLI_MAX_FS_ROOTS];
    MarimayFsPathView write_roots[MMAY_CLI_MAX_FS_ROOTS];
} MarimayCliFsCapabilities;

bool mmay_cli_fs_capabilities(
    const MarimayCliOptions *options,
    MarimayCliFsCapabilities *capabilities
);
int mmay_cli_validate_fs_capabilities(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter error_writer
);

#endif
