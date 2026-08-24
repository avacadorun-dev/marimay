#ifndef MMAY_CLI_PROGRAM_ARGUMENTS_H
#define MMAY_CLI_PROGRAM_ARGUMENTS_H

#include "cli_internal.h"

#include "mmay/runtime/program_arguments.h"

typedef struct MarimayCliProgramArguments {
    MarimayAllocator allocator;
    MarimayProgramArgument *values;
    size_t allocation_size;
    MarimayProgramArgumentList list;
} MarimayCliProgramArguments;

MarimayProgramArgumentsStatus mmay_cli_program_arguments_build(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayCliProgramArguments *arguments
);
void mmay_cli_program_arguments_dispose(
    MarimayCliProgramArguments *arguments
);
int mmay_cli_program_arguments_error(
    const MarimayCliOptions *options,
    MarimayProgramArgumentsStatus status,
    MarimayWriter standard_error
);

#endif
