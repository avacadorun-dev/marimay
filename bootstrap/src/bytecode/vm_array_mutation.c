#include "vm_internal.h"

static MarimayBytecodeRunStatus mmay_bytecode_mutation_array_valid(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeValue value,
    uint32_t layout
) {
    if (mmay_bytecode_managed_value_valid_for(
            execution, function, value, layout
        )) {
        return MMAY_BYTECODE_RUN_OK;
    }
    return execution != NULL && execution->array_descriptor_allocation_failed
        ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
        : MMAY_BYTECODE_RUN_INVALID_REFERENCE;
}

static MarimayBytecodeRunStatus mmay_bytecode_mutation_status(
    MarimayManagedStatus status,
    MarimayBytecodePanic *panic,
    MarimayAllocator allocator,
    MarimaySymbolId symbol,
    MarimayBytecodeSpan span
) {
    if (status == MMAY_MANAGED_ITERATOR_INVALIDATED) {
        return mmay_bytecode_run_begin_iterator_invalidated_panic(
            panic, allocator, symbol, span
        );
    }
    return mmay_bytecode_run_managed_status(status);
}

MarimayBytecodeRunStatus mmay_bytecode_run_array_mutation_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayBytecodePanic *panic,
    MarimaySymbolId symbol,
    bool *handled
) {
    bool writes_target;
    size_t array_register;
    MarimayBytecodeRunStatus status;
    MarimayManagedStatus managed_status;
    if (handled == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    *handled = false;
    if (instruction == NULL || registers == NULL || function == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    switch (instruction->opcode) {
        case MMAY_OPCODE_ARRAY_POP:
        case MMAY_OPCODE_ARRAY_TRUNCATE:
        case MMAY_OPCODE_ARRAY_CLEAR:
        case MMAY_OPCODE_ARRAY_RESERVE:
        case MMAY_OPCODE_ARRAY_GENERATION:
        case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION:
            break;
        default:
            return MMAY_BYTECODE_RUN_OK;
    }
    *handled = true;
    writes_target = instruction->opcode == MMAY_OPCODE_ARRAY_POP
        || instruction->opcode == MMAY_OPCODE_ARRAY_GENERATION
        || instruction->opcode == MMAY_OPCODE_ARRAY_REQUIRE_GENERATION;
    array_register = (size_t)instruction->operands[writes_target ? 1 : 0]
        .unsigned_value;
    status = mmay_bytecode_mutation_array_valid(
        execution, function, registers[array_register],
        mmay_bytecode_register_layout(function, (uint32_t)array_register)
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        return status;
    }
    switch (instruction->opcode) {
        case MMAY_OPCODE_ARRAY_POP: {
            MarimayManagedCell cell;
            bool popped = false;
            managed_status = mmay_managed_array_pop(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                &cell,
                &popped
            );
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_mutation_status(
                    managed_status, panic, allocator, symbol, instruction->span
                );
            }
            if (!popped) {
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            }
            return mmay_bytecode_managed_cell_to_value(
                execution, cell,
                &registers[(size_t)instruction->operands[0].unsigned_value]
            );
        }
        case MMAY_OPCODE_ARRAY_TRUNCATE:
            managed_status = mmay_managed_array_truncate(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                registers[(size_t)instruction->operands[1].unsigned_value].as.i64
            );
            break;
        case MMAY_OPCODE_ARRAY_CLEAR:
            managed_status = mmay_managed_array_clear(
                &execution->managed_heap,
                registers[array_register].as.managed_reference
            );
            break;
        case MMAY_OPCODE_ARRAY_RESERVE:
            managed_status = mmay_managed_array_reserve(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                registers[(size_t)instruction->operands[1].unsigned_value].as.i64
            );
            break;
        case MMAY_OPCODE_ARRAY_GENERATION: {
            uint64_t generation = 0;
            managed_status = mmay_managed_array_mutation_generation(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                &generation
            );
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_mutation_status(
                    managed_status, panic, allocator, symbol, instruction->span
                );
            }
            registers[(size_t)instruction->operands[0].unsigned_value]
                .as.integer_bits = generation;
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION: {
            uint64_t expected = registers[
                (size_t)instruction->operands[2].unsigned_value
            ].as.integer_bits;
            managed_status = mmay_managed_array_check_mutation_generation(
                &execution->managed_heap,
                registers[array_register].as.managed_reference,
                expected
            );
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_mutation_status(
                    managed_status, panic, allocator, symbol, instruction->span
                );
            }
            registers[(size_t)instruction->operands[0].unsigned_value] =
                registers[array_register];
            return MMAY_BYTECODE_RUN_OK;
        }
        default:
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return mmay_bytecode_mutation_status(
        managed_status, panic, allocator, symbol, instruction->span
    );
}
