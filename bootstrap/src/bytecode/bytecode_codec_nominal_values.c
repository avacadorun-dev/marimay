#include "bytecode_codec_nominal_values.h"

#include <limits.h>

bool ruda_bytecode_nominal_values_encode(
    const RudaBytecodeFunction *function,
    RudaBytecodeEncodeBuffer *buffer
) {
    size_t index = 0;
    if (!ruda_bytecode_codec_encode_u64(buffer, function->result_layout)
        || !ruda_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->register_layouts.length
        )) {
        return false;
    }
    for (index = 0; index < function->register_layouts.length; index += 1) {
        const uint32_t *layout = ruda_vector_at_const(
            &function->register_layouts,
            index
        );
        if (layout == NULL || !ruda_bytecode_codec_encode_u64(buffer, *layout)) {
            return false;
        }
    }
    if (!ruda_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->calls.length
        )) {
        return false;
    }
    for (index = 0; index < function->calls.length; index += 1) {
        const RudaBytecodeCall *call = ruda_vector_at_const(
            &function->calls,
            index
        );
        size_t argument_index = 0;
        if (call == NULL || !ruda_bytecode_codec_encode_u64(
                buffer,
                call->result_layout
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                call->interface_slot
            ) || !ruda_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)call->argument_count
            )) {
            return false;
        }
        for (argument_index = 0; argument_index < call->argument_count;
            argument_index += 1) {
            const RudaBytecodeCallArgument *argument = ruda_vector_at_const(
                &function->call_arguments,
                call->argument_start + argument_index
            );
            if (argument == NULL || !ruda_bytecode_codec_encode_u64(
                    buffer,
                    argument->layout
                )) {
                return false;
            }
        }
    }
    return true;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_values_decode_u32(
    RudaBytecodeCursor *cursor,
    uint32_t *result
) {
    uint64_t value = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value > UINT32_MAX) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    *result = (uint32_t)value;
    return RUDA_BYTECODE_CODEC_OK;
}

static RudaBytecodeCodecStatus ruda_bytecode_nominal_values_decode_count(
    RudaBytecodeCursor *cursor,
    size_t expected
) {
    uint64_t value = 0;
    size_t count = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (!ruda_bytecode_codec_u64_to_size(value, &count) || count != expected) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    return RUDA_BYTECODE_CODEC_OK;
}

RudaBytecodeCodecStatus ruda_bytecode_nominal_values_decode(
    RudaBytecodeFunction *function,
    RudaBytecodeCursor *cursor
) {
    uint32_t layout = RUDA_BYTECODE_NO_LAYOUT;
    size_t index = 0;
    RudaBytecodeCodecStatus status = ruda_bytecode_nominal_values_decode_u32(
        cursor,
        &layout
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    function->result_layout = layout;
    status = ruda_bytecode_nominal_values_decode_count(
        cursor,
        function->register_types.length
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        status = ruda_bytecode_nominal_values_decode_u32(cursor, &layout);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!ruda_bytecode_set_register_layout(
                function,
                (uint32_t)index,
                layout
            )) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    status = ruda_bytecode_nominal_values_decode_count(
        cursor,
        function->calls.length
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < function->calls.length; index += 1) {
        RudaBytecodeCall *call = ruda_vector_at(&function->calls, index);
        size_t argument_index = 0;
        if (call == NULL) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        status = ruda_bytecode_nominal_values_decode_u32(
            cursor,
            &call->result_layout
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_values_decode_u32(
                cursor,
                &call->interface_slot
            );
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_nominal_values_decode_count(
                cursor,
                call->argument_count
            );
        }
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        for (argument_index = 0; argument_index < call->argument_count;
            argument_index += 1) {
            RudaBytecodeCallArgument *argument = ruda_vector_at(
                &function->call_arguments,
                call->argument_start + argument_index
            );
            if (argument == NULL) {
                return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
            }
            status = ruda_bytecode_nominal_values_decode_u32(
                cursor,
                &argument->layout
            );
            if (status != RUDA_BYTECODE_CODEC_OK) {
                return status;
            }
        }
    }
    return RUDA_BYTECODE_CODEC_OK;
}
