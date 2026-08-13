#include "ruda/native/c_emitter.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/base/vector.h"

typedef struct RudaNativeCBuffer {
    RudaVector bytes;
    size_t limit;
    bool limited;
} RudaNativeCBuffer;

typedef struct RudaNativeCCycleFrame {
    size_t function_index;
    size_t block_index;
    size_t instruction_index;
} RudaNativeCCycleFrame;

static int ruda_native_c_symbol_compare(RudaSymbolId left, RudaSymbolId right) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes));
}

static int ruda_native_c_function_pointer_compare(
    const void *left,
    const void *right
) {
    const RudaMirFunction *const *left_function = left;
    const RudaMirFunction *const *right_function = right;
    return ruda_native_c_symbol_compare(
        (*left_function)->symbol,
        (*right_function)->symbol
    );
}

static bool ruda_native_c_type_supported(RudaTypeId type, bool result) {
    return type == RUDA_TYPE_BOOL || type == RUDA_TYPE_I64
        || type == RUDA_TYPE_STRING
        || (result && (type == RUDA_TYPE_VOID || type == RUDA_TYPE_NEVER));
}

static const char *ruda_native_c_type(RudaTypeId type) {
    if (type == RUDA_TYPE_BOOL) {
        return "bool";
    }
    if (type == RUDA_TYPE_STRING) {
        return "ruda_g_string";
    }
    return "int64_t";
}

static bool ruda_native_c_buffer_append(
    RudaNativeCBuffer *buffer, const char *text, size_t length
) {
    size_t total = 0;
    if (buffer == NULL || (text == NULL && length != 0)
        || !ruda_size_add(buffer->bytes.length, length, &total)) {
        return false;
    }
    if (total > buffer->limit) {
        buffer->limited = true;
        return false;
    }
    return ruda_vector_append(&buffer->bytes, text, length);
}

static bool ruda_native_c_write(RudaNativeCBuffer *buffer, const char *format, ...) {
    char text[512];
    va_list arguments;
    int written = 0;
    va_start(arguments, format);
    written = vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    return written >= 0 && (size_t)written < sizeof(text)
        && ruda_native_c_buffer_append(buffer, text, (size_t)written);
}

static bool ruda_native_c_symbol(RudaNativeCBuffer *buffer, RudaSymbolId symbol) {
    size_t index = 0;
    if (!ruda_native_c_write(buffer, "ruda_g_f_")) {
        return false;
    }
    for (index = 0; index < sizeof(symbol.bytes); index += 1) {
        if (!ruda_native_c_write(buffer, "%02x", (unsigned int)symbol.bytes[index])) {
            return false;
        }
    }
    return true;
}

