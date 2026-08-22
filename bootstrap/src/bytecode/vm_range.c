#include "vm_internal.h"

#include <string.h>

static RudaBytecodeRunStatus ruda_bytecode_range_array_valid(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeValue value,
    uint32_t layout
) {
    if (ruda_bytecode_managed_value_valid_for(
            execution, function, value, layout
        )) {
        return RUDA_BYTECODE_RUN_OK;
    }
    return execution != NULL && execution->array_descriptor_allocation_failed
        ? RUDA_BYTECODE_RUN_ALLOCATION_FAILED
        : RUDA_BYTECODE_RUN_INVALID_REFERENCE;
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
    status = ruda_bytecode_range_array_valid(
        execution, function, registers[array_register],
        ruda_bytecode_register_layout(function, (uint32_t)array_register)
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    if (instruction->opcode == RUDA_OPCODE_ARRAY_GET_IN_BOUNDS) {
        RudaManagedCell cell;
        memset(&cell, 0, sizeof(cell));
        managed_status = ruda_managed_array_get_in_bounds(
            &execution->managed_heap,
            registers[array_register].as.managed_reference,
            (size_t)registers[index_register].as.i64,
            &cell
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
        managed_status = ruda_managed_array_set_in_bounds(
            &execution->managed_heap,
            registers[array_register].as.managed_reference,
            (size_t)registers[index_register].as.i64,
            cell
        );
        return managed_status == RUDA_MANAGED_OK
            ? RUDA_BYTECODE_RUN_OK
            : ruda_bytecode_run_managed_status(managed_status);
    }
}
