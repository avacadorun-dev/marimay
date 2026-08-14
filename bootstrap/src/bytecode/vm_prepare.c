#include "vm_internal.h"

#include <limits.h>
#include <string.h>

bool ruda_bytecode_run_consume_step(
    size_t *steps,
    RudaBytecodeRunOptions options
) {
    if (*steps >= options.max_steps
        || (options.remaining_shared_steps != NULL
            && *options.remaining_shared_steps == 0)) {
        return false;
    }
    *steps += 1;
    if (options.remaining_shared_steps != NULL) {
        *options.remaining_shared_steps -= 1;
    }
    return true;
}

void ruda_bytecode_run_set_trap(
    RudaBytecodeRunOptions options,
    RudaBytecodeSpan span
) {
    if (options.trap != NULL) {
        options.trap->span = span;
        options.trap->has_span = true;
    }
}

void ruda_bytecode_run_reset_panic(RudaBytecodePanic *panic) {
    if (panic != NULL && panic->initialized) {
        ruda_owned_bytes_dispose(&panic->message);
        panic->frames.length = 0;
        panic->code = NULL;
        memset(&panic->symbol, 0, sizeof(panic->symbol));
        memset(&panic->span, 0, sizeof(panic->span));
    }
}

RudaBytecodeRunStatus ruda_bytecode_run_begin_panic(
    RudaBytecodePanic *panic,
    RudaAllocator allocator,
    RudaBytecodeExecution *execution,
    RudaBytecodeValue message,
    RudaSymbolId symbol,
    RudaBytecodeSpan span
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    RudaBytecodePanicFrame frame;
    if (!ruda_bytecode_execution_string_bytes(
            execution, message, &bytes, &length
        )) {
        return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
    }
    if (panic == NULL) {
        return RUDA_BYTECODE_RUN_PANIC;
    }
    ruda_bytecode_run_reset_panic(panic);
    if (!ruda_owned_bytes_copy(&panic->message, allocator, bytes, length)) {
        ruda_bytecode_run_reset_panic(panic);
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    panic->code = "G4000";
    panic->symbol = symbol;
    panic->span = span;
    frame.symbol = symbol;
    frame.call_span = span;
    if (!ruda_vector_push(&panic->frames, &frame)) {
        ruda_bytecode_run_reset_panic(panic);
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return RUDA_BYTECODE_RUN_PANIC;
}

static size_t ruda_bytecode_append_decimal_u64(
    unsigned char *buffer,
    size_t offset,
    uint64_t value
) {
    unsigned char digits[3 * sizeof(uint64_t)];
    size_t count = 0;
    do {
        digits[count++] = (unsigned char)('0' + value % UINT64_C(10));
        value /= UINT64_C(10);
    } while (value != 0);
    while (count != 0) {
        buffer[offset++] = digits[--count];
    }
    return offset;
}

static size_t ruda_bytecode_append_decimal_i64(
    unsigned char *buffer,
    size_t offset,
    int64_t value
) {
    uint64_t magnitude = (uint64_t)value;
    if (value < 0) {
        buffer[offset++] = '-';
        magnitude = UINT64_C(0) - magnitude;
    }
    return ruda_bytecode_append_decimal_u64(buffer, offset, magnitude);
}

RudaBytecodeRunStatus ruda_bytecode_run_begin_bounds_panic(
    RudaBytecodePanic *panic,
    RudaAllocator allocator,
    int64_t index,
    size_t length,
    RudaSymbolId symbol,
    RudaBytecodeSpan span
) {
    static const unsigned char prefix[] = "array index ";
    static const unsigned char suffix[] = " is outside length ";
    unsigned char message[sizeof(prefix) + sizeof(suffix)
        + 6 * sizeof(uint64_t)];
    size_t message_length = 0;
    RudaBytecodePanicFrame frame;
    if (panic == NULL) {
        return RUDA_BYTECODE_RUN_PANIC;
    }
    ruda_bytecode_run_reset_panic(panic);
    memcpy(message + message_length, prefix, sizeof(prefix) - 1);
    message_length += sizeof(prefix) - 1;
    message_length = ruda_bytecode_append_decimal_i64(
        message, message_length, index
    );
    memcpy(message + message_length, suffix, sizeof(suffix) - 1);
    message_length += sizeof(suffix) - 1;
    message_length = ruda_bytecode_append_decimal_u64(
        message, message_length, (uint64_t)length
    );
    if (!ruda_owned_bytes_copy(
            &panic->message, allocator, message, message_length
        )) {
        ruda_bytecode_run_reset_panic(panic);
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    panic->code = "G4002";
    panic->symbol = symbol;
    panic->span = span;
    frame.symbol = symbol;
    frame.call_span = span;
    if (!ruda_vector_push(&panic->frames, &frame)) {
        ruda_bytecode_run_reset_panic(panic);
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return RUDA_BYTECODE_RUN_PANIC;
}

RudaBytecodeRunStatus ruda_bytecode_run_append_panic_frame(
    RudaBytecodePanic *panic,
    RudaSymbolId symbol,
    RudaBytecodeSpan call_span
) {
    RudaBytecodePanicFrame frame;
    if (panic == NULL) {
        return RUDA_BYTECODE_RUN_PANIC;
    }
    frame.symbol = symbol;
    frame.call_span = call_span;
    if (!ruda_vector_push(&panic->frames, &frame)) {
        ruda_bytecode_run_reset_panic(panic);
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return RUDA_BYTECODE_RUN_PANIC;
}

bool ruda_bytecode_run_string_value_valid(
    RudaBytecodeExecution *execution,
    RudaBytecodeValue value
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    return ruda_bytecode_execution_string_bytes(
        execution, value, &bytes, &length
    );
}

bool ruda_bytecode_run_function_uses_strings(
    const RudaBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL) {
        return false;
    }
    if (function->string_constants.length != 0) {
        return true;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && *type == RUDA_BYTECODE_TYPE_STRING) {
            return true;
        }
    }
    return function->result_type == RUDA_BYTECODE_TYPE_STRING;
}

bool ruda_bytecode_run_program_uses_strings(
    const RudaBytecodeProgram *program
) {
    size_t index = 0;
    if (program == NULL) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function = ruda_vector_at_const(
            &program->functions, index
        );
        if (function == NULL || ruda_bytecode_run_function_uses_strings(
                &function->body
            )) {
            return true;
        }
    }
    return false;
}

bool ruda_bytecode_run_function_uses_arrays(
    const RudaBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL || ruda_bytecode_type_is_array(function->result_type)
        || function->array_constructions.length != 0) {
        return function != NULL;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && ruda_bytecode_type_is_array(*type)) {
            return true;
        }
    }
    return false;
}

