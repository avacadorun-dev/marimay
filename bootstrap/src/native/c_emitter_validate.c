#include "c_emitter_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ruda_native_c_symbol_compare(RudaSymbolId left, RudaSymbolId right) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes));
}

int ruda_native_c_function_pointer_compare(
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

const RudaMirArray *ruda_native_c_array(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    size_t index = 0;
    if (function == NULL || type < RUDA_TYPE_FIRST_DYNAMIC) {
        return NULL;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const RudaMirArray *array = ruda_vector_at_const(
            &function->arrays,
            index
        );
        if (array != NULL && array->type == type) {
            return array;
        }
    }
    return NULL;
}

RudaNativeCArrayKind ruda_native_c_array_kind(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    const RudaMirArray *array = ruda_native_c_array(function, type);
    const RudaMirArray *element = NULL;
    if (array == NULL) {
        return type >= RUDA_TYPE_FIRST_DYNAMIC
            ? RUDA_NATIVE_C_ARRAY_UNSUPPORTED
            : RUDA_NATIVE_C_ARRAY_NONE;
    }
    if (array->element_type == RUDA_TYPE_F64) {
        return RUDA_NATIVE_C_ARRAY_F64;
    }
    element = ruda_native_c_array(function, array->element_type);
    return element != NULL && element->element_type == RUDA_TYPE_F64
        ? RUDA_NATIVE_C_ARRAY_ARRAY_F64
        : RUDA_NATIVE_C_ARRAY_GENERIC;
}

const char *ruda_native_c_type(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    (void)function;
    if (type == RUDA_TYPE_BOOL) {
        return "bool";
    }
    if (type == RUDA_TYPE_STRING) {
        return "ruda_g_string";
    }
    if (type == RUDA_TYPE_I8) {
        return "int8_t";
    }
    if (type == RUDA_TYPE_I16) {
        return "int16_t";
    }
    if (type == RUDA_TYPE_I32) {
        return "int32_t";
    }
    if (type == RUDA_TYPE_U8) {
        return "uint8_t";
    }
    if (type == RUDA_TYPE_U16) {
        return "uint16_t";
    }
    if (type == RUDA_TYPE_U32) {
        return "uint32_t";
    }
    if (type == RUDA_TYPE_U64) {
        return "uint64_t";
    }
    if (type == RUDA_TYPE_F32) {
        return "uint32_t";
    }
    if (type == RUDA_TYPE_F64) {
        return "uint64_t";
    }
    if (type >= RUDA_TYPE_FIRST_DYNAMIC) {
        return "RudaManagedReference";
    }
    return "int64_t";
}

const char *ruda_native_c_integer_kind_name(RudaTypeId type) {
    switch (type) {
        case RUDA_TYPE_I8: return "RUDA_INTEGER_I8";
        case RUDA_TYPE_I16: return "RUDA_INTEGER_I16";
        case RUDA_TYPE_I32: return "RUDA_INTEGER_I32";
        case RUDA_TYPE_I64: return "RUDA_INTEGER_I64";
        case RUDA_TYPE_U8: return "RUDA_INTEGER_U8";
        case RUDA_TYPE_U16: return "RUDA_INTEGER_U16";
        case RUDA_TYPE_U32: return "RUDA_INTEGER_U32";
        case RUDA_TYPE_U64: return "RUDA_INTEGER_U64";
        default: return NULL;
    }
}

const char *ruda_native_c_integer_conversion_name(RudaTypeId type) {
    switch (type) {
        case RUDA_TYPE_I8: return "ruda_integer_to_i8";
        case RUDA_TYPE_I16: return "ruda_integer_to_i16";
        case RUDA_TYPE_I32: return "ruda_integer_to_i32";
        case RUDA_TYPE_I64: return "ruda_integer_to_i64";
        case RUDA_TYPE_U8: return "ruda_integer_to_u8";
        case RUDA_TYPE_U16: return "ruda_integer_to_u16";
        case RUDA_TYPE_U32: return "ruda_integer_to_u32";
        case RUDA_TYPE_U64: return "ruda_integer_to_u64";
        default: return NULL;
    }
}

bool ruda_native_c_buffer_append(
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

bool ruda_native_c_write(RudaNativeCBuffer *buffer, const char *format, ...) {
    char text[4096];
    va_list arguments;
    int written = 0;
    va_start(arguments, format);
    written = vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    return written >= 0 && (size_t)written < sizeof(text)
        && ruda_native_c_buffer_append(buffer, text, (size_t)written);
}

bool ruda_native_c_symbol(RudaNativeCBuffer *buffer, RudaSymbolId symbol) {
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

bool ruda_native_c_symbol_hex(
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

bool ruda_native_c_symbol_data(
    RudaNativeCBuffer *buffer,
    RudaSymbolId symbol
) {
    return ruda_native_c_write(buffer, "ruda_g_y_")
        && ruda_native_c_symbol_hex(buffer, symbol);
}

bool ruda_native_c_string_data(
    RudaNativeCBuffer *buffer,
    RudaSymbolId symbol,
    size_t index
) {
    return ruda_native_c_write(buffer, "ruda_g_c_")
        && ruda_native_c_symbol_hex(buffer, symbol)
        && ruda_native_c_write(buffer, "_%zu", index);
}

const RudaMirFunction *ruda_native_c_find(
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

const RudaMirCall *ruda_native_c_call(
    const RudaMirFunction *function, RudaMirCallId call
) {
    return function != NULL ? ruda_vector_at_const(&function->calls, call) : NULL;
}

const RudaMirCallArgument *ruda_native_c_call_argument(
    const RudaMirFunction *function, size_t index
) {
    return function != NULL
        ? ruda_vector_at_const(&function->call_arguments, index)
        : NULL;
}

const RudaMirArrayConstruction *ruda_native_c_array_construction(
    const RudaMirFunction *function,
    RudaMirArrayConstructionId construction
) {
    return function != NULL
        ? ruda_vector_at_const(&function->array_constructions, construction)
        : NULL;
}

const RudaMirArrayArgument *ruda_native_c_array_argument(
    const RudaMirFunction *function,
    size_t index
) {
    return function != NULL
        ? ruda_vector_at_const(&function->array_arguments, index)
        : NULL;
}

const char *ruda_native_c_array_descriptor(
    const RudaMirFunction *function,
    RudaTypeId type
) {
    switch (ruda_native_c_array_kind(function, type)) {
        case RUDA_NATIVE_C_ARRAY_F64:
            return "&ruda_managed_array_f64_descriptor";
        case RUDA_NATIVE_C_ARRAY_ARRAY_F64:
            return "&ruda_managed_array_array_f64_descriptor";
        case RUDA_NATIVE_C_ARRAY_GENERIC: {
            static char name[64];
            (void)snprintf(
                name, sizeof(name), "&ruda_g_ad_%u", (unsigned int)type
            );
            return name;
        }
        case RUDA_NATIVE_C_ARRAY_NONE:
        case RUDA_NATIVE_C_ARRAY_UNSUPPORTED:
            return "NULL";
    }
    return "NULL";
}

size_t ruda_native_c_managed_register_count(
    const RudaMirFunction *function
) {
    size_t index = 0;
    size_t result = 0;
    if (function == NULL) {
        return 0;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL
            && (*type == RUDA_TYPE_STRING
                || *type >= RUDA_TYPE_FIRST_DYNAMIC)) {
            result += 1;
        }
    }
    return result;
}

bool ruda_native_c_call_supported(
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
        || ruda_native_c_array_kind(function, call->result_type)
            != ruda_native_c_array_kind(callee, callee->result_type)
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
        if (argument == NULL || parameter == NULL || argument->type != *parameter
            || ruda_native_c_array_kind(function, argument->type)
                != ruda_native_c_array_kind(callee, *parameter)) {
            return false;
        }
    }
    return true;
}

bool ruda_native_c_interface_call_supported(
    const RudaMirFunction *function,
    RudaMirCallId call_id,
    const RudaMirFunction *const *functions,
    size_t count,
    RudaTypeId required_result
) {
    const RudaMirCall *call = ruda_native_c_call(function, call_id);
    const RudaMirCallArgument *receiver = call != NULL
        ? ruda_native_c_call_argument(function, call->argument_start)
        : NULL;
    RudaNativeCManagedLayout interface_layout;
    const RudaMirInterface *interface = NULL;
    size_t function_index = 0;
    bool found_construction = false;
    if (call == NULL || receiver == NULL || call->argument_count == 0
        || call->interface_slot == UINT32_MAX
        || call->result_type != required_result
        || !ruda_native_c_find_managed_layout(
            functions, count, receiver->type, &interface_layout
        ) || interface_layout.kind != RUDA_NATIVE_C_MANAGED_INTERFACE) {
        return false;
    }
    interface = ruda_vector_at_const(
        &interface_layout.function->interfaces, interface_layout.index
    );
    if (interface == NULL || call->interface_slot >= interface->method_count) {
        return false;
    }
    for (function_index = 0; function_index < count; function_index += 1) {
        const RudaMirFunction *owner = functions[function_index];
        size_t construction_index = 0;
        for (construction_index = 0;
            construction_index < owner->interface_constructions.length;
            construction_index += 1) {
            const RudaMirInterfaceConstruction *construction =
                ruda_vector_at_const(
                    &owner->interface_constructions, construction_index
                );
            const RudaMirInterface *constructed = construction != NULL
                ? ruda_vector_at_const(
                    &owner->interfaces, construction->interface_id
                ) : NULL;
            const RudaSymbolId *method = construction != NULL
                && call->interface_slot < construction->method_count
                ? ruda_vector_at_const(
                    &owner->interface_methods,
                    construction->method_start + call->interface_slot
                ) : NULL;
            const RudaMirFunction *callee = method != NULL
                ? ruda_native_c_find(functions, count, *method) : NULL;
            size_t argument_index = 0;
            if (constructed == NULL || constructed->type != receiver->type) {
                continue;
            }
            found_construction = true;
            if (callee == NULL || callee->result_type != call->result_type
                || callee->parameter_count != call->argument_count) {
                return false;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const RudaMirCallArgument *argument =
                    ruda_native_c_call_argument(
                        function, call->argument_start + argument_index
                    );
                const RudaTypeId *parameter = ruda_vector_at_const(
                    &callee->register_types, argument_index
                );
                RudaTypeId expected = argument_index == 0
                    ? construction->payload_type
                    : (argument != NULL ? argument->type : RUDA_TYPE_INVALID);
                if (parameter == NULL || *parameter != expected) {
                    return false;
                }
            }
        }
    }
    return found_construction;
}

RudaNativeCEmitStatus ruda_native_c_validate_function(
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
    if (!ruda_native_c_program_type_supported(
            functions, count, function->result_type, true
        )) {
        return RUDA_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaTypeId *type = ruda_vector_at_const(&function->register_types, index);
        if (type == NULL || !ruda_native_c_program_type_supported(
                functions, count, *type, false
            )) {
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
                case RUDA_MIR_CONSTANT_I64: case RUDA_MIR_CONSTANT_INTEGER:
                case RUDA_MIR_CONSTANT_BOOL:
                case RUDA_MIR_CONSTANT_F64:
                case RUDA_MIR_COPY: case RUDA_MIR_ADD_CHECKED_I64:
                case RUDA_MIR_ADD_PROVEN_I64:
                case RUDA_MIR_SUBTRACT_CHECKED_I64: case RUDA_MIR_MULTIPLY_CHECKED_I64:
                case RUDA_MIR_SUBTRACT_PROVEN_I64:
                case RUDA_MIR_MULTIPLY_PROVEN_I64:
                case RUDA_MIR_DIVIDE_CHECKED_I64: case RUDA_MIR_REMAINDER_CHECKED_I64:
                case RUDA_MIR_NEGATE_CHECKED_I64: case RUDA_MIR_NOT_BOOL:
                case RUDA_MIR_BITWISE_NOT_I64: case RUDA_MIR_BITWISE_AND_I64:
                case RUDA_MIR_BITWISE_XOR_I64: case RUDA_MIR_BITWISE_OR_I64:
                case RUDA_MIR_SHIFT_LEFT_CHECKED_I64: case RUDA_MIR_SHIFT_RIGHT_I64:
                case RUDA_MIR_EQUAL_I64: case RUDA_MIR_EQUAL_BOOL:
                case RUDA_MIR_CONSTANT_STRING: case RUDA_MIR_EQUAL_STRING:
                case RUDA_MIR_LESS_I64: case RUDA_MIR_LESS_EQUAL_I64:
                case RUDA_MIR_ADD_F64: case RUDA_MIR_SUBTRACT_F64:
                case RUDA_MIR_MULTIPLY_F64: case RUDA_MIR_DIVIDE_F64:
                case RUDA_MIR_NEGATE_F64: case RUDA_MIR_EQUAL_F64:
                case RUDA_MIR_LESS_F64: case RUDA_MIR_LESS_EQUAL_F64:
                case RUDA_MIR_ARRAY_NEW: case RUDA_MIR_ARRAY_LENGTH:
                case RUDA_MIR_ARRAY_REQUIRE_INDEX:
                case RUDA_MIR_ARRAY_ASSUME_INDEX: case RUDA_MIR_ARRAY_GET:
                case RUDA_MIR_ARRAY_SET: case RUDA_MIR_ARRAY_GET_IN_BOUNDS:
                case RUDA_MIR_ARRAY_SET_IN_BOUNDS:
                case RUDA_MIR_ARRAY_PUSH:
                case RUDA_MIR_ARRAY_POP:
                case RUDA_MIR_ARRAY_TRUNCATE:
                case RUDA_MIR_ARRAY_CLEAR:
                case RUDA_MIR_ARRAY_RESERVE:
                case RUDA_MIR_ARRAY_GENERATION:
                case RUDA_MIR_ARRAY_REQUIRE_GENERATION:
                case RUDA_MIR_RECORD_NEW: case RUDA_MIR_RECORD_GET:
                case RUDA_MIR_RECORD_SET: case RUDA_MIR_RECORD_IDENTITY_SAME:
                case RUDA_MIR_ENUM_NEW: case RUDA_MIR_ENUM_IS_VARIANT:
                case RUDA_MIR_ENUM_REQUIRE_VARIANT:
                case RUDA_MIR_ENUM_GET_PAYLOAD: case RUDA_MIR_INTERFACE_NEW:
                case RUDA_MIR_INTEGER_POLICY:
                case RUDA_MIR_INTEGER_CONVERT:
                case RUDA_MIR_FORMAT_F32:
                case RUDA_MIR_FORMAT_F64:
                case RUDA_MIR_IO_PRINTLN:
                case RUDA_MIR_TEST_OUTPUT_BEGIN:
                case RUDA_MIR_TEST_OUTPUT_END:
                    break;
                case RUDA_MIR_DEFER_REGISTER:
                case RUDA_MIR_DEFER_CALL: {
                    const RudaMirCall *call = ruda_native_c_call(
                        function, instruction->call
                    );
                    bool supported = call != NULL
                        && call->result_type == RUDA_TYPE_VOID;
                    if (supported && call->interface_slot == UINT32_MAX) {
                        supported = ruda_native_c_call_supported(
                            function, instruction->call, functions, count,
                            RUDA_TYPE_VOID
                        );
                    } else if (supported) {
                        supported = ruda_native_c_interface_call_supported(
                            function, instruction->call, functions, count,
                            RUDA_TYPE_VOID
                        );
                    }
                    if (!supported) {
                        return RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL;
                    }
                    break;
                }
                case RUDA_MIR_CALL: {
                    const RudaMirCall *call = ruda_native_c_call(function, instruction->call);
                    if (call == NULL
                        || !ruda_native_c_program_type_supported(
                            functions, count, call->result_type, false
                        )
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
                case RUDA_MIR_CALL_INTERFACE: {
                    const RudaMirCall *call = ruda_native_c_call(
                        function, instruction->call
                    );
                    if (call == NULL || !ruda_native_c_interface_call_supported(
                            function, instruction->call, functions, count,
                            call->result_type
                        )) return RUDA_NATIVE_C_EMIT_UNSUPPORTED_CALL;
                    break;
                }
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

bool ruda_native_c_function_index(
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
bool ruda_native_c_has_cycle(
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
                        && (instruction->kind == RUDA_MIR_CALL
                            || instruction->kind == RUDA_MIR_DEFER_CALL)) {
                        const RudaMirCall *candidate = ruda_native_c_call(
                            function, instruction->call
                        );
                        if (candidate == NULL) {
                            return true;
                        }
                        if (candidate->interface_slot == UINT32_MAX) {
                            call_id = instruction->call;
                        }
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
