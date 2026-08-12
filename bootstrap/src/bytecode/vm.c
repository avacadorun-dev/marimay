#include "ruda/bytecode/vm.h"

#include <string.h>

#include "ruda/base/checked_i64.h"

RudaBytecodeRunOptions ruda_bytecode_run_default_options(void) {
    RudaBytecodeRunOptions options;
    options.max_steps = 10000000;
    options.max_call_depth = 1024;
    options.remaining_shared_steps = NULL;
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

static RudaBytecodeRunStatus ruda_bytecode_run_value_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    switch (instruction->opcode) {
        case RUDA_OPCODE_CONST_I64:
            registers[target].as.i64 = instruction->operands[1].signed_value;
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_CONST_BOOL:
            registers[target].as.boolean = instruction->operands[1].unsigned_value
                != 0;
            return RUDA_BYTECODE_RUN_OK;
        case RUDA_OPCODE_COPY:
            registers[target] = registers[
                (size_t)instruction->operands[1].unsigned_value
            ];
            return RUDA_BYTECODE_RUN_OK;
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
        case RUDA_OPCODE_BOOL_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.boolean
                == registers[second].as.boolean;
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
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
}

static bool ruda_bytecode_run_consume_step(
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

static void ruda_bytecode_run_set_trap(
    RudaBytecodeRunOptions options,
    RudaBytecodeSpan span
) {
    if (options.trap != NULL) {
        options.trap->span = span;
        options.trap->has_span = true;
    }
}

static bool ruda_bytecode_run_function_has_calls(
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
                || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID) {
                return true;
            }
        }
    }
    return false;
}

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
    size_t register_count = 0;
    size_t register_bytes = 0;
    size_t index = 0;
    size_t steps = 0;
    size_t current_block = 0;
    RudaBytecodeRunStatus status = RUDA_BYTECODE_RUN_OK;
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
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType *expected = ruda_vector_at_const(
            &function->register_types,
            index
        );
        if (expected == NULL || arguments[index].type != *expected) {
            return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
        }
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
    }
    for (index = 0; index < argument_count; index += 1) {
        registers[index] = arguments[index];
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
            status = ruda_bytecode_run_value_instruction(
                instruction,
                registers
            );
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
} RudaBytecodeRunFrame;

static void ruda_bytecode_run_frame_dispose(
    RudaBytecodeRunFrame *frame,
    const RudaAllocator *allocator
) {
    if (frame != NULL && allocator != NULL) {
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
    bool return_is_void
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
            ruda_bytecode_run_frame_dispose(frame, &allocator);
            return RUDA_BYTECODE_RUN_INVALID_BYTECODE;
        }
        frame->registers[index].type = *type;
    }
    for (index = 0; index < argument_count; index += 1) {
        frame->registers[index] = arguments[index];
    }
    frame->function = function;
    frame->return_destination = return_destination;
    frame->return_is_void = return_is_void;
    return RUDA_BYTECODE_RUN_OK;
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
    size_t index = 0;
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
    status = ruda_bytecode_run_program_verify_status(
        ruda_bytecode_program_verify(program)
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        return status;
    }
    entry = ruda_bytecode_program_function(program, program->entry_function);
    if (entry == NULL || argument_count != entry->body.parameter_count) {
        return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType *expected = ruda_vector_at_const(
            &entry->body.register_types,
            index
        );
        if (expected == NULL || arguments[index].type != *expected) {
            return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
        }
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
        &frames[0], entry, arguments, argument_count, allocator, 0, false
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        ruda_bytecode_run_frame_dispose(&frames[0], &allocator);
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
            if (frame_count == 1) {
                *result = completed;
                status = RUDA_BYTECODE_RUN_OK;
                goto cleanup;
            }
            {
                RudaBytecodeRunFrame *parent = &frames[frame_count - 2];
                uint32_t destination = frame->return_destination;
                bool return_is_void = frame->return_is_void;
                ruda_bytecode_run_frame_dispose(frame, &allocator);
                frame_count -= 1;
                if (!return_is_void) {
                    parent->registers[destination] = completed;
                }
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
        if (instruction->opcode == RUDA_OPCODE_CALL_DIRECT
            || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID) {
            bool returns_void = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID;
            uint64_t call_wire = instruction->operands[
                returns_void ? 0 : 1
            ].unsigned_value;
            const RudaBytecodeCall *call = call_wire < body->calls.length
                ? ruda_bytecode_call(body, (RudaBytecodeCallId)call_wire)
                : NULL;
            const RudaBytecodeProgramFunction *callee = call != NULL
                ? ruda_bytecode_program_function_by_symbol(program, call->callee)
                : NULL;
            size_t argument_index = 0;
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
                returns_void ? 0 : (uint32_t)instruction->operands[0].unsigned_value,
                returns_void
            );
            if (status != RUDA_BYTECODE_RUN_OK) {
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
                        &allocator
                    );
                    status = RUDA_BYTECODE_RUN_INVALID_BYTECODE;
                    goto cleanup;
                }
                frames[frame_count].registers[argument_index] =
                    frames[frame_count - 1].registers[argument->register_id];
            }
            frame_count += 1;
            continue;
        }
        status = ruda_bytecode_run_value_instruction(instruction, frame->registers);
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
        ruda_bytecode_run_frame_dispose(&frames[frame_count], &allocator);
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
