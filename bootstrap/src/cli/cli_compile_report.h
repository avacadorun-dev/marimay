#ifndef RUDA_CLI_COMPILE_REPORT_H
#define RUDA_CLI_COMPILE_REPORT_H

#include "cli_check_targets.h"

int ruda_cli_compile_target_error(
    const RudaCliOptions *options,
    const RudaCliCheckTarget *target,
    RudaCliCheckTargetStatus status,
    RudaWriter standard_error
);
int ruda_cli_compile_status_error(
    const RudaCliOptions *options,
    RudaCompilerStatus status,
    RudaWriter standard_error
);
int ruda_cli_compile_render_result(
    const RudaCliOptions *options,
    const RudaCompilerResult *result,
    RudaWriter standard_error
);

#endif
