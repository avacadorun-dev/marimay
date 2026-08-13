#include "ruda/bytecode/vm.h"

#include <string.h>

#include "ruda/base/checked_i64.h"
#include "ruda/base/f64.h"

RudaBytecodeExecutionOptions ruda_bytecode_execution_default_options(void) {
    RudaBytecodeExecutionOptions options;
    options.max_strings = 1000000;
    options.max_total_string_bytes = 64U * 1024U * 1024U;
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
        )) {
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
    memset(execution, 0, sizeof(*execution));
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

static RudaBytecodeRunStatus ruda_bytecode_run_value_instruction(
    const RudaBytecodeInstruction *instruction,
    RudaBytecodeValue *registers,
    RudaBytecodeExecution *execution,
    const RudaBytecodeFunction *function
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

static void ruda_bytecode_run_reset_panic(RudaBytecodePanic *panic) {
    if (panic != NULL && panic->initialized) {
        ruda_owned_bytes_dispose(&panic->message);
        panic->frames.length = 0;
        panic->code = NULL;
        memset(&panic->symbol, 0, sizeof(panic->symbol));
        memset(&panic->span, 0, sizeof(panic->span));
    }
}

static RudaBytecodeRunStatus ruda_bytecode_run_begin_panic(
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

static RudaBytecodeRunStatus ruda_bytecode_run_append_panic_frame(
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

static bool ruda_bytecode_run_string_value_valid(
    RudaBytecodeExecution *execution,
    RudaBytecodeValue value
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    return ruda_bytecode_execution_string_bytes(
        execution, value, &bytes, &length
    );
}

static bool ruda_bytecode_run_function_uses_strings(
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

static bool ruda_bytecode_run_program_uses_strings(
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

static bool ruda_bytecode_run_function_uses_f64(
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

static bool ruda_bytecode_run_program_uses_f64(
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
                || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID
                || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER) {
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
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType *expected = ruda_vector_at_const(
            &function->register_types,
            index
        );
        if (expected == NULL || arguments[index].type != *expected) {
            return RUDA_BYTECODE_RUN_INVALID_ARGUMENT;
        }
        if (*expected == RUDA_BYTECODE_TYPE_STRING
            && !ruda_bytecode_run_string_value_valid(
                options.execution, arguments[index]
            )) {
            return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
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
                if (completed.type == RUDA_BYTECODE_TYPE_STRING
                    && !ruda_bytecode_run_string_value_valid(
                        options.execution, completed
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
            status = ruda_bytecode_run_value_instruction(
                instruction,
                registers,
                options.execution,
                function
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
    bool return_is_never;
    RudaBytecodeSpan call_span;
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
    frame->return_is_never = return_is_never;
    frame->call_span = call_span;
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
        if (*expected == RUDA_BYTECODE_TYPE_STRING
            && !ruda_bytecode_run_string_value_valid(
                options.execution, arguments[index]
            )) {
            return RUDA_BYTECODE_RUN_INVALID_REFERENCE;
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
        &frames[0], entry, arguments, argument_count, allocator, 0, false,
        false, (RudaBytecodeSpan){0}
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
            || instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER) {
            bool returns_void = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_VOID;
            bool returns_never = instruction->opcode == RUDA_OPCODE_CALL_DIRECT_NEVER;
            uint64_t call_wire = instruction->operands[
                returns_void || returns_never ? 0 : 1
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
                returns_void || returns_never
                    ? 0
                    : (uint32_t)instruction->operands[0].unsigned_value,
                returns_void,
                returns_never,
                instruction->span
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
        status = ruda_bytecode_run_value_instruction(
            instruction, frame->registers, options.execution, body
        );
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
