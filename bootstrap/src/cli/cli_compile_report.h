#ifndef MMAY_CLI_COMPILE_REPORT_H
#define MMAY_CLI_COMPILE_REPORT_H

#include "cli_check_targets.h"

int mmay_cli_compile_target_error(
    const MarimayCliOptions *options,
    const MarimayCliCheckTarget *target,
    MarimayCliCheckTargetStatus status,
    MarimayWriter standard_error
);
int mmay_cli_compile_status_error(
    const MarimayCliOptions *options,
    MarimayCompilerStatus status,
    MarimayWriter standard_error
);
int mmay_cli_compile_render_result(
    const MarimayCliOptions *options,
    const MarimayCompilerResult *result,
    MarimayWriter standard_error
);

#endif
