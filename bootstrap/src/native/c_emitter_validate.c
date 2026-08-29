#include "c_emitter_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mmay_native_c_symbol_compare(MarimaySymbolId left, MarimaySymbolId right) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes));
}

int mmay_native_c_function_pointer_compare(
    const void *left,
    const void *right
) {
    const MarimayMirFunction *const *left_function = left;
    const MarimayMirFunction *const *right_function = right;
    return mmay_native_c_symbol_compare(
        (*left_function)->symbol,
        (*right_function)->symbol
    );
}

const MarimayMirArray *mmay_native_c_array(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    if (function == NULL || type < MMAY_TYPE_FIRST_DYNAMIC) {
        return NULL;
    }
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *array = mmay_vector_at_const(
            &function->arrays,
            index
        );
        if (array != NULL && array->type == type) {
            return array;
        }
    }
    return NULL;
}

MarimayNativeCArrayKind mmay_native_c_array_kind(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    const MarimayMirArray *array = mmay_native_c_array(function, type);
    const MarimayMirArray *element = NULL;
    if (array == NULL) {
        return type >= MMAY_TYPE_FIRST_DYNAMIC
            ? MMAY_NATIVE_C_ARRAY_UNSUPPORTED
            : MMAY_NATIVE_C_ARRAY_NONE;
    }
    if (array->element_type == MMAY_TYPE_F64) {
        return MMAY_NATIVE_C_ARRAY_F64;
    }
    element = mmay_native_c_array(function, array->element_type);
    return element != NULL && element->element_type == MMAY_TYPE_F64
        ? MMAY_NATIVE_C_ARRAY_ARRAY_F64
        : MMAY_NATIVE_C_ARRAY_GENERIC;
}

const char *mmay_native_c_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    (void)function;
    if (type == MMAY_TYPE_BOOL) {
        return "bool";
    }
    if (type == MMAY_TYPE_STRING) {
        return "mmay_g_string";
    }
    if (type == MMAY_TYPE_I8) {
        return "int8_t";
    }
    if (type == MMAY_TYPE_I16) {
        return "int16_t";
    }
    if (type == MMAY_TYPE_I32) {
        return "int32_t";
    }
    if (type == MMAY_TYPE_U8) {
        return "uint8_t";
    }
    if (type == MMAY_TYPE_U16) {
        return "uint16_t";
    }
    if (type == MMAY_TYPE_U32 || type == MMAY_TYPE_RUNE) {
        return "uint32_t";
    }
    if (type == MMAY_TYPE_U64) {
        return "uint64_t";
    }
    if (type == MMAY_TYPE_F32) {
        return "uint32_t";
    }
    if (type == MMAY_TYPE_F64) {
        return "uint64_t";
    }
    if (type >= MMAY_TYPE_FIRST_DYNAMIC) {
        return "MarimayManagedReference";
    }
    return "int64_t";
}

const char *mmay_native_c_integer_kind_name(MarimayTypeId type) {
    switch (type) {
        case MMAY_TYPE_I8: return "MMAY_INTEGER_I8";
        case MMAY_TYPE_I16: return "MMAY_INTEGER_I16";
        case MMAY_TYPE_I32: return "MMAY_INTEGER_I32";
        case MMAY_TYPE_I64: return "MMAY_INTEGER_I64";
        case MMAY_TYPE_U8: return "MMAY_INTEGER_U8";
        case MMAY_TYPE_U16: return "MMAY_INTEGER_U16";
        case MMAY_TYPE_U32: return "MMAY_INTEGER_U32";
        case MMAY_TYPE_U64: return "MMAY_INTEGER_U64";
        default: return NULL;
    }
}

