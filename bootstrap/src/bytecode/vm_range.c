#include "vm_internal.h"

#include <string.h>

static const MarimayManagedArrayDescriptor *mmay_bytecode_range_descriptor(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeValue value,
    uint32_t layout,
    MarimayBytecodeRunStatus *status
) {
    const MarimayManagedArrayDescriptor *descriptor = NULL;
    if (status != NULL) {
        *status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    }
    if (execution == NULL || function == NULL || status == NULL) {
        return NULL;
    }
    descriptor = mmay_bytecode_array_descriptor_for(
        execution, function, value.type, layout
    );
    if (descriptor != NULL) {
        *status = MMAY_BYTECODE_RUN_OK;
        return descriptor;
    }
    if (execution->array_descriptor_allocation_failed) {
        *status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return NULL;
}

MarimayBytecodeRunStatus mmay_bytecode_run_range_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
) {
    size_t array_register = 0, index_register = 0, value_register = 0;
    MarimayBytecodeRunStatus status;
    MarimayManagedStatus managed_status;
    const MarimayManagedArrayDescriptor *descriptor = NULL;
    size_t current_length = 0;
    if (instruction == NULL || registers == NULL || execution == NULL
        || function == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (instruction->opcode == MMAY_OPCODE_ARRAY_GET_IN_BOUNDS) {
        array_register = (size_t)instruction->operands[1].unsigned_value;
        index_register = (size_t)instruction->operands[2].unsigned_value;
    } else if (instruction->opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS) {
        array_register = (size_t)instruction->operands[0].unsigned_value;
        index_register = (size_t)instruction->operands[1].unsigned_value;
        value_register = (size_t)instruction->operands[2].unsigned_value;
    } else {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    descriptor = mmay_bytecode_range_descriptor(
        execution, function, registers[array_register],
        mmay_bytecode_register_layout(function, (uint32_t)array_register),
        &status
    );
    if (descriptor == NULL || status != MMAY_BYTECODE_RUN_OK) {
        return status;
    }
    if (instruction->opcode == MMAY_OPCODE_ARRAY_GET_IN_BOUNDS) {
        MarimayManagedCell cell;
        memset(&cell, 0, sizeof(cell));
        managed_status = mmay_managed_array_access_get(
            &execution->managed_heap,
            registers[array_register].as.managed_reference,
            descriptor,
            registers[index_register].as.i64,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS,
            &cell,
            &current_length
        );
        if (managed_status != MMAY_MANAGED_OK) {
            return mmay_bytecode_run_managed_status(managed_status);
        }
        return mmay_bytecode_managed_cell_to_value(
            execution, cell,
            &registers[(size_t)instruction->operands[0].unsigned_value]
        );
    }
    {
        MarimayManagedCell cell;
        memset(&cell, 0, sizeof(cell));
        status = mmay_bytecode_value_to_managed_cell(
            execution, &registers[value_register], &cell
        );
        if (status != MMAY_BYTECODE_RUN_OK) {
            return status;
        }
        managed_status = mmay_managed_array_access_set(
            &execution->managed_heap,
            registers[array_register].as.managed_reference,
            descriptor,
            registers[index_register].as.i64,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS,
            cell,
            &current_length
        );
        return managed_status == MMAY_MANAGED_OK
            ? MMAY_BYTECODE_RUN_OK
            : mmay_bytecode_run_managed_status(managed_status);
    }
}
