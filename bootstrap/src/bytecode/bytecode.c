#include "mmay/bytecode/bytecode.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mmay/base/utf8.h"
#include "bytecode_internal.h"
#include "../generated/opcode_data.inc"
bool mmay_bytecode_type_is_register(MarimayBytecodeType type) {
    return type == MMAY_BYTECODE_TYPE_BOOL || type == MMAY_BYTECODE_TYPE_I64
        || type == MMAY_BYTECODE_TYPE_I8 || type == MMAY_BYTECODE_TYPE_I16
        || type == MMAY_BYTECODE_TYPE_I32 || type == MMAY_BYTECODE_TYPE_U8
        || type == MMAY_BYTECODE_TYPE_U16 || type == MMAY_BYTECODE_TYPE_U32
        || type == MMAY_BYTECODE_TYPE_U64
        || type == MMAY_BYTECODE_TYPE_RUNE
        || type == MMAY_BYTECODE_TYPE_F64 || type == MMAY_BYTECODE_TYPE_F32
        || type == MMAY_BYTECODE_TYPE_STRING
        || type == MMAY_BYTECODE_TYPE_ARRAY_F64
        || type == MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64
        || type == MMAY_BYTECODE_TYPE_ARRAY
        || type == MMAY_BYTECODE_TYPE_RECORD || type == MMAY_BYTECODE_TYPE_ENUM
        || type == MMAY_BYTECODE_TYPE_INTERFACE
        || type == MMAY_BYTECODE_TYPE_MAP || type == MMAY_BYTECODE_TYPE_SET;
}

bool mmay_bytecode_type_is_integer(MarimayBytecodeType type) {
    return type == MMAY_BYTECODE_TYPE_I64
        || type == MMAY_BYTECODE_TYPE_I8 || type == MMAY_BYTECODE_TYPE_I16
        || type == MMAY_BYTECODE_TYPE_I32 || type == MMAY_BYTECODE_TYPE_U8
        || type == MMAY_BYTECODE_TYPE_U16 || type == MMAY_BYTECODE_TYPE_U32
        || type == MMAY_BYTECODE_TYPE_U64;
}

bool mmay_bytecode_type_is_signed_integer(MarimayBytecodeType type) {
    return type == MMAY_BYTECODE_TYPE_I64
        || type == MMAY_BYTECODE_TYPE_I8 || type == MMAY_BYTECODE_TYPE_I16
        || type == MMAY_BYTECODE_TYPE_I32;
}

bool mmay_bytecode_type_is_extended_scalar(MarimayBytecodeType type) {
    return mmay_bytecode_type_is_integer(type)
        || type == MMAY_BYTECODE_TYPE_F32
        || type == MMAY_BYTECODE_TYPE_RUNE;
}

bool mmay_bytecode_type_is_array(MarimayBytecodeType type) {
    return type == MMAY_BYTECODE_TYPE_ARRAY_F64
        || type == MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64
        || type == MMAY_BYTECODE_TYPE_ARRAY;
}

bool mmay_bytecode_type_is_managed(MarimayBytecodeType type) {
    return mmay_bytecode_type_is_array(type)
        || type == MMAY_BYTECODE_TYPE_RECORD
        || type == MMAY_BYTECODE_TYPE_ENUM
        || type == MMAY_BYTECODE_TYPE_INTERFACE
        || type == MMAY_BYTECODE_TYPE_MAP || type == MMAY_BYTECODE_TYPE_SET;
}

MarimayBytecodeType mmay_bytecode_array_element_type(
    MarimayBytecodeType array_type
) {
    if (array_type == MMAY_BYTECODE_TYPE_ARRAY_F64) {
        return MMAY_BYTECODE_TYPE_F64;
    }
    if (array_type == MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64) {
        return MMAY_BYTECODE_TYPE_ARRAY_F64;
    }
    return MMAY_BYTECODE_TYPE_INVALID;
}

