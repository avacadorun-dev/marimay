#include "vm_internal.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/checked_i64.h"
#include "ruda/base/f64.h"

RudaBytecodeExecutionOptions ruda_bytecode_execution_default_options(void) {
    RudaBytecodeExecutionOptions options;
    options.max_strings = 1000000;
    options.max_total_string_bytes = 64U * 1024U * 1024U;
    options.managed_heap = ruda_managed_heap_default_options();
    return options;
}

bool ruda_bytecode_execution_init(
    RudaBytecodeExecution *execution,
    RudaAllocator allocator,
    RudaBytecodeExecutionOptions options
) {
    if (execution == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || options.max_strings == 0
        || options.max_strings > UINT32_MAX || options.max_total_string_bytes == 0) {
        return false;
    }
    memset(execution, 0, sizeof(*execution));
    if (!ruda_vector_init(
            &execution->strings, allocator, sizeof(RudaOwnedBytes)
        ) || !ruda_map_init(
            &execution->string_index, allocator, sizeof(RudaBytecodeReference),
            UINT64_C(0x7275646162746573), UINT64_C(0x7472696e67696478)
        ) || !ruda_managed_heap_init(
            &execution->managed_heap, allocator, options.managed_heap
        )) {
        ruda_managed_heap_dispose(&execution->managed_heap);
        ruda_map_dispose(&execution->string_index);
        ruda_vector_dispose(&execution->strings);
        return false;
    }
    execution->allocator = allocator;
    execution->options = options;
    execution->initialized = true;
    return true;
}

void ruda_bytecode_execution_dispose(RudaBytecodeExecution *execution) {
    size_t index = 0;
    if (execution == NULL) {
        return;
    }
    for (index = 0; index < execution->strings.length; index += 1) {
        RudaOwnedBytes *string = ruda_vector_at(&execution->strings, index);
        if (string != NULL) {
            ruda_owned_bytes_dispose(string);
        }
    }
    ruda_map_dispose(&execution->string_index);
    ruda_vector_dispose(&execution->strings);
    ruda_managed_heap_dispose(&execution->managed_heap);
    memset(execution, 0, sizeof(*execution));
}

static const RudaManagedArrayDescriptor *ruda_bytecode_array_descriptor(
    RudaBytecodeType type
) {
    if (type == RUDA_BYTECODE_TYPE_ARRAY_F64) {
        return &ruda_managed_array_f64_descriptor;
    }
    if (type == RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64) {
        return &ruda_managed_array_array_f64_descriptor;
    }
    return NULL;
}

bool ruda_bytecode_managed_value_valid(
    RudaBytecodeExecution *execution,
    RudaBytecodeValue value
) {
    const RudaManagedArrayDescriptor *actual = NULL;
    const RudaManagedArrayDescriptor *expected =
        ruda_bytecode_array_descriptor(value.type);
    return execution != NULL && execution->initialized && expected != NULL
        && ruda_managed_array_describe(
            &execution->managed_heap, value.as.managed_reference, &actual
        ) == RUDA_MANAGED_OK && actual == expected;
}

bool ruda_bytecode_execution_string_bytes(
    const RudaBytecodeExecution *execution,
    RudaBytecodeValue value,
    const unsigned char **bytes,
    size_t *length
) {
    const RudaOwnedBytes *string = execution != NULL && execution->initialized
            && value.type == RUDA_BYTECODE_TYPE_STRING
            && value.as.reference != RUDA_BYTECODE_INVALID_REFERENCE
        ? ruda_vector_at_const(&execution->strings, value.as.reference)
        : NULL;
    if (string == NULL || string->data == NULL || bytes == NULL || length == NULL) {
        return false;
    }
    *bytes = string->data;
    *length = string->length;
    return true;
}

bool ruda_bytecode_panic_init(RudaBytecodePanic *panic, RudaAllocator allocator) {
    if (panic == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL) {
        return false;
    }
    memset(panic, 0, sizeof(*panic));
    if (!ruda_vector_init(
            &panic->frames, allocator, sizeof(RudaBytecodePanicFrame)
        )) {
        return false;
    }
    panic->initialized = true;
    return true;
}

void ruda_bytecode_panic_dispose(RudaBytecodePanic *panic) {
    if (panic == NULL) {
        return;
    }
    ruda_owned_bytes_dispose(&panic->message);
    ruda_vector_dispose(&panic->frames);
    memset(panic, 0, sizeof(*panic));
}

const RudaBytecodePanicFrame *ruda_bytecode_panic_frame(
    const RudaBytecodePanic *panic,
    size_t index
) {
    return panic != NULL && panic->initialized
        ? ruda_vector_at_const(&panic->frames, index)
        : NULL;
}