const char *mmay_native_c_integer_conversion_name(MarimayTypeId type) {
    switch (type) {
        case MMAY_TYPE_I8: return "mmay_integer_to_i8";
        case MMAY_TYPE_I16: return "mmay_integer_to_i16";
        case MMAY_TYPE_I32: return "mmay_integer_to_i32";
        case MMAY_TYPE_I64: return "mmay_integer_to_i64";
        case MMAY_TYPE_U8: return "mmay_integer_to_u8";
        case MMAY_TYPE_U16: return "mmay_integer_to_u16";
        case MMAY_TYPE_U32: return "mmay_integer_to_u32";
        case MMAY_TYPE_U64: return "mmay_integer_to_u64";
        default: return NULL;
    }
}

bool mmay_native_c_buffer_append(
    MarimayNativeCBuffer *buffer, const char *text, size_t length
) {
    size_t total = 0;
    if (buffer == NULL || (text == NULL && length != 0)
        || !mmay_size_add(buffer->bytes.length, length, &total)) {
        return false;
    }
    if (total > buffer->limit) {
        buffer->limited = true;
        return false;
    }
    return mmay_vector_append(&buffer->bytes, text, length);
}

bool mmay_native_c_write(MarimayNativeCBuffer *buffer, const char *format, ...) {
    char text[4096];
    va_list arguments;
    int written = 0;
    va_start(arguments, format);
    written = vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    return written >= 0 && (size_t)written < sizeof(text)
        && mmay_native_c_buffer_append(buffer, text, (size_t)written);
}

bool mmay_native_c_symbol(MarimayNativeCBuffer *buffer, MarimaySymbolId symbol) {
    size_t index = 0;
    if (!mmay_native_c_write(buffer, "mmay_g_f_")) {
        return false;
    }
    for (index = 0; index < sizeof(symbol.bytes); index += 1) {
        if (!mmay_native_c_write(buffer, "%02x", (unsigned int)symbol.bytes[index])) {
            return false;
        }
    }
    return true;
}

bool mmay_native_c_symbol_hex(
    MarimayNativeCBuffer *buffer,
    MarimaySymbolId symbol
) {
    size_t index = 0;
    for (index = 0; index < sizeof(symbol.bytes); index += 1) {
        if (!mmay_native_c_write(
                buffer,
                "%02x",
                (unsigned int)symbol.bytes[index]
            )) {
            return false;
        }
    }
    return true;
}

bool mmay_native_c_symbol_data(
    MarimayNativeCBuffer *buffer,
    MarimaySymbolId symbol
) {
    return mmay_native_c_write(buffer, "mmay_g_y_")
        && mmay_native_c_symbol_hex(buffer, symbol);
}

bool mmay_native_c_string_data(
    MarimayNativeCBuffer *buffer,
    MarimaySymbolId symbol,
    size_t index
) {
    return mmay_native_c_write(buffer, "mmay_g_c_")
        && mmay_native_c_symbol_hex(buffer, symbol)
        && mmay_native_c_write(buffer, "_%zu", index);
}

const MarimayMirFunction *mmay_native_c_find(
    const MarimayMirFunction *const *functions, size_t count, MarimaySymbolId symbol
) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int comparison = mmay_native_c_symbol_compare(functions[middle]->symbol, symbol);
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

const MarimayMirCall *mmay_native_c_call(
    const MarimayMirFunction *function, MarimayMirCallId call
) {
    return function != NULL ? mmay_vector_at_const(&function->calls, call) : NULL;
}

const MarimayMirCallArgument *mmay_native_c_call_argument(
    const MarimayMirFunction *function, size_t index
) {
    return function != NULL
        ? mmay_vector_at_const(&function->call_arguments, index)
        : NULL;
}

const MarimayMirArrayConstruction *mmay_native_c_array_construction(
    const MarimayMirFunction *function,
    MarimayMirArrayConstructionId construction
) {
    return function != NULL
        ? mmay_vector_at_const(&function->array_constructions, construction)
        : NULL;
}

const MarimayMirArrayArgument *mmay_native_c_array_argument(
    const MarimayMirFunction *function,
    size_t index
) {
    return function != NULL
        ? mmay_vector_at_const(&function->array_arguments, index)
        : NULL;
}

