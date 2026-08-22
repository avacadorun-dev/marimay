#ifndef MMAY_CLI_RESOURCE_REPORT_H
#define MMAY_CLI_RESOURCE_REPORT_H

#include "cli_internal.h"

#include "mmay/bytecode/vm.h"

bool mmay_cli_resource_render(
    MarimayDiagnosticStyle style,
    MarimayBytecodeRunStatus status,
    const MarimayManagedHeap *heap,
    MarimayWriter writer,
    bool *classified
);

#endif