static RudaBytecodeRunStatus ruda_bytecode_execution_intern(
    RudaBytecodeExecution *execution,
    const unsigned char *bytes,
    size_t length,
    RudaBytecodeReference *result
) {
    const RudaBytecodeReference *found = NULL;
    RudaOwnedBytes owned;
    RudaBytecodeReference reference;
    size_t next_bytes = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || (bytes == NULL && length != 0)) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    found = ruda_map_get_const(&execution->string_index, bytes, length);
    if (found != NULL) {
        *result = *found;
        return RUDA_BYTECODE_RUN_OK;
    }
    if (execution->strings.length >= execution->options.max_strings
        || execution->strings.length >= UINT32_MAX
        || !ruda_size_add(execution->total_string_bytes, length, &next_bytes)
        || next_bytes > execution->options.max_total_string_bytes) {
        return RUDA_BYTECODE_RUN_LIMIT_REACHED;
    }
    memset(&owned, 0, sizeof(owned));
    if (!ruda_owned_bytes_copy(&owned, execution->allocator, bytes, length)) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    reference = (RudaBytecodeReference)execution->strings.length;
    if (!ruda_vector_push(&execution->strings, &owned)
        || !ruda_map_put(&execution->string_index, bytes, length, &reference)) {
        if (execution->strings.length != 0
            && reference == execution->strings.length - 1) {
            execution->strings.length -= 1;
        }
        ruda_owned_bytes_dispose(&owned);
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    execution->total_string_bytes = next_bytes;
    *result = reference;
    return RUDA_BYTECODE_RUN_OK;
}

RudaBytecodeRunOptions ruda_bytecode_run_default_options(void) {
    RudaBytecodeRunOptions options;
    options.max_steps = 10000000;
    options.max_call_depth = 1024;
    options.remaining_shared_steps = NULL;
    options.execution = NULL;
    options.panic = NULL;
    options.trap = NULL;
    return options;
}

static RudaBytecodeRunStatus ruda_bytecode_run_checked_status(
    RudaCheckedI64Status status
) {
    if (status == RUDA_CHECKED_I64_OK) {
        return RUDA_BYTECODE_RUN_OK;
    }
    if (status == RUDA_CHECKED_I64_DIVISION_BY_ZERO) {
        return RUDA_BYTECODE_RUN_DIVISION_BY_ZERO;
    }
    if (status == RUDA_CHECKED_I64_INVALID_SHIFT) {
        return RUDA_BYTECODE_RUN_INVALID_SHIFT;
    }
    return RUDA_BYTECODE_RUN_OVERFLOW;
}

