#ifndef MMAY_CLI_NATIVE_BUILD_H
#define MMAY_CLI_NATIVE_BUILD_H

#include "cli_check_targets.h"

#include "mmay/check/entry.h"
#include "mmay/native/build.h"

int mmay_cli_native_build_checked(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const MarimayCliCheckTarget *target,
    const MarimayCompilerResult *compiler,
    const MarimayCheckedEntry *entry,
    bool announce_artifact,
    MarimayWriter standard_output,
    MarimayWriter standard_error,
    MarimayNativeBuildResult *result
);

#endif
