#ifndef MMAY_CLI_CHECK_TARGETS_H
#define MMAY_CLI_CHECK_TARGETS_H

#include "cli_internal.h"

#include "mmay/check/service.h"
#include "mmay/platform/directory.h"

typedef enum MarimayCliCheckTargetStatus {
    MMAY_CLI_CHECK_TARGET_OK,
    MMAY_CLI_CHECK_TARGET_SOURCE_INVALID,
    MMAY_CLI_CHECK_TARGET_INPUT_ERROR,
    MMAY_CLI_CHECK_TARGET_LIMIT_REACHED,
    MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED,
    MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR
} MarimayCliCheckTargetStatus;

typedef struct MarimayCliCheckPackage {
    MarimayOwnedBytes import_path;
    MarimayOwnedBytes directory;
    MarimayVector file_ids;
} MarimayCliCheckPackage;

typedef struct MarimayCliCheckTarget {
    MarimayAllocator allocator;
    MarimayCompilerService service;
    MarimayVector packages;
    MarimayVector compiler_packages;
    MarimayOwnedBytes selected_import_path;
    MarimayOwnedBytes root_directory;
    MarimaySourceManager manifest_sources;
    MarimayManifestParseResult manifest;
    const MarimaySourceSnapshot *problem_source;
    const char *problem_code;
    const char *problem_message;
    size_t problem_start;
    size_t problem_end;
    bool module_wide;
    bool service_initialized;
    bool packages_initialized;
    bool compiler_packages_initialized;
    bool manifest_sources_initialized;
} MarimayCliCheckTarget;

enum {
    MMAY_CLI_CHECK_MAX_PATH_BYTES = 32768,
    MMAY_CLI_CHECK_MAX_NAME_BYTES = 4096,
    MMAY_CLI_CHECK_MAX_SOURCE_BYTES = 16 * 1024 * 1024,
    MMAY_CLI_CHECK_MAX_DEPTH = 256,
    MMAY_CLI_CHECK_MAX_DIRECTORIES = 100000,
    MMAY_CLI_CHECK_MAX_ENTRIES = 1000000,
    MMAY_CLI_CHECK_MAX_FILES = 1024
};

MarimayCliCheckTargetStatus mmay_cli_check_target_build(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayCliCheckTarget *target
);
void mmay_cli_check_target_dispose(MarimayCliCheckTarget *target);

#endif
