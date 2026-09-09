#include "vm_internal.h"

#include "mmay/runtime/string.h"

/* Strings share the heap, precise roots, and byte budget of other VM values.
 * Never publish into a register until allocation succeeds: that register may
 * still root the source of a slice or an aliased concatenation during GC. */
MarimayBytecodeRunStatus mmay_bytecode_execution_new_string(
    MarimayBytecodeExecution *execution,
    const unsigned char *bytes,
    size_t length,
    MarimayManagedReference *result
) {
    MarimayManagedReference reference = mmay_managed_invalid_reference();
    MarimayManagedStatus status;
    if (execution == NULL || !execution->initialized || result == NULL
        || (bytes == NULL && length != 0)) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    status = mmay_managed_string_new(
        &execution->managed_heap, bytes, length, &reference
    );
    if (status == MMAY_MANAGED_OK) *result = reference;
    return mmay_bytecode_run_managed_status(status);
}

bool mmay_bytecode_execution_string_bytes(
    const MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value,
    const unsigned char **bytes,
    size_t *length
) {
    return execution != NULL && execution->initialized
        && value.type == MMAY_BYTECODE_TYPE_STRING
        && mmay_managed_string_bytes(
            &execution->managed_heap, value.as.managed_reference, bytes, length
        ) == MMAY_MANAGED_OK;
}
