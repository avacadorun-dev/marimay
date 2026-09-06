#include "vm_internal.h"

#include <limits.h>
#include <string.h>

bool mmay_bytecode_run_consume_step(
    size_t *steps,
    MarimayBytecodeRunOptions options
) {
    if ((options.max_steps != SIZE_MAX && *steps >= options.max_steps)
        || (options.remaining_shared_steps != NULL
            && *options.remaining_shared_steps == 0)) {
        return false;
    }
    if (options.max_steps != SIZE_MAX) *steps += 1;
    if (options.remaining_shared_steps != NULL) {
        *options.remaining_shared_steps -= 1;
    }
    return true;
}

void mmay_bytecode_run_set_trap(
    MarimayBytecodeRunOptions options,
    MarimayBytecodeSpan span
) {
    if (options.trap != NULL) {
        options.trap->span = span;
        options.trap->has_span = true;
    }
}

void mmay_bytecode_run_reset_panic(MarimayBytecodePanic *panic) {
    if (panic != NULL && panic->initialized) {
        size_t index = 0;
        mmay_owned_bytes_dispose(&panic->message);
        for (index = 0; index < panic->suppressed.length; index += 1) {
            MarimayBytecodeSuppressedPanic *suppressed = mmay_vector_at(
                &panic->suppressed, index
            );
            if (suppressed != NULL) {
                mmay_owned_bytes_dispose(&suppressed->message);
                mmay_vector_dispose(&suppressed->frames);
            }
        }
        panic->frames.length = 0;
        panic->suppressed.length = 0;
        panic->code = NULL;
        memset(&panic->symbol, 0, sizeof(panic->symbol));
        memset(&panic->span, 0, sizeof(panic->span));
    }
}

static bool mmay_bytecode_run_push_suppressed(
    MarimayBytecodePanic *primary,
    MarimayBytecodeSuppressedPanic *suppressed
) {
    if (!mmay_vector_push(&primary->suppressed, suppressed)) {
        return false;
    }
    memset(&suppressed->message, 0, sizeof(suppressed->message));
    memset(&suppressed->frames, 0, sizeof(suppressed->frames));
    return true;
}

bool mmay_bytecode_run_append_suppressed_panic(
    MarimayBytecodePanic *primary,
    MarimayBytecodePanic *secondary
) {
    MarimayBytecodeSuppressedPanic observed;
    size_t index = 0;
    if (primary == NULL || secondary == NULL || !primary->initialized
        || !secondary->initialized) {
        return false;
    }
    memset(&observed, 0, sizeof(observed));
    observed.code = secondary->code;
    observed.message = secondary->message;
    observed.symbol = secondary->symbol;
    observed.span = secondary->span;
    observed.frames = secondary->frames;
    if (!mmay_bytecode_run_push_suppressed(primary, &observed)) {
        return false;
    }
    memset(&secondary->message, 0, sizeof(secondary->message));
    memset(&secondary->frames, 0, sizeof(secondary->frames));
    for (index = 0; index < secondary->suppressed.length; index += 1) {
        MarimayBytecodeSuppressedPanic *nested = mmay_vector_at(
            &secondary->suppressed, index
        );
        if (nested != NULL
            && !mmay_bytecode_run_push_suppressed(primary, nested)) {
            return false;
        }
    }
    return true;
}

MarimayBytecodeRunStatus mmay_bytecode_run_begin_panic(
    MarimayBytecodePanic *panic,
    MarimayAllocator allocator,
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue message,
    MarimaySymbolId symbol,
    MarimayBytecodeSpan span
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    MarimayBytecodePanicFrame frame;
    if (!mmay_bytecode_execution_string_bytes(
            execution, message, &bytes, &length
        )) {
        return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    }
    if (panic == NULL) {
        return MMAY_BYTECODE_RUN_PANIC;
    }
    mmay_bytecode_run_reset_panic(panic);
    if (!mmay_owned_bytes_copy(&panic->message, allocator, bytes, length)) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    panic->code = "G4000";
    panic->symbol = symbol;
    panic->span = span;
    frame.symbol = symbol;
    frame.call_span = span;
    if (!mmay_vector_push(&panic->frames, &frame)) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return MMAY_BYTECODE_RUN_PANIC;
}

