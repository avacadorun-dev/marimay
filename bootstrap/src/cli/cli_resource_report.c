#include "cli_resource_report.h"

#include <stdio.h>
#include <string.h>

#include "ruda/generated/diagnostic_codes.h"

bool ruda_cli_resource_render(
    RudaDiagnosticStyle style,
    RudaBytecodeRunStatus status,
    const RudaManagedHeap *heap,
    RudaWriter writer,
    bool *classified
) {
    RudaDiagnostic diagnostic;
    RudaManagedResourceFailure failure;
    char message[192];
    int length = 0;
    if (classified == NULL) return false;
    *classified = false;
    memset(&diagnostic, 0, sizeof(diagnostic));
    memset(&failure, 0, sizeof(failure));
    diagnostic.code = RUDA_DIAGNOSTIC_RUNTIME_FAILURE;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    if (status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED) {
        diagnostic.message = "runtime host allocation failed";
        diagnostic.runtime_resource = RUDA_RUNTIME_RESOURCE_HOST_ALLOCATION;
    } else if (status == RUDA_BYTECODE_RUN_LIMIT_REACHED
        && ruda_managed_heap_last_resource_failure(heap, &failure)
        && failure.classification == RUDA_MANAGED_RESOURCE_FAILURE_BUDGET) {
        length = snprintf(
            message, sizeof(message),
            "managed heap budget exceeded: live=%zu bytes, requested=%zu bytes, limit=%zu bytes",
            failure.live_bytes, failure.requested_bytes, failure.limit_bytes
        );
        if (length < 0 || (size_t)length >= sizeof(message)) return false;
        diagnostic.message = message;
        diagnostic.runtime_resource =
            RUDA_RUNTIME_RESOURCE_MANAGED_HEAP_BUDGET;
        diagnostic.resource_live_bytes = failure.live_bytes;
        diagnostic.resource_requested_bytes = failure.requested_bytes;
        diagnostic.resource_limit_bytes = failure.limit_bytes;
    } else {
        return true;
    }
    *classified = true;
    return ruda_diagnostic_render(writer, style, &diagnostic);
}
