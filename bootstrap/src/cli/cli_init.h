#ifndef MMAY_CLI_INIT_H
#define MMAY_CLI_INIT_H

#include "cli_internal.h"

int mmay_cli_init_project(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output,
    MarimayWriter standard_error
);

#endif
