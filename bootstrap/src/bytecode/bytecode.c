#include "ruda/bytecode/bytecode.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ruda/base/utf8.h"
#include "bytecode_internal.h"
#include "../generated/opcode_data.inc"
bool ruda_bytecode_type_is_register(RudaBytecodeType type) {
    return type == RUDA_BYTECODE_TYPE_BOOL || type == RUDA_BYTECODE_TYPE_I64
        || type == RUDA_BYTECODE_TYPE_I8 || type == RUDA_BYTECODE_TYPE_I16
        || type == RUDA_BYTECODE_TYPE_I32 || type == RUDA_BYTECODE_TYPE_U8
        || type == RUDA_BYTECODE_TYPE_U16 || type == RUDA_BYTECODE_TYPE_U32
        || type == RUDA_BYTECODE_TYPE_U64
        || type == RUDA_BYTECODE_TYPE_F64 || type == RUDA_BYTECODE_TYPE_F32
        || type == RUDA_BYTECODE_TYPE_STRING
        || type == RUDA_BYTECODE_TYPE_ARRAY_F64
        || type == RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64
        || type == RUDA_BYTECODE_TYPE_ARRAY
        || type == RUDA_BYTECODE_TYPE_RECORD || type == RUDA_BYTECODE_TYPE_ENUM
        || type == RUDA_BYTECODE_TYPE_INTERFACE;
}

bool ruda_bytecode_type_is_integer(RudaBytecodeType type) {
    return type == RUDA_BYTECODE_TYPE_I64
        || type == RUDA_BYTECODE_TYPE_I8 || type == RUDA_BYTECODE_TYPE_I16
        || type == RUDA_BYTECODE_TYPE_I32 || type == RUDA_BYTECODE_TYPE_U8
        || type == RUDA_BYTECODE_TYPE_U16 || type == RUDA_BYTECODE_TYPE_U32
        || type == RUDA_BYTECODE_TYPE_U64;
}

bool ruda_bytecode_type_is_signed_integer(RudaBytecodeType type) {
    return type == RUDA_BYTECODE_TYPE_I64
        || type == RUDA_BYTECODE_TYPE_I8 || type == RUDA_BYTECODE_TYPE_I16
        || type == RUDA_BYTECODE_TYPE_I32;
}

bool ruda_bytecode_type_is_extended_scalar(RudaBytecodeType type) {
    return ruda_bytecode_type_is_integer(type)
        || type == RUDA_BYTECODE_TYPE_F32;
}

bool ruda_bytecode_type_is_array(RudaBytecodeType type) {
    return type == RUDA_BYTECODE_TYPE_ARRAY_F64
        || type == RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64
        || type == RUDA_BYTECODE_TYPE_ARRAY;
}

bool ruda_bytecode_type_is_managed(RudaBytecodeType type) {
    return ruda_bytecode_type_is_array(type)
        || type == RUDA_BYTECODE_TYPE_RECORD
        || type == RUDA_BYTECODE_TYPE_ENUM
        || type == RUDA_BYTECODE_TYPE_INTERFACE;
}

RudaBytecodeType ruda_bytecode_array_element_type(
    RudaBytecodeType array_type
) {
    if (array_type == RUDA_BYTECODE_TYPE_ARRAY_F64) {
        return RUDA_BYTECODE_TYPE_F64;
    }
    if (array_type == RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64) {
        return RUDA_BYTECODE_TYPE_ARRAY_F64;
    }
    return RUDA_BYTECODE_TYPE_INVALID;
}

RudaBytecodeType ruda_bytecode_function_array_element_type(
    const RudaBytecodeFunction *function,
    RudaBytecodeType array_type,
    uint32_t array_layout,
    uint32_t *element_layout
) {
    const RudaBytecodeArrayType *entry = NULL;
    if (element_layout != NULL) {
        *element_layout = RUDA_BYTECODE_NO_LAYOUT;
    }
    if (array_type == RUDA_BYTECODE_TYPE_ARRAY) {
        entry = ruda_bytecode_array_type(function, array_layout);
        if (entry == NULL) {
            return RUDA_BYTECODE_TYPE_INVALID;
        }
        if (element_layout != NULL) {
            *element_layout = entry->element_layout;
        }
        return entry->element_type;
    }
    return ruda_bytecode_array_element_type(array_type);
}