const char *mmay_native_c_array_descriptor(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    switch (mmay_native_c_array_kind(function, type)) {
        case MMAY_NATIVE_C_ARRAY_F64:
            return "&mmay_managed_array_f64_descriptor";
        case MMAY_NATIVE_C_ARRAY_ARRAY_F64:
            return "&mmay_managed_array_array_f64_descriptor";
        case MMAY_NATIVE_C_ARRAY_GENERIC: {
            static char name[64];
            (void)snprintf(
                name, sizeof(name), "&mmay_g_ad_%u", (unsigned int)type
            );
            return name;
        }
        case MMAY_NATIVE_C_ARRAY_NONE:
        case MMAY_NATIVE_C_ARRAY_UNSUPPORTED:
            return "NULL";
    }
    return "NULL";
}

size_t mmay_native_c_managed_register_count(
    const MarimayMirFunction *function
) {
    size_t index = 0;
    size_t result = 0;
    if (function == NULL) {
        return 0;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(
            &function->register_types,
            index
        );
        if (type != NULL
            && (*type == MMAY_TYPE_STRING
                || *type >= MMAY_TYPE_FIRST_DYNAMIC)) {
            result += 1;
        }
    }
    return result;
}

bool mmay_native_c_call_supported(
    const MarimayMirFunction *function,
    MarimayMirCallId call_id,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayTypeId required_result
) {
    const MarimayMirCall *call = mmay_native_c_call(function, call_id);
    const MarimayMirFunction *callee = call != NULL
        ? mmay_native_c_find(functions, count, call->callee)
        : NULL;
    size_t argument_index = 0;
    if (callee == NULL || call->result_type != required_result
        || callee->result_type != call->result_type
        || mmay_native_c_array_kind(function, call->result_type)
            != mmay_native_c_array_kind(callee, callee->result_type)
        || call->argument_count != callee->parameter_count) {
        return false;
    }
    for (argument_index = 0;
        argument_index < call->argument_count;
        argument_index += 1) {
        const MarimayMirCallArgument *argument = mmay_native_c_call_argument(
            function,
            call->argument_start + argument_index
        );
        const MarimayTypeId *parameter = mmay_vector_at_const(
            &callee->register_types,
            argument_index
        );
        if (argument == NULL || parameter == NULL || argument->type != *parameter
            || mmay_native_c_array_kind(function, argument->type)
                != mmay_native_c_array_kind(callee, *parameter)) {
            return false;
        }
    }
    return true;
}

bool mmay_native_c_interface_call_supported(
    const MarimayMirFunction *function,
    MarimayMirCallId call_id,
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimayTypeId required_result
) {
    const MarimayMirCall *call = mmay_native_c_call(function, call_id);
    const MarimayMirCallArgument *receiver = call != NULL
        ? mmay_native_c_call_argument(function, call->argument_start)
        : NULL;
    MarimayNativeCManagedLayout interface_layout;
    const MarimayMirInterface *interface = NULL;
    size_t function_index = 0;
    bool found_construction = false;
    if (call == NULL || receiver == NULL || call->argument_count == 0
        || call->interface_slot == UINT32_MAX
        || call->result_type != required_result
        || !mmay_native_c_find_managed_layout(
            functions, count, receiver->type, &interface_layout
        ) || interface_layout.kind != MMAY_NATIVE_C_MANAGED_INTERFACE) {
        return false;
    }
    interface = mmay_vector_at_const(
        &interface_layout.function->interfaces, interface_layout.index
    );
    if (interface == NULL || call->interface_slot >= interface->method_count) {
        return false;
    }
    for (function_index = 0; function_index < count; function_index += 1) {
        const MarimayMirFunction *owner = functions[function_index];
        size_t construction_index = 0;
        for (construction_index = 0;
            construction_index < owner->interface_constructions.length;
            construction_index += 1) {
            const MarimayMirInterfaceConstruction *construction =
                mmay_vector_at_const(
                    &owner->interface_constructions, construction_index
                );
            const MarimayMirInterface *constructed = construction != NULL
                ? mmay_vector_at_const(
                    &owner->interfaces, construction->interface_id
                ) : NULL;
            const MarimaySymbolId *method = construction != NULL
                && call->interface_slot < construction->method_count
                ? mmay_vector_at_const(
                    &owner->interface_methods,
                    construction->method_start + call->interface_slot
                ) : NULL;
            const MarimayMirFunction *callee = method != NULL
                ? mmay_native_c_find(functions, count, *method) : NULL;
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
                const MarimayMirCallArgument *argument =
                    mmay_native_c_call_argument(
                        function, call->argument_start + argument_index
                    );
                const MarimayTypeId *parameter = mmay_vector_at_const(
                    &callee->register_types, argument_index
                );
                MarimayTypeId expected = argument_index == 0
                    ? construction->payload_type
                    : (argument != NULL ? argument->type : MMAY_TYPE_INVALID);
                if (parameter == NULL || *parameter != expected) {
                    return false;
                }
            }
        }
    }
    return found_construction;
}