static RudaBytecodeRunStatus ruda_bytecode_run_managed_status(
    RudaManagedStatus status
) {
    switch (status) {
        case RUDA_MANAGED_OK:
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_MANAGED_ALLOCATION_FAILED:
            return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        case RUDA_MANAGED_LIMIT_REACHED:
            return RUDA_BYTECODE_RUN_LIMIT_REACHED;
        case RUDA_MANAGED_LENGTH_LIMIT:
            /* ARRAY_NEW cannot reach this after verified bounded decoding. */
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
        case RUDA_MANAGED_INVALID_REFERENCE:
            return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
        case RUDA_MANAGED_BOUNDS:
            return RUDA_BYTECODE_RUN_PANIC;
        case RUDA_MANAGED_INVALID_ARGUMENT:
        case RUDA_MANAGED_TYPE_MISMATCH:
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
}

RudaBytecodeRunStatus ruda_bytecode_run_value_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaAllocator allocator,
    int64_t *bounds_index,
    size_t *bounds_length
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    switch (instruction->opcode) {
        case RUDA_OPCODE_CONST_I64:
            registers[target].as.i64 = instruction->operands[1].signed_value;
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_CONST_F64:
            registers[target].as.f64_bits = instruction->operands[1].unsigned_value;
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_CONST_BOOL:
            registers[target].as.boolean = instruction->operands[1].unsigned_value
                != 0;
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_CONST_STRING: {
            const RudaBytecodeStringConstant *constant =
                ruda_bytecode_string_constant(
                    function,
                    (RudaBytecodeStringId)instruction->operands[1].unsigned_value
                );
            RudaBytecodeReference reference = RUDA_BYTECODE_INVALID_REFERENCE;
            if (constant == NULL || execution == NULL || !execution->initialized) {
                return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            }
            RudaBytecodeRunStatus status = ruda_bytecode_execution_intern(
                    execution,
                    constant->value.data,
                    constant->value.length,
                    &reference
                );
            if (status != RUDA_BYTECODE_RUN_OK) {
                return status;
            }
            registers[target].as.reference = reference;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_COPY:
            registers[target] = registers[
                (size_t)instruction->operands[1].unsigned_value
            ];
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_ARRAY_NEW: {
            const RudaBytecodeArrayConstruction *construction =
                ruda_bytecode_array_construction(
                    function,
                    (RudaBytecodeArrayConstructionId)
                        instruction->operands[1].unsigned_value
                );
            const RudaManagedArrayDescriptor *descriptor = construction != NULL
                ? ruda_bytecode_array_descriptor(construction->array_type)
                : NULL;
            RudaManagedCell *cells = NULL;
            size_t cell_bytes = 0;
            size_t index = 0;
            RudaManagedStatus managed_status;
            if (execution == NULL || descriptor == NULL
                || construction->argument_count
                    > execution->options.managed_heap.max_array_length
                || !ruda_size_multiply(
                    construction->argument_count, sizeof(RudaManagedCell),
                    &cell_bytes
                )) {
                return descriptor == NULL
                    ? RUDA_BYTECODE_RUN_INVALID_BYTECODE
                    : RUDA_BYTECODE_RUN_LIMIT_REACHED;
            }
            if (cell_bytes != 0) {
                cells = ruda_allocate(&allocator, cell_bytes);
                if (cells == NULL) {
                    return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
                }
                memset(cells, 0, cell_bytes);
            }
            if (construction->argument_count != 0 && cells == NULL) {
                return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
            }
            for (index = 0; index < construction->argument_count; index += 1) {
                const RudaBytecodeArrayArgument *argument =
                    ruda_bytecode_array_argument(
                        function, construction->argument_start + index
                    );
                if (argument == NULL) {
                    ruda_deallocate(&allocator, cells, cell_bytes);
                    return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
                }
                if (construction->array_type == RUDA_BYTECODE_TYPE_ARRAY_F64) {
                    cells[index].f64_bits =
                        registers[argument->register_id].as.f64_bits;
                } else {
                    cells[index].reference =
                        registers[argument->register_id].as.managed_reference;
                }
            }
            managed_status = ruda_managed_array_new(
                &execution->managed_heap, descriptor, cells,
                construction->argument_count,
                &registers[target].as.managed_reference
            );
            ruda_deallocate(&allocator, cells, cell_bytes);
            return ruda_bytecode_run_managed_status(managed_status);
        }
        case RUDA_OPCODE_ARRAY_LENGTH: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t length = 0;
            if (!ruda_bytecode_managed_value_valid(
                    execution, registers[first]
                )) {
                return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
            }
            RudaManagedStatus managed_status = execution != NULL
                ? ruda_managed_array_length(
                    &execution->managed_heap,
                    registers[first].as.managed_reference,
                    &length
                )
                : RUDA_MANAGED_INVALID_ARGUMENT;
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_run_managed_status(managed_status);
            }
            if ((uintmax_t)length > (uintmax_t)INT64_MAX) {
                return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            }
            registers[target].as.i64 = (int64_t)length;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_ARRAY_REQUIRE_INDEX:
        case RUDA_OPCODE_ARRAY_GET: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            RudaManagedCell cell;
            size_t length = 0;
            int64_t index = registers[second].as.i64;
            if (!ruda_bytecode_managed_value_valid(
                    execution, registers[first]
                )) {
                return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
            }
            RudaManagedStatus managed_status = execution != NULL
                ? ruda_managed_array_get(
                    &execution->managed_heap,
                    registers[first].as.managed_reference,
                    index,
                    &cell,
                    &length
                )
                : RUDA_MANAGED_INVALID_ARGUMENT;
            if (managed_status == RUDA_MANAGED_BOUNDS) {
                if (bounds_index != NULL) {
                    *bounds_index = index;
                }
                if (bounds_length != NULL) {
                    *bounds_length = length;
                }
                return RUDA_BYTECODE_RUN_PANIC;
            }
            if (managed_status != RUDA_MANAGED_OK) {
                return ruda_bytecode_run_managed_status(managed_status);
            }
            if (instruction->opcode == RUDA_OPCODE_ARRAY_REQUIRE_INDEX) {
                registers[target] = registers[first];
            } else if (registers[target].type == RUDA_BYTECODE_TYPE_F64) {
                registers[target].as.f64_bits = cell.f64_bits;
            } else {
                registers[target].as.managed_reference = cell.reference;
            }
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_ARRAY_SET: {
            size_t array = (size_t)instruction->operands[0].unsigned_value;
            size_t index_register =
                (size_t)instruction->operands[1].unsigned_value;
            size_t value = (size_t)instruction->operands[2].unsigned_value;
            RudaManagedCell cell;
            size_t length = 0;
            int64_t index = registers[index_register].as.i64;
            RudaManagedStatus managed_status;
            memset(&cell, 0, sizeof(cell));
            if (!ruda_bytecode_managed_value_valid(
                    execution, registers[array]
                )) {
                return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
            }
            if (registers[value].type == RUDA_BYTECODE_TYPE_F64) {
                cell.f64_bits = registers[value].as.f64_bits;
            } else {
                cell.reference = registers[value].as.managed_reference;
            }
            managed_status = execution != NULL
                ? ruda_managed_array_set(
                    &execution->managed_heap,
                    registers[array].as.managed_reference,
                    index,
                    cell,
                    &length
                )
                : RUDA_MANAGED_INVALID_ARGUMENT;
            if (managed_status == RUDA_MANAGED_BOUNDS) {
                if (bounds_index != NULL) {
                    *bounds_index = index;
                }
                if (bounds_length != NULL) {
                    *bounds_length = length;
                }
                return RUDA_BYTECODE_RUN_PANIC;
            }
            return ruda_bytecode_run_managed_status(managed_status);
        }
        case RUDA_OPCODE_I64_ADD_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_add(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_subtract(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_multiply(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_DIVIDE_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_divide(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_REMAINDER_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_remainder(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_I64_NEGATE_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            return ruda_bytecode_run_checked_status(ruda_checked_i64_negate(
                registers[first].as.i64,
                &registers[target].as.i64
            ));
        }
        case RUDA_OPCODE_F64_NEGATE: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            registers[target].as.f64_bits = ruda_f64_negate(
                registers[first].as.f64_bits
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_BOOL_NOT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            registers[target].as.boolean = !registers[first].as.boolean;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_BITWISE_NOT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            registers[target].as.i64 = ruda_i64_bitwise_not(
                registers[first].as.i64
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_BITWISE_AND: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.i64 = ruda_i64_bitwise_and(
                registers[first].as.i64,
                registers[second].as.i64
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_BITWISE_XOR: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.i64 = ruda_i64_bitwise_xor(
                registers[first].as.i64,
                registers[second].as.i64
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_BITWISE_OR: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.i64 = ruda_i64_bitwise_or(
                registers[first].as.i64,
                registers[second].as.i64
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(
                ruda_checked_i64_shift_left(
                    registers[first].as.i64,
                    registers[second].as.i64,
                    &registers[target].as.i64
                )
            );
        }
        case RUDA_OPCODE_I64_SHIFT_RIGHT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            return ruda_bytecode_run_checked_status(
                ruda_checked_i64_shift_right(
                    registers[first].as.i64,
                    registers[second].as.i64,
                    &registers[target].as.i64
                )
            );
        }
        case RUDA_OPCODE_I64_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.i64
                == registers[second].as.i64;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_F64_ADD: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.f64_bits = ruda_f64_add(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_F64_SUBTRACT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.f64_bits = ruda_f64_subtract(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_F64_MULTIPLY: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.f64_bits = ruda_f64_multiply(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_F64_DIVIDE: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.f64_bits = ruda_f64_divide(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_F64_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = ruda_f64_equal(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_F64_LESS: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = ruda_f64_less(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_F64_LESS_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = ruda_f64_less_equal(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_BOOL_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.boolean
                == registers[second].as.boolean;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_STRING_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            const unsigned char *left = NULL;
            const unsigned char *right = NULL;
            size_t left_length = 0;
            size_t right_length = 0;
            if (!ruda_bytecode_execution_string_bytes(
                    execution, registers[first], &left, &left_length
                ) || !ruda_bytecode_execution_string_bytes(
                    execution, registers[second], &right, &right_length
                )) {
                return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
            }
            registers[target].as.boolean = left_length == right_length
                && (left_length == 0 || memcmp(left, right, left_length) == 0);
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_LESS: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.i64
                < registers[second].as.i64;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_I64_LESS_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.i64
                <= registers[second].as.i64;
            return RUDA_BYTECODE_RUN_OK;
        }
        case RUDA_OPCODE_INVALID:
        case RUDA_OPCODE_JUMP:
        case RUDA_OPCODE_BRANCH_BOOL:
        case RUDA_OPCODE_RETURN:
        case RUDA_OPCODE_RETURN_VOID:
        case RUDA_OPCODE_CALL_DIRECT:
        case RUDA_OPCODE_CALL_DIRECT_VOID:
        case RUDA_OPCODE_CALL_DIRECT_NEVER:
        case RUDA_OPCODE_PANIC_STRING:
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
}