static bool ruda_native_c_symbol_hex(
    RudaNativeCBuffer *buffer,
    RudaSymbolId symbol
) {
    size_t index = 0;
    for (index = 0; index < sizeof(symbol.bytes); index += 1) {
        if (!ruda_native_c_write(
                buffer,
                "%02x",
                (unsigned int)symbol.bytes[index]
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_native_c_symbol_data(
    RudaNativeCBuffer *buffer,
    RudaSymbolId symbol
) {
    return ruda_native_c_write(buffer, "ruda_g_y_")
        && ruda_native_c_symbol_hex(buffer, symbol);
}

static bool ruda_native_c_string_data(
    RudaNativeCBuffer *buffer,
    RudaSymbolId symbol,
    size_t index
) {
    return ruda_native_c_write(buffer, "ruda_g_c_")
        && ruda_native_c_symbol_hex(buffer, symbol)
        && ruda_native_c_write(buffer, "_%zu", index);
}

static const RudaMirFunction *ruda_native_c_find(
    const RudaMirFunction *const *functions, size_t count, RudaSymbolId symbol
) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int comparison = ruda_native_c_symbol_compare(functions[middle]->symbol, symbol);
        if (comparison == 0) {
            return functions[middle];
        }
        if (comparison < 0) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return NULL;
}

static const RudaMirCall *ruda_native_c_call(
    const RudaMirFunction *function, RudaMirCallId call
) {
    return function != NULL ? ruda_vector_at_const(&function->calls, call) : NULL;
}

static const RudaMirCallArgument *ruda_native_c_call_argument(
    const RudaMirFunction *function, size_t index
) {
    return function != NULL
        ? ruda_vector_at_const(&function->call_arguments, index)
        : NULL;
}

static bool ruda_native_c_call_supported(
    const RudaMirFunction *function,
    RudaMirCallId call_id,
    const RudaMirFunction *const *functions,
    size_t count,
    RudaTypeId required_result
) {
    const RudaMirCall *call = ruda_native_c_call(function, call_id);
    const RudaMirFunction *callee = call != NULL
        ? ruda_native_c_find(functions, count, call->callee)
        : NULL;
    size_t argument_index = 0;
    if (callee == NULL || call->result_type != required_result
        || callee->result_type != call->result_type
        || call->argument_count != callee->parameter_count) {
        return false;
    }
    for (argument_index = 0;
        argument_index < call->argument_count;
        argument_index += 1) {
        const RudaMirCallArgument *argument = ruda_native_c_call_argument(
            function,
            call->argument_start + argument_index
        );
        const RudaTypeId *parameter = ruda_vector_at_const(
            &callee->register_types,
            argument_index
        );
        if (argument == NULL || parameter == NULL || argument->type != *parameter) {
            return false;
        }
    }
    return true;
}

static RudaNativeCEmitStatus ruda_native_c_validate_function(
    const RudaMirFunction *function,
    const RudaMirFunction *const *functions,
    size_t count
) {
    size_t index = 0;
    if (function == NULL || !function->initialized) {
        return RUDA_NATIVE_C_EMIT_INVALID_MIR;
    }
    RudaMirVerifyStatus verify_status = ruda_mir_verify(function);
    if (verify_status == RUDA_MIR_VERIFY_ALLOCATION_FAILED) {
        return RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED;
    }
    if (verify_status == RUDA_MIR_VERIFY_LIMIT_REACHED) {
        return RUDA_NATIVE_C_EMIT_LIMIT_REACHED;
    }
    if (verify_status != RUDA_MIR_VERIFY_OK) {
        return RUDA_NATIVE_C_EMIT_INVALID_MIR;
    }
    if (!ruda_native_c_type_supported(function->result_type, true)) {
        return RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(&function->register_types, index);
        if (type == NULL || !ruda_native_c_type_supported(*type, false)) {
            return RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
        }
    }
    for (index = 0; index < function->blocks.length; index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(&function->blocks, index);
        size_t instruction_index = 0;
        if (block == NULL) {
            return RUDA_NATIVE_C_EMIT_INVALID_MIR;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *instruction = ruda_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL) {
                return RUDA_NATIVE_C_EMIT_INVALID_MIR;
            }
            switch (instruction->kind) {
                case RUDA_MIR_CONSTANT_I64: case RUDA_MIR_CONSTANT_BOOL:
                case RUDA_MIR_COPY: case RUDA_MIR_ADD_CHECKED_I64:
                case RUDA_MIR_SUBTRACT_CHECKED_I64: case RUDA_MIR_MULTIPLY_CHECKED_I64:
                case RUDA_MIR_DIVIDE_CHECKED_I64: case RUDA_MIR_REMAINDER_CHECKED_I64:
                case RUDA_MIR_NEGATE_CHECKED_I64: case RUDA_MIR_NOT_BOOL:
                case RUDA_MIR_BITWISE_NOT_I64: case RUDA_MIR_BITWISE_AND_I64:
                case RUDA_MIR_BITWISE_XOR_I64: case RUDA_MIR_BITWISE_OR_I64:
                case RUDA_MIR_SHIFT_LEFT_CHECKED_I64: case RUDA_MIR_SHIFT_RIGHT_I64:
                case RUDA_MIR_EQUAL_I64: case RUDA_MIR_EQUAL_BOOL:
                case RUDA_MIR_CONSTANT_STRING: case RUDA_MIR_EQUAL_STRING:
                case RUDA_MIR_LESS_I64: case RUDA_MIR_LESS_EQUAL_I64:
                    break;
                case RUDA_MIR_CALL: {
                    const RudaMirCall *call = ruda_native_c_call(function, instruction->call);
                    if (call == NULL
                        || !ruda_native_c_type_supported(call->result_type, false)
                        || !ruda_native_c_call_supported(
                            function,
                            instruction->call,
                            functions,
                            count,
                            call->result_type
                        )) {
                        return RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL;
                    }
                    break;
                }
                case RUDA_MIR_RECORD_NEW: case RUDA_MIR_RECORD_GET:
                case RUDA_MIR_RECORD_SET: case RUDA_MIR_RECORD_IDENTITY_SAME:
                case RUDA_MIR_ENUM_NEW: case RUDA_MIR_ENUM_IS_VARIANT:
                case RUDA_MIR_ENUM_REQUIRE_VARIANT: case RUDA_MIR_ENUM_GET_PAYLOAD:
                    return RUDA_NATIVE_C_EMIT_UNSUPPORTED_INSTRUCTION;
            }
        }
        if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            if (!ruda_native_c_call_supported(
                    function,
                    block->terminator.call,
                    functions,
                    count,
                    RUDA_TYPE_NEVER
                )) {
                return RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL;
            }
        } else if (block->terminator.kind != RUDA_MIR_RETURN
            && block->terminator.kind != RUDA_MIR_GOTO
            && block->terminator.kind != RUDA_MIR_BRANCH
            && block->terminator.kind != RUDA_MIR_PANIC) {
            return RUDA_NATIVE_C_EMIT_UNSUPPORTED_TERMINATOR;
        }
    }
    return RUDA_NATIVE_C_EMIT_OK;
}

static bool ruda_native_c_function_index(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaSymbolId symbol,
    size_t *result
) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int comparison = ruda_native_c_symbol_compare(
            functions[middle]->symbol, symbol
        );
        if (comparison == 0) {
            *result = middle;
            return true;
        }
        if (comparison < 0) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return false;
}

