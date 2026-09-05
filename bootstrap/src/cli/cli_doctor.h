#ifndef MMAY_CLI_DOCTOR_H
#define MMAY_CLI_DOCTOR_H

#include "cli_internal.h"

int mmay_cli_doctor(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output
);

#endif
