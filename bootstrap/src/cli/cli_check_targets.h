#ifndef RUDA_CLI_CHECK_TARGETS_H
#define RUDA_CLI_CHECK_TARGETS_H

#include "cli_internal.h"

#include "ruda/check/service.h"
#include "ruda/platform/directory.h"

typedef enum RudaCliCheckTargetStatus {
    RUDA_CLI_CHECK_TARGET_OK,
    RUDA_CLI_CHECK_TARGET_SOURCE_INVALID,
    RUDA_CLI_CHECK_TARGET_INPUT_ERROR,
    RUDA_CLI_CHECK_TARGET_LIMIT_REACHED,
    RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED,
    RUDA_CLI_CHECK_TARGET_INTERNAL_ERROR
} RudaCliCheckTargetStatus;

typedef struct RudaCliCheckPackage {
    RudaOwnedBytes import_path;
    RudaOwnedBytes directory;
    RudaVector file_ids;
} RudaCliCheckPackage;

typedef struct RudaCliCheckTarget {
    RudaAllocator allocator;
    RudaCompilerService service;
    RudaVector packages;
    RudaVector compiler_packages;
    RudaOwnedBytes selected_import_path;
    RudaSourceManager manifest_sources;
    RudaManifestParseResult manifest;
    const RudaSourceSnapshot *problem_source;
    const char *problem_code;
    const char *problem_message;
    size_t problem_start;
    size_t problem_end;
    bool module_wide;
    bool service_initialized;
    bool packages_initialized;
    bool compiler_packages_initialized;
    bool manifest_sources_initialized;
} RudaCliCheckTarget;

enum {
    RUDA_CLI_CHECK_MAX_PATH_BYTES = 32768,
    RUDA_CLI_CHECK_MAX_NAME_BYTES = 4096,
    RUDA_CLI_CHECK_MAX_SOURCE_BYTES = 16 * 1024 * 1024,
    RUDA_CLI_CHECK_MAX_DEPTH = 256,
    RUDA_CLI_CHECK_MAX_DIRECTORIES = 100000,
    RUDA_CLI_CHECK_MAX_ENTRIES = 1000000,
    RUDA_CLI_CHECK_MAX_FILES = 1024
};

RudaCliCheckTargetStatus ruda_cli_check_target_build(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaCliCheckTarget *target
);
void ruda_cli_check_target_dispose(RudaCliCheckTarget *target);

#endif