bool ruda_bytecode_run_program_uses_arrays(
    const RudaBytecodeProgram *program
) {
    size_t index = 0;
    if (program == NULL) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function = ruda_vector_at_const(
            &program->functions, index
        );
        if (function == NULL
            || ruda_bytecode_run_function_uses_arrays(&function->body)) {
            return true;
        }
    }
    return false;
}

bool ruda_bytecode_run_function_uses_managed(
    const RudaBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL
        || ruda_bytecode_type_is_managed(function->result_type)) {
        return function != NULL;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL && ruda_bytecode_type_is_managed(*type)) {
            return true;
        }
    }
    return false;
}

bool ruda_bytecode_run_program_uses_managed(
    const RudaBytecodeProgram *program
) {
    size_t index = 0;
    if (program == NULL) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function = ruda_vector_at_const(
            &program->functions,
            index
        );
        if (function == NULL
            || ruda_bytecode_run_function_uses_managed(&function->body)) {
            return true;
        }
    }
    return false;
}

RudaBytecodeRunStatus ruda_bytecode_run_arguments_validate(
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    const RudaBytecodeValue *arguments,
    size_t argument_count
) {
    size_t index = 0;
    if (function == NULL || (arguments == NULL && argument_count != 0)
        || argument_count != function->parameter_count) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType *expected = ruda_vector_at_const(
            &function->register_types, index
        );
        uint32_t layout = ruda_bytecode_register_layout(
            function, (uint32_t)index
        );
        if (expected == NULL || arguments[index].type != *expected) {
            return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
        }
        if (*expected == RUDA_BYTECODE_TYPE_STRING
            && !ruda_bytecode_run_string_value_valid(
                execution, arguments[index]
            )) {
            return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
        }
        if (ruda_bytecode_type_is_array(*expected)
            && !ruda_bytecode_managed_value_valid(
                execution, arguments[index]
            )) {
            return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
        }
        if (*expected == RUDA_BYTECODE_TYPE_RECORD
            || *expected == RUDA_BYTECODE_TYPE_ENUM) {
            RudaBytecodeRunStatus status =
                ruda_bytecode_run_nominal_value_validate(
                    execution,
                    function,
                    arguments[index],
                    *expected,
                    layout
                );
            if (status != RUDA_BYTECODE_RUN_OK) {
                return status;
            }
        }
        if (*expected == RUDA_BYTECODE_TYPE_INTERFACE
            && !ruda_bytecode_run_interface_value_valid(
                execution, function, arguments[index], layout
            )) {
            return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
        }
    }
    return RUDA_BYTECODE_RUN_OK;
}

