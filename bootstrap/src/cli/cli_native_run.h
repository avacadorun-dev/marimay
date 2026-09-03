#ifndef MMAY_CLI_NATIVE_RUN_H
#define MMAY_CLI_NATIVE_RUN_H

#include "cli_native_build.h"

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
