#ifndef RUDA_CLI_RESOURCE_REPORT_H
#define RUDA_CLI_RESOURCE_REPORT_H

#include "cli_internal.h"

#include "ruda/bytecode/vm.h"

bool ruda_cli_resource_render(
    RudaDiagnosticStyle style,
    RudaBytecodeRunStatus status,
    const RudaManagedHeap *heap,
    RudaWriter writer,
    bool *classified
);

#endif
