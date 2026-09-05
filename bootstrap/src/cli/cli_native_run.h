#ifndef MMAY_CLI_NATIVE_RUN_H
#define MMAY_CLI_NATIVE_RUN_H

#include "cli_native_build.h"

typedef struct MarimayCliNativeArguments {
    MarimayAllocator allocator;
    const char **items;
    size_t count;
    size_t allocated_bytes;
} MarimayCliNativeArguments;

bool mmay_cli_native_arguments_init(
    MarimayAllocator allocator,
    const MarimayOwnedBytes *artifact_path,
    const MarimayCliOptions *options,
    MarimayCliNativeArguments *arguments
);

void mmay_cli_native_arguments_dispose(
    MarimayCliNativeArguments *arguments
);

int mmay_cli_native_run_checked(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const MarimayCliCheckTarget *target,
    const MarimayCompilerResult *compiler,
    const MarimayCheckedEntry *entry,
    MarimayReadStream standard_input,
    MarimayWriter standard_output,
    MarimayWriter standard_error
);

#endif
