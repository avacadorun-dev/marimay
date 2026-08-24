#include "vm_internal.h"

#include <string.h>

#include "mmay/bytecode/vm_arguments.h"

MarimayBytecodeRunStatus mmay_bytecode_execution_program_arguments(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *entry,
    MarimayProgramArgumentList arguments,
    MarimayBytecodeValue *result
) {
    const MarimayBytecodeType *parameter = NULL;
    const MarimayManagedArrayDescriptor *descriptor = NULL;
    MarimayManagedStatus status;
    if (result != NULL) memset(result, 0, sizeof(*result));
    if (execution == NULL || !execution->initialized
        || entry == NULL || !entry->initialized
        || entry->parameter_count != 1 || result == NULL
        || mmay_program_arguments_validate(arguments)
            != MMAY_PROGRAM_ARGUMENTS_OK) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    parameter = mmay_vector_at_const(&entry->register_types, 0);
    if (parameter == NULL || *parameter != MMAY_BYTECODE_TYPE_ARRAY) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    descriptor = mmay_bytecode_array_descriptor_for(
        execution, entry, *parameter, mmay_bytecode_register_layout(entry, 0)
    );
    if (descriptor == NULL) {
        return execution->array_descriptor_allocation_failed
            ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    result->type = *parameter;
    result->as.managed_reference = mmay_managed_invalid_reference();
    status = mmay_program_arguments_managed(
        &execution->managed_heap, descriptor, arguments,
        &result->as.managed_reference
    );
    return mmay_bytecode_run_managed_status(status);
}