MarimayBytecodeRunStatus mmay_bytecode_run_begin_test_panic(
    MarimayBytecodePanic *panic,
    MarimayAllocator allocator,
    const unsigned char *message,
    size_t message_length,
    MarimaySymbolId symbol,
    MarimayBytecodeSpan span
) {
    MarimayBytecodePanicFrame frame;
    if (message == NULL && message_length != 0) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (panic == NULL) return MMAY_BYTECODE_RUN_PANIC;
    mmay_bytecode_run_reset_panic(panic);
    if (!mmay_owned_bytes_copy(
            &panic->message, allocator, message, message_length
        )) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    panic->code = "G4004";
    panic->symbol = symbol;
    panic->span = span;
    frame.symbol = symbol;
    frame.call_span = span;
    if (!mmay_vector_push(&panic->frames, &frame)) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return MMAY_BYTECODE_RUN_PANIC;
}

static size_t mmay_bytecode_append_decimal_u64(
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

static size_t mmay_bytecode_append_decimal_i64(
    unsigned char *buffer,
    size_t offset,
    int64_t value
) {
    uint64_t magnitude = (uint64_t)value;
    if (value < 0) {
        buffer[offset++] = '-';
        magnitude = UINT64_C(0) - magnitude;
    }
    return mmay_bytecode_append_decimal_u64(buffer, offset, magnitude);
}

MarimayBytecodeRunStatus mmay_bytecode_run_begin_bounds_panic(
    MarimayBytecodePanic *panic,
    MarimayAllocator allocator,
    int64_t index,
    size_t length,
    MarimaySymbolId symbol,
    MarimayBytecodeSpan span
) {
    static const unsigned char prefix[] = "array index ";
    static const unsigned char suffix[] = " is outside length ";
    unsigned char message[sizeof(prefix) + sizeof(suffix)
        + 6 * sizeof(uint64_t)];
    size_t message_length = 0;
    MarimayBytecodePanicFrame frame;
    if (panic == NULL) {
        return MMAY_BYTECODE_RUN_PANIC;
    }
    mmay_bytecode_run_reset_panic(panic);
    memcpy(message + message_length, prefix, sizeof(prefix) - 1);
    message_length += sizeof(prefix) - 1;
    message_length = mmay_bytecode_append_decimal_i64(
        message, message_length, index
    );
    memcpy(message + message_length, suffix, sizeof(suffix) - 1);
    message_length += sizeof(suffix) - 1;
    message_length = mmay_bytecode_append_decimal_u64(
        message, message_length, (uint64_t)length
    );
    if (!mmay_owned_bytes_copy(
            &panic->message, allocator, message, message_length
        )) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    panic->code = "G4002";
    panic->symbol = symbol;
    panic->span = span;
    frame.symbol = symbol;
    frame.call_span = span;
    if (!mmay_vector_push(&panic->frames, &frame)) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return MMAY_BYTECODE_RUN_PANIC;
}

MarimayBytecodeRunStatus mmay_bytecode_run_begin_iterator_invalidated_panic(
    MarimayBytecodePanic *panic,
    MarimayAllocator allocator,
    MarimaySymbolId symbol,
    MarimayBytecodeSpan span
) {
    static const unsigned char message[] =
        "array iterator invalidated by mutation";
    MarimayBytecodePanicFrame frame;
    if (panic == NULL) {
        return MMAY_BYTECODE_RUN_PANIC;
    }
    mmay_bytecode_run_reset_panic(panic);
    if (!mmay_owned_bytes_copy(
            &panic->message, allocator, message, sizeof(message) - 1
        )) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    panic->code = "G4005";
    panic->symbol = symbol;
    panic->span = span;
    frame.symbol = symbol;
    frame.call_span = span;
    if (!mmay_vector_push(&panic->frames, &frame)) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return MMAY_BYTECODE_RUN_PANIC;
}

MarimayBytecodeRunStatus mmay_bytecode_run_append_panic_frame(
    MarimayBytecodePanic *panic,
    MarimaySymbolId symbol,
    MarimayBytecodeSpan call_span
) {
    MarimayBytecodePanicFrame frame;
    if (panic == NULL) {
        return MMAY_BYTECODE_RUN_PANIC;
    }
    frame.symbol = symbol;
    frame.call_span = call_span;
    if (!mmay_vector_push(&panic->frames, &frame)) {
        mmay_bytecode_run_reset_panic(panic);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    return MMAY_BYTECODE_RUN_PANIC;
}

bool mmay_bytecode_run_string_value_valid(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    return mmay_bytecode_execution_string_bytes(
        execution, value, &bytes, &length
    );
}

bool mmay_bytecode_run_function_uses_strings(
    const MarimayBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL) {
        return false;
    }
    if (function->string_constants.length != 0) {
        return true;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && *type == MMAY_BYTECODE_TYPE_STRING) {
            return true;
        }
    }
    return function->result_type == MMAY_BYTECODE_TYPE_STRING;
}

