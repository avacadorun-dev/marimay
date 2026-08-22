#ifndef MMAY_CLI_RUN_REPORT_H
#define MMAY_CLI_RUN_REPORT_H

#include "cli_internal.h"

#include "mmay/bytecode/vm.h"
#include "mmay/check/entry.h"

int mmay_cli_run_entry_error(
    const MarimayCliOptions *options,
    MarimayCheckEntryStatus status,
    MarimayWriter standard_error
);
int mmay_cli_run_pipeline_error(
    const MarimayCliOptions *options,
    const char *message,
    MarimayWriter standard_error
);
int mmay_cli_run_runtime_error(
    const MarimayCliOptions *options,
    MarimayBytecodeRunStatus status,
    const MarimayBytecodeExecution *execution,
    const MarimayBytecodePanic *panic,
    MarimayWriter standard_error
);
int mmay_cli_run_expected_error(
    const MarimayCliOptions *options,
    const unsigned char *message,
    size_t message_length,
    MarimayWriter standard_error
);

#endif