RudaBytecodeRunStatus ruda_bytecode_run_roots_init(
    RudaBytecodeManagedRoots *roots,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function,
    RudaBytecodeValue *registers,
    RudaAllocator allocator
) {
    size_t count = 0;
    size_t index = 0;
    RudaManagedStatus status;
    if (roots == NULL || execution == NULL || function == NULL
        || registers == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    memset(roots, 0, sizeof(*roots));
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && ruda_bytecode_type_is_managed(*type)) {
            count += 1;
        }
    }
    if (!ruda_size_multiply(
            count, sizeof(RudaManagedReference *), &roots->location_bytes
        )) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (roots->location_bytes != 0) {
        roots->locations = ruda_allocate(&allocator, roots->location_bytes);
        if (roots->locations == NULL) {
            return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        }
    }
    count = 0;
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && ruda_bytecode_type_is_managed(*type)) {
            roots->locations[count++] = &registers[index].as.managed_reference;
        }
    }
    status = ruda_managed_root_push(
        &execution->managed_heap, &roots->frame, roots->locations, count
    );
    if (status != RUDA_MANAGED_OK) {
        ruda_deallocate(
            &allocator, roots->locations, roots->location_bytes
        );
        memset(roots, 0, sizeof(*roots));
        return status == RUDA_MANAGED_INVALID_REFERENCE
            ? RUDA_BYTECODE_RUN_INVALID_REFERENCE
            : RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return RUDA_BYTECODE_RUN_OK;
}

void ruda_bytecode_run_roots_dispose(
    RudaBytecodeManagedRoots *roots,
    RudaBytecodeExecution *execution,
    RudaAllocator allocator
) {
    if (roots == NULL) {
        return;
    }
    if (execution != NULL && roots->frame.linked) {
        (void)ruda_managed_root_pop(&execution->managed_heap, &roots->frame);
    }
    ruda_deallocate(&allocator, roots->locations, roots->location_bytes);
    memset(roots, 0, sizeof(*roots));
}

bool ruda_bytecode_run_function_uses_f64(
    const RudaBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL) {
        return false;
    }
    if (function->result_type == RUDA_BYTECODE_TYPE_F64) {
        return true;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && *type == RUDA_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    return false;
}

bool ruda_bytecode_run_program_uses_f64(
    const RudaBytecodeProgram *program
) {
    size_t index = 0;
    if (program == NULL) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function = ruda_vector_at_const(
            &program->functions, index
        );
        if (function == NULL
            || ruda_bytecode_run_function_uses_f64(&function->body)) {
            return true;
        }
    }
    return false;
}

bool ruda_bytecode_run_function_has_calls(
    const RudaBytecodeFunction *function
) {
    size_t block_index = 0;
    if (function == NULL || function->calls.length != 0
        || function->call_arguments.length != 0) {
        return true;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL) {
            return true;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (instruction == NULL
                || instruction->opcode == RUDA_OPCODE_CALL_DIRECT
                || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID
                || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER
                || instruction->opcode == RUDA_OPCODE_CALL_INTERFACE) {
                return true;
            }
        }
    }
    return false;
}
