#include "vm_internal.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/f64.h"

RudaBytecodeRunStatus ruda_bytecode_run(
    const RudaBytecodeFunction *function,
    const RudaBytecodeValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
) {
    RudaBytecodeValue *registers = NULL;
    RudaBytecodeValue completed;
    RudaBytecodeManagedRoots managed_roots;
    size_t register_count = 0;
    size_t register_bytes = 0;
    size_t index = 0;
    size_t steps = 0;
    size_t current_block = 0;
    RudaBytecodeRunStatus status = RUDA_BYTECODE_RUN_OK;
    memset(&managed_roots, 0, sizeof(managed_roots));
    if (options.trap != NULL) {
        memset(options.trap, 0, sizeof(*options.trap));
    }
    if (function == NULL || !function->initialized || result == NULL
        || allocator.allocate == NULL || allocator.deallocate == NULL
        || options.max_steps == 0 || options.max_call_depth == 0
        || argument_count != function->parameter_count
        || (arguments == NULL && argument_count != 0)) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if ((options.execution != NULL && !options.execution->initialized)
        || (options.panic != NULL && !options.panic->initialized)) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    ruda_bytecode_run_reset_panic(options.panic);
    {
        RudaBytecodeVerifyStatus verify_status = ruda_bytecode_verify(function);
        if (verify_status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED) {
            return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        if (verify_status == RUDA_BYTECODE_VERIFY_LIMIT_REACHED) {
            return RUDA_BYTECODE_RUN_LIMIT_REACHED;
        }
        if (verify_status != RUDA_BYTECODE_VERIFY_OK) {
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
        }
    }
    if (ruda_bytecode_run_function_has_calls(function)) {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (ruda_bytecode_run_function_uses_f64(function)
        && !ruda_f64_binary64_supported()) {
        return RUDA_BYTECODE_RUN_UNSUPPORTED_HOST;
    }
    if (ruda_bytecode_run_function_uses_strings(function)
        && options.execution == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (ruda_bytecode_run_function_uses_managed(function)
        && options.execution == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    status = ruda_bytecode_run_arguments_validate(
        options.execution, function, arguments, argument_count
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    register_count = function->register_types.length;
    if (!ruda_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(RudaBytecodeValue),
            &register_bytes
        )) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    registers = ruda_allocate(&allocator, register_bytes);
    if (registers == NULL) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(registers, 0, register_bytes);
    for (index = 0; index < register_count; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types,
            index
        );
        registers[index].type = *type;
        if (ruda_bytecode_type_is_managed(*type)) {
            registers[index].as.managed_reference =
                ruda_managed_invalid_reference();
        }
    }
    for (index = 0; index < argument_count; index += 1) {
        registers[index] = arguments[index];
    }
    if (ruda_bytecode_run_function_uses_managed(function)) {
        status = ruda_bytecode_run_roots_init(
            &managed_roots, options.execution, function, registers, allocator
        );
        if (status != RUDA_BYTECODE_RUN_OK) {
            goto cleanup;
        }
    }
    memset(&completed, 0, sizeof(completed));
    for (;;) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks,
            current_block
        );
        bool transferred = false;
        for (index = 0; index < block->instructions.length; index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                index
            );
            if (!ruda_bytecode_run_consume_step(&steps, options)) {
                status = RUDA_BYTECODE_RUN_STEP_LIMIT;
                if (options.trap != NULL) {
                    options.trap->span = instruction->span;
                    options.trap->has_span = true;
                }
                goto cleanup;
            }
            if (instruction->opcode == RUDA_OPCODE_RETURN) {
                completed = registers[
                    (size_t)instruction->operands[0].unsigned_value
                ];
                if (completed.type == RUDA_BYTECODE_TYPE_STRING
                    && !ruda_bytecode_run_string_value_valid(
                        options.execution, completed
                    )) {
                    status = RUDA_BYTECODE_RUN_INVALID_REFERENCE;
                    goto cleanup;
                }
                if (ruda_bytecode_type_is_array(completed.type)
                    && !ruda_bytecode_managed_value_valid(
                        options.execution, completed
                    )) {
                    status = RUDA_BYTECODE_RUN_INVALID_REFERENCE;
                    goto cleanup;
                }
                if (completed.type == RUDA_BYTECODE_TYPE_INTERFACE
                    && !ruda_bytecode_run_interface_value_valid(
                        options.execution,
                        function,
                        completed,
                        function->result_layout
                    )) {
                    status = RUDA_BYTECODE_RUN_INVALID_REFERENCE;
                    goto cleanup;
                }
                *result = completed;
                goto cleanup;
            }
            if (instruction->opcode == RUDA_OPCODE_RETURN_VOID) {
                completed.type = RUDA_BYTECODE_TYPE_VOID;
                *result = completed;
                goto cleanup;
            }
            if (instruction->opcode == RUDA_OPCODE_JUMP) {
                current_block = (size_t)instruction->operands[0].unsigned_value;
                transferred = true;
                break;
            }
            if (instruction->opcode == RUDA_OPCODE_BRANCH_BOOL) {
                size_t condition = (size_t)instruction->operands[0].unsigned_value;
                current_block = (size_t)instruction->operands[
                    registers[condition].as.boolean ? 1 : 2
                ].unsigned_value;
                transferred = true;
                break;
            }
            if (instruction->opcode == RUDA_OPCODE_PANIC_STRING) {
                status = ruda_bytecode_run_begin_panic(
                    options.panic,
                    allocator,
                    options.execution,
                    registers[(size_t)instruction->operands[0].unsigned_value],
                    (RudaSymbolId){{0}},
                    instruction->span
                );
                goto cleanup;
            }
            {
                int64_t bounds_index = 0;
                size_t bounds_length = 0;
                status = ruda_bytecode_run_value_instruction(
                instruction,
                registers,
                options.execution,
                function,
                allocator,
                &bounds_index,
                &bounds_length
                );
                if (status == RUDA_BYTECODE_RUN_PANIC) {
                    status = ruda_bytecode_run_begin_bounds_panic(
                        options.panic, allocator, bounds_index, bounds_length,
                        (RudaSymbolId){{0}}, instruction->span
                    );
                }
            }
            if (status != RUDA_BYTECODE_RUN_OK) {
                if (options.trap != NULL
                    && (status == RUDA_BYTECODE_RUN_OVERFLOW
                        || status == RUDA_BYTECODE_RUN_DIVISION_BY_ZERO
                        || status == RUDA_BYTECODE_RUN_INVALID_SHIFT)) {
                    options.trap->span = instruction->span;
                    options.trap->has_span = true;
                }
                goto cleanup;
            }
        }
        if (!transferred) {
            status = RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            break;
        }
    }

cleanup:
    ruda_bytecode_run_roots_dispose(
        &managed_roots, options.execution, allocator
    );
    ruda_deallocate(&allocator, registers, register_bytes);
    return status;
}