bool mmay_bytecode_run_program_uses_strings(
    const MarimayBytecodeProgram *program
) {
    size_t index = 0;
    if (program == NULL) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function = mmay_vector_at_const(
            &program->functions, index
        );
        if (function == NULL || mmay_bytecode_run_function_uses_strings(
                &function->body
            )) {
            return true;
        }
    }
    return false;
}

bool mmay_bytecode_run_function_uses_arrays(
    const MarimayBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL || mmay_bytecode_type_is_array(function->result_type)
        || function->array_constructions.length != 0) {
        return function != NULL;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && mmay_bytecode_type_is_array(*type)) {
            return true;
        }
    }
    return false;
}

bool mmay_bytecode_run_program_uses_arrays(
    const MarimayBytecodeProgram *program
) {
    size_t index = 0;
    if (program == NULL) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function = mmay_vector_at_const(
            &program->functions, index
        );
        if (function == NULL
            || mmay_bytecode_run_function_uses_arrays(&function->body)) {
            return true;
        }
    }
    return false;
}

bool mmay_bytecode_run_function_uses_managed(
    const MarimayBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL
        || mmay_bytecode_type_is_managed(function->result_type)) {
        return function != NULL;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL && mmay_bytecode_type_is_managed(*type)) {
            return true;
        }
    }
    return false;
}

bool mmay_bytecode_run_program_uses_managed(
    const MarimayBytecodeProgram *program
) {
    size_t index = 0;
    if (program == NULL) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function = mmay_vector_at_const(
            &program->functions,
            index
        );
        if (function == NULL
            || mmay_bytecode_run_function_uses_managed(&function->body)) {
            return true;
        }
    }
    return false;
}

MarimayBytecodeRunStatus mmay_bytecode_run_arguments_validate(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeValue *arguments,
    size_t argument_count
) {
    size_t index = 0;
    if (function == NULL || (arguments == NULL && argument_count != 0)
        || argument_count != function->parameter_count) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayBytecodeType *expected = mmay_vector_at_const(
            &function->register_types, index
        );
        uint32_t layout = mmay_bytecode_register_layout(
            function, (uint32_t)index
        );
        if (expected == NULL || arguments[index].type != *expected) {
            return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
        }
        if (*expected == MMAY_BYTECODE_TYPE_STRING
            && !mmay_bytecode_run_string_value_valid(
                execution, arguments[index]
            )) {
            return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        }
        if (mmay_bytecode_type_is_array(*expected)
            && !mmay_bytecode_managed_value_valid_for(
                execution, function, arguments[index], layout
            )) {
            return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        }
        if (*expected == MMAY_BYTECODE_TYPE_RECORD
            || *expected == MMAY_BYTECODE_TYPE_ENUM) {
            MarimayBytecodeRunStatus status =
                mmay_bytecode_run_nominal_value_validate(
                    execution,
                    function,
                    arguments[index],
                    *expected,
                    layout
                );
            if (status != MMAY_BYTECODE_RUN_OK) {
                return status;
            }
        }
        if (*expected == MMAY_BYTECODE_TYPE_INTERFACE
            && !mmay_bytecode_run_interface_value_valid(
                execution, function, arguments[index], layout
            )) {
            return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        }
    }
    return MMAY_BYTECODE_RUN_OK;
}