/* Explicit DFS: stack depth is bounded by the already bounded function set. */
static bool ruda_native_c_has_cycle(
    const RudaMirFunction *const *functions,
    size_t count,
    unsigned char *states,
    RudaNativeCCycleFrame *stack
) {
    size_t root = 0;
    for (root = 0; root < count; root += 1) {
        size_t depth = 0;
        if (states[root] == 2) {
            continue;
        }
        states[root] = 1;
        stack[depth].function_index = root;
        stack[depth].block_index = 0;
        stack[depth].instruction_index = 0;
        depth += 1;
        while (depth != 0) {
            RudaNativeCCycleFrame *frame = &stack[depth - 1];
            const RudaMirFunction *function = functions[frame->function_index];
            const RudaMirBlock *block = NULL;
            if (frame->block_index >= function->blocks.length) {
                states[frame->function_index] = 2;
                depth -= 1;
                continue;
            }
            block = ruda_vector_at_const(&function->blocks, frame->block_index);
            if (block == NULL) {
                return true;
            }
            if (frame->instruction_index > block->instructions.length
                || (frame->instruction_index == block->instructions.length
                    && block->terminator.kind != RUDA_MIR_CALL_NEVER)) {
                frame->block_index += 1;
                frame->instruction_index = 0;
                continue;
            }
            {
                RudaMirCallId call_id = RUDA_MIR_NO_CALL;
                const RudaMirInstruction *instruction = NULL;
                if (frame->instruction_index < block->instructions.length) {
                    instruction = ruda_vector_at_const(
                        &block->instructions,
                        frame->instruction_index
                    );
                    if (instruction != NULL
                        && instruction->kind == RUDA_MIR_CALL) {
                        call_id = instruction->call;
                    }
                } else {
                    call_id = block->terminator.call;
                }
                frame->instruction_index += 1;
                if (call_id != RUDA_MIR_NO_CALL) {
                    const RudaMirCall *call = ruda_native_c_call(
                        function,
                        call_id
                    );
                    size_t callee_index = 0;
                    if (call == NULL || !ruda_native_c_function_index(
                            functions, count, call->callee, &callee_index
                        ) || states[callee_index] == 1) {
                        return true;
                    }
                    if (states[callee_index] == 0) {
                        if (depth >= count) {
                            return true;
                        }
                        states[callee_index] = 1;
                        stack[depth].function_index = callee_index;
                        stack[depth].block_index = 0;
                        stack[depth].instruction_index = 0;
                        depth += 1;
                    }
                }
            }
        }
    }
    return false;
}

static bool ruda_native_c_emit_signature(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function, bool prototype
) {
    size_t index = 0;
    if (!ruda_native_c_write(buffer, "static int ") || !ruda_native_c_symbol(buffer, function->symbol)
        || !ruda_native_c_write(buffer, "(ruda_g_panic *ruda_g_p")) return false;
    for (index = 0; index < function->parameter_count; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(&function->register_types, index);
        if (!ruda_native_c_write(buffer, ", %s ruda_g_v_%zu", ruda_native_c_type(*type), index)) return false;
    }
    if (function->result_type != RUDA_TYPE_VOID
        && function->result_type != RUDA_TYPE_NEVER
        && !ruda_native_c_write(buffer, ", %s *ruda_g_out", ruda_native_c_type(function->result_type))) return false;
    return ruda_native_c_write(buffer,
        ", size_t *ruda_g_start, size_t *ruda_g_end)%s", prototype ? ";\n" : " {\n"
    );
}