MarimayBytecodeType mmay_bytecode_function_array_element_type(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeType array_type,
    uint32_t array_layout,
    uint32_t *element_layout
) {
    const MarimayBytecodeArrayType *entry = NULL;
    if (element_layout != NULL) {
        *element_layout = MMAY_BYTECODE_NO_LAYOUT;
    }
    if (array_type == MMAY_BYTECODE_TYPE_ARRAY) {
        entry = mmay_bytecode_array_type(function, array_layout);
        if (entry == NULL) {
            return MMAY_BYTECODE_TYPE_INVALID;
        }
        if (element_layout != NULL) {
            *element_layout = entry->element_layout;
        }
        return entry->element_type;
    }
    return mmay_bytecode_array_element_type(array_type);
}

bool mmay_bytecode_type_is_result(MarimayBytecodeType type) {
    return mmay_bytecode_type_is_register(type)
        || type == MMAY_BYTECODE_TYPE_VOID || type == MMAY_BYTECODE_TYPE_NEVER;
}

bool mmay_bytecode_type_is_local(MarimayBytecodeType type) {
    return mmay_bytecode_type_is_register(type) || type == MMAY_BYTECODE_TYPE_VOID;
}

const char *mmay_bytecode_type_name(MarimayBytecodeType type) {
    switch (type) {
        case MMAY_BYTECODE_TYPE_BOOL:
            return "bool";
        case MMAY_BYTECODE_TYPE_I64:
            return "i64";
        case MMAY_BYTECODE_TYPE_I8:
            return "i8";
        case MMAY_BYTECODE_TYPE_I16:
            return "i16";
        case MMAY_BYTECODE_TYPE_I32:
            return "i32";
        case MMAY_BYTECODE_TYPE_U8:
            return "u8";
        case MMAY_BYTECODE_TYPE_U16:
            return "u16";
        case MMAY_BYTECODE_TYPE_U32:
            return "u32";
        case MMAY_BYTECODE_TYPE_U64:
            return "u64";
        case MMAY_BYTECODE_TYPE_RUNE:
            return "rune";
        case MMAY_BYTECODE_TYPE_F64:
            return "f64";
        case MMAY_BYTECODE_TYPE_F32:
            return "f32";
        case MMAY_BYTECODE_TYPE_VOID:
            return "void";
        case MMAY_BYTECODE_TYPE_STRING:
            return "string";
        case MMAY_BYTECODE_TYPE_NEVER:
            return "never";
        case MMAY_BYTECODE_TYPE_ARRAY_F64:
            return "[]f64";
        case MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64:
            return "[][]f64";
        case MMAY_BYTECODE_TYPE_ARRAY:
            return "[]T";
        case MMAY_BYTECODE_TYPE_RECORD:
            return "record";
        case MMAY_BYTECODE_TYPE_ENUM:
            return "enum";
        case MMAY_BYTECODE_TYPE_INTERFACE:
            return "interface";
        case MMAY_BYTECODE_TYPE_MAP:
            return "Map";
        case MMAY_BYTECODE_TYPE_SET:
            return "Set";
        case MMAY_BYTECODE_TYPE_INVALID:
            break;
    }
    return "invalid";
}

bool mmay_bytecode_function_uses_arrays(
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
    for (index = 0; index < function->calls.length; index += 1) {
        const MarimayBytecodeCall *call = mmay_vector_at_const(
            &function->calls, index
        );
        if (call != NULL && mmay_bytecode_type_is_array(call->result_type)) {
            return true;
        }
    }
    for (index = 0; index < function->call_arguments.length; index += 1) {
        const MarimayBytecodeCallArgument *argument = mmay_vector_at_const(
            &function->call_arguments, index
        );
        if (argument != NULL && mmay_bytecode_type_is_array(argument->type)) {
            return true;
        }
    }
    for (index = 0; index < function->record_fields.length; index += 1) {
        const MarimayBytecodeRecordField *field = mmay_vector_at_const(
            &function->record_fields,
            index
        );
        if (field != NULL && mmay_bytecode_type_is_array(field->type)) {
            return true;
        }
    }
    for (index = 0; index < function->enum_payloads.length; index += 1) {
        const MarimayBytecodeEnumPayload *payload = mmay_vector_at_const(
            &function->enum_payloads,
            index
        );
        if (payload != NULL && mmay_bytecode_type_is_array(payload->type)) {
            return true;
        }
    }
    for (index = 0; index < function->interface_constructions.length;
        index += 1) {
        const MarimayBytecodeInterfaceConstruction *construction =
            mmay_vector_at_const(&function->interface_constructions, index);
        if (construction != NULL
            && mmay_bytecode_type_is_array(construction->payload_type)) {
            return true;
        }
    }
    return false;
}

