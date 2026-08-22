#include "bytecode_codec_nominal_values.h"

#include <limits.h>

bool mmay_bytecode_nominal_values_encode(
    const MarimayBytecodeFunction *function,
    MarimayBytecodeEncodeBuffer *buffer
) {
    size_t index = 0;
    if (!mmay_bytecode_codec_encode_u64(buffer, function->result_layout)
        || !mmay_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->register_layouts.length
        )) {
        return false;
    }
    for (index = 0; index < function->register_layouts.length; index += 1) {
        const uint32_t *layout = mmay_vector_at_const(
            &function->register_layouts,
            index
        );
        if (layout == NULL || !mmay_bytecode_codec_encode_u64(buffer, *layout)) {
            return false;
        }
    }
    if (!mmay_bytecode_codec_encode_u64(
            buffer,
            (uint64_t)function->calls.length
        )) {
        return false;
    }
    for (index = 0; index < function->calls.length; index += 1) {
        const MarimayBytecodeCall *call = mmay_vector_at_const(
            &function->calls,
            index
        );
        size_t argument_index = 0;
        if (call == NULL || !mmay_bytecode_codec_encode_u64(
                buffer,
                call->result_layout
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                call->interface_slot
            ) || !mmay_bytecode_codec_encode_u64(
                buffer,
                (uint64_t)call->argument_count
            )) {
            return false;
        }
        for (argument_index = 0; argument_index < call->argument_count;
            argument_index += 1) {
            const MarimayBytecodeCallArgument *argument = mmay_vector_at_const(
                &function->call_arguments,
                call->argument_start + argument_index
            );
            if (argument == NULL || !mmay_bytecode_codec_encode_u64(
                    buffer,
                    argument->layout
                )) {
                return false;
            }
        }
    }
    return true;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_values_decode_u32(
    MarimayBytecodeCursor *cursor,
    uint32_t *result
) {
    uint64_t value = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value > UINT32_MAX) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    *result = (uint32_t)value;
    return MMAY_BYTECODE_CODEC_OK;
}

static MarimayBytecodeCodecStatus mmay_bytecode_nominal_values_decode_count(
    MarimayBytecodeCursor *cursor,
    size_t expected
) {
    uint64_t value = 0;
    size_t count = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_codec_decode_u64(
        cursor,
        &value
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if (!mmay_bytecode_codec_u64_to_size(value, &count) || count != expected) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    return MMAY_BYTECODE_CODEC_OK;
}

MarimayBytecodeCodecStatus mmay_bytecode_nominal_values_decode(
    MarimayBytecodeFunction *function,
    MarimayBytecodeCursor *cursor
) {
    uint32_t layout = MMAY_BYTECODE_NO_LAYOUT;
    size_t index = 0;
    MarimayBytecodeCodecStatus status = mmay_bytecode_nominal_values_decode_u32(
        cursor,
        &layout
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    function->result_layout = layout;
    status = mmay_bytecode_nominal_values_decode_count(
        cursor,
        function->register_types.length
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < function->register_types.length; index += 1) {
        status = mmay_bytecode_nominal_values_decode_u32(cursor, &layout);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!mmay_bytecode_set_register_layout(
                function,
                (uint32_t)index,
                layout
            )) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
    }
    status = mmay_bytecode_nominal_values_decode_count(
        cursor,
        function->calls.length
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    for (index = 0; index < function->calls.length; index += 1) {
        MarimayBytecodeCall *call = mmay_vector_at(&function->calls, index);
        size_t argument_index = 0;
        if (call == NULL) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        status = mmay_bytecode_nominal_values_decode_u32(
            cursor,
            &call->result_layout
        );
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_values_decode_u32(
                cursor,
                &call->interface_slot
            );
        }
        if (status == MMAY_BYTECODE_CODEC_OK) {
            status = mmay_bytecode_nominal_values_decode_count(
                cursor,
                call->argument_count
            );
        }
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        for (argument_index = 0; argument_index < call->argument_count;
            argument_index += 1) {
            MarimayBytecodeCallArgument *argument = mmay_vector_at(
                &function->call_arguments,
                call->argument_start + argument_index
            );
            if (argument == NULL) {
                return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
            }
            status = mmay_bytecode_nominal_values_decode_u32(
                cursor,
                &argument->layout
            );
            if (status != MMAY_BYTECODE_CODEC_OK) {
                return status;
            }
        }
    }
    return MMAY_BYTECODE_CODEC_OK;
}
