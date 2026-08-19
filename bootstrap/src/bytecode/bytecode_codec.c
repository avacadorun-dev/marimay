#include "ruda/bytecode/bytecode.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/utf8.h"
#include "bytecode_internal.h"
#include "bytecode_codec_internal.h"

static const unsigned char ruda_bytecode_magic[RUDA_BYTECODE_MAGIC_LENGTH] = {
    'R', 'U', 'D', 'A', 'B', 'C', 'F', 0
};

bool ruda_bytecode_codec_encode_append(
    RudaBytecodeEncodeBuffer *buffer,
    const void *bytes,
    size_t length
) {
    size_t next = 0;
    if (buffer == NULL || (bytes == NULL && length != 0)
        || !ruda_size_add(buffer->bytes.length, length, &next)) {
        return false;
    }
    if (next > buffer->limit) {
        buffer->limit_reached = true;
        return false;
    }
    return ruda_vector_append(&buffer->bytes, bytes, length);
}

bool ruda_bytecode_codec_encode_u64(
    RudaBytecodeEncodeBuffer *buffer,
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
    return ruda_bytecode_codec_encode_append(buffer, bytes, count);
}

static bool ruda_bytecode_encode_i64(
    RudaBytecodeEncodeBuffer *buffer,
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
    return ruda_bytecode_codec_encode_u64(buffer, encoded);
}

