#ifndef MMAY_CLI_H
#define MMAY_CLI_H

#include "mmay/base/stream.h"
#include "mmay/base/writer.h"
#include "mmay/session.h"

enum {
    MMAY_EXIT_SUCCESS = 0,
    MMAY_EXIT_SOURCE_FAILURE = 1,
    MMAY_EXIT_USAGE = 2,
    MMAY_EXIT_DEPENDENCY = 3,
    MMAY_EXIT_CAPABILITY = 4,
    MMAY_EXIT_DIVERGENCE = 5,
    MMAY_EXIT_INTERNAL = 70,
    MMAY_EXIT_INTERRUPTED = 130
};

int mmay_cli_run(
    MarimaySession *session,
    int argument_count,
    const char *const arguments[],
    MarimayReadStream standard_input,
    MarimayWriter standard_output,
    MarimayWriter standard_error
);

#endif