bool ruda_bytecode_type_is_result(RudaBytecodeType type) {
    return ruda_bytecode_type_is_register(type)
        || type == RUDA_BYTECODE_TYPE_VOID || type == RUDA_BYTECODE_TYPE_NEVER;
}

bool ruda_bytecode_type_is_local(RudaBytecodeType type) {
    return ruda_bytecode_type_is_register(type) || type == RUDA_BYTECODE_TYPE_VOID;
}

const char *ruda_bytecode_type_name(RudaBytecodeType type) {
    switch (type) {
        case RUDA_BYTECODE_TYPE_BOOL:
            return "bool";
        case RUDA_BYTECODE_TYPE_I64:
            return "i64";
        case RUDA_BYTECODE_TYPE_I8:
            return "i8";
        case RUDA_BYTECODE_TYPE_I16:
            return "i16";
        case RUDA_BYTECODE_TYPE_I32:
            return "i32";
        case RUDA_BYTECODE_TYPE_U8:
            return "u8";
        case RUDA_BYTECODE_TYPE_U16:
            return "u16";
        case RUDA_BYTECODE_TYPE_U32:
            return "u32";
        case RUDA_BYTECODE_TYPE_U64:
            return "u64";
        case RUDA_BYTECODE_TYPE_F64:
            return "f64";
        case RUDA_BYTECODE_TYPE_F32:
            return "f32";
        case RUDA_BYTECODE_TYPE_VOID:
            return "void";
        case RUDA_BYTECODE_TYPE_STRING:
            return "string";
        case RUDA_BYTECODE_TYPE_NEVER:
            return "never";
        case RUDA_BYTECODE_TYPE_ARRAY_F64:
            return "[]f64";
        case RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64:
            return "[][]f64";
        case RUDA_BYTECODE_TYPE_ARRAY:
            return "[]T";
        case RUDA_BYTECODE_TYPE_RECORD:
            return "record";
        case RUDA_BYTECODE_TYPE_ENUM:
            return "enum";
        case RUDA_BYTECODE_TYPE_INTERFACE:
            return "interface";
        case RUDA_BYTECODE_TYPE_INVALID:
            break;
    }
    return "invalid";
}

bool ruda_bytecode_function_uses_arrays(
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
    for (index = 0; index < function->calls.length; index += 1) {
        const RudaBytecodeCall *call = ruda_vector_at_const(
            &function->calls, index
        );
        if (call != NULL && ruda_bytecode_type_is_array(call->result_type)) {
            return true;
        }
    }
    for (index = 0; index < function->call_arguments.length; index += 1) {
        const RudaBytecodeCallArgument *argument = ruda_vector_at_const(
            &function->call_arguments, index
        );
        if (argument != NULL && ruda_bytecode_type_is_array(argument->type)) {
            return true;
        }
    }
    for (index = 0; index < function->record_fields.length; index += 1) {
        const RudaBytecodeRecordField *field = ruda_vector_at_const(
            &function->record_fields,
            index
        );
        if (field != NULL && ruda_bytecode_type_is_array(field->type)) {
            return true;
        }
    }
    for (index = 0; index < function->enum_payloads.length; index += 1) {
        const RudaBytecodeEnumPayload *payload = ruda_vector_at_const(
            &function->enum_payloads,
            index
        );
        if (payload != NULL && ruda_bytecode_type_is_array(payload->type)) {
            return true;
        }
    }
    for (index = 0; index < function->interface_constructions.length;
        index += 1) {
        const RudaBytecodeInterfaceConstruction *construction =
            ruda_vector_at_const(&function->interface_constructions, index);
        if (construction != NULL
            && ruda_bytecode_type_is_array(construction->payload_type)) {
            return true;
        }
    }
    return false;
}

