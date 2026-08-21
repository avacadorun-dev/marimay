#include "vm_internal.h"

static RudaBytecodeRunStatus ruda_bytecode_mutation_array_valid(
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

static RudaBytecodeRunStatus ruda_bytecode_mutation_status(
    RudaManagedStatus status,
    RudaBytecodePanic *panic,
    RudaAllocator allocator,
    RudaSymbolId symbol,
    RudaBytecodeSpan span
) {
    if (status == RUDA_MANAGED_ITERATOR_INVALIDATED) {
        return ruda_bytecode_run_begin_iterator_invalidated_panic(
            panic, allocator, symbol, span
        );
    }
    return ruda_bytecode_run_managed_status(status);
}

RudaBytecodeRunStatus ruda_bytecode_run_array_mutation_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaBytecodePanic *panic,
    RudaSymbolId symbol,
    bool *handled
) {
    bool writes_target;
    size_t array_register;
    RudaBytecodeRunStatus status;
    RudaManagedStatus managed_status;
    if (handled == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    *handled = false;
    if (instruction == NULL || registers == NULL || function == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    switch (instruction->opcode) {
        case RUDA_OPCODE_ARRAY_POP:
        case RUDA_OPCODE_ARRAY_TRUNCATE:
        case RUDA_OPCODE_ARRAY_CLEAR:
        case RUDA_OPCODE_ARRAY_RESERVE:
        case RUDA_OPCODE_ARRAY_GENERATION:
        case RUDA_OPCODE_ARRAY_REQUIRE_GENERATION:
            break;
        default:
            return RUDA_BYTECODE_RUN_OK;
    }
    *handled = true;
    writes_target = instruction->opcode == RUDA_OPCODE_ARRAY_POP
        || instruction->opcode == RUDA_OPCODE_ARRAY_GENERATION
        || instruction->opcode == RUDA_OPCODE_ARRAY_REQUIRE_GENERATION;
    array_register = (size_t)instruction->operands[writes_target ? 1 : 0]
        .unsigned_value;
    status = ruda_bytecode_mutation_array_valid(
        execution, function, registers[array_register],
        ruda_bytecode_register_layout(function, (uint32_t)array_register)
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    switch (instruction->opcode) {
        case RUDA_OPCODE_ARRAY_POP: {
            RudaManagedCell cell;
            bool popped = false;
            managed_status = ruda_managed_array_pop(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                &cell,
                &popped
            );
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_mutation_status(
                    managed_status, panic, allocator, symbol, instruction->span
                );
            }
            if (!popped) {
                return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            }
            return ruda_bytecode_managed_cell_to_value(
                execution, cell,
                &registers[(size_t)instruction->operands[0].unsigned_value]
            );
        }
        case RUDA_OPCODE_ARRAY_TRUNCATE:
            managed_status = ruda_managed_array_truncate(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                registers[(size_t)instruction->operands[1].unsigned_value].as.i64
            );
            break;
        case RUDA_OPCODE_ARRAY_CLEAR:
            managed_status = ruda_managed_array_clear(
                &execution->managed_heap,
                registers[array_register].as.managed_reference
            );
            break;
        case RUDA_OPCODE_ARRAY_RESERVE:
            managed_status = ruda_managed_array_reserve(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                registers[(size_t)instruction->operands[1].unsigned_value].as.i64
            );
            break;
        case RUDA_OPCODE_ARRAY_GENERATION: {
            uint64_t generation = 0;
            managed_status = ruda_managed_array_mutation_generation(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                &generation
            );
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_mutation_status(
                    managed_status, panic, allocator, symbol, instruction->span
                );
            }
            registers[(size_t)instruction->operands[0].unsigned_value]
                .as.integer_bits = generation;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_ARRAY_REQUIRE_GENERATION: {
            uint64_t expected = registers[
                (size_t)instruction->operands[2].unsigned_value
            ].as.integer_bits;
            managed_status = ruda_managed_array_check_mutation_generation(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                expected
            );
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_mutation_status(
                    managed_status, panic, allocator, symbol, instruction->span
                );
            }
            registers[(size_t)instruction->operands[0].unsigned_value] =
                registers[array_register];
            return RUDA_BYTECODE_RUN_OK;
        }
        default:
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return ruda_bytecode_mutation_status(
        managed_status, panic, allocator, symbol, instruction->span
    );
}
