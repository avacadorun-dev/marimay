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
        || (result && type == RUDA_TYPE_VOID);
}

static const char *ruda_native_c_type(RudaTypeId type) {
    return type == RUDA_TYPE_BOOL ? "bool" : "int64_t";
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
    if (function->string_constants.length != 0) {
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
                case RUDA_MIR_LESS_I64: case RUDA_MIR_LESS_EQUAL_I64:
                    break;
                case RUDA_MIR_CALL: {
                    const RudaMirCall *call = ruda_native_c_call(function, instruction->call);
                    const RudaMirFunction *callee = call != NULL
                        ? ruda_native_c_find(functions, count, call->callee) : NULL;
                    if (callee == NULL || !ruda_native_c_type_supported(call->result_type, false)
                        || callee->result_type != call->result_type
                        || call->argument_count != callee->parameter_count
                        || call->result_type == RUDA_TYPE_VOID) {
                        return RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL;
                    }
                    {
                        size_t argument_index = 0;
                        for (argument_index = 0;
                            argument_index < call->argument_count;
                            argument_index += 1) {
                            const RudaMirCallArgument *argument =
                                ruda_native_c_call_argument(
                                    function,
                                    call->argument_start + argument_index
                                );
                            const RudaTypeId *parameter = ruda_vector_at_const(
                                &callee->register_types, argument_index
                            );
                            if (argument == NULL || parameter == NULL
                                || argument->type != *parameter) {
                                return RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL;
                            }
                        }
                    }
                    break;
                }
                case RUDA_MIR_CONSTANT_STRING: case RUDA_MIR_EQUAL_STRING:
                case RUDA_MIR_RECORD_NEW: case RUDA_MIR_RECORD_GET:
                case RUDA_MIR_RECORD_SET: case RUDA_MIR_RECORD_IDENTITY_SAME:
                case RUDA_MIR_ENUM_NEW: case RUDA_MIR_ENUM_IS_VARIANT:
                case RUDA_MIR_ENUM_REQUIRE_VARIANT: case RUDA_MIR_ENUM_GET_PAYLOAD:
                    return RUDA_NATIVE_C_EMIT_UNSUPPORTED_INSTRUCTION;
            }
        }
        if (block->terminator.kind != RUDA_MIR_RETURN
            && block->terminator.kind != RUDA_MIR_GOTO
            && block->terminator.kind != RUDA_MIR_BRANCH) {
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
            if (block == NULL || frame->instruction_index >= block->instructions.length) {
                frame->block_index += 1;
                frame->instruction_index = 0;
                continue;
            }
            {
                const RudaMirInstruction *instruction = ruda_vector_at_const(
                    &block->instructions, frame->instruction_index
                );
                frame->instruction_index += 1;
                if (instruction != NULL && instruction->kind == RUDA_MIR_CALL) {
                    const RudaMirCall *call = ruda_native_c_call(
                        function, instruction->call
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
        || !ruda_native_c_write(buffer, "(")) return false;
    for (index = 0; index < function->parameter_count; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(&function->register_types, index);
        if ((index != 0 && !ruda_native_c_write(buffer, ", "))
            || !ruda_native_c_write(buffer, "%s ruda_g_v_%zu", ruda_native_c_type(*type), index)) return false;
    }
    if (function->result_type != RUDA_TYPE_VOID) {
        if ((function->parameter_count != 0 && !ruda_native_c_write(buffer, ", "))
            || !ruda_native_c_write(buffer, "%s *ruda_g_out", ruda_native_c_type(function->result_type))) return false;
    }
    if ((function->parameter_count != 0 || function->result_type != RUDA_TYPE_VOID)
        && !ruda_native_c_write(buffer, ", ")) return false;
    return ruda_native_c_write(buffer,
        "size_t *ruda_g_start, size_t *ruda_g_end)%s", prototype ? ";\n" : " {\n"
    );
}

static bool ruda_native_c_emit_function(
    RudaNativeCBuffer *buffer, const RudaMirFunction *function,
    const RudaMirFunction *const *functions, size_t count
) {
    size_t index = 0;
    if (!ruda_native_c_emit_signature(buffer, function, false)
        || !ruda_native_c_write(
            buffer, "  int ruda_g_s;\n  (void)ruda_g_s;\n"
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
                case RUDA_MIR_COPY: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u;\n", i->target, i->first)) return false; break;
                case RUDA_MIR_NOT_BOOL: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = !ruda_g_v_%u;\n", i->target, i->first)) return false; break;
                case RUDA_MIR_EQUAL_I64: case RUDA_MIR_EQUAL_BOOL: if (!ruda_native_c_write(buffer, "  ruda_g_v_%u = ruda_g_v_%u == ruda_g_v_%u;\n", i->target, i->first, i->second)) return false; break;
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
                    size_t argument_index = 0;
                    if (!ruda_native_c_write(buffer, "  ruda_g_s = ") || !ruda_native_c_symbol(buffer, callee->symbol) || !ruda_native_c_write(buffer, "(")) return false;
                    for (argument_index = 0; argument_index < call->argument_count; argument_index += 1) {
                        const RudaMirCallArgument *argument = ruda_native_c_call_argument(function, call->argument_start + argument_index);
                        if ((argument_index != 0 && !ruda_native_c_write(buffer, ", ")) || !ruda_native_c_write(buffer, "ruda_g_v_%u", argument->register_id)) return false;
                    }
                    if (call->argument_count != 0 && !ruda_native_c_write(buffer, ", ")) return false;
                    if (!ruda_native_c_write(buffer, "&ruda_g_v_%u, ruda_g_start, ruda_g_end);\n  if (ruda_g_s != 0) return ruda_g_s;\n", i->target)) return false;
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
        } else if (function->result_type == RUDA_TYPE_VOID) {
            if (!ruda_native_c_write(buffer, "  return 0;\n")) return false;
        } else if (!ruda_native_c_write(buffer, "  *ruda_g_out = ruda_g_v_%u; return 0;\n", block->terminator.value)) return false;
    }
    return ruda_native_c_write(buffer, "}\n\n");
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
    if (!ruda_native_c_write(&buffer, "#include <stdbool.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <stdio.h>\n#include \"ruda/base/checked_i64.h\"\n\n")) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) if (!ruda_native_c_emit_signature(&buffer, ordered[index], true)) goto emit_fail;
    if (!ruda_native_c_write(&buffer, "\n")) goto emit_fail;
    for (index = 0; index < program->function_count; index += 1) if (!ruda_native_c_emit_function(&buffer, ordered[index], ordered, program->function_count)) goto emit_fail;
    if (!ruda_native_c_write(&buffer, "int main(void) { size_t ruda_g_start = 0, ruda_g_end = 0; int ruda_g_s; ")) goto emit_fail;
    if (entry->result_type == RUDA_TYPE_I64) { if (!ruda_native_c_write(&buffer, "int64_t ruda_g_result; ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); return 1; } printf(\"ok:i64:%%lld\\n\", (long long)ruda_g_result); return 0; }\n")) goto emit_fail; }
    else if (entry->result_type == RUDA_TYPE_BOOL) { if (!ruda_native_c_write(&buffer, "bool ruda_g_result; ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_result, &ruda_g_start, &ruda_g_end); if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); return 1; } printf(\"ok:bool:%%s\\n\", ruda_g_result ? \"true\" : \"false\"); return 0; }\n")) goto emit_fail; }
    else { if (!ruda_native_c_write(&buffer, "ruda_g_s = ") || !ruda_native_c_symbol(&buffer, entry->symbol) || !ruda_native_c_write(&buffer, "(&ruda_g_start, &ruda_g_end); if (ruda_g_s) { printf(\"trap:%%d:%%zu:%%zu\\n\", ruda_g_s, ruda_g_start, ruda_g_end); return 1; } puts(\"ok:void\"); return 0; }\n")) goto emit_fail; }
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
