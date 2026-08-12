#ifndef RUDA_CLI_H
#define RUDA_CLI_H

#include "ruda/base/stream.h"
#include "ruda/base/writer.h"
#include "ruda/session.h"

enum {
    RUDA_EXIT_SUCCESS = 0,
    RUDA_EXIT_SOURCE_FAILURE = 1,
    RUDA_EXIT_USAGE = 2,
    RUDA_EXIT_DEPENDENCY = 3,
    RUDA_EXIT_CAPABILITY = 4,
    RUDA_EXIT_DIVERGENCE = 5,
    RUDA_EXIT_INTERNAL = 70,
    RUDA_EXIT_INTERRUPTED = 130
};

int ruda_cli_run(
    RudaSession *session,
    int argument_count,
    const char *const arguments[],
    RudaReadStream standard_input,
    RudaWriter standard_output,
    RudaWriter standard_error
);

#endif