bool ruda_bytecode_function_uses_f64(const RudaBytecodeFunction *function) {
    size_t index = 0;
    if (function == NULL || function->result_type == RUDA_BYTECODE_TYPE_F64) {
        return function != NULL;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && *type == RUDA_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->calls.length; index += 1) {
        const RudaBytecodeCall *call = ruda_vector_at_const(&function->calls, index);
        if (call != NULL && call->result_type == RUDA_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->call_arguments.length; index += 1) {
        const RudaBytecodeCallArgument *argument = ruda_vector_at_const(
            &function->call_arguments, index
        );
        if (argument != NULL && argument->type == RUDA_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->record_fields.length; index += 1) {
        const RudaBytecodeRecordField *field = ruda_vector_at_const(
            &function->record_fields,
            index
        );
        if (field != NULL && field->type == RUDA_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->enum_payloads.length; index += 1) {
        const RudaBytecodeEnumPayload *payload = ruda_vector_at_const(
            &function->enum_payloads,
            index
        );
        if (payload != NULL && payload->type == RUDA_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->interface_constructions.length;
        index += 1) {
        const RudaBytecodeInterfaceConstruction *construction =
            ruda_vector_at_const(&function->interface_constructions, index);
        if (construction != NULL
            && construction->payload_type == RUDA_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    return false;
}

RudaBytecodeFunctionOptions ruda_bytecode_function_default_options(void) {
    RudaBytecodeFunctionOptions options;
    options.max_registers = 65536;
    options.max_blocks = 16384;
    options.max_instructions = 1000000;
    options.max_calls = 100000;
    options.max_call_arguments = 1000000;
    options.max_string_constants = 100000;
    options.max_string_bytes = 16U * 1024U * 1024U;
    options.max_array_constructions = 100000;
    options.max_array_arguments = 1000000;
    options.max_record_layouts = 100000;
    options.max_record_fields = 1000000;
    options.max_record_constructions = 100000;
    options.max_record_arguments = 1000000;
    options.max_enum_layouts = 100000;
    options.max_enum_variants = 1000000;
    options.max_enum_payloads = 1000000;
    options.max_enum_constructions = 100000;
    options.max_enum_arguments = 1000000;
    options.max_interface_layouts = 100000;
    options.max_interface_constructions = 100000;
    options.max_interface_methods = 1000000;
    options.max_source_files = 4096;
    options.max_encoded_bytes = 64U * 1024U * 1024U;
    options.max_verify_work = 16000000;
    return options;
}

const RudaOpcodeMetadata *ruda_opcode_metadata(RudaOpcode opcode) {
    if ((int)opcode < 0 || (size_t)opcode >= RUDA_OPCODE_COUNT) {
        return NULL;
    }
    if (ruda_opcode_metadata_table[(size_t)opcode].opcode != opcode) {
        return NULL;
    }
    return &ruda_opcode_metadata_table[(size_t)opcode];
}

const RudaOpcodeMetadata *ruda_opcode_metadata_from_wire(uint64_t wire_id) {
    if (wire_id >= RUDA_OPCODE_COUNT) {
        return NULL;
    }
    if ((uint64_t)ruda_opcode_metadata_table[(size_t)wire_id].wire_id
        != wire_id) {
        return NULL;
    }
    return &ruda_opcode_metadata_table[(size_t)wire_id];
}

bool ruda_bytecode_function_init(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaBytecodeType result_type,
    size_t source_count,
    const RudaBytecodeType *parameter_types,
    size_t parameter_count,
    RudaBytecodeFunctionOptions options
) {
    size_t index = 0;
    if (function == NULL || !ruda_bytecode_type_is_result(result_type)
        || source_count == 0 || source_count > options.max_source_files
        || parameter_count > options.max_registers
        || (parameter_types == NULL && parameter_count != 0)
        || options.max_registers == 0 || options.max_blocks == 0
        || options.max_instructions == 0 || options.max_calls == 0
        || options.max_call_arguments == 0 || options.max_string_constants == 0
        || options.max_string_bytes == 0 || options.max_array_constructions == 0
        || options.max_array_arguments == 0 || options.max_record_layouts == 0
        || options.max_record_fields == 0 || options.max_record_constructions == 0
        || options.max_record_arguments == 0 || options.max_enum_layouts == 0
        || options.max_enum_variants == 0 || options.max_enum_payloads == 0
        || options.max_enum_constructions == 0 || options.max_enum_arguments == 0
        || options.max_interface_layouts == 0 || options.max_interface_constructions == 0
        || options.max_interface_methods == 0 || options.max_source_files == 0
        || options.max_encoded_bytes < RUDA_BYTECODE_MAGIC_LENGTH
        || options.max_verify_work == 0) {
        return false;
    }
    memset(function, 0, sizeof(*function));
    function->allocator = allocator;
    function->result_type = result_type;
    function->result_layout = RUDA_BYTECODE_NO_LAYOUT;
    function->source_count = source_count;
    function->parameter_count = parameter_count;
    function->options = options;
    if (!ruda_vector_init(
            &function->register_types,
            allocator,
            sizeof(RudaBytecodeType)
        ) || !ruda_vector_init(
            &function->blocks,
            allocator,
            sizeof(RudaBytecodeBlock)
        ) || !ruda_vector_init(
            &function->calls,
            allocator,
            sizeof(RudaBytecodeCall)
        ) || !ruda_vector_init(
            &function->call_arguments,
            allocator,
            sizeof(RudaBytecodeCallArgument)
        ) || !ruda_vector_init(
            &function->string_constants,
            allocator,
            sizeof(RudaBytecodeStringConstant)
        ) || !ruda_vector_init(
            &function->array_constructions,
            allocator,
            sizeof(RudaBytecodeArrayConstruction)
        ) || !ruda_vector_init(
            &function->array_arguments,
            allocator,
            sizeof(RudaBytecodeArrayArgument)
        ) || !ruda_vector_init(
            &function->array_types,
            allocator,
            sizeof(RudaBytecodeArrayType)
        ) || !ruda_vector_init(&function->register_layouts, allocator, sizeof(uint32_t))
        || !ruda_vector_init(&function->record_layouts, allocator, sizeof(RudaBytecodeRecordLayout))
        || !ruda_vector_init(&function->record_fields, allocator, sizeof(RudaBytecodeRecordField))
        || !ruda_vector_init(&function->record_constructions, allocator, sizeof(RudaBytecodeRecordConstruction))
        || !ruda_vector_init(&function->record_arguments, allocator, sizeof(RudaBytecodeRecordArgument))
        || !ruda_vector_init(&function->enum_layouts, allocator, sizeof(RudaBytecodeEnumLayout))
        || !ruda_vector_init(&function->enum_variants, allocator, sizeof(RudaBytecodeEnumVariant))
        || !ruda_vector_init(&function->enum_payloads, allocator, sizeof(RudaBytecodeEnumPayload))
        || !ruda_vector_init(&function->enum_constructions, allocator, sizeof(RudaBytecodeEnumConstruction))
        || !ruda_vector_init(&function->enum_arguments, allocator, sizeof(RudaBytecodeEnumArgument))
        || !ruda_vector_init(&function->interface_layouts, allocator, sizeof(RudaBytecodeInterfaceLayout))
        || !ruda_vector_init(&function->interface_constructions, allocator, sizeof(RudaBytecodeInterfaceConstruction))
        || !ruda_vector_init(&function->interface_methods, allocator, sizeof(RudaSymbolId))) {
        ruda_bytecode_function_dispose(function);
        return false;
    }
    for (index = 0; index < parameter_count; index += 1) {
        uint32_t layout = RUDA_BYTECODE_NO_LAYOUT;
        if (!ruda_bytecode_type_is_register(parameter_types[index])
            || !ruda_vector_push(
                &function->register_types,
                &parameter_types[index]
            ) || !ruda_vector_push(&function->register_layouts, &layout)) {
            ruda_bytecode_function_dispose(function);
            return false;
        }
    }
    function->initialized = true;
    return true;
}

void ruda_bytecode_function_dispose(RudaBytecodeFunction *function) {
    size_t index = 0;
    if (function == NULL) {
        return;
    }
    for (index = 0; index < function->blocks.length; index += 1) {
        RudaBytecodeBlock *block = ruda_vector_at(&function->blocks, index);
        if (block != NULL && block->initialized) {
            ruda_vector_dispose(&block->instructions);
            block->initialized = false;
        }
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        RudaBytecodeStringConstant *constant = ruda_vector_at(
            &function->string_constants,
            index
        );
        if (constant != NULL) {
            ruda_owned_bytes_dispose(&constant->value);
        }
    }
    ruda_vector_dispose(&function->string_constants);
    ruda_vector_dispose(&function->array_arguments);
    ruda_vector_dispose(&function->array_types);
    ruda_vector_dispose(&function->array_constructions);
    ruda_vector_dispose(&function->register_layouts);
    ruda_vector_dispose(&function->record_arguments);
    ruda_vector_dispose(&function->record_constructions);
    ruda_vector_dispose(&function->record_fields);
    ruda_vector_dispose(&function->record_layouts);
    ruda_vector_dispose(&function->enum_arguments);
    ruda_vector_dispose(&function->enum_constructions);
    ruda_vector_dispose(&function->enum_payloads);
    ruda_vector_dispose(&function->enum_variants);
    ruda_vector_dispose(&function->enum_layouts);
    ruda_vector_dispose(&function->interface_methods);
    ruda_vector_dispose(&function->interface_constructions);
    ruda_vector_dispose(&function->interface_layouts);
    ruda_vector_dispose(&function->blocks);
    ruda_vector_dispose(&function->register_types);
    ruda_vector_dispose(&function->call_arguments);
    ruda_vector_dispose(&function->calls);
    memset(function, 0, sizeof(*function));
}

bool ruda_bytecode_add_register(
    RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t *result
) {
    uint32_t layout = RUDA_BYTECODE_NO_LAYOUT;
    if (function == NULL || !function->initialized || result == NULL
        || !ruda_bytecode_type_is_local(type)
        || function->register_types.length >= function->options.max_registers
        || function->register_types.length >= UINT32_MAX) {
        return false;
    }
    if (!ruda_vector_push(&function->register_types, &type)
        || !ruda_vector_push(&function->register_layouts, &layout)) {
        if (function->register_types.length > function->register_layouts.length) {
            function->register_types.length -= 1;
        }
        return false;
    }
    *result = (uint32_t)(function->register_types.length - 1);
    return true;
}

bool ruda_bytecode_add_block(
    RudaBytecodeFunction *function,
    uint32_t *result
) {
    RudaBytecodeBlock block;
    if (function == NULL || !function->initialized || result == NULL
        || function->blocks.length >= function->options.max_blocks
        || function->blocks.length >= UINT32_MAX) {
        return false;
    }
    memset(&block, 0, sizeof(block));
    block.allocator = function->allocator;
    if (!ruda_vector_init(
            &block.instructions,
            function->allocator,
            sizeof(RudaBytecodeInstruction)
        )) {
        return false;
    }
    block.initialized = true;
    if (!ruda_vector_push(&function->blocks, &block)) {
        ruda_vector_dispose(&block.instructions);
        return false;
    }
    *result = (uint32_t)(function->blocks.length - 1);
    return true;
}

bool ruda_bytecode_append_instruction(
    RudaBytecodeFunction *function,
    uint32_t block_id,
    const RudaBytecodeInstruction *instruction
) {
    RudaBytecodeBlock *block = NULL;
    size_t next_count = 0;
    if (function == NULL || !function->initialized || instruction == NULL
        || block_id >= function->blocks.length
        || !ruda_size_add(function->instruction_count, 1, &next_count)
        || next_count > function->options.max_instructions) {
        return false;
    }
    block = ruda_vector_at(&function->blocks, block_id);
    if (block == NULL || !block->initialized
        || !ruda_vector_push(&block->instructions, instruction)) {
        return false;
    }
    function->instruction_count = next_count;
    return true;
}

bool ruda_bytecode_add_call(
    RudaBytecodeFunction *function,
    RudaSymbolId callee,
    RudaBytecodeType result_type,
    const RudaBytecodeCallArgument *arguments,
    size_t argument_count,
    RudaBytecodeCallId *result
) {
    RudaBytecodeCall call;
    size_t next_argument_count = 0;
    size_t index = 0;
    memset(&call, 0, sizeof(call));
    call.result_layout = RUDA_BYTECODE_NO_LAYOUT;
    call.interface_slot = UINT32_MAX;
    if (function == NULL || !function->initialized || result == NULL
        || !ruda_bytecode_type_is_result(result_type)
        || (arguments == NULL && argument_count != 0)
        || function->calls.length >= function->options.max_calls
        || function->calls.length >= UINT32_MAX
        || !ruda_size_add(
            function->call_arguments.length,
            argument_count,
            &next_argument_count
        ) || next_argument_count > function->options.max_call_arguments) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const RudaBytecodeType *type = arguments[index].register_id
                < function->register_types.length
            ? ruda_vector_at_const(
                &function->register_types,
                arguments[index].register_id
            )
            : NULL;
        if (type == NULL || !ruda_bytecode_type_is_register(arguments[index].type)
            || *type != arguments[index].type) {
            return false;
        }
    }
    if (!ruda_vector_reserve(&function->calls, function->calls.length + 1)
        || !ruda_vector_reserve(
            &function->call_arguments,
            next_argument_count
        )) {
        return false;
    }
    call.callee = callee;
    call.result_type = result_type;
    call.argument_start = function->call_arguments.length;
    call.argument_count = argument_count;
    if (!ruda_vector_append(
            &function->call_arguments,
            arguments,
            argument_count
        ) || !ruda_vector_push(&function->calls, &call)) {
        function->call_arguments.length = call.argument_start;
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        RudaBytecodeCallArgument *stored = ruda_vector_at(
            &function->call_arguments,
            call.argument_start + index
        );
        if (stored == NULL) {
            function->call_arguments.length = call.argument_start;
            function->calls.length -= 1;
            return false;
        }
        stored->layout = ruda_bytecode_register_layout(
            function,
            stored->register_id
        );
    }
    *result = (RudaBytecodeCallId)(function->calls.length - 1);
    return true;
}

const RudaBytecodeCall *ruda_bytecode_call(
    const RudaBytecodeFunction *function,
    RudaBytecodeCallId call
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(&function->calls, call)
        : NULL;
}

const RudaBytecodeCallArgument *ruda_bytecode_call_argument(
    const RudaBytecodeFunction *function,
    size_t index
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(&function->call_arguments, index)
        : NULL;
}

bool ruda_bytecode_add_string_constant(
    RudaBytecodeFunction *function,
    const unsigned char *bytes,
    size_t length,
    RudaBytecodeStringId *result
) {
    RudaBytecodeStringConstant constant;
    size_t next_bytes = 0;
    if (function == NULL || !function->initialized || result == NULL
        || (bytes == NULL && length != 0)
        || !ruda_utf8_validate((const char *)bytes, length, NULL)
        || function->string_constants.length
            >= function->options.max_string_constants
        || function->string_constants.length >= UINT32_MAX
        || !ruda_size_add(function->string_bytes, length, &next_bytes)
        || next_bytes > function->options.max_string_bytes) {
        return false;
    }
    memset(&constant, 0, sizeof(constant));
    if (!ruda_owned_bytes_copy(
            &constant.value,
            function->allocator,
            bytes,
            length
        )) {
        return false;
    }
    if (!ruda_vector_push(&function->string_constants, &constant)) {
        ruda_owned_bytes_dispose(&constant.value);
        return false;
    }
    function->string_bytes = next_bytes;
    *result = (RudaBytecodeStringId)(function->string_constants.length - 1);
    return true;
}

const RudaBytecodeStringConstant *ruda_bytecode_string_constant(
    const RudaBytecodeFunction *function,
    RudaBytecodeStringId string_id
) {
    return function != NULL && function->initialized
        ? ruda_vector_at_const(&function->string_constants, string_id)
        : NULL;
}

static bool ruda_bytecode_writef(RudaWriter writer, const char *format, ...) {
    char buffer[256];
    va_list arguments;
    int length = 0;
    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    return length >= 0 && (size_t)length < sizeof(buffer)
        && ruda_writer_write(writer, buffer, (size_t)length);
}

RudaBytecodeDisassembleStatus ruda_bytecode_disassemble(
    const RudaBytecodeFunction *function,
    RudaWriter writer
) {
    size_t register_index = 0;
    size_t block_index = 0;
    if (function == NULL || writer.write == NULL) {
        return RUDA_BYTECODE_DISASSEMBLE_INVALID_ARGUMENT;
    }
    if (ruda_bytecode_verify(function) != RUDA_BYTECODE_VERIFY_OK) {
        return RUDA_BYTECODE_DISASSEMBLE_INVALID_BYTECODE;
    }
    if (!ruda_bytecode_writef(
            writer,
            "bytecode function v%d result=%s sources=%zu parameters=%zu "
            "registers=%zu blocks=%zu\n",
            RUDA_BYTECODE_FORMAT_VERSION,
            ruda_bytecode_type_name(function->result_type),
            function->source_count,
            function->parameter_count,
            function->register_types.length,
            function->blocks.length
        ) || !ruda_writer_write_cstr(writer, "registers")) {
        return RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED;
    }
    for (register_index = 0;
        register_index < function->register_types.length;
        register_index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types,
            register_index
        );
        if (!ruda_bytecode_writef(
                writer,
                " r%zu:%s",
                register_index,
                ruda_bytecode_type_name(*type)
            )) {
            return RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED;
        }
    }
    if (!ruda_writer_write_cstr(writer, "\n")) {
        return RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (!ruda_bytecode_writef(writer, "block b%zu:\n", block_index)) {
            return RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            const RudaOpcodeMetadata *metadata = ruda_opcode_metadata(
                instruction->opcode
            );
            size_t operand_index = 0;
            if (!ruda_bytecode_writef(writer, "  %s", metadata->mnemonic)) {
                return RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED;
            }
            for (operand_index = 0; operand_index < metadata->operand_count;
                operand_index += 1) {
                const char *separator = operand_index == 0 ? " " : ", ";
                bool wrote = false;
                if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_REGISTER) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%sr%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_BLOCK) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%sb%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_CALL) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%sc%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_STRING) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%ss%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_ARRAY_CONSTRUCTION) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%sa%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_RECORD_CONSTRUCTION) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%src%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_ENUM_CONSTRUCTION) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%sec%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_INTERFACE_CONSTRUCTION) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%sic%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_I64) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%s%" PRId64,
                        separator,
                        instruction->operands[operand_index].signed_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_F64_BITS) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%s0x%016" PRIx64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_BOOL) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%s%s",
                        separator,
                        instruction->operands[operand_index].unsigned_value == 0
                            ? "false"
                            : "true"
                    );
                } else if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_U64) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%s%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                }
                if (!wrote) {
                    return RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED;
                }
            }
            if (!ruda_bytecode_writef(
                    writer,
                    " @s%" PRIu64 ":%" PRIu64 "..%" PRIu64 "\n",
                    instruction->span.source,
                    instruction->span.start,
                    instruction->span.end
                )) {
                return RUDA_BYTECODE_DISASSEMBLE_WRITE_FAILED;
            }
        }
    }
    return RUDA_BYTECODE_DISASSEMBLE_OK;
}