MarimayBytecodeRunStatus mmay_bytecode_run_roots_init(
    MarimayBytecodeManagedRoots *roots,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayBytecodeValue *registers,
    MarimayAllocator allocator
) {
    size_t count = 0;
    size_t index = 0;
    MarimayManagedStatus status;
    if (roots == NULL || execution == NULL || function == NULL
        || registers == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    memset(roots, 0, sizeof(*roots));
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && mmay_bytecode_type_is_managed(*type)) {
            count += 1;
        }
    }
    if (!mmay_size_multiply(
            count, sizeof(MarimayManagedReference *), &roots->location_bytes
        )) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (roots->location_bytes != 0) {
        roots->locations = mmay_allocate(&allocator, roots->location_bytes);
        if (roots->locations == NULL) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
    }
    count = 0;
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && mmay_bytecode_type_is_managed(*type)) {
            roots->locations[count++] = &registers[index].as.managed_reference;
        }
    }
    status = mmay_managed_root_push(
        &execution->managed_heap, &roots->frame, roots->locations, count
    );
    if (status != MMAY_MANAGED_OK) {
        mmay_deallocate(
            &allocator, roots->locations, roots->location_bytes
        );
        memset(roots, 0, sizeof(*roots));
        return status == MMAY_MANAGED_INVALID_REFERENCE
            ? MMAY_BYTECODE_RUN_INVALID_REFERENCE
            : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return MMAY_BYTECODE_RUN_OK;
}

void mmay_bytecode_run_roots_dispose(
    MarimayBytecodeManagedRoots *roots,
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator
) {
    if (roots == NULL) {
        return;
    }
    if (execution != NULL && roots->frame.linked) {
        (void)mmay_managed_root_pop(&execution->managed_heap, &roots->frame);
    }
    mmay_deallocate(&allocator, roots->locations, roots->location_bytes);
    memset(roots, 0, sizeof(*roots));
}

bool mmay_bytecode_run_function_uses_f64(
    const MarimayBytecodeFunction *function
) {
    size_t index = 0;
    if (function == NULL) {
        return false;
    }
    if (function->result_type == MMAY_BYTECODE_TYPE_F64) {
        return true;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && *type == MMAY_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    return false;
}

bool mmay_bytecode_run_program_uses_f64(
    const MarimayBytecodeProgram *program
) {
    size_t index = 0;
    if (program == NULL) {
        return false;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function = mmay_vector_at_const(
            &program->functions, index
        );
        if (function == NULL
            || mmay_bytecode_run_function_uses_f64(&function->body)) {
            return true;
        }
    }
    return false;
}

bool mmay_bytecode_run_function_has_calls(
    const MarimayBytecodeFunction *function
) {
    size_t block_index = 0;
    if (function == NULL || function->calls.length != 0
        || function->call_arguments.length != 0) {
        return true;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
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
            const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
                &block->instructions,
                instruction_index
            );
            if (instruction == NULL
                || instruction->opcode == MMAY_OPCODE_CALL_DIRECT
                || instruction->opcode == MMAY_OPCODE_CALL_DIRECT_VOID
                || instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER
                || instruction->opcode == MMAY_OPCODE_CALL_INTERFACE) {
                return true;
            }
        }
    }
    return false;
}
