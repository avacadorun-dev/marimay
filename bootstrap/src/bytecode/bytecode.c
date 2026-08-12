#include "ruda/bytecode/bytecode.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../generated/opcode_data.inc"

enum {
    RUDA_BYTECODE_MAGIC_LENGTH = 8
};

static const unsigned char ruda_bytecode_magic[RUDA_BYTECODE_MAGIC_LENGTH] = {
    'R', 'U', 'D', 'A', 'B', 'C', 'F', 0
};

typedef struct RudaBytecodeEncodeBuffer {
    RudaVector bytes;
    size_t limit;
    bool limit_reached;
} RudaBytecodeEncodeBuffer;

typedef struct RudaBytecodeCursor {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
} RudaBytecodeCursor;

static bool ruda_bytecode_type_is_register(RudaBytecodeType type) {
    return type == RUDA_BYTECODE_TYPE_BOOL || type == RUDA_BYTECODE_TYPE_I64;
}

static bool ruda_bytecode_type_is_result(RudaBytecodeType type) {
    return ruda_bytecode_type_is_register(type)
        || type == RUDA_BYTECODE_TYPE_VOID;
}

static const char *ruda_bytecode_type_name(RudaBytecodeType type) {
    switch (type) {
        case RUDA_BYTECODE_TYPE_BOOL:
            return "bool";
        case RUDA_BYTECODE_TYPE_I64:
            return "i64";
        case RUDA_BYTECODE_TYPE_VOID:
            return "void";
        case RUDA_BYTECODE_TYPE_INVALID:
            break;
    }
    return "invalid";
}