MarimayNativeCEmitStatus mmay_native_c_validate_function(
    const MarimayMirFunction *function,
    const MarimayMirFunction *const *functions,
    size_t count
) {
    size_t index = 0;
    if (function == NULL || !function->initialized) {
        return MMAY_NATIVE_C_EMIT_INVALID_MIR;
    }
    MarimayMirVerifyStatus verify_status = mmay_mir_verify(function);
    if (verify_status == MMAY_MIR_VERIFY_ALLOCATION_FAILED) {
        return MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED;
    }
    if (verify_status == MMAY_MIR_VERIFY_LIMIT_REACHED) {
        return MMAY_NATIVE_C_EMIT_LIMIT_REACHED;
    }
    if (verify_status != MMAY_MIR_VERIFY_OK) {
        return MMAY_NATIVE_C_EMIT_INVALID_MIR;
    }
    if (!mmay_native_c_program_type_supported(
            functions, count, function->result_type, true
        )) {
        return MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayTypeId *type = mmay_vector_at_const(&function->register_types, index);
        if (type == NULL || !mmay_native_c_program_type_supported(
                functions, count, *type, false
            )) {
            return MMAY_NATIVE_C_EMIT_UNSUPPORTED_TYPE;
        }
    }
    for (index = 0; index < function->blocks.length; index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(&function->blocks, index);
        size_t instruction_index = 0;
        if (block == NULL) {
            return MMAY_NATIVE_C_EMIT_INVALID_MIR;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction == NULL) {
                return MMAY_NATIVE_C_EMIT_INVALID_MIR;
            }
            switch (instruction->kind) {
                case MMAY_MIR_CONSTANT_I64: case MMAY_MIR_CONSTANT_INTEGER:
                case MMAY_MIR_CONSTANT_BOOL:
                case MMAY_MIR_CONSTANT_F64:
                case MMAY_MIR_COPY: case MMAY_MIR_ADD_CHECKED_I64:
                case MMAY_MIR_ADD_PROVEN_I64:
                case MMAY_MIR_SUBTRACT_CHECKED_I64: case MMAY_MIR_MULTIPLY_CHECKED_I64:
                case MMAY_MIR_SUBTRACT_PROVEN_I64:
                case MMAY_MIR_MULTIPLY_PROVEN_I64:
                case MMAY_MIR_DIVIDE_CHECKED_I64: case MMAY_MIR_REMAINDER_CHECKED_I64:
                case MMAY_MIR_NEGATE_CHECKED_I64: case MMAY_MIR_NOT_BOOL:
                case MMAY_MIR_BITWISE_NOT_I64: case MMAY_MIR_BITWISE_AND_I64:
                case MMAY_MIR_BITWISE_XOR_I64: case MMAY_MIR_BITWISE_OR_I64:
                case MMAY_MIR_SHIFT_LEFT_CHECKED_I64: case MMAY_MIR_SHIFT_RIGHT_I64:
                case MMAY_MIR_EQUAL_I64: case MMAY_MIR_EQUAL_BOOL:
                case MMAY_MIR_CONSTANT_STRING: case MMAY_MIR_EQUAL_STRING:
                case MMAY_MIR_LESS_I64: case MMAY_MIR_LESS_EQUAL_I64:
                case MMAY_MIR_ADD_F64: case MMAY_MIR_SUBTRACT_F64:
                case MMAY_MIR_MULTIPLY_F64: case MMAY_MIR_DIVIDE_F64:
                case MMAY_MIR_NEGATE_F64: case MMAY_MIR_EQUAL_F64:
                case MMAY_MIR_LESS_F64: case MMAY_MIR_LESS_EQUAL_F64:
                case MMAY_MIR_ARRAY_NEW: case MMAY_MIR_ARRAY_LENGTH:
                case MMAY_MIR_ARRAY_REQUIRE_INDEX:
                case MMAY_MIR_ARRAY_ASSUME_INDEX: case MMAY_MIR_ARRAY_GET:
                case MMAY_MIR_ARRAY_SET: case MMAY_MIR_ARRAY_GET_IN_BOUNDS:
                case MMAY_MIR_ARRAY_SET_IN_BOUNDS:
                case MMAY_MIR_ARRAY_PUSH:
                case MMAY_MIR_ARRAY_POP:
                case MMAY_MIR_ARRAY_TRUNCATE:
                case MMAY_MIR_ARRAY_CLEAR:
                case MMAY_MIR_ARRAY_RESERVE:
                case MMAY_MIR_ARRAY_GENERATION:
                case MMAY_MIR_ARRAY_REQUIRE_GENERATION:
                case MMAY_MIR_RECORD_NEW: case MMAY_MIR_RECORD_GET:
                case MMAY_MIR_RECORD_SET: case MMAY_MIR_RECORD_IDENTITY_SAME:
                case MMAY_MIR_ENUM_NEW: case MMAY_MIR_ENUM_IS_VARIANT:
                case MMAY_MIR_ENUM_REQUIRE_VARIANT:
                case MMAY_MIR_ENUM_GET_PAYLOAD: case MMAY_MIR_INTERFACE_NEW:
                case MMAY_MIR_INTEGER_POLICY:
                case MMAY_MIR_INTEGER_CONVERT:
                case MMAY_MIR_FORMAT_F32:
                case MMAY_MIR_FORMAT_F64:
                case MMAY_MIR_FORMAT_BOOL:
                case MMAY_MIR_FORMAT_INTEGER:
                case MMAY_MIR_STRING_CONCAT:
                case MMAY_MIR_PARSE_INTEGER:
                case MMAY_MIR_TEXT_UNARY:
                case MMAY_MIR_TEXT_BINARY:
                case MMAY_MIR_PATH:
                case MMAY_MIR_FS_UNARY:
                case MMAY_MIR_FS_BINARY:
                case MMAY_MIR_ENV_READ:
                case MMAY_MIR_PROCESS_CONTEXT:
                case MMAY_MIR_IO_PRINTLN:
                case MMAY_MIR_IO_WRITE:
                case MMAY_MIR_IO_FLUSH:
                case MMAY_MIR_IO_READ_LINE:
                case MMAY_MIR_IO_READ_BYTES:
                case MMAY_MIR_IO_WRITE_BYTES:
                case MMAY_MIR_TEST_OUTPUT_BEGIN:
                case MMAY_MIR_TEST_OUTPUT_END:
                    break;
                case MMAY_MIR_DEFER_REGISTER:
                case MMAY_MIR_DEFER_CALL: {
                    const MarimayMirCall *call = mmay_native_c_call(
                        function, instruction->call
                    );
                    bool supported = call != NULL
                        && call->result_type == MMAY_TYPE_VOID;
                    if (supported && call->interface_slot == UINT32_MAX) {
                        supported = mmay_native_c_call_supported(
                            function, instruction->call, functions, count,
                            MMAY_TYPE_VOID
                        );
                    } else if (supported) {
                        supported = mmay_native_c_interface_call_supported(
                            function, instruction->call, functions, count,
                            MMAY_TYPE_VOID
                        );
                    }
                    if (!supported) {
                        return MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL;
                    }
                    break;
                }
                case MMAY_MIR_CALL: {
                    const MarimayMirCall *call = mmay_native_c_call(function, instruction->call);
                    if (call == NULL
                        || !mmay_native_c_program_type_supported(
                            functions, count, call->result_type, false
                        )
                        || !mmay_native_c_call_supported(
                            function,
                            instruction->call,
                            functions,
                            count,
                            call->result_type
                        )) {
                        return MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL;
                    }
                    break;
                }
                case MMAY_MIR_CALL_INTERFACE: {
                    const MarimayMirCall *call = mmay_native_c_call(
                        function, instruction->call
                    );
                    if (call == NULL || !mmay_native_c_interface_call_supported(
                            function, instruction->call, functions, count,
                            call->result_type
                        )) return MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL;
                    break;
                }
            }
        }
        if (block->terminator.kind == MMAY_MIR_CALL_NEVER) {
            if (!mmay_native_c_call_supported(
                    function,
                    block->terminator.call,
                    functions,
                    count,
                    MMAY_TYPE_NEVER
                )) {
                return MMAY_NATIVE_C_EMIT_UNSUPPORTED_CALL;
            }
        } else if (block->terminator.kind != MMAY_MIR_RETURN
            && block->terminator.kind != MMAY_MIR_GOTO
            && block->terminator.kind != MMAY_MIR_BRANCH
            && block->terminator.kind != MMAY_MIR_PANIC) {
            return MMAY_NATIVE_C_EMIT_UNSUPPORTED_TERMINATOR;
        }
    }
    return MMAY_NATIVE_C_EMIT_OK;
}

