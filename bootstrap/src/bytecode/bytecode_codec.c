#include "mmay/bytecode/bytecode.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/utf8.h"
#include "bytecode_internal.h"
#include "bytecode_codec_internal.h"

static const unsigned char mmay_bytecode_magic[MMAY_BYTECODE_MAGIC_LENGTH] = {
    'R', 'U', 'D', 'A', 'B', 'C', 'F', 0
};

bool mmay_bytecode_codec_encode_append(
    MarimayBytecodeEncodeBuffer *buffer,
    const void *bytes,
    size_t length
) {
    size_t next = 0;
    if (buffer == NULL || (bytes == NULL && length != 0)
        || !mmay_size_add(buffer->bytes.length, length, &next)) {
        return false;
    }
    if (next > buffer->limit) {
        buffer->limit_reached = true;
        return false;
    }
    return mmay_vector_append(&buffer->bytes, bytes, length);
}

bool mmay_bytecode_codec_encode_u64(
    MarimayBytecodeEncodeBuffer *buffer,
    uint64_t value
) {
    unsigned char bytes[10];
    size_t count = 0;
    do {
        unsigned char byte = (unsigned char)(value & UINT64_C(0x7f));
        value >>= 7;
        if (value != 0) {
            byte = (unsigned char)(byte | 0x80U);
        }
        bytes[count] = byte;
        count += 1;
    } while (value != 0);
    return mmay_bytecode_codec_encode_append(buffer, bytes, count);
}

static bool mmay_bytecode_encode_i64(
    MarimayBytecodeEncodeBuffer *buffer,
    int64_t value
) {
    uint64_t encoded = 0;
    if (value < 0) {
        uint64_t magnitude = value == INT64_MIN
            ? (UINT64_C(1) << 63)
            : (uint64_t)(-value);
        encoded = magnitude * UINT64_C(2) - UINT64_C(1);
    } else {
        encoded = (uint64_t)value * UINT64_C(2);
    }
    return mmay_bytecode_codec_encode_u64(buffer, encoded);
}