RudaBytecodeFunctionOptions ruda_bytecode_function_default_options(void) {
    RudaBytecodeFunctionOptions options;
    options.max_registers = 65536;
    options.max_blocks = 16384;
    options.max_instructions = 1000000;
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
        || options.max_instructions == 0 || options.max_source_files == 0
        || options.max_encoded_bytes < RUDA_BYTECODE_MAGIC_LENGTH
        || options.max_verify_work == 0) {
        return false;
    }
    memset(function, 0, sizeof(*function));
    function->allocator = allocator;
    function->result_type = result_type;
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
        )) {
        ruda_vector_dispose(&function->register_types);
        ruda_vector_dispose(&function->blocks);
        memset(function, 0, sizeof(*function));
        return false;
    }
    for (index = 0; index < parameter_count; index += 1) {
        if (!ruda_bytecode_type_is_register(parameter_types[index])
            || !ruda_vector_push(
                &function->register_types,
                &parameter_types[index]
            )) {
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
    ruda_vector_dispose(&function->blocks);
    ruda_vector_dispose(&function->register_types);
    memset(function, 0, sizeof(*function));
}

bool ruda_bytecode_add_register(
    RudaBytecodeFunction *function,
    RudaBytecodeType type,
    uint32_t *result
) {
    if (function == NULL || !function->initialized || result == NULL
        || !ruda_bytecode_type_is_register(type)
        || function->register_types.length >= function->options.max_registers
        || function->register_types.length >= UINT32_MAX) {
        return false;
    }
    if (!ruda_vector_push(&function->register_types, &type)) {
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

static bool ruda_bytecode_encode_append(
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

static bool ruda_bytecode_encode_u64(
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
    return ruda_bytecode_encode_append(buffer, bytes, count);
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
    return ruda_bytecode_encode_u64(buffer, encoded);
}

static RudaBytecodeCodecStatus ruda_bytecode_decode_u64(
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
    RudaBytecodeCodecStatus status = ruda_bytecode_decode_u64(cursor, &encoded);
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

static bool ruda_bytecode_u64_to_size(uint64_t value, size_t *result) {
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
    if (!ruda_bytecode_encode_append(
            &buffer,
            ruda_bytecode_magic,
            RUDA_BYTECODE_MAGIC_LENGTH
        ) || !ruda_bytecode_encode_u64(&buffer, RUDA_BYTECODE_FORMAT_VERSION)
        || !ruda_bytecode_encode_u64(&buffer, 0)
        || !ruda_bytecode_encode_u64(&buffer, (uint64_t)function->result_type)
        || !ruda_bytecode_encode_u64(&buffer, (uint64_t)function->source_count)
        || !ruda_bytecode_encode_u64(&buffer, (uint64_t)function->parameter_count)
        || !ruda_bytecode_encode_u64(
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
        if (type == NULL || !ruda_bytecode_encode_u64(&buffer, (uint64_t)*type)) {
            status = buffer.limit_reached
                ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    if (!ruda_bytecode_encode_u64(&buffer, (uint64_t)function->blocks.length)) {
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
        if (block == NULL || !ruda_bytecode_encode_u64(
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
            if (metadata == NULL || !ruda_bytecode_encode_u64(
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
                    : ruda_bytecode_encode_u64(
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
            if (!ruda_bytecode_encode_u64(&buffer, instruction->span.source)
                || !ruda_bytecode_encode_u64(&buffer, instruction->span.start)
                || !ruda_bytecode_encode_u64(&buffer, instruction->span.end)) {
                status = buffer.limit_reached
                    ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                    : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
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

    status = ruda_bytecode_decode_u64(&cursor, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value != RUDA_BYTECODE_FORMAT_VERSION) {
        return RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    status = ruda_bytecode_decode_u64(&cursor, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value != 0) {
        return RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    status = ruda_bytecode_decode_u64(&cursor, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value > RUDA_BYTECODE_TYPE_VOID
        || !ruda_bytecode_type_is_result((RudaBytecodeType)value)) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    {
        RudaBytecodeType result_type = (RudaBytecodeType)value;
        uint64_t source_count_wire = 0;
        uint64_t parameter_count_wire = 0;
        uint64_t register_count_wire = 0;
        size_t source_count = 0;
        size_t parameter_count = 0;
        status = ruda_bytecode_decode_u64(&cursor, &source_count_wire);
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_decode_u64(&cursor, &parameter_count_wire);
        }
        if (status == RUDA_BYTECODE_CODEC_OK) {
            status = ruda_bytecode_decode_u64(&cursor, &register_count_wire);
        }
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!ruda_bytecode_u64_to_size(source_count_wire, &source_count)
            || !ruda_bytecode_u64_to_size(
                parameter_count_wire,
                &parameter_count
            ) || !ruda_bytecode_u64_to_size(
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
    }
    for (index = 0; index < register_count; index += 1) {
        RudaBytecodeType type;
        status = ruda_bytecode_decode_u64(&cursor, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (value > RUDA_BYTECODE_TYPE_VOID
            || !ruda_bytecode_type_is_register((RudaBytecodeType)value)) {
            status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
            goto cleanup;
        }
        type = (RudaBytecodeType)value;
        if (!ruda_vector_push(&function.register_types, &type)) {
            status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    status = ruda_bytecode_decode_u64(&cursor, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    if (!ruda_bytecode_u64_to_size(value, &block_count)
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
        status = ruda_bytecode_decode_u64(&cursor, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!ruda_bytecode_u64_to_size(value, &instruction_count)
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
            status = ruda_bytecode_decode_u64(&cursor, &value);
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
                    status = ruda_bytecode_decode_u64(
                        &cursor,
                        &instruction.operands[operand_index].unsigned_value
                    );
                }
                if (status != RUDA_BYTECODE_CODEC_OK) {
                    goto cleanup;
                }
            }
            status = ruda_bytecode_decode_u64(
                &cursor,
                &instruction.span.source
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                status = ruda_bytecode_decode_u64(
                    &cursor,
                    &instruction.span.start
                );
            }
            if (status == RUDA_BYTECODE_CODEC_OK) {
                status = ruda_bytecode_decode_u64(
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

static bool ruda_bytecode_consume_verify_work(
    size_t *used,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (used == NULL || !ruda_size_add(*used, amount, &next) || next > limit) {
        return false;
    }
    *used = next;
    return true;
}

static const RudaBytecodeType *ruda_bytecode_register_type(
    const RudaBytecodeFunction *function,
    uint64_t register_id
) {
    if (function == NULL || register_id >= function->register_types.length) {
        return NULL;
    }
    return ruda_vector_at_const(
        &function->register_types,
        (size_t)register_id
    );
}

static const RudaBytecodeInstruction *ruda_bytecode_block_terminator(
    const RudaBytecodeBlock *block
) {
    if (block == NULL || block->instructions.length == 0) {
        return NULL;
    }
    return ruda_vector_at_const(
        &block->instructions,
        block->instructions.length - 1
    );
}

static bool ruda_bytecode_opcode_writes_register(RudaOpcode opcode) {
    switch (opcode) {
        case RUDA_OPCODE_CONST_I64:
        case RUDA_OPCODE_CONST_BOOL:
        case RUDA_OPCODE_COPY:
        case RUDA_OPCODE_I64_ADD_CHECKED:
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED:
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED:
        case RUDA_OPCODE_I64_DIVIDE_CHECKED:
        case RUDA_OPCODE_I64_REMAINDER_CHECKED:
        case RUDA_OPCODE_I64_NEGATE_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_NOT:
        case RUDA_OPCODE_BOOL_NOT:
        case RUDA_OPCODE_I64_EQUAL:
        case RUDA_OPCODE_BOOL_EQUAL:
        case RUDA_OPCODE_I64_LESS:
        case RUDA_OPCODE_I64_LESS_EQUAL:
        case RUDA_OPCODE_I64_BITWISE_AND:
        case RUDA_OPCODE_I64_BITWISE_XOR:
        case RUDA_OPCODE_I64_BITWISE_OR:
        case RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED:
        case RUDA_OPCODE_I64_SHIFT_RIGHT:
            return true;
        case RUDA_OPCODE_INVALID:
        case RUDA_OPCODE_JUMP:
        case RUDA_OPCODE_BRANCH_BOOL:
        case RUDA_OPCODE_RETURN:
        case RUDA_OPCODE_RETURN_VOID:
            return false;
    }
    return false;
}

static void ruda_bytecode_transfer_assignments(
    const RudaBytecodeBlock *block,
    unsigned char *assigned,
    size_t register_count
) {
    size_t instruction_index = 0;
    if (block == NULL || assigned == NULL) {
        return;
    }
    for (instruction_index = 0;
        instruction_index + 1 < block->instructions.length;
        instruction_index += 1) {
        const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
            &block->instructions,
            instruction_index
        );
        if (instruction != NULL
            && ruda_bytecode_opcode_writes_register(instruction->opcode)
            && instruction->operands[0].unsigned_value < register_count) {
            assigned[(size_t)instruction->operands[0].unsigned_value] = 1;
        }
    }
}

static RudaBytecodeVerifyStatus ruda_bytecode_require_assigned(
    const unsigned char *assigned,
    size_t register_count,
    uint64_t register_id
) {
    if (register_id >= register_count) {
        return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    return assigned[(size_t)register_id] != 0
        ? RUDA_BYTECODE_VERIFY_OK
        : RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_value_instruction(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    unsigned char *assigned
) {
    uint64_t target = instruction->operands[0].unsigned_value;
    uint64_t first = 0;
    uint64_t second = 0;
    const RudaBytecodeType *target_type = ruda_bytecode_register_type(
        function,
        target
    );
    const RudaBytecodeType *first_type = NULL;
    const RudaBytecodeType *second_type = NULL;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (target_type == NULL) {
        return RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
    }
    switch (instruction->opcode) {
        case RUDA_OPCODE_CONST_I64:
            status = *target_type == RUDA_BYTECODE_TYPE_I64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_CONST_BOOL:
            if (instruction->operands[1].unsigned_value > 1) {
                return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
            }
            status = *target_type == RUDA_BYTECODE_TYPE_BOOL
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_COPY:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL && *target_type == *first_type
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_I64_NEGATE_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_NOT:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == RUDA_BYTECODE_TYPE_I64
                    && *first_type == RUDA_BYTECODE_TYPE_I64
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_BOOL_NOT:
            first = instruction->operands[1].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            status = first_type != NULL
                    && *target_type == RUDA_BYTECODE_TYPE_BOOL
                    && *first_type == RUDA_BYTECODE_TYPE_BOOL
                ? RUDA_BYTECODE_VERIFY_OK
                : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            break;
        case RUDA_OPCODE_I64_ADD_CHECKED:
        case RUDA_OPCODE_I64_SUBTRACT_CHECKED:
        case RUDA_OPCODE_I64_MULTIPLY_CHECKED:
        case RUDA_OPCODE_I64_DIVIDE_CHECKED:
        case RUDA_OPCODE_I64_REMAINDER_CHECKED:
        case RUDA_OPCODE_I64_BITWISE_AND:
        case RUDA_OPCODE_I64_BITWISE_XOR:
        case RUDA_OPCODE_I64_BITWISE_OR:
        case RUDA_OPCODE_I64_SHIFT_LEFT_CHECKED:
        case RUDA_OPCODE_I64_SHIFT_RIGHT:
        case RUDA_OPCODE_I64_EQUAL:
        case RUDA_OPCODE_I64_LESS:
        case RUDA_OPCODE_I64_LESS_EQUAL:
        case RUDA_OPCODE_BOOL_EQUAL:
            first = instruction->operands[1].unsigned_value;
            second = instruction->operands[2].unsigned_value;
            status = ruda_bytecode_require_assigned(
                assigned,
                function->register_types.length,
                first
            );
            if (status == RUDA_BYTECODE_VERIFY_OK) {
                status = ruda_bytecode_require_assigned(
                    assigned,
                    function->register_types.length,
                    second
                );
            }
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                return status;
            }
            first_type = ruda_bytecode_register_type(function, first);
            second_type = ruda_bytecode_register_type(function, second);
            if (instruction->opcode == RUDA_OPCODE_BOOL_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_BOOL
                        && *second_type == RUDA_BYTECODE_TYPE_BOOL
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else if (instruction->opcode == RUDA_OPCODE_I64_EQUAL
                || instruction->opcode == RUDA_OPCODE_I64_LESS
                || instruction->opcode == RUDA_OPCODE_I64_LESS_EQUAL) {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_BOOL
                        && *first_type == RUDA_BYTECODE_TYPE_I64
                        && *second_type == RUDA_BYTECODE_TYPE_I64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            } else {
                status = first_type != NULL && second_type != NULL
                        && *target_type == RUDA_BYTECODE_TYPE_I64
                        && *first_type == RUDA_BYTECODE_TYPE_I64
                        && *second_type == RUDA_BYTECODE_TYPE_I64
                    ? RUDA_BYTECODE_VERIFY_OK
                    : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
            }
            break;
        case RUDA_OPCODE_INVALID:
        case RUDA_OPCODE_JUMP:
        case RUDA_OPCODE_BRANCH_BOOL:
        case RUDA_OPCODE_RETURN:
        case RUDA_OPCODE_RETURN_VOID:
            return RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
    }
    if (status == RUDA_BYTECODE_VERIFY_OK) {
        assigned[(size_t)target] = 1;
    }
    return status;
}

static RudaBytecodeVerifyStatus ruda_bytecode_verify_terminator(
    const RudaBytecodeFunction *function,
    const RudaBytecodeInstruction *instruction,
    const unsigned char *assigned
) {
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    const RudaBytecodeType *type = NULL;
    if (instruction->opcode == RUDA_OPCODE_JUMP) {
        return RUDA_BYTECODE_VERIFY_OK;
    }
    if (instruction->opcode == RUDA_OPCODE_BRANCH_BOOL) {
        status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        type = ruda_bytecode_register_type(
            function,
            instruction->operands[0].unsigned_value
        );
        return type != NULL && *type == RUDA_BYTECODE_TYPE_BOOL
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_RETURN) {
        if (function->result_type == RUDA_BYTECODE_TYPE_VOID) {
            return RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
        }
        status = ruda_bytecode_require_assigned(
            assigned,
            function->register_types.length,
            instruction->operands[0].unsigned_value
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            return status;
        }
        type = ruda_bytecode_register_type(
            function,
            instruction->operands[0].unsigned_value
        );
        return type != NULL && *type == function->result_type
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    if (instruction->opcode == RUDA_OPCODE_RETURN_VOID) {
        return function->result_type == RUDA_BYTECODE_TYPE_VOID
            ? RUDA_BYTECODE_VERIFY_OK
            : RUDA_BYTECODE_VERIFY_TYPE_MISMATCH;
    }
    return RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
}

RudaBytecodeVerifyStatus ruda_bytecode_verify(
    const RudaBytecodeFunction *function
) {
    size_t register_count = 0;
    size_t block_count = 0;
    size_t instruction_total = 0;
    size_t edge_count = 0;
    size_t cell_count = 0;
    size_t block_index = 0;
    size_t work = 0;
    size_t *predecessor_counts = NULL;
    size_t *predecessor_offsets = NULL;
    size_t *predecessor_cursors = NULL;
    uint32_t *predecessors = NULL;
    unsigned char *reachable = NULL;
    uint32_t *queue = NULL;
    unsigned char *incoming = NULL;
    unsigned char *outgoing = NULL;
    unsigned char *scratch = NULL;
    size_t predecessor_counts_size = 0;
    size_t predecessor_offsets_size = 0;
    size_t predecessor_cursors_size = 0;
    size_t predecessors_size = 0;
    size_t reachable_size = 0;
    size_t queue_size = 0;
    size_t cells_size = 0;
    size_t scratch_size = 0;
    RudaBytecodeVerifyStatus status = RUDA_BYTECODE_VERIFY_OK;
    if (function == NULL || !function->initialized
        || !ruda_bytecode_type_is_result(function->result_type)
        || function->source_count == 0
        || function->source_count > function->options.max_source_files
        || function->parameter_count > function->register_types.length
        || function->register_types.length > function->options.max_registers
        || function->register_types.length > UINT32_MAX
        || function->blocks.length == 0
        || function->blocks.length > function->options.max_blocks
        || function->blocks.length > UINT32_MAX
        || function->instruction_count > function->options.max_instructions
        || function->options.max_verify_work == 0) {
        return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
    }
    register_count = function->register_types.length;
    block_count = function->blocks.length;
    for (block_index = 0; block_index < register_count; block_index += 1) {
        const RudaBytecodeType *type = ruda_vector_at_const(
            &function->register_types,
            block_index
        );
        if (type == NULL || !ruda_bytecode_type_is_register(*type)) {
            return RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        }
    }
    if (!ruda_size_multiply(block_count, sizeof(size_t), &predecessor_counts_size)
        || !ruda_size_add(block_count, 1, &predecessor_offsets_size)
        || !ruda_size_multiply(
            predecessor_offsets_size,
            sizeof(size_t),
            &predecessor_offsets_size
        ) || !ruda_size_multiply(
            block_count,
            sizeof(size_t),
            &predecessor_cursors_size
        ) || !ruda_size_multiply(block_count, sizeof(unsigned char), &reachable_size)
        || !ruda_size_multiply(block_count, sizeof(uint32_t), &queue_size)) {
        return RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
    }
    predecessor_counts = ruda_allocate_array(
        &function->allocator,
        block_count,
        sizeof(size_t)
    );
    if (predecessor_counts == NULL) {
        return RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED;
    }
    memset(predecessor_counts, 0, predecessor_counts_size);
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (block == NULL || !block->initialized
            || block->instructions.length == 0
            || !ruda_size_add(
                instruction_total,
                block->instructions.length,
                &instruction_total
            ) || instruction_total > function->options.max_instructions) {
            status = RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
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
            bool should_terminate = instruction_index + 1
                == block->instructions.length;
            size_t operand_index = 0;
            if (metadata == NULL
                || (metadata->flags & RUDA_OPCODE_FLAG_INVALID) != 0) {
                status = RUDA_BYTECODE_VERIFY_INVALID_OPCODE;
                goto cleanup;
            }
            if (((metadata->flags & RUDA_OPCODE_FLAG_TERMINATOR) != 0)
                != should_terminate) {
                status = RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                goto cleanup;
            }
            if (instruction->span.source >= function->source_count
                || instruction->span.end < instruction->span.start) {
                status = RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
                goto cleanup;
            }
            for (operand_index = 0; operand_index < metadata->operand_count;
                operand_index += 1) {
                RudaBytecodeOperandKind kind = metadata->operands[operand_index];
                if (kind == RUDA_BYTECODE_OPERAND_REGISTER
                    || kind == RUDA_BYTECODE_OPERAND_BLOCK
                    || kind == RUDA_BYTECODE_OPERAND_BOOL) {
                    uint64_t operand = instruction->operands[operand_index]
                        .unsigned_value;
                    if (kind == RUDA_BYTECODE_OPERAND_REGISTER
                        && operand >= register_count) {
                        status = RUDA_BYTECODE_VERIFY_INVALID_REGISTER;
                        goto cleanup;
                    }
                    if (kind == RUDA_BYTECODE_OPERAND_BLOCK
                        && operand >= block_count) {
                        status = RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
                        goto cleanup;
                    }
                    if (kind == RUDA_BYTECODE_OPERAND_BOOL && operand > 1) {
                        status = RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
                        goto cleanup;
                    }
                }
            }
            if (!ruda_bytecode_consume_verify_work(
                    &work,
                    1,
                    function->options.max_verify_work
                )) {
                status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        }
        {
            const RudaBytecodeInstruction *terminator =
                ruda_bytecode_block_terminator(block);
            if (terminator->opcode == RUDA_OPCODE_JUMP) {
                size_t target = (size_t)terminator->operands[0].unsigned_value;
                if (!ruda_size_add(predecessor_counts[target], 1,
                        &predecessor_counts[target])
                    || !ruda_size_add(edge_count, 1, &edge_count)) {
                    status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            } else if (terminator->opcode == RUDA_OPCODE_BRANCH_BOOL) {
                size_t then_block = (size_t)terminator->operands[1].unsigned_value;
                size_t otherwise = (size_t)terminator->operands[2].unsigned_value;
                if (!ruda_size_add(predecessor_counts[then_block], 1,
                        &predecessor_counts[then_block])
                    || !ruda_size_add(predecessor_counts[otherwise], 1,
                        &predecessor_counts[otherwise])
                    || !ruda_size_add(edge_count, 2, &edge_count)) {
                    status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
        }
    }
    if (instruction_total != function->instruction_count) {
        status = RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT;
        goto cleanup;
    }
    predecessor_offsets = ruda_allocate_array(
        &function->allocator,
        block_count + 1,
        sizeof(size_t)
    );
    predecessor_cursors = ruda_allocate_array(
        &function->allocator,
        block_count,
        sizeof(size_t)
    );
    if (!ruda_size_multiply(
            edge_count == 0 ? 1 : edge_count,
            sizeof(uint32_t),
            &predecessors_size
        )) {
        status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        goto cleanup;
    }
    predecessors = ruda_allocate_array(
        &function->allocator,
        edge_count == 0 ? 1 : edge_count,
        sizeof(uint32_t)
    );
    reachable = ruda_allocate_array(
        &function->allocator,
        block_count,
        sizeof(unsigned char)
    );
    queue = ruda_allocate_array(
        &function->allocator,
        block_count,
        sizeof(uint32_t)
    );
    if (predecessor_offsets == NULL || predecessor_cursors == NULL
        || predecessors == NULL || reachable == NULL || queue == NULL) {
        status = RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    predecessor_offsets[0] = 0;
    for (block_index = 0; block_index < block_count; block_index += 1) {
        if (!ruda_size_add(
                predecessor_offsets[block_index],
                predecessor_counts[block_index],
                &predecessor_offsets[block_index + 1]
            )) {
            status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
            goto cleanup;
        }
        predecessor_cursors[block_index] = predecessor_offsets[block_index];
    }
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        const RudaBytecodeInstruction *terminator =
            ruda_bytecode_block_terminator(block);
        if (terminator->opcode == RUDA_OPCODE_JUMP) {
            size_t target = (size_t)terminator->operands[0].unsigned_value;
            predecessors[predecessor_cursors[target]] = (uint32_t)block_index;
            predecessor_cursors[target] += 1;
        } else if (terminator->opcode == RUDA_OPCODE_BRANCH_BOOL) {
            size_t then_block = (size_t)terminator->operands[1].unsigned_value;
            size_t otherwise = (size_t)terminator->operands[2].unsigned_value;
            predecessors[predecessor_cursors[then_block]] = (uint32_t)block_index;
            predecessor_cursors[then_block] += 1;
            predecessors[predecessor_cursors[otherwise]] = (uint32_t)block_index;
            predecessor_cursors[otherwise] += 1;
        }
    }
    memset(reachable, 0, reachable_size);
    {
        size_t queue_read = 0;
        size_t queue_write = 1;
        queue[0] = 0;
        reachable[0] = 1;
        while (queue_read < queue_write) {
            uint32_t current = queue[queue_read];
            const RudaBytecodeBlock *block = ruda_vector_at_const(
                &function->blocks,
                current
            );
            const RudaBytecodeInstruction *terminator =
                ruda_bytecode_block_terminator(block);
            uint64_t targets[2];
            size_t target_count = 0;
            size_t target_index = 0;
            queue_read += 1;
            if (terminator->opcode == RUDA_OPCODE_JUMP) {
                targets[0] = terminator->operands[0].unsigned_value;
                target_count = 1;
            } else if (terminator->opcode == RUDA_OPCODE_BRANCH_BOOL) {
                targets[0] = terminator->operands[1].unsigned_value;
                targets[1] = terminator->operands[2].unsigned_value;
                target_count = 2;
            }
            for (target_index = 0; target_index < target_count;
                target_index += 1) {
                size_t target = (size_t)targets[target_index];
                if (reachable[target] == 0) {
                    reachable[target] = 1;
                    queue[queue_write] = (uint32_t)target;
                    queue_write += 1;
                }
            }
            if (!ruda_bytecode_consume_verify_work(
                    &work,
                    target_count + 1,
                    function->options.max_verify_work
                )) {
                status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
                goto cleanup;
            }
        }
    }
    for (block_index = 0; block_index < block_count; block_index += 1) {
        if (reachable[block_index] == 0) {
            status = RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW;
            goto cleanup;
        }
    }
    if (!ruda_size_multiply(block_count, register_count, &cell_count)
        || cell_count > function->options.max_verify_work
        || !ruda_size_multiply(
            cell_count == 0 ? 1 : cell_count,
            sizeof(unsigned char),
            &cells_size
        ) || !ruda_size_multiply(
            register_count == 0 ? 1 : register_count,
            sizeof(unsigned char),
            &scratch_size
        )) {
        status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
        goto cleanup;
    }
    incoming = ruda_allocate_array(
        &function->allocator,
        cell_count == 0 ? 1 : cell_count,
        sizeof(unsigned char)
    );
    outgoing = ruda_allocate_array(
        &function->allocator,
        cell_count == 0 ? 1 : cell_count,
        sizeof(unsigned char)
    );
    scratch = ruda_allocate_array(
        &function->allocator,
        register_count == 0 ? 1 : register_count,
        sizeof(unsigned char)
    );
    if (incoming == NULL || outgoing == NULL || scratch == NULL) {
        status = RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (cell_count != 0) {
        memset(incoming, 1, cells_size);
        memset(outgoing, 1, cells_size);
        memset(incoming, 0, register_count);
        memset(outgoing, 0, register_count);
        memset(incoming, 1, function->parameter_count);
        memcpy(outgoing, incoming, register_count);
        ruda_bytecode_transfer_assignments(
            ruda_vector_at_const(&function->blocks, 0),
            outgoing,
            register_count
        );
    }
    {
        bool changed = false;
        do {
            changed = false;
            for (block_index = 1; block_index < block_count; block_index += 1) {
                size_t predecessor_index = 0;
                unsigned char *incoming_row = incoming
                    + block_index * register_count;
                unsigned char *outgoing_row = outgoing
                    + block_index * register_count;
                const RudaBytecodeBlock *block = ruda_vector_at_const(
                    &function->blocks,
                    block_index
                );
                if (register_count != 0) {
                    memset(scratch, 1, register_count);
                }
                for (predecessor_index = predecessor_offsets[block_index];
                    predecessor_index < predecessor_offsets[block_index + 1];
                    predecessor_index += 1) {
                    size_t register_index = 0;
                    const unsigned char *predecessor_row = outgoing
                        + (size_t)predecessors[predecessor_index] * register_count;
                    for (register_index = 0; register_index < register_count;
                        register_index += 1) {
                        scratch[register_index] = (unsigned char)(
                            scratch[register_index]
                            & predecessor_row[register_index]
                        );
                    }
                    if (!ruda_bytecode_consume_verify_work(
                            &work,
                            register_count,
                            function->options.max_verify_work
                        )) {
                        status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
                        goto cleanup;
                    }
                }
                if (register_count != 0
                    && memcmp(incoming_row, scratch, register_count) != 0) {
                    memcpy(incoming_row, scratch, register_count);
                    changed = true;
                }
                ruda_bytecode_transfer_assignments(
                    block,
                    scratch,
                    register_count
                );
                if (register_count != 0
                    && memcmp(outgoing_row, scratch, register_count) != 0) {
                    memcpy(outgoing_row, scratch, register_count);
                    changed = true;
                }
                if (!ruda_bytecode_consume_verify_work(
                        &work,
                        register_count * 2,
                        function->options.max_verify_work
                    )) {
                    status = RUDA_BYTECODE_VERIFY_LIMIT_REACHED;
                    goto cleanup;
                }
            }
        } while (changed);
    }
    for (block_index = 0; block_index < block_count; block_index += 1) {
        const RudaBytecodeBlock *block = ruda_vector_at_const(
            &function->blocks,
            block_index
        );
        size_t instruction_index = 0;
        if (register_count != 0) {
            memcpy(
                scratch,
                incoming + block_index * register_count,
                register_count
            );
        }
        for (instruction_index = 0;
            instruction_index + 1 < block->instructions.length;
            instruction_index += 1) {
            const RudaBytecodeInstruction *instruction = ruda_vector_at_const(
                &block->instructions,
                instruction_index
            );
            status = ruda_bytecode_verify_value_instruction(
                function,
                instruction,
                scratch
            );
            if (status != RUDA_BYTECODE_VERIFY_OK) {
                goto cleanup;
            }
        }
        status = ruda_bytecode_verify_terminator(
            function,
            ruda_bytecode_block_terminator(block),
            scratch
        );
        if (status != RUDA_BYTECODE_VERIFY_OK) {
            goto cleanup;
        }
    }

cleanup:
    ruda_deallocate(
        &function->allocator,
        scratch,
        scratch_size
    );
    ruda_deallocate(
        &function->allocator,
        outgoing,
        cells_size
    );
    ruda_deallocate(
        &function->allocator,
        incoming,
        cells_size
    );
    ruda_deallocate(
        &function->allocator,
        queue,
        queue_size
    );
    ruda_deallocate(
        &function->allocator,
        reachable,
        reachable_size
    );
    ruda_deallocate(
        &function->allocator,
        predecessors,
        predecessors_size
    );
    ruda_deallocate(
        &function->allocator,
        predecessor_cursors,
        predecessor_cursors_size
    );
    ruda_deallocate(
        &function->allocator,
        predecessor_offsets,
        predecessor_offsets_size
    );
    ruda_deallocate(
        &function->allocator,
        predecessor_counts,
        predecessor_counts_size
    );
    return status;
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
                    == RUDA_BYTECODE_OPERAND_I64) {
                    wrote = ruda_bytecode_writef(
                        writer,
                        "%s%" PRId64,
                        separator,
                        instruction->operands[operand_index].signed_value
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