bool mmay_native_c_function_index(
    const MarimayMirFunction *const *functions,
    size_t count,
    MarimaySymbolId symbol,
    size_t *result
) {
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        int comparison = mmay_native_c_symbol_compare(
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
bool mmay_native_c_has_cycle(
    const MarimayMirFunction *const *functions,
    size_t count,
    unsigned char *states,
    MarimayNativeCCycleFrame *stack
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
            MarimayNativeCCycleFrame *frame = &stack[depth - 1];
            const MarimayMirFunction *function = functions[frame->function_index];
            const MarimayMirBlock *block = NULL;
            if (frame->block_index >= function->blocks.length) {
                states[frame->function_index] = 2;
                depth -= 1;
                continue;
            }
            block = mmay_vector_at_const(&function->blocks, frame->block_index);
            if (block == NULL) {
                return true;
            }
            if (frame->instruction_index > block->instructions.length
                || (frame->instruction_index == block->instructions.length
                    && block->terminator.kind != MMAY_MIR_CALL_NEVER)) {
                frame->block_index += 1;
                frame->instruction_index = 0;
                continue;
            }
            {
                MarimayMirCallId call_id = MMAY_MIR_NO_CALL;
                const MarimayMirInstruction *instruction = NULL;
                if (frame->instruction_index < block->instructions.length) {
                    instruction = mmay_vector_at_const(
                        &block->instructions,
                        frame->instruction_index
                    );
                    if (instruction != NULL
                        && (instruction->kind == MMAY_MIR_CALL
                            || instruction->kind == MMAY_MIR_DEFER_CALL)) {
                        const MarimayMirCall *candidate = mmay_native_c_call(
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
                if (call_id != MMAY_MIR_NO_CALL) {
                    const MarimayMirCall *call = mmay_native_c_call(
                        function,
                        call_id
                    );
                    size_t callee_index = 0;
                    if (call == NULL || !mmay_native_c_function_index(
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