MarimayBytecodeCodecStatus mmay_bytecode_codec_decode_u64(
    MarimayBytecodeCursor *cursor,
    uint64_t *result
) {
    uint64_t value = 0;
    size_t index = 0;
    if (cursor == NULL || result == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    for (index = 0; index < 10; index += 1) {
        unsigned char byte = 0;
        uint64_t payload = 0;
        if (cursor->offset >= cursor->length) {
            return MMAY_BYTECODE_CODEC_TRUNCATED;
        }
        byte = cursor->bytes[cursor->offset];
        cursor->offset += 1;
        payload = (uint64_t)(byte & 0x7fU);
        if (index == 9 && payload > 1) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        value |= payload << (index * 7);
        if ((byte & 0x80U) == 0) {
            if (index != 0 && payload == 0) {
                return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
            }
            *result = value;
            return MMAY_BYTECODE_CODEC_OK;
        }
    }
    return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
}

static MarimayBytecodeCodecStatus mmay_bytecode_decode_i64(
    MarimayBytecodeCursor *cursor,
    int64_t *result
) {
    uint64_t encoded = 0;
    uint64_t magnitude = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_codec_decode_u64(
        cursor,
        &encoded
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    magnitude = (encoded >> 1) + (encoded & UINT64_C(1));
    if ((encoded & UINT64_C(1)) == 0) {
        *result = (int64_t)magnitude;
    } else if (magnitude == (UINT64_C(1) << 63)) {
        *result = INT64_MIN;
    } else {
        *result = -(int64_t)magnitude;
    }
    return MMAY_BYTECODE_CODEC_OK;
}

bool mmay_bytecode_codec_u64_to_size(uint64_t value, size_t *result) {
    if (result == NULL || value > SIZE_MAX) {
        return false;
    }
    *result = (size_t)value;
    return true;
}

static MarimayBytecodeCodecStatus mmay_bytecode_codec_from_verify(
    MarimayBytecodeVerifyStatus status
) {
    if (status == MMAY_BYTECODE_VERIFY_OK) {
        return MMAY_BYTECODE_CODEC_OK;
    }
    if (status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
    }
    if (status == MMAY_BYTECODE_VERIFY_LIMIT_REACHED) {
        return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
    }
    return MMAY_BYTECODE_CODEC_VERIFY_FAILED;
}

MarimayBytecodeCodecStatus mmay_bytecode_encode_function(
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    MarimayOwnedBytes *output
) {
    MarimayBytecodeEncodeBuffer buffer;
    MarimayBytecodeVerifyStatus verify_status;
    MarimayBytecodeCodecStatus status = MMAY_BYTECODE_CODEC_OK;
    size_t block_index = 0;
    bool generic_arrays = function != NULL && function->array_types.length != 0;
    bool collections = function != NULL
        && function->collection_types.length != 0;
    bool typed_layouts = generic_arrays || collections;
    if (output == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    output->allocator = allocator;
    output->data = NULL;
    output->length = 0;
    if (function == NULL || !function->initialized) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    verify_status = mmay_bytecode_verify(function);
    status = mmay_bytecode_codec_from_verify(verify_status);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    memset(&buffer, 0, sizeof(buffer));
    buffer.limit = function->options.max_encoded_bytes;
    if (!mmay_vector_init(&buffer.bytes, allocator, sizeof(unsigned char))) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    if (!mmay_bytecode_codec_encode_append(
            &buffer,
            mmay_bytecode_magic,
            MMAY_BYTECODE_MAGIC_LENGTH
        ) || !mmay_bytecode_codec_encode_u64(&buffer, MMAY_BYTECODE_FORMAT_VERSION)
        || !mmay_bytecode_codec_encode_u64(
            &buffer,
            (function->calls.length == 0 ? 0 : MMAY_BYTECODE_FEATURE_CALLS)
                | (function->string_constants.length == 0
                    ? 0
                    : MMAY_BYTECODE_FEATURE_STRINGS)
                | (mmay_bytecode_function_uses_f64(function)
                    ? MMAY_BYTECODE_FEATURE_F64
                    : 0)
                | (mmay_bytecode_function_uses_arrays(function)
                    ? MMAY_BYTECODE_FEATURE_ARRAYS
                    : 0)
                | (mmay_bytecode_function_uses_nominal(function)
                    ? MMAY_BYTECODE_FEATURE_NOMINAL
                    : 0)
                | (generic_arrays ? MMAY_BYTECODE_FEATURE_ARRAY_TYPES : 0)
                | (collections ? MMAY_BYTECODE_FEATURE_COLLECTIONS : 0)
        )
        || !mmay_bytecode_codec_encode_u64(&buffer, (uint64_t)function->result_type)
        || (typed_layouts && !mmay_bytecode_codec_encode_u64(
            &buffer, (uint64_t)function->result_layout
        ))
        || !mmay_bytecode_codec_encode_u64(&buffer, (uint64_t)function->source_count)
        || !mmay_bytecode_codec_encode_u64(&buffer, (uint64_t)function->parameter_count)
        || !mmay_bytecode_codec_encode_u64(
            &buffer,
            (uint64_t)function->register_types.length
        )) {
        status = buffer.limit_reached
            ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
            : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (block_index = 0; block_index < function->register_types.length;
        block_index += 1) {
        const MarimayBytecodeType *type = mmay_vector_at_const(
            &function->register_types,
            block_index
        );
        const uint32_t *layout = mmay_vector_at_const(
            &function->register_layouts, block_index
        );
        if (type == NULL || layout == NULL
            || !mmay_bytecode_codec_encode_u64(&buffer, (uint64_t)*type)
            || (typed_layouts && !mmay_bytecode_codec_encode_u64(
                &buffer, (uint64_t)*layout
            ))) {
            status = buffer.limit_reached
                ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (generic_arrays) {
        if (!mmay_bytecode_codec_encode_u64(
                &buffer, (uint64_t)function->array_types.length
            )) {
            status = buffer.limit_reached
                ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (block_index = 0; block_index < function->array_types.length;
            block_index += 1) {
            const MarimayBytecodeArrayType *array_type = mmay_bytecode_array_type(
                function, (uint32_t)block_index
            );
            if (array_type == NULL
                || !mmay_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)array_type->element_type
                ) || !mmay_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)array_type->element_layout
                )) {
                status = buffer.limit_reached
                    ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                    : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if (collections) {
        if (!mmay_bytecode_codec_encode_u64(
                &buffer, (uint64_t)function->collection_types.length
            )) goto encode_failure;
        for (block_index = 0;
            block_index < function->collection_types.length;
            block_index += 1) {
            const MarimayBytecodeCollectionType *collection =
                mmay_bytecode_collection_type(
                    function, (uint32_t)block_index
                );
            if (collection == NULL
                || !mmay_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)collection->key_type
                ) || !mmay_bytecode_codec_encode_u64(
                    &buffer, collection->key_layout
                ) || !mmay_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)collection->value_type
                ) || !mmay_bytecode_codec_encode_u64(
                    &buffer, collection->value_layout
                ) || !mmay_bytecode_codec_encode_u64(
                    &buffer, collection->is_set ? 1U : 0U
                )) goto encode_failure;
        }
    }
    if (!mmay_bytecode_codec_encode_u64(&buffer, (uint64_t)function->blocks.length)) {
        status = buffer.limit_reached
            ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
            : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const MarimayBytecodeBlock *block = mmay_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL || !mmay_bytecode_codec_encode_u64(
                &buffer,
                (uint64_t)block->instructions.length
            )) {
            status = buffer.limit_reached
                ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayBytecodeInstruction *instruction = mmay_vector_at_const(
                &block->instructions,
                instruction_index
            );
            const MarimayOpcodeMetadata *metadata = instruction != NULL
                ? mmay_opcode_metadata(instruction->opcode)
                : NULL;
            size_t operand_index = 0;
            if (metadata == NULL || !mmay_bytecode_codec_encode_u64(
                    &buffer,
                    metadata->wire_id
                )) {
                status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            for (operand_index = 0; operand_index < metadata->operand_count;
                operand_index += 1) {
                bool wrote = metadata->operands[operand_index]
                        == MMAY_BYTECODE_OPERAND_I64
                    ? mmay_bytecode_encode_i64(
                        &buffer,
                        instruction->operands[operand_index].signed_value
                    )
                    : mmay_bytecode_codec_encode_u64(
                        &buffer,
                        instruction->operands[operand_index].unsigned_value
                    );
                if (!wrote) {
                    status = buffer.limit_reached
                        ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                        : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
            if (!mmay_bytecode_codec_encode_u64(&buffer, instruction->span.source)
                || !mmay_bytecode_codec_encode_u64(&buffer, instruction->span.start)
                || !mmay_bytecode_codec_encode_u64(&buffer, instruction->span.end)) {
                status = buffer.limit_reached
                    ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                    : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if (function->calls.length != 0) {
        size_t call_index = 0;
        if (!mmay_bytecode_codec_encode_u64(
                &buffer,
                (uint64_t)function->calls.length
            )) {
            status = buffer.limit_reached
                ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (call_index = 0; call_index < function->calls.length;
            call_index += 1) {
            const MarimayBytecodeCall *call = mmay_vector_at_const(
                &function->calls,
                call_index
            );
            size_t argument_index = 0;
            if (call == NULL || !mmay_bytecode_codec_encode_append(
                    &buffer,
                    call->callee.bytes,
                    sizeof(call->callee.bytes)
                ) || !mmay_bytecode_codec_encode_u64(
                    &buffer,
                    (uint64_t)call->result_type
                ) || (typed_layouts && !mmay_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)call->result_layout
                )) || !mmay_bytecode_codec_encode_u64(
                    &buffer,
                    (uint64_t)call->argument_count
                )) {
                status = buffer.limit_reached
                    ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                    : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const MarimayBytecodeCallArgument *argument =
                    mmay_vector_at_const(
                        &function->call_arguments,
                        call->argument_start + argument_index
                    );
                if (argument == NULL || !mmay_bytecode_codec_encode_u64(
                        &buffer,
                        argument->register_id
                    ) || !mmay_bytecode_codec_encode_u64(
                        &buffer,
                        (uint64_t)argument->type
                    ) || (typed_layouts && !mmay_bytecode_codec_encode_u64(
                        &buffer, (uint64_t)argument->layout
                    ))) {
                    status = buffer.limit_reached
                        ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                        : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
        }
    }
    if (function->string_constants.length != 0) {
        size_t string_index = 0;
        if (!mmay_bytecode_codec_encode_u64(
                &buffer,
                (uint64_t)function->string_constants.length
            )) {
            status = buffer.limit_reached
                ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (string_index = 0;
            string_index < function->string_constants.length;
            string_index += 1) {
            const MarimayBytecodeStringConstant *constant =
                mmay_bytecode_string_constant(
                    function,
                    (MarimayBytecodeStringId)string_index
                );
            if (constant == NULL || !mmay_bytecode_codec_encode_u64(
                    &buffer,
                    (uint64_t)constant->value.length
                ) || !mmay_bytecode_codec_encode_append(
                    &buffer,
                    constant->value.data,
                    constant->value.length
                )) {
                status = buffer.limit_reached
                    ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                    : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if (mmay_bytecode_function_uses_arrays(function)) {
        size_t construction_index = 0;
        if (!mmay_bytecode_codec_encode_u64(
                &buffer, (uint64_t)function->array_constructions.length
            )) {
            status = buffer.limit_reached
                ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (construction_index = 0;
            construction_index < function->array_constructions.length;
            construction_index += 1) {
            const MarimayBytecodeArrayConstruction *construction =
                mmay_bytecode_array_construction(
                    function, (MarimayBytecodeArrayConstructionId)construction_index
                );
            size_t argument_index = 0;
            if (construction == NULL || !mmay_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)construction->array_type
                ) || (generic_arrays && !mmay_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)construction->array_layout
                )) || !mmay_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)construction->argument_count
                )) {
                status = buffer.limit_reached
                    ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                    : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            for (argument_index = 0;
                argument_index < construction->argument_count;
                argument_index += 1) {
                const MarimayBytecodeArrayArgument *argument =
                    mmay_bytecode_array_argument(
                        function, construction->argument_start + argument_index
                    );
                if (argument == NULL || !mmay_bytecode_codec_encode_u64(
                        &buffer, argument->register_id
                    ) || !mmay_bytecode_codec_encode_u64(
                        &buffer, (uint64_t)argument->type
                    ) || (typed_layouts && !mmay_bytecode_codec_encode_u64(
                        &buffer, (uint64_t)argument->layout
                    ))) {
                    status = buffer.limit_reached
                        ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                        : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
        }
    }
    if (mmay_bytecode_function_uses_nominal(function)) {
        status = mmay_bytecode_encode_nominal_metadata(function, &buffer);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
    }
    goto encode_complete;
encode_failure:
    status = buffer.limit_reached
        ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
        : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
    goto cleanup;
encode_complete:
    if (!mmay_owned_bytes_copy(
            output,
            allocator,
            buffer.bytes.data,
            buffer.bytes.length
        )) {
        status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
    }

cleanup:
    mmay_vector_dispose(&buffer.bytes);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        mmay_owned_bytes_dispose(output);
    }
    return status;
}

MarimayBytecodeCodecStatus mmay_bytecode_decode_function(
    const unsigned char *bytes,
    size_t length,
    MarimayAllocator allocator,
    MarimayBytecodeFunctionOptions options,
    MarimayBytecodeFunction *output
) {
    MarimayBytecodeCursor cursor;
    MarimayBytecodeFunction function;
    MarimayBytecodeCodecStatus status = MMAY_BYTECODE_CODEC_OK;
    uint64_t value = 0;
    uint64_t features = 0;
    bool generic_arrays = false;
    bool collections = false;
    bool typed_layouts = false;
    size_t register_count = 0;
    size_t block_count = 0;
    size_t index = 0;
    if (output == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    if (bytes == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    memset(&function, 0, sizeof(function));
    if (length > options.max_encoded_bytes) {
        return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
    }
    if (length < MMAY_BYTECODE_MAGIC_LENGTH) {
        return MMAY_BYTECODE_CODEC_TRUNCATED;
    }
    if (memcmp(bytes, mmay_bytecode_magic, MMAY_BYTECODE_MAGIC_LENGTH) != 0) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    cursor.bytes = bytes;
    cursor.length = length;
    cursor.offset = MMAY_BYTECODE_MAGIC_LENGTH;

    status = mmay_bytecode_codec_decode_u64(&cursor, &value);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value != MMAY_BYTECODE_FORMAT_VERSION) {
        return MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    status = mmay_bytecode_codec_decode_u64(&cursor, &value);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if ((value & ~(uint64_t)(
            MMAY_BYTECODE_FEATURE_CALLS | MMAY_BYTECODE_FEATURE_STRINGS
                | MMAY_BYTECODE_FEATURE_F64 | MMAY_BYTECODE_FEATURE_ARRAYS
                | MMAY_BYTECODE_FEATURE_NOMINAL
                | MMAY_BYTECODE_FEATURE_ARRAY_TYPES
                | MMAY_BYTECODE_FEATURE_COLLECTIONS
        )) != 0) {
        return MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    if ((value & MMAY_BYTECODE_FEATURE_ARRAY_TYPES) != 0
        && (value & MMAY_BYTECODE_FEATURE_ARRAYS) == 0) {
        return MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    features = value;
    generic_arrays = (features & MMAY_BYTECODE_FEATURE_ARRAY_TYPES) != 0;
    collections = (features & MMAY_BYTECODE_FEATURE_COLLECTIONS) != 0;
    typed_layouts = generic_arrays || collections;
    status = mmay_bytecode_codec_decode_u64(&cursor, &value);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if ((value > (collections ? MMAY_BYTECODE_TYPE_SET
            : ((features & MMAY_BYTECODE_FEATURE_NOMINAL) != 0
            ? MMAY_BYTECODE_TYPE_INTERFACE
            : MMAY_BYTECODE_TYPE_ARRAY))
            && !mmay_bytecode_type_is_extended_scalar((MarimayBytecodeType)value))
        || !mmay_bytecode_type_is_result((MarimayBytecodeType)value)) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    {
        MarimayBytecodeType result_type = (MarimayBytecodeType)value;
        uint64_t result_layout_wire = 0;
        uint64_t source_count_wire = 0;
        uint64_t parameter_count_wire = 0;
        uint64_t register_count_wire = 0;
        size_t source_count = 0;
        size_t parameter_count = 0;
        if (typed_layouts) {
            status = mmay_bytecode_codec_decode_u64(
                &cursor, &result_layout_wire
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_codec_decode_u64(
                &cursor, &source_count_wire
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_codec_decode_u64(&cursor, &parameter_count_wire);
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_codec_decode_u64(&cursor, &register_count_wire);
        }
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!mmay_bytecode_codec_u64_to_size(source_count_wire, &source_count)
            || !mmay_bytecode_codec_u64_to_size(
                parameter_count_wire,
                &parameter_count
            ) || !mmay_bytecode_codec_u64_to_size(
                register_count_wire,
                &register_count
            ) || source_count == 0 || source_count > options.max_source_files
            || parameter_count > register_count
            || register_count > options.max_registers
            || register_count > UINT32_MAX) {
            return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (!mmay_bytecode_function_init(
                &function,
                allocator,
                result_type,
                source_count,
                NULL,
                0,
                options
            )) {
            return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
        }
        function.parameter_count = parameter_count;
        if (typed_layouts && result_layout_wire > UINT32_MAX) {
            mmay_bytecode_function_dispose(&function);
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (typed_layouts) {
            function.result_layout = (uint32_t)result_layout_wire;
        }
    }
    for (index = 0; index < register_count; index += 1) {
        MarimayBytecodeType type;
        uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
        status = mmay_bytecode_codec_decode_u64(&cursor, &value);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if ((value > (collections ? MMAY_BYTECODE_TYPE_SET
                : ((features & MMAY_BYTECODE_FEATURE_NOMINAL) != 0
                ? MMAY_BYTECODE_TYPE_INTERFACE
                : MMAY_BYTECODE_TYPE_ARRAY))
                && !mmay_bytecode_type_is_extended_scalar((MarimayBytecodeType)value))
            || !mmay_bytecode_type_is_local((MarimayBytecodeType)value)
            || (index < function.parameter_count
                && !mmay_bytecode_type_is_register((MarimayBytecodeType)value))) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
            goto cleanup;
        }
        type = (MarimayBytecodeType)value;
        if (typed_layouts) {
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            layout = (uint32_t)value;
        }
        if (!mmay_vector_push(&function.register_types, &type)
            || !mmay_vector_push(&function.register_layouts, &layout)) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (generic_arrays) {
        status = mmay_bytecode_codec_decode_u64(&cursor, &value);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
    }
    {
        size_t array_type_count = 0;
        if (generic_arrays && (!mmay_bytecode_codec_u64_to_size(
                value, &array_type_count)
            || array_type_count > options.max_array_constructions
            || array_type_count > UINT32_MAX)) {
            status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        if (generic_arrays && !mmay_vector_reserve(
                &function.array_types, array_type_count
            )) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (index = 0; generic_arrays && index < array_type_count;
            index += 1) {
            MarimayBytecodeType element_type;
            uint32_t element_layout = MMAY_BYTECODE_NO_LAYOUT;
            uint32_t array_layout = UINT32_MAX;
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK
                || !mmay_bytecode_type_is_register(
                    (MarimayBytecodeType)value
                )) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            element_type = (MarimayBytecodeType)value;
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            element_layout = (uint32_t)value;
            if (!mmay_bytecode_add_array_type(
                    &function, element_type, element_layout, &array_layout
                ) || array_layout != index) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
        }
    }
    if (collections) {
        size_t collection_count = 0;
        status = mmay_bytecode_codec_decode_u64(&cursor, &value);
        if (status != MMAY_BYTECODE_CODEC_OK
            || !mmay_bytecode_codec_u64_to_size(value, &collection_count)
            || collection_count > options.max_collection_layouts
            || collection_count > UINT32_MAX
            || !mmay_vector_reserve(
                &function.collection_types, collection_count
            )) {
            status = status == MMAY_BYTECODE_CODEC_OK
                ? MMAY_BYTECODE_CODEC_LIMIT_REACHED : status;
            goto cleanup;
        }
        for (index = 0; index < collection_count; index += 1) {
            MarimayBytecodeCollectionType collection;
            uint64_t fields[5];
            size_t field = 0;
            memset(&collection, 0, sizeof(collection));
            for (field = 0; field < 5; field += 1) {
                status = mmay_bytecode_codec_decode_u64(
                    &cursor, &fields[field]
                );
                if (status != MMAY_BYTECODE_CODEC_OK) break;
            }
            if (status != MMAY_BYTECODE_CODEC_OK) goto cleanup;
            if (fields[0] > MMAY_BYTECODE_TYPE_RUNE
                || fields[1] > UINT32_MAX
                || fields[2] > MMAY_BYTECODE_TYPE_RUNE
                || fields[3] > UINT32_MAX || fields[4] > 1
                || !mmay_bytecode_type_is_register(
                    (MarimayBytecodeType)fields[0]
                ) || (fields[4] != 0
                    ? (fields[2] != MMAY_BYTECODE_TYPE_INVALID
                        || fields[3] != MMAY_BYTECODE_NO_LAYOUT)
                    : !mmay_bytecode_type_is_register(
                        (MarimayBytecodeType)fields[2]
                    ))) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            collection.key_type = (MarimayBytecodeType)fields[0];
            collection.key_layout = (uint32_t)fields[1];
            collection.value_type = (MarimayBytecodeType)fields[2];
            collection.value_layout = (uint32_t)fields[3];
            collection.is_set = fields[4] != 0;
            if (!mmay_vector_push(
                    &function.collection_types, &collection
                )) {
                status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    status = mmay_bytecode_codec_decode_u64(&cursor, &value);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    if (!mmay_bytecode_codec_u64_to_size(value, &block_count)
        || block_count == 0 || block_count > options.max_blocks
        || block_count > UINT32_MAX) {
        status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        goto cleanup;
    }
    for (index = 0; index < block_count; index += 1) {
        uint32_t block_id = 0;
        size_t instruction_count = 0;
        size_t instruction_index = 0;
        size_t next_total = 0;
        status = mmay_bytecode_codec_decode_u64(&cursor, &value);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!mmay_bytecode_codec_u64_to_size(value, &instruction_count)
            || instruction_count == 0
            || !mmay_size_add(
                function.instruction_count,
                instruction_count,
                &next_total
            ) || next_total > options.max_instructions) {
            status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        if (!mmay_bytecode_add_block(&function, &block_id)) {
            status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (instruction_index = 0;
            instruction_index < instruction_count;
            instruction_index += 1) {
            MarimayBytecodeInstruction instruction;
            const MarimayOpcodeMetadata *metadata = NULL;
            size_t operand_index = 0;
            memset(&instruction, 0, sizeof(instruction));
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            metadata = mmay_opcode_metadata_from_wire(value);
            if (metadata == NULL
                || (metadata->flags & MMAY_OPCODE_FLAG_INVALID) != 0) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            instruction.opcode = metadata->opcode;
            for (operand_index = 0; operand_index < metadata->operand_count;
                operand_index += 1) {
                if (metadata->operands[operand_index]
                    == MMAY_BYTECODE_OPERAND_I64) {
                    status = mmay_bytecode_decode_i64(
                        &cursor,
                        &instruction.operands[operand_index].signed_value
                    );
                } else {
                    status = mmay_bytecode_codec_decode_u64(
                        &cursor,
                        &instruction.operands[operand_index].unsigned_value
                    );
                }
                if (status != MMAY_BYTECODE_CODEC_OK) {
                    goto cleanup;
                }
            }
            status = mmay_bytecode_codec_decode_u64(
                &cursor,
                &instruction.span.source
            );
            if (status == MMAY_BYTECODE_CODEC_OK) {
                status = mmay_bytecode_codec_decode_u64(
                    &cursor,
                    &instruction.span.start
                );
            }
            if (status == MMAY_BYTECODE_CODEC_OK) {
                status = mmay_bytecode_codec_decode_u64(
                    &cursor,
                    &instruction.span.end
                );
            }
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!mmay_bytecode_append_instruction(
                    &function,
                    block_id,
                    &instruction
                )) {
                status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if ((features & MMAY_BYTECODE_FEATURE_CALLS) != 0) {
        size_t call_count = 0;
        status = mmay_bytecode_codec_decode_u64(&cursor, &value);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!mmay_bytecode_codec_u64_to_size(value, &call_count)
            || call_count > options.max_calls || call_count > UINT32_MAX) {
            status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        if (call_count == 0) {
            status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
            goto cleanup;
        }
        for (index = 0; index < call_count; index += 1) {
            MarimaySymbolId callee;
            MarimayBytecodeType result_type;
            uint32_t result_layout = MMAY_BYTECODE_NO_LAYOUT;
            size_t argument_count = 0;
            size_t argument_bytes = 0;
            size_t argument_index = 0;
            MarimayBytecodeCallArgument *arguments = NULL;
            MarimayBytecodeCallId call_id = MMAY_BYTECODE_NO_CALL;
            if (cursor.length - cursor.offset < sizeof(callee.bytes)) {
                status = MMAY_BYTECODE_CODEC_TRUNCATED;
                goto cleanup;
            }
            memcpy(
                callee.bytes,
                cursor.bytes + cursor.offset,
                sizeof(callee.bytes)
            );
            cursor.offset += sizeof(callee.bytes);
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if ((value > (collections ? MMAY_BYTECODE_TYPE_SET
                    : ((features & MMAY_BYTECODE_FEATURE_NOMINAL) != 0
                    ? MMAY_BYTECODE_TYPE_INTERFACE
                    : MMAY_BYTECODE_TYPE_ARRAY))
                    && !mmay_bytecode_type_is_extended_scalar((MarimayBytecodeType)value))
                || !mmay_bytecode_type_is_result((MarimayBytecodeType)value)) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            result_type = (MarimayBytecodeType)value;
            if (typed_layouts) {
                status = mmay_bytecode_codec_decode_u64(&cursor, &value);
                if (status != MMAY_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                    status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                    goto cleanup;
                }
                result_layout = (uint32_t)value;
            }
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!mmay_bytecode_codec_u64_to_size(value, &argument_count)
                || argument_count > options.max_call_arguments
                || !mmay_size_add(
                    function.call_arguments.length,
                    argument_count,
                    &argument_bytes
                ) || argument_bytes > options.max_call_arguments
                || !mmay_size_multiply(
                    argument_count,
                    sizeof(MarimayBytecodeCallArgument),
                    &argument_bytes
                )) {
                status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (argument_bytes != 0) {
                arguments = mmay_allocate(&allocator, argument_bytes);
                if (arguments == NULL) {
                    status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
            if (argument_count != 0 && arguments == NULL) {
                status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            for (argument_index = 0; argument_index < argument_count;
                argument_index += 1) {
                arguments[argument_index].layout = MMAY_BYTECODE_NO_LAYOUT;
            }
            for (argument_index = 0; argument_index < argument_count;
                argument_index += 1) {
                if (arguments == NULL) {
                    status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                    break;
                }
                status = mmay_bytecode_codec_decode_u64(&cursor, &value);
                if (status != MMAY_BYTECODE_CODEC_OK) {
                    break;
                }
                if (value >= function.register_types.length
                    || value > UINT32_MAX) {
                    status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                    break;
                }
                arguments[argument_index].register_id = (uint32_t)value;
                status = mmay_bytecode_codec_decode_u64(&cursor, &value);
                if (status != MMAY_BYTECODE_CODEC_OK) {
                    break;
                }
                if ((value > (collections ? MMAY_BYTECODE_TYPE_SET
                        : ((features & MMAY_BYTECODE_FEATURE_NOMINAL) != 0
                        ? MMAY_BYTECODE_TYPE_INTERFACE
                        : MMAY_BYTECODE_TYPE_ARRAY))
                        && !mmay_bytecode_type_is_extended_scalar((MarimayBytecodeType)value))
                    || !mmay_bytecode_type_is_register(
                        (MarimayBytecodeType)value
                    )) {
                    status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                    break;
                }
                arguments[argument_index].type = (MarimayBytecodeType)value;
                if (typed_layouts) {
                    status = mmay_bytecode_codec_decode_u64(&cursor, &value);
                    if (status != MMAY_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                        status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                        break;
                    }
                    arguments[argument_index].layout = (uint32_t)value;
                }
                {
                    const MarimayBytecodeType *register_type =
                        mmay_vector_at_const(
                            &function.register_types,
                            arguments[argument_index].register_id
                        );
                    if (register_type == NULL
                        || *register_type != arguments[argument_index].type
                        || (typed_layouts && mmay_bytecode_register_layout(
                            &function,
                            arguments[argument_index].register_id
                        ) != arguments[argument_index].layout)) {
                        status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                        break;
                    }
                }
            }
            if (status == MMAY_BYTECODE_CODEC_OK
                && !mmay_bytecode_add_call(
                    &function,
                    callee,
                    result_type,
                    arguments,
                    argument_count,
                    &call_id
                )) {
                status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
            if (status == MMAY_BYTECODE_CODEC_OK) {
                MarimayBytecodeCall *call = mmay_vector_at(
                    &function.calls, call_id
                );
                if (call == NULL) {
                    status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                } else {
                    call->result_layout = result_layout;
                }
            }
            mmay_deallocate(
                &allocator,
                arguments,
                argument_bytes
            );
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if ((size_t)call_id != index) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
        }
    }
    if ((features & MMAY_BYTECODE_FEATURE_STRINGS) != 0) {
        size_t string_count = 0;
        status = mmay_bytecode_codec_decode_u64(&cursor, &value);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!mmay_bytecode_codec_u64_to_size(value, &string_count)
            || string_count == 0 || string_count > options.max_string_constants
            || string_count > UINT32_MAX) {
            status = string_count == 0
                ? MMAY_BYTECODE_CODEC_INVALID_ENCODING
                : MMAY_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        for (index = 0; index < string_count; index += 1) {
            size_t string_length = 0;
            MarimayBytecodeStringId string_id = MMAY_BYTECODE_NO_STRING;
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!mmay_bytecode_codec_u64_to_size(value, &string_length)
                || string_length > options.max_string_bytes
                || string_length > cursor.length - cursor.offset) {
                status = string_length > cursor.length - cursor.offset
                    ? MMAY_BYTECODE_CODEC_TRUNCATED
                    : MMAY_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (!mmay_utf8_validate(
                    (const char *)(cursor.bytes + cursor.offset),
                    string_length,
                    NULL
                )) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            if (!mmay_bytecode_add_string_constant(
                    &function,
                    cursor.bytes + cursor.offset,
                    string_length,
                    &string_id
                )) {
                size_t next_bytes = 0;
                status = !mmay_size_add(
                        function.string_bytes,
                        string_length,
                        &next_bytes
                    ) || next_bytes > options.max_string_bytes
                    ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                    : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            if ((size_t)string_id != index) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            cursor.offset += string_length;
        }
    }
    if ((features & MMAY_BYTECODE_FEATURE_ARRAYS) != 0) {
        size_t construction_count = 0;
        status = mmay_bytecode_codec_decode_u64(&cursor, &value);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!mmay_bytecode_codec_u64_to_size(value, &construction_count)
            || construction_count > options.max_array_constructions
            || construction_count > UINT32_MAX) {
            status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        for (index = 0; index < construction_count; index += 1) {
            MarimayBytecodeType array_type;
            MarimayBytecodeType element_type;
            size_t argument_count = 0;
            size_t argument_bytes = 0;
            size_t argument_index = 0;
            MarimayBytecodeArrayArgument *arguments = NULL;
            MarimayBytecodeArrayConstructionId construction_id =
                MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
            uint32_t array_layout = MMAY_BYTECODE_NO_LAYOUT;
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (value > MMAY_BYTECODE_TYPE_ARRAY
                || !mmay_bytecode_type_is_array((MarimayBytecodeType)value)) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            array_type = (MarimayBytecodeType)value;
            if (generic_arrays) {
                status = mmay_bytecode_codec_decode_u64(&cursor, &value);
                if (status != MMAY_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                    status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                    goto cleanup;
                }
                array_layout = (uint32_t)value;
            }
            element_type = mmay_bytecode_function_array_element_type(
                &function, array_type, array_layout, NULL
            );
            if (element_type == MMAY_BYTECODE_TYPE_INVALID) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            status = mmay_bytecode_codec_decode_u64(&cursor, &value);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!mmay_bytecode_codec_u64_to_size(value, &argument_count)
                || argument_count > options.max_array_arguments
                || !mmay_size_add(
                    function.array_arguments.length, argument_count,
                    &argument_bytes
                ) || argument_bytes > options.max_array_arguments
                || !mmay_size_multiply(
                    argument_count, sizeof(MarimayBytecodeArrayArgument),
                    &argument_bytes
                )) {
                status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (argument_bytes != 0) {
                arguments = mmay_allocate(&allocator, argument_bytes);
                if (arguments == NULL) {
                    status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
            for (argument_index = 0; argument_index < argument_count;
                argument_index += 1) {
                MarimayBytecodeArrayArgument *argument = arguments != NULL
                    ? &arguments[argument_index]
                    : NULL;
                if (argument == NULL) {
                    status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                    break;
                }
                status = mmay_bytecode_codec_decode_u64(&cursor, &value);
                if (status != MMAY_BYTECODE_CODEC_OK) {
                    break;
                }
                if (value >= function.register_types.length
                    || value > UINT32_MAX) {
                    status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                    break;
                }
                argument->register_id = (uint32_t)value;
                status = mmay_bytecode_codec_decode_u64(&cursor, &value);
                if (status != MMAY_BYTECODE_CODEC_OK) {
                    break;
                }
                if (value != (uint64_t)element_type) {
                    status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                    break;
                }
                argument->type = (MarimayBytecodeType)value;
                if (typed_layouts) {
                    status = mmay_bytecode_codec_decode_u64(&cursor, &value);
                    if (status != MMAY_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                        status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                        break;
                    }
                    argument->layout = (uint32_t)value;
                }
                {
                    const MarimayBytecodeType *register_type =
                        mmay_vector_at_const(
                            &function.register_types,
                            argument->register_id
                        );
                    if (register_type == NULL
                        || *register_type != element_type
                        || (typed_layouts && (array_type == MMAY_BYTECODE_TYPE_ARRAY
                            && mmay_bytecode_register_layout(
                                &function, argument->register_id
                            ) != argument->layout))) {
                        status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                        break;
                    }
                }
            }
            if (status == MMAY_BYTECODE_CODEC_OK
                && !mmay_bytecode_add_array_construction_with_layout(
                    &function, array_type, array_layout,
                    arguments, argument_count,
                    &construction_id
                )) {
                status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
            mmay_deallocate(&allocator, arguments, argument_bytes);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if ((size_t)construction_id != index) {
                status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
        }
    }
    if ((features & MMAY_BYTECODE_FEATURE_NOMINAL) != 0) {
        status = mmay_bytecode_decode_nominal_metadata(&function, &cursor);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
    }
    if (((features & MMAY_BYTECODE_FEATURE_F64) != 0)
        != mmay_bytecode_function_uses_f64(&function)) {
        status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        goto cleanup;
    }
    if (((features & MMAY_BYTECODE_FEATURE_ARRAYS) != 0)
        != mmay_bytecode_function_uses_arrays(&function)) {
        status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        goto cleanup;
    }
    if (((features & MMAY_BYTECODE_FEATURE_NOMINAL) != 0)
        != mmay_bytecode_function_uses_nominal(&function)) {
        status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        goto cleanup;
    }
    if (collections != (function.collection_types.length != 0)) {
        status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        goto cleanup;
    }
    if (cursor.offset != cursor.length) {
        status = MMAY_BYTECODE_CODEC_TRAILING_DATA;
        goto cleanup;
    }
    status = mmay_bytecode_codec_from_verify(mmay_bytecode_verify(&function));
    if (status != MMAY_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    *output = function;
    memset(&function, 0, sizeof(function));
    return MMAY_BYTECODE_CODEC_OK;

cleanup:
    mmay_bytecode_function_dispose(&function);
    return status;
}