static bool ruda_native_c_emit_call_arguments(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirCall *call
) {
    size_t argument_index = 0;
    for (argument_index = 0;
        argument_index < call->argument_count;
        argument_index += 1) {
        const RudaMirCallArgument *argument = ruda_native_c_call_argument(
            function,
            call->argument_start + argument_index
        );
        if (!ruda_native_c_write(
                buffer,
                ", ruda_g_v_%u",
                argument->register_id
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_native_c_emit_propagate_call(
    RudaNativeCBuffer *buffer,
    RudaSymbolId symbol,
    RudaMirSpan span,
    bool returns_never
) {
    if (!ruda_native_c_write(buffer, "  if (ruda_g_s == 4) {\n    if (!ruda_g_append_frame(ruda_g_p, ")
        || !ruda_native_c_symbol_data(buffer, symbol)
        || !ruda_native_c_write(
            buffer,
            ", %zu, %zu)) return 5;\n    return 4;\n  }\n",
            span.start,
            span.end
        )) {
        return false;
    }
    if (returns_never) {
        return ruda_native_c_write(
            buffer,
            "  if (ruda_g_s == 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; return 5; }\n  return ruda_g_s;\n",
            span.start,
            span.end
        );
    }
    return ruda_native_c_write(
        buffer,
        "  if (ruda_g_s != 0) return ruda_g_s;\n"
    );
}

static bool ruda_native_c_emit_function(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirFunction *const *functions, size_t count
) {
    size_t index = 0;
    if (!ruda_native_c_emit_signature(buffer, function, false)
        || !ruda_native_c_write(
            buffer,
            "  int ruda_g_s;\n  (void)ruda_g_s;\n  (void)ruda_g_p;\n"
        )) return false;
    if (!ruda_native_c_write(
            buffer, "  (void)ruda_g_start;\n  (void)ruda_g_end;\n"
        )) return false;
    for (index = 0; index < function->parameter_count; index += 1) {
        if (!ruda_native_c_write(buffer, "  (void)ruda_g_v_%zu;\n", index)) {
            return false;
        }
    }
    for (index = function->parameter_count; index < function->register_types.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(&function->register_types, index);
        if (!ruda_native_c_write(buffer, "  %s ruda_g_v_%zu;\n", ruda_native_c_type(*type), index)) return false;
    }
    if (!ruda_native_c_write(buffer, "  goto ruda_g_b_0;\n")) return false;
    for (index = 0; index < function->blocks.length; index += 1) {
        const RudaMirBlock *block = ruda_vector_at_const(&function->blocks, index);
        size_t instruction_index = 0;
        if (!ruda_native_c_write(buffer, "ruda_g_b_%zu:\n", index)) return false;
        for (instruction_index = 0; instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaMirInstruction *i = ruda_vector_at_const(&block->instructions, instruction_index);
            const char *helper = NULL;
            switch (i->kind) {
                case RUDA_MIR_CONSTANT_I64:
                    if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_i64_from_twos_complement(UINT64_C(0x%016llx));\n", i->target, (unsigned long long)(uint64_t)i->integer)) return false;
                    break;
                case RUDA_MIR_CONSTANT_BOOL: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = %s;\n", i->target, i->boolean ? "true" : "false")) return false; break;
                case RUDA_MIR_CONSTANT_STRING: {
                    const RudaMirStringConstant *constant = ruda_vector_at_const(
                        &function->string_constants,
                        i->string_constant
                    );
                    if (constant == NULL
                        || !ruda_native_c_write(
                            buffer,
                            "  ruda_g_v_%u.data = ",
                            i->target
                        )
                        || !ruda_native_c_string_data(
                            buffer,
                            function->symbol,
                            i->string_constant
                        )
                        || !ruda_native_c_write(
                            buffer,
                            "; ruda_g_v_%u.length = %zu;\n",
                            i->target,
                            constant->value.length
                        )) {
                        return false;
                    }
                    break;
                }
                case RUDA_MIR_COPY: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u;\n", i->target, i->first)) return false; break;
                case RUDA_MIR_NOT_BOOL: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = !ruda_g_v_%u;\n", i->target, i->first)) return false; break;
                case RUDA_MIR_EQUAL_I64: case RUDA_MIR_EQUAL_BOOL: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u == ruda_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_EQUAL_STRING: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_string_equal(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_LESS_I64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u < ruda_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_LESS_EQUAL_I64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u <= ruda_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
                case RUDA_MIR_BITWISE_NOT_I64: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_i64_bitwise_not(ruda_g_v_%u);\n", i->target, i->first)) return false; break;
                case RUDA_MIR_BITWISE_AND_I64: case RUDA_MIR_BITWISE_XOR_I64: case RUDA_MIR_BITWISE_OR_I64: {
                    const char *name = i->kind == RUDA_MIR_BITWISE_AND_I64 ? "and" : i->kind == RUDA_MIR_BITWISE_XOR_I64 ? "xor" : "or";
                    if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_i64_bitwise_%s(ruda_g_v_%u, ruda_g_v_%u);\n", i->target, name, i->first, i->second)) return false; break;
                }
                case RUDA_MIR_ADD_CHECKED_I64: helper = "ruda_checked_i64_add"; break;
                case RUDA_MIR_SUBTRACT_CHECKED_I64: helper = "ruda_checked_i64_subtract"; break;
                case RUDA_MIR_MULTIPLY_CHECKED_I64: helper = "ruda_checked_i64_multiply"; break;
                case RUDA_MIR_DIVIDE_CHECKED_I64: helper = "ruda_checked_i64_divide"; break;
                case RUDA_MIR_REMAINDER_CHECKED_I64: helper = "ruda_checked_i64_remainder"; break;
                case RUDA_MIR_SHIFT_LEFT_CHECKED_I64: helper = "ruda_checked_i64_shift_left"; break;
                case RUDA_MIR_SHIFT_RIGHT_I64: helper = "ruda_checked_i64_shift_right"; break;
                case RUDA_MIR_NEGATE_CHECKED_I64: helper = "ruda_checked_i64_negate"; break;
                case RUDA_MIR_CALL: {
                    const RudaMirCall *call = ruda_native_c_call(function, i->call);
                    const RudaMirFunction *callee = ruda_native_c_find(functions, count, call->callee);
                    if (!ruda_native_c_write(buffer, "  ruda_g_s = ")
                        || !ruda_native_c_symbol(buffer, callee->symbol)
                        || !ruda_native_c_write(buffer, "(ruda_g_p")
                        || !ruda_native_c_emit_call_arguments(buffer, function, call)
                        || !ruda_native_c_write(
                            buffer,
                            ", &ruda_g_v_%u, ruda_g_start, ruda_g_end);\n",
                            i->target
                        )
                        || !ruda_native_c_emit_propagate_call(
                            buffer,
                            function->symbol,
                            i->span,
                            false
                        )) return false;
                    break;
                }
                default: return false;
            }
            if (helper != NULL) {
                if (i->kind == RUDA_MIR_NEGATE_CHECKED_I64) {
                    if (!ruda_native_c_write(buffer, "  ruda_g_s = (int)%s(ruda_g_v_%u, &ruda_g_v_%u);\n", helper, i->first, i->target)) return false;
                } else if (!ruda_native_c_write(buffer, "  ruda_g_s = (int)%s(ruda_g_v_%u, ruda_g_v_%u, &ruda_g_v_%u);\n", helper, i->first, i->second, i->target)) return false;
                if (!ruda_native_c_write(buffer, "  if (ruda_g_s != 0) { *ruda_g_start = %zu; *ruda_g_end = %zu; return ruda_g_s; }\n", i->span.start, i->span.end)) return false;
            }
        }
        if (block->terminator.kind == RUDA_MIR_GOTO) {
            if (!ruda_native_c_write(buffer, "  goto ruda_g_b_%u;\n", block->terminator.target)) return false;
        } else if (block->terminator.kind == RUDA_MIR_BRANCH) {
            if (!ruda_native_c_write(buffer, "  if (ruda_g_v_%u) goto ruda_g_b_%u; else goto ruda_g_b_%u;\n", block->terminator.value, block->terminator.target, block->terminator.otherwise)) return false;
        } else if (block->terminator.kind == RUDA_MIR_PANIC) {
            if (!ruda_native_c_write(
                    buffer,
                    "  return ruda_g_begin_panic(ruda_g_p, ruda_g_v_%u, ",
                    block->terminator.value
                )
                || !ruda_native_c_symbol_data(buffer, function->symbol)
                || !ruda_native_c_write(
                    buffer,
                    ", %zu, %zu);\n",
                    block->terminator.span.start,
                    block->terminator.span.end
                )) return false;
        } else if (block->terminator.kind == RUDA_MIR_CALL_NEVER) {
            const RudaMirCall *call = ruda_native_c_call(
                function,
                block->terminator.call
            );
            const RudaMirFunction *callee = ruda_native_c_find(
                functions,
                count,
                call->callee
            );
            if (!ruda_native_c_write(buffer, "  ruda_g_s = ")
                || !ruda_native_c_symbol(buffer, callee->symbol)
                || !ruda_native_c_write(buffer, "(ruda_g_p")
                || !ruda_native_c_emit_call_arguments(buffer, function, call)
                || !ruda_native_c_write(
                    buffer,
                    ", ruda_g_start, ruda_g_end);\n"
                )
                || !ruda_native_c_emit_propagate_call(
                    buffer,
                    function->symbol,
                    block->terminator.span,
                    true
                )) return false;
        } else if (function->result_type == RUDA_TYPE_VOID) {
            if (!ruda_native_c_write(buffer, "  return 0;\n")) return false;
        } else if (!ruda_native_c_write(buffer, "  *ruda_g_out = ruda_g_v_%u; return 0;\n", block->terminator.value)) return false;
    }
    return ruda_native_c_write(buffer, "}\n\n");
}

static bool ruda_native_c_emit_symbol_and_strings(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function
) {
    size_t index = 0;
    if (!ruda_native_c_write(buffer, "static const unsigned char ")
        || !ruda_native_c_symbol_data(buffer, function->symbol)
        || !ruda_native_c_write(buffer, "[16] = {")
        || !ruda_native_c_write(buffer, "0x%02x", (unsigned int)function->symbol.bytes[0])) {
        return false;
    }
    for (index = 1; index < sizeof(function->symbol.bytes); index += 1) {
        if (!ruda_native_c_write(
                buffer,
                ",0x%02x",
                (unsigned int)function->symbol.bytes[index]
            )) {
            return false;
        }
    }
    if (!ruda_native_c_write(buffer, "};\n")) {
        return false;
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        const RudaMirStringConstant *constant = ruda_vector_at_const(
            &function->string_constants,
            index
        );
        size_t byte_index = 0;
        if (constant == NULL || !ruda_native_c_write(buffer, "static const unsigned char ")
            || !ruda_native_c_string_data(buffer, function->symbol, index)
            || !ruda_native_c_write(
                buffer,
                "[%zu] = {",
                constant->value.length == 0 ? 1 : constant->value.length
            )) {
            return false;
        }
        if (constant->value.length == 0) {
            if (!ruda_native_c_write(buffer, "0")) {
                return false;
            }
        } else {
            for (byte_index = 0;
                byte_index < constant->value.length;
                byte_index += 1) {
                if (!ruda_native_c_write(
                        buffer,
                        byte_index == 0
                            ? "0x%02x"
                            : (byte_index % 16 == 0 ? ",\n0x%02x" : ",0x%02x"),
                        (unsigned int)constant->value.data[byte_index]
                    )) {
                    return false;
                }
            }
        }
        if (!ruda_native_c_write(buffer, "};\n")) {
            return false;
        }
    }
    return true;
}

static bool ruda_native_c_emit_runtime(
    RudaNativeCBuffer *buffer,
    size_t max_frames
) {
    return ruda_native_c_write(buffer,
        "typedef struct { const unsigned char *data; size_t length; } ruda_g_string;\n")
        && ruda_native_c_write(buffer,
            "typedef struct { const unsigned char *symbol; size_t start; size_t end; } ruda_g_frame;\n")
        && ruda_native_c_write(buffer,
            "typedef struct { const char *code; ruda_g_string message; const unsigned char *symbol; size_t start; size_t end; ruda_g_frame *frames; size_t frame_count; size_t frame_capacity; } ruda_g_panic;\n")
        && ruda_native_c_write(buffer,
            "static bool ruda_g_string_equal(ruda_g_string a, ruda_g_string b) { return a.length == b.length && (a.length == 0 || memcmp(a.data, b.data, a.length) == 0); }\n")
        && ruda_native_c_write(buffer,
            "static bool ruda_g_append_frame(ruda_g_panic *p, const unsigned char *symbol, size_t start, size_t end) { if (p == NULL || p->frame_count >= p->frame_capacity) return false; p->frames[p->frame_count].symbol = symbol; p->frames[p->frame_count].start = start; p->frames[p->frame_count].end = end; p->frame_count += 1; return true; }\n")
        && ruda_native_c_write(buffer,
            "static int ruda_g_begin_panic(ruda_g_panic *p, ruda_g_string message, const unsigned char *symbol, size_t start, size_t end) { if (p == NULL) return 4; p->code = \"G4000\"; p->message = message; p->symbol = symbol; p->start = start; p->end = end; p->frame_count = 0; return ruda_g_append_frame(p, symbol, start, end) ? 4 : 5; }\n")
        && ruda_native_c_write(buffer,
            "static void ruda_g_print_hex(const unsigned char *data, size_t length) { size_t i; for (i = 0; i < length; i += 1) printf(\"%%02x\", (unsigned int)data[i]); }\n")
        && ruda_native_c_write(buffer,
            "static void ruda_g_print_panic(const ruda_g_panic *p) { size_t i; printf(\"panic:%%s:\", p->code); ruda_g_print_hex(p->symbol, 16); printf(\":%%zu:%%zu:%%zu:\", p->start, p->end, p->message.length); ruda_g_print_hex(p->message.data, p->message.length); printf(\":%%zu\", p->frame_count); for (i = 0; i < p->frame_count; i += 1) { putchar(':'); ruda_g_print_hex(p->frames[i].symbol, 16); printf(\":%%zu:%%zu\", p->frames[i].start, p->frames[i].end); } putchar('\\n'); }\n")
        && ruda_native_c_write(
            buffer,
            "#define RUDA_G_MAX_FRAMES %zu\n\n",
            max_frames
        );
}

static bool ruda_native_c_emit_keep_assets(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *const *functions,
    size_t count
) {
    size_t function_index = 0;
    if (!ruda_native_c_write(buffer, "static void ruda_g_keep_assets(void) {\n")) {
        return false;
    }
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *function = functions[function_index];
        size_t string_index = 0;
        if (!ruda_native_c_write(buffer, "  (void)")
            || !ruda_native_c_symbol_data(buffer, function->symbol)
            || !ruda_native_c_write(buffer, ";\n")) {
            return false;
        }
        for (string_index = 0;
            string_index < function->string_constants.length;
            string_index += 1) {
            if (!ruda_native_c_write(buffer, "  (void)")
                || !ruda_native_c_string_data(
                    buffer,
                    function->symbol,
                    string_index
                )
                || !ruda_native_c_write(buffer, ";\n")) {
                return false;
            }
        }
    }
    return ruda_native_c_write(
        buffer,
        "  (void)&ruda_g_string_equal;\n  (void)&ruda_g_begin_panic;\n}\n\n"
    );
}

RudaNativeCEmitOptions ruda_native_c_emit_default_options(void) {
    RudaNativeCEmitOptions options;
    options.max_functions = 100000;
    options.max_generated_bytes = 64U * 1024U * 1024U;
    return options;
}

RudaNativeCEmitStatus ruda_native_c_emit_program(
    const RudaNativeCProgram *program, RudaAllocator allocator,
    RudaNativeCEmitOptions options, RudaOwnedBytes *output
) {
    const RudaMirFunction **ordered = NULL;
    unsigned char *states = NULL;
    RudaNativeCCycleFrame *cycle_stack = NULL;
    RudaNativeCBuffer buffer;
    RudaNativeCEmitStatus status = RUDA_NATIVE_C_EMIT_OK;
    size_t bytes = 0;
    size_t cycle_stack_bytes = 0;
    size_t index = 0;
    const RudaMirFunction *entry = NULL;
    if (output == NULL) return RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT;
    memset(output, 0, sizeof(*output));
    if (program == NULL || program->functions == NULL || program->function_count == 0
        || options.max_functions == 0
        || options.max_generated_bytes == 0 || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return RUDA_NATIVE_C_EMIT_INVALID_ARGUMENT;
    }
    if (program->function_count > options.max_functions) {
        return RUDA_NATIVE_C_EMIT_LIMIT_REACHED;
    }
    if (!ruda_size_multiply(program->function_count, sizeof(*ordered), &bytes)
        || !ruda_size_multiply(
            program->function_count,
            sizeof(*cycle_stack),
            &cycle_stack_bytes
        )) {
        return RUDA_NATIVE_C_EMIT_LIMIT_REACHED;
    }
    ordered = ruda_allocate(&allocator, bytes);
    states = ruda_allocate(&allocator, program->function_count);
    cycle_stack = ruda_allocate(&allocator, cycle_stack_bytes);
    if (ordered == NULL || states == NULL || cycle_stack == NULL) {
        status = RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED;
        goto cleanup;
    }
    memset(states, 0, program->function_count);
    for (index = 0; index < program->function_count; index += 1) {
        if (program->functions[index] == NULL) {
            status = RUDA_NATIVE_C_EMIT_INVALID_MIR;
            goto cleanup;
        }
        ordered[index] = program->functions[index];
    }
    qsort(
        ordered,
        program->function_count,
        sizeof(*ordered),
        ruda_native_c_function_pointer_compare
    );
    for (index = 0; index < program->function_count; index += 1) {
        if (index != 0 && ruda_native_c_symbol_compare(
                ordered[index - 1]->symbol, ordered[index]->symbol
            ) == 0) {
            status = RUDA_NATIVE_C_EMIT_INVALID_MIR;
            goto cleanup;
        }
        status = ruda_native_c_validate_function(
            ordered[index], ordered, program->function_count
        );
        if (status != RUDA_NATIVE_C_EMIT_OK) {
            goto cleanup;
        }
        if (ruda_native_c_symbol_compare(
                ordered[index]->symbol, program->entry_symbol
            ) == 0) {
            entry = ordered[index];
        }
    }
    if (entry == NULL || entry->parameter_count != 0) {
        status = RUDA_NATIVE_C_EMIT_INVALID_MIR;
        goto cleanup;
    }
    if (ruda_native_c_has_cycle(
            ordered,
            program->function_count,
            states,
            cycle_stack
        )) {
        status = RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL;
        goto cleanup;
    }
    memset(&buffer, 0, sizeof(buffer)); buffer.limit = options.max_generated_bytes;
    if (!ruda_vector_init(&buffer.bytes, allocator, sizeof(char))) { status = RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED; goto cleanup; }
    if (!ruda_native_c_write(&buffer, "#include <stdbool.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <stdio.h>\n#include <string.h>\n#include \"ruda/base/checked_i64.h\"\n\n")
        || !ruda_native_c_emit_runtime(&buffer, program->function_count)) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) {
        if (!ruda_native_c_emit_symbol_and_strings(&buffer, ordered[index])) goto emit_fail;
    }
    if (!ruda_native_c_write(&buffer, "\n")
        || !ruda_native_c_emit_keep_assets(
            &buffer,
            ordered,
            program->function_count
        )) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) if (!ruda_native_c_emit_signature(&buffer, ordered[index], true)) goto emit_fail;
    if (!ruda_native_c_write(&buffer, "\n")) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) if (!ruda_native_c_emit_function(&buffer, ordered[index], ordered, program->function_count)) goto emit_fail;
    if (!ruda_native_c_write(&buffer, "int main(void) { size_t ruda_g_start = 0, ruda_g_end = 0; int ruda_g_s; static ruda_g_frame ruda_g_frames[RUDA_G_MAX_FRAMES]; ruda_g_panic ruda_g_p = {0}; ruda_g_keep_assets(); ruda_g_p.frames = ruda_g_frames; ruda_g_p.frame_capacity = RUDA_G_MAX_FRAMES; ")) goto emit_fail;
    if (entry->result_type == RUDA_TYPE_I64) { if (!ruda_native_c_write(&buffer, "int64_t ruda_g_result; ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); return 1; } printf(\"ok:i64:%%lld\\n\", (long long)ruda_g_result); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == RUDA_TYPE_BOOL) { if (!ruda_native_c_write(&buffer, "bool ruda_g_result; ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); return 1; } printf(\"ok:bool:%%s\\n\", ruda_g_result ? \"true\" : \"false\"); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == RUDA_TYPE_STRING) { if (!ruda_native_c_write(&buffer, "ruda_g_string ruda_g_result; ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_p, &ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); return 1; } printf(\"ok:string:%%zu:\", ruda_g_result.length); ruda_g_print_hex(ruda_g_result.data, ruda_g_result.length); putchar('\\n'); return 0; }\n")) goto emit_fail; }
    else { if (!ruda_native_c_write(&buffer, "ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_p, &ruda_g_start, &ruda_g_end); if (ruda_g_s == 4) { ruda_g_print_panic(&ruda_g_p); return 1; } if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); return 1; } puts(\"ok:void\"); return 0; }\n")) goto emit_fail; }
    if (!ruda_owned_bytes_copy(output, allocator, buffer.bytes.data, buffer.bytes.length)) { status = RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED; goto cleanup_buffer; }
    goto cleanup_buffer;
emit_fail:
    status = buffer.limited ? RUDA_NATIVE_C_EMIT_LIMIT_REACHED : RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED;
cleanup_buffer:
    ruda_vector_dispose(&buffer.bytes);
cleanup:
    ruda_deallocate(&allocator, states, program != NULL ? program->function_count : 0);
    ruda_deallocate(&allocator, cycle_stack, cycle_stack_bytes);
    ruda_deallocate(&allocator, ordered, bytes);
    if (status != RUDA_NATIVE_C_EMIT_OK) ruda_owned_bytes_dispose(output);
    return status;
}