RudaBytecodeCodecStatus ruda_bytecode_codec_decode_u64(
    RudaBytecodeCursor *cursor,
    uint64_t *result
) {
    uint64_t value = 0;
    size_t index = 0;
    if (cursor == NULL || result == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    for (index = 0; index < 10; index += 1) {
        unsigned char byte = 0;
        uint64_t payload = 0;
        if (cursor->offset >= cursor->length) {
            return RUDA_BYTECODE_CODEC_TRUNCATED;
        }
        byte = cursor->bytes[cursor->offset];
        cursor->offset += 1;
        payload = (uint64_t)(byte & 0x7fU);
        if (index == 9 && payload > 1) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        value |= payload << (index * 7);
        if ((byte & 0x80U) == 0) {
            if (index != 0 && payload == 0) {
                return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
            }
            *result = value;
            return RUDA_BYTECODE_CODEC_OK;
        }
    }
    return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
}

static RudaBytecodeCodecStatus ruda_bytecode_decode_i64(
    RudaBytecodeCursor *cursor,
    int64_t *result
) {
    uint64_t encoded = 0;
    uint64_t magnitude = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_codec_decode_u64(
        cursor,
        &encoded
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
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
    return RUDA_BYTECODE_CODEC_OK;
}

bool ruda_bytecode_codec_u64_to_size(uint64_t value, size_t *result) {
    if (result == NULL || value > SIZE_MAX) {
        return false;
    }
    *result = (size_t)value;
    return true;
}

static RudaBytecodeCodecStatus ruda_bytecode_codec_from_verify(
    RudaBytecodeVerifyStatus status
) {
    if (status == RUDA_BYTECODE_VERIFY_OK) {
        return RUDA_BYTECODE_CODEC_OK;
    }
    if (status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED) {
        return RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
    }
    if (status == RUDA_BYTECODE_VERIFY_LIMIT_REACHED) {
        return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
    }
    return RUDA_BYTECODE_CODEC_VERIFY_FAILED;
}

RudaBytecodeCodecStatus ruda_bytecode_encode_function(
    const RudaBytecodeFunction *function,
    RudaAllocator allocator,
    RudaOwnedBytes *output
) {
    RudaBytecodeEncodeBuffer buffer;
    RudaBytecodeVerifyStatus verify_status;
    RudaBytecodeCodecStatus status = RUDA_BYTECODE_CODEC_OK;
    size_t block_index = 0;
    bool generic_arrays = function != NULL && function->array_types.length != 0;
    if (output == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    output->allocator = allocator;
    output->data = NULL;
    output->length = 0;
    if (function == NULL || !function->initialized) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    verify_status = ruda_bytecode_verify(function);
    status = ruda_bytecode_codec_from_verify(verify_status);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    memset(&buffer, 0, sizeof(buffer));
    buffer.limit = function->options.max_encoded_bytes;
    if (!ruda_vector_init(&buffer.bytes, allocator, sizeof(unsigned char))) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    if (!ruda_bytecode_codec_encode_append(
            &buffer,
            ruda_bytecode_magic,
            RUDA_BYTECODE_MAGIC_LENGTH
        ) || !ruda_bytecode_codec_encode_u64(&buffer, RUDA_BYTECODE_FORMAT_VERSION)
        || !ruda_bytecode_codec_encode_u64(
            &buffer,
            (function->calls.length == 0 ? 0 : RUDA_BYTECODE_FEATURE_CALLS)
                | (function->string_constants.length == 0
                    ? 0
                    : RUDA_BYTECODE_FEATURE_STRINGS)
                | (ruda_bytecode_function_uses_f64(function)
                    ? RUDA_BYTECODE_FEATURE_F64
                    : 0)
                | (ruda_bytecode_function_uses_arrays(function)
                    ? RUDA_BYTECODE_FEATURE_ARRAYS
                    : 0)
                | (ruda_bytecode_function_uses_nominal(function)
                    ? RUDA_BYTECODE_FEATURE_NOMINAL
                    : 0)
                | (generic_arrays ? RUDA_BYTECODE_FEATURE_ARRAY_TYPES : 0)
        )
        || !ruda_bytecode_codec_encode_u64(&buffer, (uint64_t)function->result_type)
        || (generic_arrays && !ruda_bytecode_codec_encode_u64(
            &buffer, (uint64_t)function->result_layout
        ))
        || !ruda_bytecode_codec_encode_u64(&buffer, (uint64_t)function->source_count)
        || !ruda_bytecode_codec_encode_u64(&buffer, (uint64_t)function->parameter_count)
        || !ruda_bytecode_codec_encode_u64(
            &buffer,
            (uint64_t)function->register_types.length
        )) {
        status = buffer.limit_reached
            ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
            : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (block_index = 0; block_index < function->register_types.length;
        block_index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types,
            block_index
        );
        const uint32_t *layout = ruda_vector_at_const(
            &function->register_layouts, block_index
        );
        if (type == NULL || layout == NULL
            || !ruda_bytecode_codec_encode_u64(&buffer, (uint64_t)*type)
            || (generic_arrays && !ruda_bytecode_codec_encode_u64(
                &buffer, (uint64_t)*layout
            ))) {
            status = buffer.limit_reached
                ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (generic_arrays) {
        if (!ruda_bytecode_codec_encode_u64(
                &buffer, (uint64_t)function->array_types.length
            )) {
            status = buffer.limit_reached
                ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (block_index = 0; block_index < function->array_types.length;
            block_index += 1) {
            const RudaBytecodeArrayType *array_type = ruda_bytecode_array_type(
                function, (uint32_t)block_index
            );
            if (array_type == NULL
                || !ruda_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)array_type->element_type
                ) || !ruda_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)array_type->element_layout
                )) {
                status = buffer.limit_reached
                    ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                    : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if (!ruda_bytecode_codec_encode_u64(&buffer, (uint64_t)function->blocks.length)) {
        status = buffer.limit_reached
            ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
            : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL || !ruda_bytecode_codec_encode_u64(
                &buffer,
                (uint64_t)block->instructions.length
            )) {
            status = buffer.limit_reached
                ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (instruction_index = 0;
            instruction_index < block->instructions.length;
            instruction_index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            const RudaOpcodeMetadata *metadata = instruction != NULL
                ? ruda_opcode_metadata(instruction->opcode)
                : NULL;
            size_t operand_index = 0;
            if (metadata == NULL || !ruda_bytecode_codec_encode_u64(
                    &buffer,
                    metadata->wire_id
                )) {
                status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            for (operand_index = 0; operand_index < metadata->operand_count;
                operand_index += 1) {
                bool wrote = metadata->operands[operand_index]
                        == RUDA_BYTECODE_OPERAND_I64
                    ? ruda_bytecode_encode_i64(
                        &buffer,
                        instruction->operands[operand_index].signed_value
                    )
                    : ruda_bytecode_codec_encode_u64(
                        &buffer,
                        instruction->operands[operand_index].unsigned_value
                    );
                if (!wrote) {
                    status = buffer.limit_reached
                        ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                        : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
            if (!ruda_bytecode_codec_encode_u64(&buffer, instruction->span.source)
                || !ruda_bytecode_codec_encode_u64(&buffer, instruction->span.start)
                || !ruda_bytecode_codec_encode_u64(&buffer, instruction->span.end)) {
                status = buffer.limit_reached
                    ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                    : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if (function->calls.length != 0) {
        size_t call_index = 0;
        if (!ruda_bytecode_codec_encode_u64(
                &buffer,
                (uint64_t)function->calls.length
            )) {
            status = buffer.limit_reached
                ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (call_index = 0; call_index < function->calls.length;
            call_index += 1) {
            const RudaBytecodeCall *call = ruda_vector_at_const(
                &function->calls,
                call_index
            );
            size_t argument_index = 0;
            if (call == NULL || !ruda_bytecode_codec_encode_append(
                    &buffer,
                    call->callee.bytes,
                    sizeof(call->callee.bytes)
                ) || !ruda_bytecode_codec_encode_u64(
                    &buffer,
                    (uint64_t)call->result_type
                ) || (generic_arrays && !ruda_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)call->result_layout
                )) || !ruda_bytecode_codec_encode_u64(
                    &buffer,
                    (uint64_t)call->argument_count
                )) {
                status = buffer.limit_reached
                    ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                    : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const RudaBytecodeCallArgument *argument =
                    ruda_vector_at_const(
                        &function->call_arguments,
                        call->argument_start + argument_index
                    );
                if (argument == NULL || !ruda_bytecode_codec_encode_u64(
                        &buffer,
                        argument->register_id
                    ) || !ruda_bytecode_codec_encode_u64(
                        &buffer,
                        (uint64_t)argument->type
                    ) || (generic_arrays && !ruda_bytecode_codec_encode_u64(
                        &buffer, (uint64_t)argument->layout
                    ))) {
                    status = buffer.limit_reached
                        ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                        : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
        }
    }
    if (function->string_constants.length != 0) {
        size_t string_index = 0;
        if (!ruda_bytecode_codec_encode_u64(
                &buffer,
                (uint64_t)function->string_constants.length
            )) {
            status = buffer.limit_reached
                ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (string_index = 0;
            string_index < function->string_constants.length;
            string_index += 1) {
            const RudaBytecodeStringConstant *constant =
                ruda_bytecode_string_constant(
                    function,
                    (RudaBytecodeStringId)string_index
                );
            if (constant == NULL || !ruda_bytecode_codec_encode_u64(
                    &buffer,
                    (uint64_t)constant->value.length
                ) || !ruda_bytecode_codec_encode_append(
                    &buffer,
                    constant->value.data,
                    constant->value.length
                )) {
                status = buffer.limit_reached
                    ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                    : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if (ruda_bytecode_function_uses_arrays(function)) {
        size_t construction_index = 0;
        if (!ruda_bytecode_codec_encode_u64(
                &buffer, (uint64_t)function->array_constructions.length
            )) {
            status = buffer.limit_reached
                ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (construction_index = 0;
            construction_index < function->array_constructions.length;
            construction_index += 1) {
            const RudaBytecodeArrayConstruction *construction =
                ruda_bytecode_array_construction(
                    function, (RudaBytecodeArrayConstructionId)construction_index
                );
            size_t argument_index = 0;
            if (construction == NULL || !ruda_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)construction->array_type
                ) || (generic_arrays && !ruda_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)construction->array_layout
                )) || !ruda_bytecode_codec_encode_u64(
                    &buffer, (uint64_t)construction->argument_count
                )) {
                status = buffer.limit_reached
                    ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                    : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            for (argument_index = 0;
                argument_index < construction->argument_count;
                argument_index += 1) {
                const RudaBytecodeArrayArgument *argument =
                    ruda_bytecode_array_argument(
                        function, construction->argument_start + argument_index
                    );
                if (argument == NULL || !ruda_bytecode_codec_encode_u64(
                        &buffer, argument->register_id
                    ) || !ruda_bytecode_codec_encode_u64(
                        &buffer, (uint64_t)argument->type
                    ) || (generic_arrays && !ruda_bytecode_codec_encode_u64(
                        &buffer, (uint64_t)argument->layout
                    ))) {
                    status = buffer.limit_reached
                        ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                        : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
        }
    }
    if (ruda_bytecode_function_uses_nominal(function)) {
        status = ruda_bytecode_encode_nominal_metadata(function, &buffer);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
    }
    if (!ruda_owned_bytes_copy(
            output,
            allocator,
            buffer.bytes.data,
            buffer.bytes.length
        )) {
        status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
    }

cleanup:
    ruda_vector_dispose(&buffer.bytes);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        ruda_owned_bytes_dispose(output);
    }
    return status;
}

RudaBytecodeCodecStatus ruda_bytecode_decode_function(
    const unsigned char *bytes,
    size_t length,
    RudaAllocator allocator,
    RudaBytecodeFunctionOptions options,
    RudaBytecodeFunction *output
) {
    RudaBytecodeCursor cursor;
    RudaBytecodeFunction function;
    RudaBytecodeCodecStatus status = RUDA_BYTECODE_CODEC_OK;
    uint64_t value = 0;
    uint64_t features = 0;
    bool generic_arrays = false;
    size_t register_count = 0;
    size_t block_count = 0;
    size_t index = 0;
    if (output == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    if (bytes == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    memset(&function, 0, sizeof(function));
    if (length > options.max_encoded_bytes) {
        return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
    }
    if (length < RUDA_BYTECODE_MAGIC_LENGTH) {
        return RUDA_BYTECODE_CODEC_TRUNCATED;
    }
    if (memcmp(bytes, ruda_bytecode_magic, RUDA_BYTECODE_MAGIC_LENGTH) != 0) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    cursor.bytes = bytes;
    cursor.length = length;
    cursor.offset = RUDA_BYTECODE_MAGIC_LENGTH;

    status = ruda_bytecode_codec_decode_u64(&cursor, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value != RUDA_BYTECODE_FORMAT_VERSION) {
        return RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    status = ruda_bytecode_codec_decode_u64(&cursor, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if ((value & ~(uint64_t)(
            RUDA_BYTECODE_FEATURE_CALLS | RUDA_BYTECODE_FEATURE_STRINGS
                | RUDA_BYTECODE_FEATURE_F64 | RUDA_BYTECODE_FEATURE_ARRAYS
                | RUDA_BYTECODE_FEATURE_NOMINAL
                | RUDA_BYTECODE_FEATURE_ARRAY_TYPES
        )) != 0) {
        return RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    if ((value & RUDA_BYTECODE_FEATURE_ARRAY_TYPES) != 0
        && (value & RUDA_BYTECODE_FEATURE_ARRAYS) == 0) {
        return RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    features = value;
    generic_arrays = (features & RUDA_BYTECODE_FEATURE_ARRAY_TYPES) != 0;
    status = ruda_bytecode_codec_decode_u64(&cursor, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if ((value > ((features & RUDA_BYTECODE_FEATURE_NOMINAL) != 0
            ? RUDA_BYTECODE_TYPE_INTERFACE
            : RUDA_BYTECODE_TYPE_ARRAY)
            && !ruda_bytecode_type_is_extended_scalar((RudaBytecodeType)value))
        || !ruda_bytecode_type_is_result((RudaBytecodeType)value)) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    {
        RudaBytecodeType result_type = (RudaBytecodeType)value;
        uint64_t result_layout_wire = 0;
        uint64_t source_count_wire = 0;
        uint64_t parameter_count_wire = 0;
        uint64_t register_count_wire = 0;
        size_t source_count = 0;
        size_t parameter_count = 0;
        if (generic_arrays) {
            status = ruda_bytecode_codec_decode_u64(
                &cursor, &result_layout_wire
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_codec_decode_u64(
                &cursor, &source_count_wire
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_codec_decode_u64(&cursor, &parameter_count_wire);
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_codec_decode_u64(&cursor, &register_count_wire);
        }
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!ruda_bytecode_codec_u64_to_size(source_count_wire, &source_count)
            || !ruda_bytecode_codec_u64_to_size(
                parameter_count_wire,
                &parameter_count
            ) || !ruda_bytecode_codec_u64_to_size(
                register_count_wire,
                &register_count
            ) || source_count == 0 || source_count > options.max_source_files
            || parameter_count > register_count
            || register_count > options.max_registers
            || register_count > UINT32_MAX) {
            return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        }
        if (!ruda_bytecode_function_init(
                &function,
                allocator,
                result_type,
                source_count,
                NULL,
                0,
                options
            )) {
            return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
        }
        function.parameter_count = parameter_count;
        if (generic_arrays && result_layout_wire > UINT32_MAX) {
            ruda_bytecode_function_dispose(&function);
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (generic_arrays) {
            function.result_layout = (uint32_t)result_layout_wire;
        }
    }
    for (index = 0; index < register_count; index += 1) {
        RudaBytecodeType type;
        uint32_t layout = RUDA_BYTECODE_NO_LAYOUT;
        status = ruda_bytecode_codec_decode_u64(&cursor, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if ((value > ((features & RUDA_BYTECODE_FEATURE_NOMINAL) != 0
                ? RUDA_BYTECODE_TYPE_INTERFACE
                : RUDA_BYTECODE_TYPE_ARRAY)
                && !ruda_bytecode_type_is_extended_scalar((RudaBytecodeType)value))
            || !ruda_bytecode_type_is_local((RudaBytecodeType)value)
            || (index < function.parameter_count
                && !ruda_bytecode_type_is_register((RudaBytecodeType)value))) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
            goto cleanup;
        }
        type = (RudaBytecodeType)value;
        if (generic_arrays) {
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            layout = (uint32_t)value;
        }
        if (!ruda_vector_push(&function.register_types, &type)
            || !ruda_vector_push(&function.register_layouts, &layout)) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (generic_arrays) {
        status = ruda_bytecode_codec_decode_u64(&cursor, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
    }
    {
        size_t array_type_count = 0;
        if (generic_arrays && (!ruda_bytecode_codec_u64_to_size(
                value, &array_type_count)
            || array_type_count > options.max_array_constructions
            || array_type_count > UINT32_MAX)) {
            status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        if (generic_arrays && !ruda_vector_reserve(
                &function.array_types, array_type_count
            )) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (index = 0; generic_arrays && index < array_type_count;
            index += 1) {
            RudaBytecodeType element_type;
            uint32_t element_layout = RUDA_BYTECODE_NO_LAYOUT;
            uint32_t array_layout = UINT32_MAX;
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK
                || !ruda_bytecode_type_is_register(
                    (RudaBytecodeType)value
                )) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            element_type = (RudaBytecodeType)value;
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            element_layout = (uint32_t)value;
            if (!ruda_bytecode_add_array_type(
                    &function, element_type, element_layout, &array_layout
                ) || array_layout != index) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
        }
    }
    status = ruda_bytecode_codec_decode_u64(&cursor, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    if (!ruda_bytecode_codec_u64_to_size(value, &block_count)
        || block_count == 0 || block_count > options.max_blocks
        || block_count > UINT32_MAX) {
        status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        goto cleanup;
    }
    for (index = 0; index < block_count; index += 1) {
        uint32_t block_id = 0;
        size_t instruction_count = 0;
        size_t instruction_index = 0;
        size_t next_total = 0;
        status = ruda_bytecode_codec_decode_u64(&cursor, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!ruda_bytecode_codec_u64_to_size(value, &instruction_count)
            || instruction_count == 0
            || !ruda_size_add(
                function.instruction_count,
                instruction_count,
                &next_total
            ) || next_total > options.max_instructions) {
            status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        if (!ruda_bytecode_add_block(&function, &block_id)) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        for (instruction_index = 0;
            instruction_index < instruction_count;
            instruction_index += 1) {
            RudaBytecodeInstruction instruction;
            const RudaOpcodeMetadata *metadata = NULL;
            size_t operand_index = 0;
            memset(&instruction, 0, sizeof(instruction));
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            metadata = ruda_opcode_metadata_from_wire(value);
            if (metadata == NULL
                || (metadata->flags & RUDA_OPCODE_FLAG_INVALID) != 0) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            instruction.opcode = metadata->opcode;
            for (operand_index = 0; operand_index < metadata->operand_count;
                operand_index += 1) {
                if (metadata->operands[operand_index]
                    == RUDA_BYTECODE_OPERAND_I64) {
                    status = ruda_bytecode_decode_i64(
                        &cursor,
                        &instruction.operands[operand_index].signed_value
                    );
                } else {
                    status = ruda_bytecode_codec_decode_u64(
                        &cursor,
                        &instruction.operands[operand_index].unsigned_value
                    );
                }
                if (status != RUDA_BYTECODE_CODEC_OK) {
                    goto cleanup;
                }
            }
            status = ruda_bytecode_codec_decode_u64(
                &cursor,
                &instruction.span.source
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                status = ruda_bytecode_codec_decode_u64(
                    &cursor,
                    &instruction.span.start
                );
            }
            if (status == RUDA_BYTECODE_CODEC_OK) {
                status = ruda_bytecode_codec_decode_u64(
                    &cursor,
                    &instruction.span.end
                );
            }
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!ruda_bytecode_append_instruction(
                    &function,
                    block_id,
                    &instruction
                )) {
                status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if ((features & RUDA_BYTECODE_FEATURE_CALLS) != 0) {
        size_t call_count = 0;
        status = ruda_bytecode_codec_decode_u64(&cursor, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!ruda_bytecode_codec_u64_to_size(value, &call_count)
            || call_count > options.max_calls || call_count > UINT32_MAX) {
            status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        if (call_count == 0) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
            goto cleanup;
        }
        for (index = 0; index < call_count; index += 1) {
            RudaSymbolId callee;
            RudaBytecodeType result_type;
            uint32_t result_layout = RUDA_BYTECODE_NO_LAYOUT;
            size_t argument_count = 0;
            size_t argument_bytes = 0;
            size_t argument_index = 0;
            RudaBytecodeCallArgument *arguments = NULL;
            RudaBytecodeCallId call_id = RUDA_BYTECODE_NO_CALL;
            if (cursor.length - cursor.offset < sizeof(callee.bytes)) {
                status = RUDA_BYTECODE_CODEC_TRUNCATED;
                goto cleanup;
            }
            memcpy(
                callee.bytes,
                cursor.bytes + cursor.offset,
                sizeof(callee.bytes)
            );
            cursor.offset += sizeof(callee.bytes);
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if ((value > ((features & RUDA_BYTECODE_FEATURE_NOMINAL) != 0
                    ? RUDA_BYTECODE_TYPE_INTERFACE
                    : RUDA_BYTECODE_TYPE_ARRAY)
                    && !ruda_bytecode_type_is_extended_scalar((RudaBytecodeType)value))
                || !ruda_bytecode_type_is_result((RudaBytecodeType)value)) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            result_type = (RudaBytecodeType)value;
            if (generic_arrays) {
                status = ruda_bytecode_codec_decode_u64(&cursor, &value);
                if (status != RUDA_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                    status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                    goto cleanup;
                }
                result_layout = (uint32_t)value;
            }
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!ruda_bytecode_codec_u64_to_size(value, &argument_count)
                || argument_count > options.max_call_arguments
                || !ruda_size_add(
                    function.call_arguments.length,
                    argument_count,
                    &argument_bytes
                ) || argument_bytes > options.max_call_arguments
                || !ruda_size_multiply(
                    argument_count,
                    sizeof(RudaBytecodeCallArgument),
                    &argument_bytes
                )) {
                status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (argument_bytes != 0) {
                arguments = ruda_allocate(&allocator, argument_bytes);
                if (arguments == NULL) {
                    status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
            if (argument_count != 0 && arguments == NULL) {
                status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            for (argument_index = 0; argument_index < argument_count;
                argument_index += 1) {
                arguments[argument_index].layout = RUDA_BYTECODE_NO_LAYOUT;
            }
            for (argument_index = 0; argument_index < argument_count;
                argument_index += 1) {
                if (arguments == NULL) {
                    status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                    break;
                }
                status = ruda_bytecode_codec_decode_u64(&cursor, &value);
                if (status != RUDA_BYTECODE_CODEC_OK) {
                    break;
                }
                if (value >= function.register_types.length
                    || value > UINT32_MAX) {
                    status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                    break;
                }
                arguments[argument_index].register_id = (uint32_t)value;
                status = ruda_bytecode_codec_decode_u64(&cursor, &value);
                if (status != RUDA_BYTECODE_CODEC_OK) {
                    break;
                }
                if ((value > ((features & RUDA_BYTECODE_FEATURE_NOMINAL) != 0
                        ? RUDA_BYTECODE_TYPE_INTERFACE
                        : RUDA_BYTECODE_TYPE_ARRAY)
                        && !ruda_bytecode_type_is_extended_scalar((RudaBytecodeType)value))
                    || !ruda_bytecode_type_is_register(
                        (RudaBytecodeType)value
                    )) {
                    status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                    break;
                }
                arguments[argument_index].type = (RudaBytecodeType)value;
                if (generic_arrays) {
                    status = ruda_bytecode_codec_decode_u64(&cursor, &value);
                    if (status != RUDA_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                        status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                        break;
                    }
                    arguments[argument_index].layout = (uint32_t)value;
                }
                {
                    const RudaBytecodeType *register_type =
                        ruda_vector_at_const(
                            &function.register_types,
                            arguments[argument_index].register_id
                        );
                    if (register_type == NULL
                        || *register_type != arguments[argument_index].type
                        || (generic_arrays && ruda_bytecode_register_layout(
                            &function,
                            arguments[argument_index].register_id
                        ) != arguments[argument_index].layout)) {
                        status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                        break;
                    }
                }
            }
            if (status == RUDA_BYTECODE_CODEC_OK
                && !ruda_bytecode_add_call(
                    &function,
                    callee,
                    result_type,
                    arguments,
                    argument_count,
                    &call_id
                )) {
                status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
            if (status == RUDA_BYTECODE_CODEC_OK) {
                RudaBytecodeCall *call = ruda_vector_at(
                    &function.calls, call_id
                );
                if (call == NULL) {
                    status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                } else {
                    call->result_layout = result_layout;
                }
            }
            ruda_deallocate(
                &allocator,
                arguments,
                argument_bytes
            );
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if ((size_t)call_id != index) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
        }
    }
    if ((features & RUDA_BYTECODE_FEATURE_STRINGS) != 0) {
        size_t string_count = 0;
        status = ruda_bytecode_codec_decode_u64(&cursor, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!ruda_bytecode_codec_u64_to_size(value, &string_count)
            || string_count == 0 || string_count > options.max_string_constants
            || string_count > UINT32_MAX) {
            status = string_count == 0
                ? RUDA_BYTECODE_CODEC_INVALID_ENCODING
                : RUDA_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        for (index = 0; index < string_count; index += 1) {
            size_t string_length = 0;
            RudaBytecodeStringId string_id = RUDA_BYTECODE_NO_STRING;
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!ruda_bytecode_codec_u64_to_size(value, &string_length)
                || string_length > options.max_string_bytes
                || string_length > cursor.length - cursor.offset) {
                status = string_length > cursor.length - cursor.offset
                    ? RUDA_BYTECODE_CODEC_TRUNCATED
                    : RUDA_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (!ruda_utf8_validate(
                    (const char *)(cursor.bytes + cursor.offset),
                    string_length,
                    NULL
                )) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            if (!ruda_bytecode_add_string_constant(
                    &function,
                    cursor.bytes + cursor.offset,
                    string_length,
                    &string_id
                )) {
                size_t next_bytes = 0;
                status = !ruda_size_add(
                        function.string_bytes,
                        string_length,
                        &next_bytes
                    ) || next_bytes > options.max_string_bytes
                    ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                    : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
            if ((size_t)string_id != index) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            cursor.offset += string_length;
        }
    }
    if ((features & RUDA_BYTECODE_FEATURE_ARRAYS) != 0) {
        size_t construction_count = 0;
        status = ruda_bytecode_codec_decode_u64(&cursor, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!ruda_bytecode_codec_u64_to_size(value, &construction_count)
            || construction_count > options.max_array_constructions
            || construction_count > UINT32_MAX) {
            status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
            goto cleanup;
        }
        for (index = 0; index < construction_count; index += 1) {
            RudaBytecodeType array_type;
            RudaBytecodeType element_type;
            size_t argument_count = 0;
            size_t argument_bytes = 0;
            size_t argument_index = 0;
            RudaBytecodeArrayArgument *arguments = NULL;
            RudaBytecodeArrayConstructionId construction_id =
                RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
            uint32_t array_layout = RUDA_BYTECODE_NO_LAYOUT;
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (value > RUDA_BYTECODE_TYPE_ARRAY
                || !ruda_bytecode_type_is_array((RudaBytecodeType)value)) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            array_type = (RudaBytecodeType)value;
            if (generic_arrays) {
                status = ruda_bytecode_codec_decode_u64(&cursor, &value);
                if (status != RUDA_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                    status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                    goto cleanup;
                }
                array_layout = (uint32_t)value;
            }
            element_type = ruda_bytecode_function_array_element_type(
                &function, array_type, array_layout, NULL
            );
            if (element_type == RUDA_BYTECODE_TYPE_INVALID) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
            status = ruda_bytecode_codec_decode_u64(&cursor, &value);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!ruda_bytecode_codec_u64_to_size(value, &argument_count)
                || argument_count > options.max_array_arguments
                || !ruda_size_add(
                    function.array_arguments.length, argument_count,
                    &argument_bytes
                ) || argument_bytes > options.max_array_arguments
                || !ruda_size_multiply(
                    argument_count, sizeof(RudaBytecodeArrayArgument),
                    &argument_bytes
                )) {
                status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (argument_bytes != 0) {
                arguments = ruda_allocate(&allocator, argument_bytes);
                if (arguments == NULL) {
                    status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                    goto cleanup;
                }
            }
            for (argument_index = 0; argument_index < argument_count;
                argument_index += 1) {
                RudaBytecodeArrayArgument *argument = arguments != NULL
                    ? &arguments[argument_index]
                    : NULL;
                if (argument == NULL) {
                    status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                    break;
                }
                status = ruda_bytecode_codec_decode_u64(&cursor, &value);
                if (status != RUDA_BYTECODE_CODEC_OK) {
                    break;
                }
                if (value >= function.register_types.length
                    || value > UINT32_MAX) {
                    status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                    break;
                }
                argument->register_id = (uint32_t)value;
                status = ruda_bytecode_codec_decode_u64(&cursor, &value);
                if (status != RUDA_BYTECODE_CODEC_OK) {
                    break;
                }
                if (value != (uint64_t)element_type) {
                    status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                    break;
                }
                argument->type = (RudaBytecodeType)value;
                if (generic_arrays) {
                    status = ruda_bytecode_codec_decode_u64(&cursor, &value);
                    if (status != RUDA_BYTECODE_CODEC_OK || value > UINT32_MAX) {
                        status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                        break;
                    }
                    argument->layout = (uint32_t)value;
                }
                {
                    const RudaBytecodeType *register_type =
                        ruda_vector_at_const(
                            &function.register_types,
                            argument->register_id
                        );
                    if (register_type == NULL
                        || *register_type != element_type
                        || (generic_arrays && (array_type == RUDA_BYTECODE_TYPE_ARRAY
                            && ruda_bytecode_register_layout(
                                &function, argument->register_id
                            ) != argument->layout))) {
                        status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                        break;
                    }
                }
            }
            if (status == RUDA_BYTECODE_CODEC_OK
                && !ruda_bytecode_add_array_construction_with_layout(
                    &function, array_type, array_layout,
                    arguments, argument_count,
                    &construction_id
                )) {
                status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            }
            ruda_deallocate(&allocator, arguments, argument_bytes);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if ((size_t)construction_id != index) {
                status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                goto cleanup;
            }
        }
    }
    if ((features & RUDA_BYTECODE_FEATURE_NOMINAL) != 0) {
        status = ruda_bytecode_decode_nominal_metadata(&function, &cursor);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
    }
    if (((features & RUDA_BYTECODE_FEATURE_F64) != 0)
        != ruda_bytecode_function_uses_f64(&function)) {
        status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        goto cleanup;
    }
    if (((features & RUDA_BYTECODE_FEATURE_ARRAYS) != 0)
        != ruda_bytecode_function_uses_arrays(&function)) {
        status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        goto cleanup;
    }
    if (((features & RUDA_BYTECODE_FEATURE_NOMINAL) != 0)
        != ruda_bytecode_function_uses_nominal(&function)) {
        status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        goto cleanup;
    }
    if (cursor.offset != cursor.length) {
        status = RUDA_BYTECODE_CODEC_TRAILING_DATA;
        goto cleanup;
    }
    status = ruda_bytecode_codec_from_verify(ruda_bytecode_verify(&function));
    if (status != RUDA_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    *output = function;
    memset(&function, 0, sizeof(function));
    return RUDA_BYTECODE_CODEC_OK;

cleanup:
    ruda_bytecode_function_dispose(&function);
    return status;
}