typedef struct RudaBytecodeRunFrame {
    const RudaBytecodeProgramFunction *function;
    RudaBytecodeValue *registers;
    size_t register_bytes;
    size_t block;
    size_t instruction;
    size_t steps;
    uint32_t return_destination;
    bool return_is_void;
    bool return_is_never;
    RudaBytecodeSpan call_span;
    RudaBytecodeManagedRoots *managed_roots;
} RudaBytecodeRunFrame;

static void ruda_bytecode_run_frame_dispose(
    RudaBytecodeRunFrame *frame,
    const RudaAllocator *allocator,
    RudaBytecodeExecution *execution
) {
    if (frame != NULL && allocator != NULL) {
        if (frame->managed_roots != NULL) {
            ruda_bytecode_run_roots_dispose(
                frame->managed_roots, execution, *allocator
            );
            ruda_deallocate(
                allocator, frame->managed_roots,
                sizeof(*frame->managed_roots)
            );
        }
        ruda_deallocate(allocator, frame->registers, frame->register_bytes);
        memset(frame, 0, sizeof(*frame));
    }
}

static RudaBytecodeRunStatus ruda_bytecode_run_frame_init(
    RudaBytecodeRunFrame *frame,
    const RudaBytecodeProgramFunction *function,
    const RudaBytecodeValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    uint32_t return_destination,
    bool return_is_void,
    bool return_is_never,
    RudaBytecodeSpan call_span
) {
    const RudaBytecodeFunction *body = function != NULL ? &function->body : NULL;
    size_t index = 0;
    if (frame == NULL || body == NULL
        || (arguments == NULL && argument_count != 0)) {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    memset(frame, 0, sizeof(*frame));
    if (!ruda_size_multiply(
            body->register_types.length == 0 ? 1 : body->register_types.length,
            sizeof(RudaBytecodeValue),
            &frame->register_bytes
        )) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    frame->registers = ruda_allocate(&allocator, frame->register_bytes);
    if (frame->registers == NULL) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(frame->registers, 0, frame->register_bytes);
    for (index = 0; index < body->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &body->register_types,
            index
        );
        if (type == NULL) {
            ruda_bytecode_run_frame_dispose(frame, &allocator, NULL);
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
        }
        frame->registers[index].type = *type;
        if (ruda_bytecode_type_is_managed(*type)) {
            frame->registers[index].as.managed_reference =
                ruda_managed_invalid_reference();
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
    return RUDA_BYTECODE_RUN_OK;
}

static RudaBytecodeRunStatus ruda_bytecode_run_frame_roots_init(
    RudaBytecodeRunFrame *frame,
    RudaBytecodeExecution *execution,
    RudaAllocator allocator
) {
    RudaBytecodeRunStatus status;
    if (frame == NULL || frame->function == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (!ruda_bytecode_run_function_uses_managed(&frame->function->body)) {
        return RUDA_BYTECODE_RUN_OK;
    }
    if (execution == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    frame->managed_roots = ruda_allocate(
        &allocator, sizeof(*frame->managed_roots)
    );
    if (frame->managed_roots == NULL) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(frame->managed_roots, 0, sizeof(*frame->managed_roots));
    status = ruda_bytecode_run_roots_init(
        frame->managed_roots, execution, &frame->function->body,
        frame->registers, allocator
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        ruda_deallocate(
            &allocator, frame->managed_roots, sizeof(*frame->managed_roots)
        );
        frame->managed_roots = NULL;
    }
    return status;
}

static RudaBytecodeRunStatus ruda_bytecode_run_program_verify_status(
    RudaBytecodeProgramVerifyStatus status
) {
    if (status == RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
        return RUDA_BYTECODE_RUN_OK;
    }
    if (status == RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    if (status == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED) {
        return RUDA_BYTECODE_RUN_LIMIT_REACHED;
    }
    return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
}

static bool ruda_bytecode_run_frames_reserve(
    RudaBytecodeRunFrame **frames,
    size_t *capacity,
    size_t required,
    size_t maximum,
    RudaAllocator allocator
) {
    RudaBytecodeRunFrame *next = NULL;
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
    if (!ruda_size_multiply(
            next_capacity,
            sizeof(RudaBytecodeRunFrame),
            &bytes
        )) {
        return false;
    }
    next = ruda_allocate(&allocator, bytes);
    if (next == NULL) {
        return false;
    }
    memset(next, 0, bytes);
    if (*frames != NULL) {
        size_t old_bytes = 0;
        if (!ruda_size_multiply(
                *capacity,
                sizeof(RudaBytecodeRunFrame),
                &old_bytes
            )) {
            ruda_deallocate(&allocator, next, bytes);
            return false;
        }
        memcpy(next, *frames, old_bytes);
        ruda_deallocate(&allocator, *frames, old_bytes);
    }
    *frames = next;
    *capacity = next_capacity;
    return true;
}

RudaBytecodeRunStatus ruda_bytecode_run_program(
    const RudaBytecodeProgram *program,
    const RudaBytecodeValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaBytecodeRunOptions options,
    RudaBytecodeValue *result
) {
    RudaBytecodeRunFrame *frames = NULL;
    size_t frame_count = 0;
    size_t frame_capacity = 0;
    RudaBytecodeRunStatus status;
    const RudaBytecodeProgramFunction *entry = NULL;
    if (options.trap != NULL) {
        memset(options.trap, 0, sizeof(*options.trap));
    }
    if (program == NULL || !program->initialized || result == NULL
        || allocator.allocate == NULL || allocator.deallocate == NULL
        || options.max_steps == 0 || options.max_call_depth == 0
        || (arguments == NULL && argument_count != 0)) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if ((options.execution != NULL && !options.execution->initialized)
        || (options.panic != NULL && !options.panic->initialized)) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    ruda_bytecode_run_reset_panic(options.panic);
    status = ruda_bytecode_run_program_verify_status(
        ruda_bytecode_program_verify(program)
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    if (ruda_bytecode_run_program_uses_f64(program)
        && !ruda_f64_binary64_supported()) {
        return RUDA_BYTECODE_RUN_UNSUPPORTED_HOST;
    }
    if (ruda_bytecode_run_program_uses_strings(program)
        && options.execution == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (ruda_bytecode_run_program_uses_managed(program)
        && options.execution == NULL) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    entry = ruda_bytecode_program_function(program, program->entry_function);
    if (entry == NULL || argument_count != entry->body.parameter_count) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    status = ruda_bytecode_run_arguments_validate(
        options.execution, &entry->body, arguments, argument_count
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    if (!ruda_bytecode_run_frames_reserve(
            &frames,
            &frame_capacity,
            1,
            options.max_call_depth,
            allocator
        )) {
        return RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    status = ruda_bytecode_run_frame_init(
        &frames[0], entry, arguments, argument_count, allocator, 0, false,
        false, (RudaBytecodeSpan){0}
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        ruda_bytecode_run_frame_dispose(
            &frames[0], &allocator, options.execution
        );
        goto cleanup;
    }
    status = ruda_bytecode_run_frame_roots_init(
        &frames[0], options.execution, allocator
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        ruda_bytecode_run_frame_dispose(
            &frames[0], &allocator, options.execution
        );
        goto cleanup;
    }
    frame_count = 1;
    for (;;) {
        RudaBytecodeRunFrame *frame = &frames[frame_count - 1];
        const RudaBytecodeFunction *body = &frame->function->body;
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &body->blocks,
            frame->block
        );
        const RudaBytecodeInstruction *instruction = block != NULL
            ? ruda_vector_at_const(&block->instructions, frame->instruction)
            : NULL;
        if (instruction == NULL) {
            status = RUDA_BYTECODE_RUN_INVALID_BYTECODE;
            goto cleanup;
        }
        if (!ruda_bytecode_run_consume_step(&frame->steps, options)) {
            status = RUDA_BYTECODE_RUN_STEP_LIMIT;
            ruda_bytecode_run_set_trap(options, instruction->span);
            goto cleanup;
        }
        if (instruction->opcode == RUDA_OPCODE_RETURN
            || instruction->opcode == RUDA_OPCODE_RETURN_VOID) {
            RudaBytecodeValue completed;
            bool returns_void = instruction->opcode == RUDA_OPCODE_RETURN_VOID;
            memset(&completed, 0, sizeof(completed));
            if (returns_void) {
                completed.type = RUDA_BYTECODE_TYPE_VOID;
            } else {
                completed = frame->registers[
                    (size_t)instruction->operands[0].unsigned_value
                ];
            }
            if (frame->return_is_never) {
                status = RUDA_BYTECODE_RUN_INVALID_BYTECODE;
                goto cleanup;
            }
            if (completed.type == RUDA_BYTECODE_TYPE_STRING
                && !ruda_bytecode_run_string_value_valid(
                    options.execution, completed
                )) {
                status = RUDA_BYTECODE_RUN_INVALID_REFERENCE;
                goto cleanup;
            }
            if (ruda_bytecode_type_is_array(completed.type)
                && !ruda_bytecode_managed_value_valid(
                    options.execution, completed
                )) {
                status = RUDA_BYTECODE_RUN_INVALID_REFERENCE;
                goto cleanup;
            }
            if (completed.type == RUDA_BYTECODE_TYPE_INTERFACE
                && !ruda_bytecode_run_interface_value_valid(
                    options.execution,
                    body,
                    completed,
                    body->result_layout
                )) {
                status = RUDA_BYTECODE_RUN_INVALID_REFERENCE;
                goto cleanup;
            }
            if (frame_count == 1) {
                *result = completed;
                status = RUDA_BYTECODE_RUN_OK;
                goto cleanup;
            }
            {
                RudaBytecodeRunFrame *parent = &frames[frame_count - 2];
                uint32_t destination = frame->return_destination;
                bool return_is_void = frame->return_is_void;
                if (!return_is_void) {
                    parent->registers[destination] = completed;
                }
                ruda_bytecode_run_frame_dispose(
                    frame, &allocator, options.execution
                );
                frame_count -= 1;
            }
            continue;
        }
        if (instruction->opcode == RUDA_OPCODE_JUMP) {
            frame->block = (size_t)instruction->operands[0].unsigned_value;
            frame->instruction = 0;
            continue;
        }
        if (instruction->opcode == RUDA_OPCODE_BRANCH_BOOL) {
            size_t condition = (size_t)instruction->operands[0].unsigned_value;
            frame->block = (size_t)instruction->operands[
                frame->registers[condition].as.boolean ? 1 : 2
            ].unsigned_value;
            frame->instruction = 0;
            continue;
        }
        if (instruction->opcode == RUDA_OPCODE_PANIC_STRING) {
            size_t panic_index = frame_count - 1;
            status = ruda_bytecode_run_begin_panic(
                options.panic,
                allocator,
                options.execution,
                frame->registers[(size_t)instruction->operands[0].unsigned_value],
                frame->function->symbol,
                instruction->span
            );
            while (status == RUDA_BYTECODE_RUN_PANIC && panic_index != 0) {
                RudaBytecodeRunFrame *callee = &frames[panic_index];
                RudaBytecodeRunFrame *caller = &frames[panic_index - 1];
                status = ruda_bytecode_run_append_panic_frame(
                    options.panic, caller->function->symbol, callee->call_span
                );
                panic_index -= 1;
            }
            goto cleanup;
        }
        if (instruction->opcode == RUDA_OPCODE_CALL_DIRECT
            || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID
            || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER
            || instruction->opcode == RUDA_OPCODE_CALL_INTERFACE) {
            bool returns_void = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID;
            bool returns_never = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER;
            bool interface_call = instruction->opcode
                == RUDA_OPCODE_CALL_INTERFACE;
            uint64_t call_wire = instruction->operands[
                returns_void || returns_never ? 0 : 1
            ].unsigned_value;
            const RudaBytecodeCall *call = call_wire < body->calls.length
                ? ruda_bytecode_call(body, (RudaBytecodeCallId)call_wire)
                : NULL;
            RudaSymbolId callee_symbol;
            RudaManagedReference interface_payload =
                ruda_managed_invalid_reference();
            const RudaBytecodeProgramFunction *callee = NULL;
            size_t argument_index = 0;
            memset(&callee_symbol, 0, sizeof(callee_symbol));
            if (interface_call && call != NULL) {
                status = ruda_bytecode_run_interface_resolve(
                    instruction,
                    frame->registers,
                    options.execution,
                    body,
                    &callee_symbol,
                    &interface_payload
                );
                if (status != RUDA_BYTECODE_RUN_OK) {
                    goto cleanup;
                }
            } else if (call != NULL) {
                callee_symbol = call->callee;
            }
            callee = call != NULL
                ? ruda_bytecode_program_function_by_symbol(
                    program,
                    callee_symbol
                )
                : NULL;
            if (call == NULL || callee == NULL || frame_count >= options.max_call_depth
            ) {
                status = frame_count >= options.max_call_depth
                    ? RUDA_BYTECODE_RUN_STEP_LIMIT
                    : RUDA_BYTECODE_RUN_INVALID_BYTECODE;
                ruda_bytecode_run_set_trap(options, instruction->span);
                goto cleanup;
            }
            if (!ruda_bytecode_run_frames_reserve(
                    &frames,
                    &frame_capacity,
                    frame_count + 1,
                    options.max_call_depth,
                    allocator
                )) {
                status = RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
                goto cleanup;
            }
            /* Reserve may relocate frames, so only now advance the caller. */
            frames[frame_count - 1].instruction += 1;
            status = ruda_bytecode_run_frame_init(
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
            if (status != RUDA_BYTECODE_RUN_OK) {
                ruda_bytecode_run_frame_dispose(
                    &frames[frame_count], &allocator, options.execution
                );
                goto cleanup;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const RudaBytecodeCallArgument *argument =
                    ruda_bytecode_call_argument(
                        body,
                        call->argument_start + argument_index
                    );
                if (argument == NULL) {
                    ruda_bytecode_run_frame_dispose(
                        &frames[frame_count],
                        &allocator,
                        options.execution
                    );
                    status = RUDA_BYTECODE_RUN_INVALID_BYTECODE;
                    goto cleanup;
                }
                if (interface_call && argument_index == 0) {
                    const RudaBytecodeType *parameter = ruda_vector_at_const(
                        &callee->body.register_types,
                        0
                    );
                    if (parameter == NULL
                        || !ruda_bytecode_type_is_managed(*parameter)) {
                        ruda_bytecode_run_frame_dispose(
                            &frames[frame_count],
                            &allocator,
                            options.execution
                        );
                        status = RUDA_BYTECODE_RUN_INVALID_BYTECODE;
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
            status = ruda_bytecode_run_frame_roots_init(
                &frames[frame_count], options.execution, allocator
            );
            if (status != RUDA_BYTECODE_RUN_OK) {
                ruda_bytecode_run_frame_dispose(
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
            status = ruda_bytecode_run_value_instruction(
                instruction, frame->registers, options.execution, body,
                allocator, &bounds_index, &bounds_length
            );
            if (status == RUDA_BYTECODE_RUN_PANIC) {
                size_t panic_index = frame_count - 1;
                status = ruda_bytecode_run_begin_bounds_panic(
                    options.panic, allocator, bounds_index, bounds_length,
                    frame->function->symbol, instruction->span
                );
                while (status == RUDA_BYTECODE_RUN_PANIC
                    && panic_index != 0) {
                    RudaBytecodeRunFrame *callee = &frames[panic_index];
                    RudaBytecodeRunFrame *caller = &frames[panic_index - 1];
                    status = ruda_bytecode_run_append_panic_frame(
                        options.panic, caller->function->symbol,
                        callee->call_span
                    );
                    panic_index -= 1;
                }
            }
        }
        if (status != RUDA_BYTECODE_RUN_OK) {
            if (status == RUDA_BYTECODE_RUN_OVERFLOW
                || status == RUDA_BYTECODE_RUN_DIVISION_BY_ZERO
                || status == RUDA_BYTECODE_RUN_INVALID_SHIFT) {
                ruda_bytecode_run_set_trap(options, instruction->span);
            }
            goto cleanup;
        }
        frame->instruction += 1;
    }

cleanup:
    while (frame_count != 0) {
        frame_count -= 1;
        ruda_bytecode_run_frame_dispose(
            &frames[frame_count], &allocator, options.execution
        );
    }
    if (frames != NULL) {
        size_t bytes = 0;
        if (ruda_size_multiply(
                frame_capacity,
                sizeof(RudaBytecodeRunFrame),
                &bytes
            )) {
            ruda_deallocate(&allocator, frames, bytes);
        }
    }
    return status;
}
