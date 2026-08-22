#include "vm_internal.h"

#include <string.h>

static const RudaManagedArrayDescriptor *ruda_bytecode_range_descriptor(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeValue value,
    uint32_t layout,
    RudaBytecodeRunStatus *status
) {
    const RudaManagedArrayDescriptor *descriptor = NULL;
    if (status != NULL) {
        *status = RUDA_BYTECODE_RUN_INVALID_REFERENCE;
    }
    if (execution == NULL || function == NULL || status == NULL) {
        return NULL;
    }
    descriptor = ruda_bytecode_array_descriptor_for(
        execution, function, value.type, layout
    );
    if (descriptor != NULL) {
        *status = RUDA_BYTECODE_RUN_OK;
        return descriptor;
    }
    if (execution->array_descriptor_allocation_failed) {
        *status = RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return NULL;
}

RudaBytecodeRunStatus ruda_bytecode_run_range_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function
) {
    size_t array_register = 0, index_register = 0, value_register = 0;
    RudaBytecodeRunStatus status;
    RudaManagedStatus managed_status;
    const RudaManagedArrayDescriptor *descriptor = NULL;
    size_t current_length = 0;
    if (instruction == NULL || registers == NULL || execution == NULL
        || function == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (instruction->opcode == RUDA_OPCODE_ARRAY_GET_IN_BOUNDS) {
        array_register = (size_t)instruction->operands[1].unsigned_value;
        index_register = (size_t)instruction->operands[2].unsigned_value;
    } else if (instruction->opcode == RUDA_OPCODE_ARRAY_SET_IN_BOUNDS) {
        array_register = (size_t)instruction->operands[0].unsigned_value;
        index_register = (size_t)instruction->operands[1].unsigned_value;
        value_register = (size_t)instruction->operands[2].unsigned_value;
    } else {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    descriptor = ruda_bytecode_range_descriptor(
        execution, function, registers[array_register],
        ruda_bytecode_register_layout(function, (uint32_t)array_register),
        &status
    );
    if (descriptor == NULL || status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    if (instruction->opcode == RUDA_OPCODE_ARRAY_GET_IN_BOUNDS) {
        RudaManagedCell cell;
        memset(&cell, 0, sizeof(cell));
        managed_status = ruda_managed_array_access_get(
            &execution->managed_heap,
            registers[array_register].as.managed_reference,
            descriptor,
            registers[index_register].as.i64,
            RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS,
            &cell,
            &current_length
        );
        if (managed_status != RUDA_MANAGED_OK) {
            return ruda_bytecode_run_managed_status(managed_status);
        }
        return ruda_bytecode_managed_cell_to_value(
            execution, cell,
            &registers[(size_t)instruction->operands[0].unsigned_value]
        );
    }
    {
        RudaManagedCell cell;
        memset(&cell, 0, sizeof(cell));
        status = ruda_bytecode_value_to_managed_cell(
            execution, &registers[value_register], &cell
        );
        if (status != RUDA_BYTECODE_RUN_OK) {
            return status;
        }
        managed_status = ruda_managed_array_access_set(
            &execution->managed_heap,
            registers[array_register].as.managed_reference,
            descriptor,
            registers[index_register].as.i64,
            RUDA_MANAGED_ARRAY_ACCESS_IN_BOUNDS,
            cell,
            &current_length
        );
        return managed_status == RUDA_MANAGED_OK
            ? RUDA_BYTECODE_RUN_OK
            : ruda_bytecode_run_managed_status(managed_status);
    }
}
