#ifndef RUDA_CLI_RUN_REPORT_H
#define RUDA_CLI_RUN_REPORT_H

#include "cli_internal.h"

#include "ruda/bytecode/vm.h"
#include "ruda/check/entry.h"

int ruda_cli_run_entry_error(
    const RudaCliOptions *options,
    RudaCheckEntryStatus status,
    RudaWriter standard_error
);
int ruda_cli_run_pipeline_error(
    const RudaCliOptions *options,
    const char *message,
    RudaWriter standard_error
);
int ruda_cli_run_runtime_error(
    const RudaCliOptions *options,
    RudaBytecodeRunStatus status,
    const RudaBytecodePanic *panic,
    RudaWriter standard_error
);
int ruda_cli_run_expected_error(
    const RudaCliOptions *options,
    const unsigned char *message,
    size_t message_length,
    RudaWriter standard_error
);

#endif