bool mmay_bytecode_function_uses_f64(const MarimayBytecodeFunction *function) {
    size_t index = 0;
    if (function == NULL || function->result_type == MMAY_BYTECODE_TYPE_F64) {
        return function != NULL;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types, index
        );
        if (type != NULL && *type == MMAY_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->calls.length; index += 1) {
        const MarimayBytecodeCall *call = mmay_vector_at_const(&function->calls, index);
        if (call != NULL && call->result_type == MMAY_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->call_arguments.length; index += 1) {
        const MarimayBytecodeCallArgument *argument = mmay_vector_at_const(
            &function->call_arguments, index
        );
        if (argument != NULL && argument->type == MMAY_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->record_fields.length; index += 1) {
        const MarimayBytecodeRecordField *field = mmay_vector_at_const(
            &function->record_fields,
            index
        );
        if (field != NULL && field->type == MMAY_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->enum_payloads.length; index += 1) {
        const MarimayBytecodeEnumPayload *payload = mmay_vector_at_const(
            &function->enum_payloads,
            index
        );
        if (payload != NULL && payload->type == MMAY_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    for (index = 0; index < function->interface_constructions.length;
        index += 1) {
        const MarimayBytecodeInterfaceConstruction *construction =
            mmay_vector_at_const(&function->interface_constructions, index);
        if (construction != NULL
            && construction->payload_type == MMAY_BYTECODE_TYPE_F64) {
            return true;
        }
    }
    return false;
}

MarimayBytecodeFunctionOptions mmay_bytecode_function_default_options(void) {
    MarimayBytecodeFunctionOptions options;
    options.max_registers = 65536;
    options.max_blocks = 16384;
    options.max_instructions = 1000000;
    options.max_calls = 100000;
    options.max_call_arguments = 1000000;
    options.max_string_constants = 100000;
    options.max_string_bytes = 16U * 1024U * 1024U;
    options.max_array_constructions = 100000;
    options.max_array_arguments = 1000000;
    options.max_collection_layouts = 100000;
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

const MarimayOpcodeMetadata *mmay_opcode_metadata(MarimayOpcode opcode) {
    if ((int)opcode < 0 || (size_t)opcode >= MMAY_OPCODE_COUNT) {
        return NULL;
    }
    if (mmay_opcode_metadata_table[(size_t)opcode].opcode != opcode) {
        return NULL;
    }
    return &mmay_opcode_metadata_table[(size_t)opcode];
}

const MarimayOpcodeMetadata *mmay_opcode_metadata_from_wire(uint64_t wire_id) {
    if (wire_id >= MMAY_OPCODE_COUNT) {
        return NULL;
    }
    if ((uint64_t)mmay_opcode_metadata_table[(size_t)wire_id].wire_id
        != wire_id) {
        return NULL;
    }
    return &mmay_opcode_metadata_table[(size_t)wire_id];
}

bool mmay_bytecode_function_init(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayBytecodeType result_type,
    size_t source_count,
    const MarimayBytecodeType *parameter_types,
    size_t parameter_count,
    MarimayBytecodeFunctionOptions options
) {
    size_t index = 0;
    if (function == NULL || !mmay_bytecode_type_is_result(result_type)
        || source_count == 0 || source_count > options.max_source_files
        || parameter_count > options.max_registers
        || (parameter_types == NULL && parameter_count != 0)
        || options.max_registers == 0 || options.max_blocks == 0
        || options.max_instructions == 0 || options.max_calls == 0
        || options.max_call_arguments == 0 || options.max_string_constants == 0
        || options.max_string_bytes == 0 || options.max_array_constructions == 0
        || options.max_array_arguments == 0 || options.max_collection_layouts == 0
        || options.max_record_layouts == 0
        || options.max_record_fields == 0 || options.max_record_constructions == 0
        || options.max_record_arguments == 0 || options.max_enum_layouts == 0
        || options.max_enum_variants == 0 || options.max_enum_payloads == 0
        || options.max_enum_constructions == 0 || options.max_enum_arguments == 0
        || options.max_interface_layouts == 0 || options.max_interface_constructions == 0
        || options.max_interface_methods == 0 || options.max_source_files == 0
        || options.max_encoded_bytes < MMAY_BYTECODE_MAGIC_LENGTH
        || options.max_verify_work == 0) {
        return false;
    }
    memset(function, 0, sizeof(*function));
    function->allocator = allocator;
    function->result_type = result_type;
    function->result_layout = MMAY_BYTECODE_NO_LAYOUT;
    function->source_count = source_count;
    function->parameter_count = parameter_count;
    function->options = options;
    if (!mmay_vector_init(
            &function->register_types,
            allocator,
            sizeof(MarimayBytecodeType)
        ) || !mmay_vector_init(
            &function->blocks,
            allocator,
            sizeof(MarimayBytecodeBlock)
        ) || !mmay_vector_init(
            &function->calls,
            allocator,
            sizeof(MarimayBytecodeCall)
        ) || !mmay_vector_init(
            &function->call_arguments,
            allocator,
            sizeof(MarimayBytecodeCallArgument)
        ) || !mmay_vector_init(
            &function->string_constants,
            allocator,
            sizeof(MarimayBytecodeStringConstant)
        ) || !mmay_vector_init(
            &function->array_constructions,
            allocator,
            sizeof(MarimayBytecodeArrayConstruction)
        ) || !mmay_vector_init(
            &function->array_arguments,
            allocator,
            sizeof(MarimayBytecodeArrayArgument)
        ) || !mmay_vector_init(
            &function->array_types,
            allocator,
            sizeof(MarimayBytecodeArrayType)
        ) || !mmay_vector_init(
            &function->collection_types,
            allocator,
            sizeof(MarimayBytecodeCollectionType)
        ) || !mmay_vector_init(&function->register_layouts, allocator, sizeof(uint32_t))
        || !mmay_vector_init(&function->record_layouts, allocator, sizeof(MarimayBytecodeRecordLayout))
        || !mmay_vector_init(&function->record_fields, allocator, sizeof(MarimayBytecodeRecordField))
        || !mmay_vector_init(&function->record_constructions, allocator, sizeof(MarimayBytecodeRecordConstruction))
        || !mmay_vector_init(&function->record_arguments, allocator, sizeof(MarimayBytecodeRecordArgument))
        || !mmay_vector_init(&function->enum_layouts, allocator, sizeof(MarimayBytecodeEnumLayout))
        || !mmay_vector_init(&function->enum_variants, allocator, sizeof(MarimayBytecodeEnumVariant))
        || !mmay_vector_init(&function->enum_payloads, allocator, sizeof(MarimayBytecodeEnumPayload))
        || !mmay_vector_init(&function->enum_constructions, allocator, sizeof(MarimayBytecodeEnumConstruction))
        || !mmay_vector_init(&function->enum_arguments, allocator, sizeof(MarimayBytecodeEnumArgument))
        || !mmay_vector_init(&function->interface_layouts, allocator, sizeof(MarimayBytecodeInterfaceLayout))
        || !mmay_vector_init(&function->interface_constructions, allocator, sizeof(MarimayBytecodeInterfaceConstruction))
        || !mmay_vector_init(&function->interface_methods, allocator, sizeof(MarimaySymbolId))) {
        mmay_bytecode_function_dispose(function);
        return false;
    }
    for (index = 0; index < parameter_count; index += 1) {
        uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
        if (!mmay_bytecode_type_is_register(parameter_types[index])
            || !mmay_vector_push(
                &function->register_types,
                &parameter_types[index]
            ) || !mmay_vector_push(&function->register_layouts, &layout)) {
            mmay_bytecode_function_dispose(function);
            return false;
        }
    }
    function->initialized = true;
    return true;
}

void mmay_bytecode_function_dispose(MarimayBytecodeFunction *function) {
    size_t index = 0;
    if (function == NULL) {
        return;
    }
    for (index = 0; index < function->blocks.length; index += 1) {
        MarimayBytecodeBlock *block = mmay_vector_at(&function->blocks, index);
        if (block != NULL && block->initialized) {
            mmay_vector_dispose(&block->instructions);
            block->initialized = false;
        }
    }
    for (index = 0; index < function->string_constants.length; index += 1) {
        MarimayBytecodeStringConstant *constant = mmay_vector_at(
            &function->string_constants,
            index
        );
        if (constant != NULL) {
            mmay_owned_bytes_dispose(&constant->value);
        }
    }
    mmay_vector_dispose(&function->string_constants);
    mmay_vector_dispose(&function->array_arguments);
    mmay_vector_dispose(&function->array_types);
    mmay_vector_dispose(&function->collection_types);
    mmay_vector_dispose(&function->array_constructions);
    mmay_vector_dispose(&function->register_layouts);
    mmay_vector_dispose(&function->record_arguments);
    mmay_vector_dispose(&function->record_constructions);
    mmay_vector_dispose(&function->record_fields);
    mmay_vector_dispose(&function->record_layouts);
    mmay_vector_dispose(&function->enum_arguments);
    mmay_vector_dispose(&function->enum_constructions);
    mmay_vector_dispose(&function->enum_payloads);
    mmay_vector_dispose(&function->enum_variants);
    mmay_vector_dispose(&function->enum_layouts);
    mmay_vector_dispose(&function->interface_methods);
    mmay_vector_dispose(&function->interface_constructions);
    mmay_vector_dispose(&function->interface_layouts);
    mmay_vector_dispose(&function->blocks);
    mmay_vector_dispose(&function->register_types);
    mmay_vector_dispose(&function->call_arguments);
    mmay_vector_dispose(&function->calls);
    memset(function, 0, sizeof(*function));
}

bool mmay_bytecode_add_register(
    MarimayBytecodeFunction *function,
    MarimayBytecodeType type,
    uint32_t *result
) {
    uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
    if (function == NULL || !function->initialized || result == NULL
        || !mmay_bytecode_type_is_local(type)
        || function->register_types.length >= function->options.max_registers
        || function->register_types.length >= UINT32_MAX) {
        return false;
    }
    if (!mmay_vector_push(&function->register_types, &type)
        || !mmay_vector_push(&function->register_layouts, &layout)) {
        if (function->register_types.length > function->register_layouts.length) {
            function->register_types.length -= 1;
        }
        return false;
    }
    *result = (uint32_t)(function->register_types.length - 1);
    return true;
}

bool mmay_bytecode_add_block(
    MarimayBytecodeFunction *function,
    uint32_t *result
) {
    MarimayBytecodeBlock block;
    if (function == NULL || !function->initialized || result == NULL
        || function->blocks.length >= function->options.max_blocks
        || function->blocks.length >= UINT32_MAX) {
        return false;
    }
    memset(&block, 0, sizeof(block));
    block.allocator = function->allocator;
    if (!mmay_vector_init(
            &block.instructions,
            function->allocator,
            sizeof(MarimayBytecodeInstruction)
        )) {
        return false;
    }
    block.initialized = true;
    if (!mmay_vector_push(&function->blocks, &block)) {
        mmay_vector_dispose(&block.instructions);
        return false;
    }
    *result = (uint32_t)(function->blocks.length - 1);
    return true;
}

bool mmay_bytecode_append_instruction(
    MarimayBytecodeFunction *function,
    uint32_t block_id,
    const MarimayBytecodeInstruction *instruction
) {
    MarimayBytecodeBlock *block = NULL;
    size_t next_count = 0;
    if (function == NULL || !function->initialized || instruction == NULL
        || block_id >= function->blocks.length
        || !mmay_size_add(function->instruction_count, 1, &next_count)
        || next_count > function->options.max_instructions) {
        return false;
    }
    block = mmay_vector_at(&function->blocks, block_id);
    if (block == NULL || !block->initialized
        || !mmay_vector_push(&block->instructions, instruction)) {
        return false;
    }
    function->instruction_count = next_count;
    return true;
}

bool mmay_bytecode_add_call(
    MarimayBytecodeFunction *function,
    MarimaySymbolId callee,
    MarimayBytecodeType result_type,
    const MarimayBytecodeCallArgument *arguments,
    size_t argument_count,
    MarimayBytecodeCallId *result
) {
    MarimayBytecodeCall call;
    size_t next_argument_count = 0;
    size_t index = 0;
    memset(&call, 0, sizeof(call));
    call.result_layout = MMAY_BYTECODE_NO_LAYOUT;
    call.interface_slot = UINT32_MAX;
    if (function == NULL || !function->initialized || result == NULL
        || !mmay_bytecode_type_is_result(result_type)
        || (arguments == NULL && argument_count != 0)
        || function->calls.length >= function->options.max_calls
        || function->calls.length >= UINT32_MAX
        || !mmay_size_add(
            function->call_arguments.length,
            argument_count,
            &next_argument_count
        ) || next_argument_count > function->options.max_call_arguments) {
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        const MarimayBytecodeType *type = arguments[index].register_id
                < function->register_types.length
            ? mmay_vector_at_const(
                &function->register_types,
                arguments[index].register_id
            )
            : NULL;
        if (type == NULL || !mmay_bytecode_type_is_register(arguments[index].type)
            || *type != arguments[index].type) {
            return false;
        }
    }
    if (!mmay_vector_reserve(&function->calls, function->calls.length + 1)
        || !mmay_vector_reserve(
            &function->call_arguments,
            next_argument_count
        )) {
        return false;
    }
    call.callee = callee;
    call.result_type = result_type;
    call.argument_start = function->call_arguments.length;
    call.argument_count = argument_count;
    if (!mmay_vector_append(
            &function->call_arguments,
            arguments,
            argument_count
        ) || !mmay_vector_push(&function->calls, &call)) {
        function->call_arguments.length = call.argument_start;
        return false;
    }
    for (index = 0; index < argument_count; index += 1) {
        MarimayBytecodeCallArgument *stored = mmay_vector_at(
            &function->call_arguments,
            call.argument_start + index
        );
        if (stored == NULL) {
            function->call_arguments.length = call.argument_start;
            function->calls.length -= 1;
            return false;
        }
        stored->layout = mmay_bytecode_register_layout(
            function,
            stored->register_id
        );
    }
    *result = (MarimayBytecodeCallId)(function->calls.length - 1);
    return true;
}

const MarimayBytecodeCall *mmay_bytecode_call(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeCallId call
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->calls, call)
        : NULL;
}

const MarimayBytecodeCallArgument *mmay_bytecode_call_argument(
    const MarimayBytecodeFunction *function,
    size_t index
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->call_arguments, index)
        : NULL;
}

bool mmay_bytecode_add_string_constant(
    MarimayBytecodeFunction *function,
    const unsigned char *bytes,
    size_t length,
    MarimayBytecodeStringId *result
) {
    MarimayBytecodeStringConstant constant;
    size_t next_bytes = 0;
    if (function == NULL || !function->initialized || result == NULL
        || (bytes == NULL && length != 0)
        || !mmay_utf8_validate((const char *)bytes, length, NULL)
        || function->string_constants.length
            >= function->options.max_string_constants
        || function->string_constants.length >= UINT32_MAX
        || !mmay_size_add(function->string_bytes, length, &next_bytes)
        || next_bytes > function->options.max_string_bytes) {
        return false;
    }
    memset(&constant, 0, sizeof(constant));
    if (!mmay_owned_bytes_copy(
            &constant.value,
            function->allocator,
            bytes,
            length
        )) {
        return false;
    }
    if (!mmay_vector_push(&function->string_constants, &constant)) {
        mmay_owned_bytes_dispose(&constant.value);
        return false;
    }
    function->string_bytes = next_bytes;
    *result = (MarimayBytecodeStringId)(function->string_constants.length - 1);
    return true;
}

const MarimayBytecodeStringConstant *mmay_bytecode_string_constant(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeStringId string_id
) {
    return function != NULL && function->initialized
        ? mmay_vector_at_const(&function->string_constants, string_id)
        : NULL;
}

static bool mmay_bytecode_writef(MarimayWriter writer, const char *format, ...) {
    char buffer[256];
    va_list arguments;
    int length = 0;
    va_start(arguments, format);
    length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    return length >= 0 && (size_t)length < sizeof(buffer)
        && mmay_writer_write(writer, buffer, (size_t)length);
}

MarimayBytecodeDisassembleStatus mmay_bytecode_disassemble(
    const MarimayBytecodeFunction *function,
    MarimayWriter writer
) {
    size_t register_index = 0;
    size_t block_index = 0;
    if (function == NULL || writer.write == NULL) {
        return MMAY_BYTECODE_DISASSEMBLE_INVALID_ARGUMENT;
    }
    if (mmay_bytecode_verify(function) != MMAY_BYTECODE_VERIFY_OK) {
        return MMAY_BYTECODE_DISASSEMBLE_INVALID_BYTECODE;
    }
    if (!mmay_bytecode_writef(
            writer,
            "bytecode function v%d result=%s sources=%zu parameters=%zu "
            "registers=%zu blocks=%zu\n",
            MMAY_BYTECODE_FORMAT_VERSION,
            mmay_bytecode_type_name(function->result_type),
            function->source_count,
            function->parameter_count,
            function->register_types.length,
            function->blocks.length
        ) || !mmay_writer_write_cstr(writer, "registers")) {
        return MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED;
    }
    for (register_index = 0;
        register_index < function->register_types.length;
        register_index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types,
            register_index
        );
        if (!mmay_bytecode_writef(
                writer,
                " r%zu:%s",
                register_index,
                mmay_bytecode_type_name(*type)
            )) {
            return MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED;
        }
    }
    if (!mmay_writer_write_cstr(writer, "\n")) {
        return MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (!mmay_bytecode_writef(writer, "block b%zu:\n", block_index)) {
            return MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
                &block->instructions,
                instruction_index
            );
            const MarimayOpcodeMetadata *metadata = mmay_opcode_metadata(
                instruction->opcode
            );
            size_t operand_index = 0;
            if (!mmay_bytecode_writef(writer, "  %s", metadata->mnemonic)) {
                return MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED;
            }
            for (operand_index = 0; operand_index < metadata->operand_count;
                operand_index += 1) {
                const char *separator = operand_index == 0 ? " " : ", ";
                bool wrote = false;
                if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_REGISTER) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%sr%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_BLOCK) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%sb%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_CALL) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%sc%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_STRING) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%ss%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_ARRAY_CONSTRUCTION) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%sa%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_RECORD_CONSTRUCTION) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%src%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_ENUM_CONSTRUCTION) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%sec%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_INTERFACE_CONSTRUCTION) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%sic%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_I64) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%s%" PRId64,
                        separator,
                        instruction->operands[operand_index].signed_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_F64_BITS) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%s0x%016" PRIx64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_BOOL) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%s%s",
                        separator,
                        instruction->operands[operand_index].unsigned_value == 0
                            ? "false"
                            : "true"
                    );
                } else if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_U64) {
                    wrote = mmay_bytecode_writef(
                        writer,
                        "%s%" PRIu64,
                        separator,
                        instruction->operands[operand_index].unsigned_value
                    );
                }
                if (!wrote) {
                    return MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED;
                }
            }
            if (!mmay_bytecode_writef(
                    writer,
                    " @s%" PRIu64 ":%" PRIu64 "..%" PRIu64 "\n",
                    instruction->span.source,
                    instruction->span.start,
                    instruction->span.end
                )) {
                return MMAY_BYTECODE_DISASSEMBLE_WRITE_FAILED;
            }
        }
    }
    return MMAY_BYTECODE_DISASSEMBLE_OK;
}
