#include "vm_internal.h"
#include <limits.h>
#include <string.h>
#include "mmay/base/f64.h"
MarimayBytecodeRunStatus mmay_bytecode_run(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeValue *result
) {
    MarimayBytecodeValue *registers = NULL;
    MarimayBytecodeValue completed;
    MarimayBytecodeManagedRoots managed_roots;
    size_t register_count = 0;
    size_t register_bytes = 0;
    size_t index = 0;
    size_t steps = 0;
    size_t current_block = 0;
    MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_OK;
    memset(&managed_roots, 0, sizeof(managed_roots));
    if (options.trap != NULL) {
        memset(options.trap, 0, sizeof(*options.trap));
    }
    if (function == NULL || !function->initialized || result == NULL
        || allocator.allocate == NULL || allocator.deallocate == NULL
        || options.max_steps == 0 || options.max_call_depth == 0
        || argument_count != function->parameter_count
        || (arguments == NULL && argument_count != 0)) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if ((options.execution != NULL && !options.execution->initialized)
        || (options.panic != NULL && !options.panic->initialized)) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (options.execution != NULL) {
        options.execution->array_descriptor_allocation_failed = false;
    }
    mmay_bytecode_run_reset_panic(options.panic);
    {
        MarimayBytecodeVerifyStatus verify_status = mmay_bytecode_verify(function);
        if (verify_status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        if (verify_status == MMAY_BYTECODE_VERIFY_LIMIT_REACHED) {
            return MMAY_BYTECODE_RUN_LIMIT_REACHED;
        }
        if (verify_status != MMAY_BYTECODE_VERIFY_OK) {
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
    }
    if (mmay_bytecode_run_function_has_calls(function)) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (mmay_bytecode_run_function_uses_f64(function)
        && !mmay_f64_binary64_supported()) {
        return MMAY_BYTECODE_RUN_UNSUPPORTED_HOST;
    }
    if (mmay_bytecode_run_function_uses_strings(function)
        && options.execution == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (mmay_bytecode_run_function_uses_managed(function)
        && options.execution == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    status = mmay_bytecode_run_arguments_validate(
        options.execution, function, arguments, argument_count
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        return status;
    }
    register_count = function->register_types.length;
    if (!mmay_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(MarimayBytecodeValue),
            &register_bytes
        )) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    registers = mmay_allocate(&allocator, register_bytes);
    if (registers == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(registers, 0, register_bytes);
    for (index = 0; index < register_count; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types,
            index
        );
        registers[index].type = *type;
        if (mmay_bytecode_type_is_managed(*type)) {
            registers[index].as.managed_reference =
                mmay_managed_invalid_reference();
        }
    }
    for (index = 0; index < argument_count; index += 1) {
        registers[index] = arguments[index];
    }
    if (mmay_bytecode_run_function_uses_managed(function)) {
        status = mmay_bytecode_run_roots_init(
            &managed_roots, options.execution, function, registers, allocator
        );
        if (status != MMAY_BYTECODE_RUN_OK) {
            goto cleanup;
        }
    }
    memset(&completed, 0, sizeof(completed));
    for (;;) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks,
            current_block
        );
        bool transferred = false;
        for (index = 0; index < block->instructions.length; index += 1) {
            const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
                &block->instructions,
                index
            );
            if (!mmay_bytecode_run_consume_step(&steps, options)) {
                status = MMAY_BYTECODE_RUN_STEP_LIMIT;
                if (options.trap != NULL) {
                    options.trap->span = instruction->span;
                    options.trap->has_span = true;
                }
                goto cleanup;
            }
            if (instruction->opcode == MMAY_OPCODE_RETURN) {
                completed = registers[
                    (size_t)instruction->operands[0].unsigned_value
                ];
                if (completed.type == MMAY_BYTECODE_TYPE_STRING
                    && !mmay_bytecode_run_string_value_valid(
                        options.execution, completed
                    )) {
                    status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
                    goto cleanup;
                }
                if (mmay_bytecode_type_is_array(completed.type)
                    && !mmay_bytecode_managed_value_valid_for(
                        options.execution, function, completed,
                        function->result_layout
                    )) {
                    status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
                    goto cleanup;
                }
                if (completed.type == MMAY_BYTECODE_TYPE_INTERFACE
                    && !mmay_bytecode_run_interface_value_valid(
                        options.execution,
                        function,
                        completed,
                        function->result_layout
                    )) {
                    status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
                    goto cleanup;
                }
                *result = completed;
                goto cleanup;
            }
            if (instruction->opcode == MMAY_OPCODE_RETURN_VOID) {
                completed.type = MMAY_BYTECODE_TYPE_VOID;
                *result = completed;
                goto cleanup;
            }
            if (instruction->opcode == MMAY_OPCODE_JUMP) {
                current_block = (size_t)instruction->operands[0].unsigned_value;
                transferred = true;
                break;
            }
            if (instruction->opcode == MMAY_OPCODE_BRANCH_BOOL) {
                size_t condition = (size_t)instruction->operands[0].unsigned_value;
                current_block = (size_t)instruction->operands[
                    registers[condition].as.boolean ? 1 : 2
                ].unsigned_value;
                transferred = true;
                break;
            }
            if (instruction->opcode == MMAY_OPCODE_PANIC_STRING) {
                status = mmay_bytecode_run_begin_panic(
                    options.panic,
                    allocator,
                    options.execution,
                    registers[(size_t)instruction->operands[0].unsigned_value],
                    (MarimaySymbolId){{0}},
                    instruction->span
                );
                goto cleanup;
            }
            {
                int64_t bounds_index = 0;
                size_t bounds_length = 0;
                status = mmay_bytecode_run_value_instruction(
                instruction, registers,
                options.execution,
                function,
                allocator,
                options.stdout_sink, options.panic,
                (MarimaySymbolId){{0}},
                &bounds_index,
                &bounds_length
                );
                if (status == MMAY_BYTECODE_RUN_PANIC
                    && (options.panic == NULL || options.panic->code == NULL)) {
                    status = mmay_bytecode_run_begin_bounds_panic(
                        options.panic, allocator, bounds_index, bounds_length,
                        (MarimaySymbolId){{0}}, instruction->span
                    );
                }
            }
            if (status != MMAY_BYTECODE_RUN_OK) {
                if (options.trap != NULL
                    && (status == MMAY_BYTECODE_RUN_OVERFLOW
                        || status == MMAY_BYTECODE_RUN_DIVISION_BY_ZERO
                        || status == MMAY_BYTECODE_RUN_INVALID_SHIFT)) {
                    options.trap->span = instruction->span;
                    options.trap->has_span = true;
                }
                goto cleanup;
            }
        }
        if (!transferred) {
            status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            break;
        }
    }
cleanup:
    mmay_bytecode_run_roots_dispose(
        &managed_roots, options.execution, allocator
    );
    mmay_deallocate(&allocator, registers, register_bytes);
    return status;
}
typedef struct MarimayBytecodeRunFrame {
    const MarimayBytecodeProgramFunction *function;
    MarimayBytecodeValue *registers;
    size_t register_bytes;
    size_t block;
    size_t instruction;
    size_t steps;
    uint32_t return_destination;
    bool return_is_void;
    bool return_is_never;
    MarimayBytecodeSpan call_span;
    MarimayBytecodeManagedRoots *managed_roots;
    MarimayBytecodeCallId *defer_stack;
    size_t defer_stack_bytes;
    size_t defer_count;
} MarimayBytecodeRunFrame;
static void mmay_bytecode_run_frame_dispose(
    MarimayBytecodeRunFrame *frame,
    const MarimayAllocator *allocator,
    MarimayBytecodeExecution *execution
) {
    if (frame != NULL && allocator != NULL) {
        if (frame->managed_roots != NULL) {
            mmay_bytecode_run_roots_dispose(
                frame->managed_roots, execution, *allocator
            );
            mmay_deallocate(
                allocator, frame->managed_roots,
                sizeof(*frame->managed_roots)
            );
        }
        mmay_deallocate(
            allocator, frame->defer_stack, frame->defer_stack_bytes
        );
        mmay_deallocate(allocator, frame->registers, frame->register_bytes);
        memset(frame, 0, sizeof(*frame));
    }
}
static MarimayBytecodeRunStatus mmay_bytecode_run_frame_init(
    MarimayBytecodeRunFrame *frame,
    const MarimayBytecodeProgramFunction *function,
    const MarimayBytecodeValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    uint32_t return_destination,
    bool return_is_void,
    bool return_is_never,
    MarimayBytecodeSpan call_span
) {
    const MarimayBytecodeFunction *body = function != NULL ? &function->body : NULL;
    size_t index = 0;
    if (frame == NULL || body == NULL
        || (arguments == NULL && argument_count != 0)) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    memset(frame, 0, sizeof(*frame));
    if (!mmay_size_multiply(
            body->register_types.length == 0 ? 1 : body->register_types.length,
            sizeof(MarimayBytecodeValue),
            &frame->register_bytes
        )) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    frame->registers = mmay_allocate(&allocator, frame->register_bytes);
    if (frame->registers == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(frame->registers, 0, frame->register_bytes);
    if (body->calls.length != 0) {
        if (!mmay_size_multiply(
                body->calls.length, sizeof(*frame->defer_stack),
                &frame->defer_stack_bytes
            )) {
            mmay_bytecode_run_frame_dispose(frame, &allocator, NULL);
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        frame->defer_stack = mmay_allocate(
            &allocator, frame->defer_stack_bytes
        );
        if (frame->defer_stack == NULL) {
            mmay_bytecode_run_frame_dispose(frame, &allocator, NULL);
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
    }
    for (index = 0; index < body->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &body->register_types,
            index
        );
        if (type == NULL) {
            mmay_bytecode_run_frame_dispose(frame, &allocator, NULL);
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        frame->registers[index].type = *type;
        if (mmay_bytecode_type_is_managed(*type)) {
            frame->registers[index].as.managed_reference =
                mmay_managed_invalid_reference();
        }
    }
    for (index = 0; index < argument_count; index += 1) {
        frame->registers[index] = arguments[index];
    }
    frame->function = function;
    frame->return_destination = return_destination;
    frame->return_is_void = return_is_void;
    frame->return_is_never = return_is_never;
    frame->call_span = call_span;
    return MMAY_BYTECODE_RUN_OK;
}
static MarimayBytecodeRunStatus mmay_bytecode_run_frame_roots_init(
    MarimayBytecodeRunFrame *frame,
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator
) {
    MarimayBytecodeRunStatus status;
    if (frame == NULL || frame->function == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (!mmay_bytecode_run_function_uses_managed(&frame->function->body)) {
        return MMAY_BYTECODE_RUN_OK;
    }
    if (execution == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    frame->managed_roots = mmay_allocate(
        &allocator, sizeof(*frame->managed_roots)
    );
    if (frame->managed_roots == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(frame->managed_roots, 0, sizeof(*frame->managed_roots));
    status = mmay_bytecode_run_roots_init(
        frame->managed_roots, execution, &frame->function->body,
        frame->registers, allocator
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        mmay_deallocate(
            &allocator, frame->managed_roots, sizeof(*frame->managed_roots)
        );
        frame->managed_roots = NULL;
    }
    return status;
}
static MarimayBytecodeRunStatus mmay_bytecode_run_program_verify_status(
    MarimayBytecodeProgramVerifyStatus status
) {
    if (status == MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
        return MMAY_BYTECODE_RUN_OK;
    }
    if (status == MMAY_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (status == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
}
static bool mmay_bytecode_run_frames_reserve(
    MarimayBytecodeRunFrame **frames,
    size_t *capacity,
    size_t required,
    size_t maximum,
    MarimayAllocator allocator
) {
    MarimayBytecodeRunFrame *next = NULL;
    size_t next_capacity = *capacity == 0 ? 1 : *capacity;
    size_t bytes = 0;
    if (frames == NULL || capacity == NULL || required > maximum) {
        return false;
    }
    if (*capacity >= required) {
        return true;
    }
    while (next_capacity < required) {
        if (next_capacity > maximum / 2) {
            next_capacity = maximum;
        } else {
            next_capacity *= 2;
        }
    }
    if (!mmay_size_multiply(
            next_capacity,
            sizeof(MarimayBytecodeRunFrame),
            &bytes
        )) {
        return false;
    }
    next = mmay_allocate(&allocator, bytes);
    if (next == NULL) {
        return false;
    }
    memset(next, 0, bytes);
    if (*frames != NULL) {
        size_t old_bytes = 0;
        if (!mmay_size_multiply(
                *capacity,
                sizeof(MarimayBytecodeRunFrame),
                &old_bytes
            )) {
            mmay_deallocate(&allocator, next, bytes);
            return false;
        }
        memcpy(next, *frames, old_bytes);
        mmay_deallocate(&allocator, *frames, old_bytes);
    }
    *frames = next;
    *capacity = next_capacity;
    return true;
}
MarimayBytecodeRunStatus mmay_bytecode_run_program(
    const MarimayBytecodeProgram *program,
    const MarimayBytecodeValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeValue *result
) {
    MarimayBytecodeRunFrame *frames = NULL;
    size_t frame_count = 0;
    size_t frame_capacity = 0;
    MarimayBytecodeRunStatus status;
    const MarimayBytecodeProgramFunction *entry = NULL;
    if (options.trap != NULL) {
        memset(options.trap, 0, sizeof(*options.trap));
    }
    if (program == NULL || !program->initialized || result == NULL
        || allocator.allocate == NULL || allocator.deallocate == NULL
        || options.max_steps == 0 || options.max_call_depth == 0
        || (arguments == NULL && argument_count != 0)) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if ((options.execution != NULL && !options.execution->initialized)
        || (options.panic != NULL && !options.panic->initialized)) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (options.execution != NULL) {
        options.execution->array_descriptor_allocation_failed = false;
    }
    mmay_bytecode_run_reset_panic(options.panic);
    status = mmay_bytecode_run_program_verify_status(
        mmay_bytecode_program_verify(program)
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        return status;
    }
    if (mmay_bytecode_run_program_uses_f64(program)
        && !mmay_f64_binary64_supported()) {
        return MMAY_BYTECODE_RUN_UNSUPPORTED_HOST;
    }
    if (mmay_bytecode_run_program_uses_strings(program)
        && options.execution == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (mmay_bytecode_run_program_uses_managed(program)
        && options.execution == NULL) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    entry = mmay_bytecode_program_function(program, program->entry_function);
    if (entry == NULL || argument_count != entry->body.parameter_count) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    status = mmay_bytecode_run_arguments_validate(
        options.execution, &entry->body, arguments, argument_count
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        return status;
    }
    if (!mmay_bytecode_run_frames_reserve(
            &frames,
            &frame_capacity,
            1,
            options.max_call_depth,
            allocator
        )) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    status = mmay_bytecode_run_frame_init(
        &frames[0], entry, arguments, argument_count, allocator, 0, false,
        false, (MarimayBytecodeSpan){0}
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        mmay_bytecode_run_frame_dispose(
            &frames[0], &allocator, options.execution
        );
        goto cleanup;
    }
    status = mmay_bytecode_run_frame_roots_init(
        &frames[0], options.execution, allocator
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        mmay_bytecode_run_frame_dispose(
            &frames[0], &allocator, options.execution
        );
        goto cleanup;
    }
    frame_count = 1;
    for (;;) {
        MarimayBytecodeRunFrame *frame = &frames[frame_count - 1];
        const MarimayBytecodeFunction *body = &frame->function->body;
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &body->blocks,
            frame->block
        );
        const MarimayBytecodeInstruction *instruction = block != NULL
            ? mmay_vector_at_const(&block->instructions, frame->instruction)
            : NULL;
        if (instruction == NULL) {
            status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            goto cleanup;
        }
        if (!mmay_bytecode_run_consume_step(&frame->steps, options)) {
            status = MMAY_BYTECODE_RUN_STEP_LIMIT;
            mmay_bytecode_run_set_trap(options, instruction->span);
            goto cleanup;
        }
        if (instruction->opcode == MMAY_OPCODE_RETURN
            || instruction->opcode == MMAY_OPCODE_RETURN_VOID) {
            MarimayBytecodeValue completed;
            bool returns_void = instruction->opcode == MMAY_OPCODE_RETURN_VOID;
            memset(&completed, 0, sizeof(completed));
            if (returns_void) {
                completed.type = MMAY_BYTECODE_TYPE_VOID;
            } else {
                completed = frame->registers[
                    (size_t)instruction->operands[0].unsigned_value
                ];
            }
            if (frame->return_is_never) {
                status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                goto cleanup;
            }
            if (completed.type == MMAY_BYTECODE_TYPE_STRING
                && !mmay_bytecode_run_string_value_valid(
                    options.execution, completed
                )) {
                status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
                goto cleanup;
            }
            if (mmay_bytecode_type_is_array(completed.type)
                && !mmay_bytecode_managed_value_valid_for(
                    options.execution, body, completed, body->result_layout
                )) {
                status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
                goto cleanup;
            }
            if (completed.type == MMAY_BYTECODE_TYPE_INTERFACE
                && !mmay_bytecode_run_interface_value_valid(
                    options.execution,
                    body,
                    completed,
                    body->result_layout
                )) {
                status = MMAY_BYTECODE_RUN_INVALID_REFERENCE;
                goto cleanup;
            }
            if (frame_count == 1) {
                *result = completed;
                status = MMAY_BYTECODE_RUN_OK;
                goto cleanup;
            }
            {
                MarimayBytecodeRunFrame *parent = &frames[frame_count - 2];
                uint32_t destination = frame->return_destination;
                bool return_is_void = frame->return_is_void;
                if (!return_is_void) {
                    parent->registers[destination] = completed;
                }
                mmay_bytecode_run_frame_dispose(
                    frame, &allocator, options.execution
                );
                frame_count -= 1;
            }
            continue;
        }
        if (instruction->opcode == MMAY_OPCODE_JUMP) {
            frame->block = (size_t)instruction->operands[0].unsigned_value;
            frame->instruction = 0;
            continue;
        }
        if (instruction->opcode == MMAY_OPCODE_BRANCH_BOOL) {
            size_t condition = (size_t)instruction->operands[0].unsigned_value;
            frame->block = (size_t)instruction->operands[
                frame->registers[condition].as.boolean ? 1 : 2
            ].unsigned_value;
            frame->instruction = 0;
            continue;
        }
        if (instruction->opcode == MMAY_OPCODE_PANIC_STRING) {
            size_t panic_index = frame_count - 1;
            status = mmay_bytecode_run_begin_panic(
                options.panic,
                allocator,
                options.execution,
                frame->registers[(size_t)instruction->operands[0].unsigned_value],
                frame->function->symbol,
                instruction->span
            );
            while (status == MMAY_BYTECODE_RUN_PANIC && panic_index != 0) {
                MarimayBytecodeRunFrame *callee = &frames[panic_index];
                MarimayBytecodeRunFrame *caller = &frames[panic_index - 1];
                status = mmay_bytecode_run_append_panic_frame(
                    options.panic, caller->function->symbol, callee->call_span
                );
                panic_index -= 1;
            }
            goto cleanup;
        }
        if (instruction->opcode == MMAY_OPCODE_DEFER_REGISTER) {
            MarimayBytecodeCallId call_id = (MarimayBytecodeCallId)
                instruction->operands[1].unsigned_value;
            if (frame->defer_count >= body->calls.length) {
                status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                goto cleanup;
            }
            frame->defer_stack[frame->defer_count] = call_id;
            frame->defer_count += 1;
            frame->registers[
                (size_t)instruction->operands[0].unsigned_value
            ].as.boolean = true;
            frame->instruction += 1;
            continue;
        }
        if (instruction->opcode == MMAY_OPCODE_DEFER_CALL) {
            MarimayBytecodeCallId call_id = (MarimayBytecodeCallId)
                instruction->operands[1].unsigned_value;
            const MarimayBytecodeCall *call = call_id < body->calls.length
                ? mmay_bytecode_call(body, call_id)
                : NULL;
            if (call == NULL || frame->defer_count == 0
                || frame->defer_stack[frame->defer_count - 1] != call_id) {
                status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                goto cleanup;
            }
            frame->defer_count -= 1;
            status = mmay_bytecode_run_deferred_program_call(
                program, body, call, instruction, frame->registers,
                allocator, options, false
            );
            if (status == MMAY_BYTECODE_RUN_PANIC) {
                status = mmay_bytecode_run_append_panic_frame(
                    options.panic, frame->function->symbol, instruction->span
                );
            }
            if (status != MMAY_BYTECODE_RUN_OK) {
                goto cleanup;
            }
            frame->instruction += 1;
            continue;
        }
        if (instruction->opcode == MMAY_OPCODE_CALL_DIRECT
            || instruction->opcode == MMAY_OPCODE_CALL_DIRECT_VOID
            || instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER
            || instruction->opcode == MMAY_OPCODE_CALL_INTERFACE) {
            bool returns_never = instruction->opcode == MMAY_OPCODE_CALL_DIRECT_NEVER;
            bool interface_call = instruction->opcode
                == MMAY_OPCODE_CALL_INTERFACE;
            uint64_t call_wire = instruction->operands[
                instruction->opcode == MMAY_OPCODE_CALL_DIRECT_VOID || returns_never ? 0 : 1
            ].unsigned_value;
            const MarimayBytecodeCall *call = call_wire < body->calls.length
                ? mmay_bytecode_call(body, (MarimayBytecodeCallId)call_wire)
                : NULL;
            bool returns_void = call != NULL && call->result_type == MMAY_BYTECODE_TYPE_VOID;
            MarimaySymbolId callee_symbol;
            MarimayManagedReference interface_payload =
                mmay_managed_invalid_reference();
            const MarimayBytecodeProgramFunction *callee = NULL;
            size_t argument_index = 0;
            memset(&callee_symbol, 0, sizeof(callee_symbol));
            if (interface_call && call != NULL) {
                status = mmay_bytecode_run_interface_resolve(
                    instruction,
                    frame->registers,
                    options.execution,
                    body,
                    &callee_symbol,
                    &interface_payload
                );
                if (status != MMAY_BYTECODE_RUN_OK) {
                    goto cleanup;
                }
            } else if (call != NULL) {
                callee_symbol = call->callee;
            }
            callee = call != NULL
                ? mmay_bytecode_program_function_by_symbol(
                    program,
                    callee_symbol
                )
                : NULL;
            if (call == NULL || callee == NULL || frame_count >= options.max_call_depth
            ) {
                status = frame_count >= options.max_call_depth
                    ? MMAY_BYTECODE_RUN_STEP_LIMIT
                    : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                mmay_bytecode_run_set_trap(options, instruction->span);
                goto cleanup;
            }
            if (!mmay_bytecode_run_frames_reserve(
                    &frames,
                    &frame_capacity,
                    frame_count + 1,
                    options.max_call_depth,
                    allocator
                )) {
                status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
                goto cleanup;
            }
            /* Reserve may relocate frames, so only now advance the caller. */
            frames[frame_count - 1].instruction += 1;
            status = mmay_bytecode_run_frame_init(
                &frames[frame_count],
                callee,
                NULL,
                0,
                allocator,
                returns_void || returns_never
                    ? 0
                    : (uint32_t)instruction->operands[0].unsigned_value,
                returns_void,
                returns_never,
                instruction->span
            );
            if (status != MMAY_BYTECODE_RUN_OK) {
                mmay_bytecode_run_frame_dispose(
                    &frames[frame_count], &allocator, options.execution
                );
                goto cleanup;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const MarimayBytecodeCallArgument *argument =
                    mmay_bytecode_call_argument(
                        body,
                        call->argument_start + argument_index
                    );
                if (argument == NULL) {
                    mmay_bytecode_run_frame_dispose(
                        &frames[frame_count],
                        &allocator,
                        options.execution
                    );
                    status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                    goto cleanup;
                }
                if (interface_call && argument_index == 0) {
                    const MarimayBytecodeType *parameter = mmay_vector_at_const(
                        &callee->body.register_types,
                        0
                    );
                    if (parameter == NULL
                        || !mmay_bytecode_type_is_managed(*parameter)) {
                        mmay_bytecode_run_frame_dispose(
                            &frames[frame_count],
                            &allocator,
                            options.execution
                        );
                        status = MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                        goto cleanup;
                    }
                    frames[frame_count].registers[0].type = *parameter;
                    frames[frame_count].registers[0].as.managed_reference =
                        interface_payload;
                } else {
                    frames[frame_count].registers[argument_index] =
                        frames[frame_count - 1]
                            .registers[argument->register_id];
                }
            }
            status = mmay_bytecode_run_frame_roots_init(
                &frames[frame_count], options.execution, allocator
            );
            if (status != MMAY_BYTECODE_RUN_OK) {
                mmay_bytecode_run_frame_dispose(
                    &frames[frame_count], &allocator, options.execution
                );
                goto cleanup;
            }
            frame_count += 1;
            continue;
        }
        {
            int64_t bounds_index = 0;
            size_t bounds_length = 0;
            status = mmay_bytecode_run_value_instruction(
                instruction, frame->registers, options.execution, body,
                allocator, options.stdout_sink, options.panic,
                frame->function->symbol,
                &bounds_index, &bounds_length
            );
            if (status == MMAY_BYTECODE_RUN_PANIC) {
                size_t panic_index = frame_count - 1;
                if (options.panic == NULL || options.panic->code == NULL) {
                    status = mmay_bytecode_run_begin_bounds_panic(
                        options.panic, allocator, bounds_index, bounds_length,
                        frame->function->symbol, instruction->span
                    );
                }
                while (status == MMAY_BYTECODE_RUN_PANIC
                    && panic_index != 0) {
                    MarimayBytecodeRunFrame *callee = &frames[panic_index];
                    MarimayBytecodeRunFrame *caller = &frames[panic_index - 1];
                    status = mmay_bytecode_run_append_panic_frame(
                        options.panic, caller->function->symbol,
                        callee->call_span
                    );
                    panic_index -= 1;
                }
            }
        }
        if (status != MMAY_BYTECODE_RUN_OK) {
            if (status == MMAY_BYTECODE_RUN_OVERFLOW
                || status == MMAY_BYTECODE_RUN_DIVISION_BY_ZERO
                || status == MMAY_BYTECODE_RUN_INVALID_SHIFT) {
                mmay_bytecode_run_set_trap(options, instruction->span);
            }
            goto cleanup;
        }
        frame->instruction += 1;
    }
cleanup:
    while (frame_count != 0) {
        MarimayBytecodeRunFrame *frame = &frames[frame_count - 1];
        const MarimayBytecodeFunction *body = &frame->function->body;
        while (frame->defer_count != 0) {
            MarimayBytecodeCallId call_id;
            const MarimayBytecodeCall *call;
            MarimayBytecodeInstruction instruction;
            MarimayBytecodeRunStatus cleanup_status;
            frame->defer_count -= 1;
            call_id = frame->defer_stack[frame->defer_count];
            call = call_id < body->calls.length
                ? mmay_bytecode_call(body, call_id)
                : NULL;
            memset(&instruction, 0, sizeof(instruction));
            instruction.opcode = MMAY_OPCODE_DEFER_CALL;
            instruction.operands[1].unsigned_value = call_id;
            cleanup_status = mmay_bytecode_run_deferred_program_call(
                program, body, call, &instruction, frame->registers,
                allocator, options, status == MMAY_BYTECODE_RUN_PANIC
            );
            if (cleanup_status == MMAY_BYTECODE_RUN_PANIC
                && status != MMAY_BYTECODE_RUN_PANIC) {
                status = MMAY_BYTECODE_RUN_PANIC;
            } else if (status == MMAY_BYTECODE_RUN_OK
                && cleanup_status != MMAY_BYTECODE_RUN_OK) {
                status = cleanup_status;
            }
        }
        frame_count -= 1;
        mmay_bytecode_run_frame_dispose(
            &frames[frame_count], &allocator, options.execution
        );
    }
    if (frames != NULL) {
        size_t bytes = 0;
        if (mmay_size_multiply(
                frame_capacity,
                sizeof(MarimayBytecodeRunFrame),
                &bytes
            )) {
            mmay_deallocate(&allocator, frames, bytes);
        }
    }
    return status;
}
