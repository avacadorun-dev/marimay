#include "ruda/bytecode/program.h"

#include <limits.h>
#include <string.h>

#include "ruda/base/hash.h"
#include "bytecode_internal.h"
#include "program_internal.h"

enum {
    RUDA_BYTECODE_PROGRAM_MAGIC_LENGTH = 8,
    RUDA_BYTECODE_PROGRAM_DIGEST_LENGTH = 32
};

static const unsigned char ruda_bytecode_program_magic[
    RUDA_BYTECODE_PROGRAM_MAGIC_LENGTH
] = { 'R', 'U', 'D', 'A', 'B', 'C', 'P', 0 };

typedef struct RudaBytecodeProgramBuffer {
    RudaVector bytes;
    size_t limit;
    bool limit_reached;
} RudaBytecodeProgramBuffer;

typedef struct RudaBytecodeProgramCursor {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
} RudaBytecodeProgramCursor;

static int ruda_bytecode_program_compare_symbols(
    RudaSymbolId left,
    RudaSymbolId right
) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes));
}

static bool ruda_bytecode_program_size_to_u64(size_t value, uint64_t *result) {
    uint64_t converted = (uint64_t)value;
    if (result == NULL || (size_t)converted != value) {
        return false;
    }
    *result = converted;
    return true;
}

static bool ruda_bytecode_program_u64_to_size(uint64_t value, size_t *result) {
    size_t converted = (size_t)value;
    if (result == NULL || (uint64_t)converted != value) {
        return false;
    }
    *result = converted;
    return true;
}

static bool ruda_bytecode_program_add_bounded(
    size_t *total,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (total == NULL || !ruda_size_add(*total, amount, &next) || next > limit) {
        return false;
    }
    *total = next;
    return true;
}

static bool ruda_bytecode_program_consume_work(
    size_t *work,
    size_t amount,
    size_t limit
) {
    return ruda_bytecode_program_add_bounded(work, amount, limit);
}

static bool ruda_bytecode_program_buffer_init(
    RudaBytecodeProgramBuffer *buffer,
    RudaAllocator allocator,
    size_t limit
) {
    if (buffer == NULL) {
        return false;
    }
    memset(buffer, 0, sizeof(*buffer));
    buffer->limit = limit;
    return ruda_vector_init(&buffer->bytes, allocator, sizeof(unsigned char));
}

static void ruda_bytecode_program_buffer_dispose(
    RudaBytecodeProgramBuffer *buffer
) {
    if (buffer != NULL) {
        ruda_vector_dispose(&buffer->bytes);
        memset(buffer, 0, sizeof(*buffer));
    }
}

static bool ruda_bytecode_program_buffer_append(
    RudaBytecodeProgramBuffer *buffer,
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

static bool ruda_bytecode_program_buffer_u64(
    RudaBytecodeProgramBuffer *buffer,
    uint64_t value
) {
    unsigned char encoded[10];
    size_t count = 0;
    do {
        unsigned char byte = (unsigned char)(value & UINT64_C(0x7f));
        value >>= 7;
        if (value != 0) {
            byte = (unsigned char)(byte | 0x80U);
        }
        encoded[count] = byte;
        count += 1;
    } while (value != 0);
    return ruda_bytecode_program_buffer_append(buffer, encoded, count);
}

static RudaBytecodeCodecStatus ruda_bytecode_program_decode_u64(
    RudaBytecodeProgramCursor *cursor,
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

static RudaBytecodeCodecStatus ruda_bytecode_program_codec_from_verify(
    RudaBytecodeProgramVerifyStatus status
) {
    if (status == RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
        return RUDA_BYTECODE_CODEC_OK;
    }
    if (status == RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED) {
        return RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
    }
    if (status == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED) {
        return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
    }
    return RUDA_BYTECODE_CODEC_VERIFY_FAILED;
}

RudaBytecodeProgramOptions ruda_bytecode_program_default_options(void) {
    RudaBytecodeProgramOptions options;
    options.max_functions = 100000;
    options.max_total_registers = 1000000;
    options.max_total_blocks = 1000000;
    options.max_total_instructions = 10000000;
    options.max_total_calls = 1000000;
    options.max_total_call_arguments = 10000000;
    options.max_total_string_constants = 1000000;
    options.max_total_string_bytes = 256U * 1024U * 1024U;
    options.max_total_array_constructions = 1000000;
    options.max_total_array_arguments = 10000000;
    options.max_total_record_layouts = 1000000;
    options.max_total_record_fields = 10000000;
    options.max_total_record_constructions = 1000000;
    options.max_total_record_arguments = 10000000;
    options.max_total_enum_layouts = 1000000;
    options.max_total_enum_variants = 10000000;
    options.max_total_enum_payloads = 10000000;
    options.max_total_enum_constructions = 1000000;
    options.max_total_enum_arguments = 10000000;
    options.max_total_interface_layouts = 1000000;
    options.max_total_interface_constructions = 1000000;
    options.max_total_interface_methods = 10000000;
    options.max_encoded_bytes = 256U * 1024U * 1024U;
    options.max_verify_work = 64000000;
    options.function_options = ruda_bytecode_function_default_options();
    return options;
}

bool ruda_bytecode_program_init(
    RudaBytecodeProgram *program,
    RudaAllocator allocator,
    size_t source_count,
    RudaBytecodeProgramOptions options
) {
    if (program == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || source_count == 0
        || source_count > options.function_options.max_source_files
        || !ruda_bytecode_program_valid_options(options)) {
        return false;
    }
    memset(program, 0, sizeof(*program));
    if (!ruda_vector_init(
            &program->functions,
            allocator,
            sizeof(RudaBytecodeProgramFunction)
        )) {
        return false;
    }
    program->allocator = allocator;
    program->source_count = source_count;
    program->entry_function = UINT32_MAX;
    program->options = options;
    program->initialized = true;
    return true;
}

void ruda_bytecode_program_dispose(RudaBytecodeProgram *program) {
    size_t index = 0;
    if (program == NULL) {
        return;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        RudaBytecodeProgramFunction *function = ruda_vector_at(
            &program->functions,
            index
        );
        if (function != NULL) {
            ruda_bytecode_function_dispose(&function->body);
        }
    }
    ruda_vector_dispose(&program->functions);
    memset(program, 0, sizeof(*program));
}

const RudaBytecodeProgramFunction *ruda_bytecode_program_function(
    const RudaBytecodeProgram *program,
    size_t index
) {
    return program != NULL && program->initialized
        ? ruda_vector_at_const(&program->functions, index)
        : NULL;
}

const RudaBytecodeProgramFunction *ruda_bytecode_program_function_by_symbol(
    const RudaBytecodeProgram *program,
    RudaSymbolId symbol
) {
    size_t low = 0;
    size_t high = 0;
    if (program == NULL || !program->initialized) {
        return NULL;
    }
    high = program->functions.length;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const RudaBytecodeProgramFunction *candidate =
            ruda_bytecode_program_function(program, middle);
        int comparison = candidate == NULL ? 1
            : ruda_bytecode_program_compare_symbols(candidate->symbol, symbol);
        if (comparison < 0) {
            low = middle + 1;
        } else if (comparison > 0) {
            high = middle;
        } else {
            return candidate;
        }
    }
    return NULL;
}

bool ruda_bytecode_program_take_function(
    RudaBytecodeProgram *program,
    RudaSymbolId symbol,
    RudaBytecodeFunction *function,
    uint32_t *result
) {
    RudaBytecodeProgramFunction owned;
    const RudaBytecodeProgramFunction *previous = NULL;
    if (program == NULL || !program->initialized || function == NULL
        || !function->initialized || result == NULL
        || program->functions.length >= program->options.max_functions
        || program->functions.length >= UINT32_MAX) {
        return false;
    }
    previous = program->functions.length == 0 ? NULL
        : ruda_bytecode_program_function(
            program,
            program->functions.length - 1
        );
    if (previous != NULL && ruda_bytecode_program_compare_symbols(
            previous->symbol,
            symbol
        ) >= 0) {
        return false;
    }
    owned.symbol = symbol;
    owned.body = *function;
    if (!ruda_vector_push(&program->functions, &owned)) {
        return false;
    }
    *result = (uint32_t)(program->functions.length - 1);
    memset(function, 0, sizeof(*function));
    return true;
}

bool ruda_bytecode_program_set_entry(
    RudaBytecodeProgram *program,
    RudaSymbolId symbol
) {
    const RudaBytecodeProgramFunction *function =
        ruda_bytecode_program_function_by_symbol(program, symbol);
    if (program == NULL || !program->initialized || function == NULL) {
        return false;
    }
    program->entry_function = (uint32_t)(function - (const RudaBytecodeProgramFunction *)
        program->functions.data);
    return true;
}

RudaBytecodeProgramVerifyStatus ruda_bytecode_program_verify(
    const RudaBytecodeProgram *program
) {
    RudaBytecodeProgramTotals totals;
    size_t work = 0;
    size_t index = 0;
    memset(&totals, 0, sizeof(totals));
    if (program == NULL || !program->initialized
        || !ruda_bytecode_program_valid_options(program->options)
        || program->source_count == 0
        || program->source_count > program->options.function_options.max_source_files
        || program->functions.length == 0
        || program->functions.length > program->options.max_functions
        || program->functions.length > UINT32_MAX
        || program->entry_function >= program->functions.length) {
        return RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function =
            ruda_bytecode_program_function(program, index);
        RudaBytecodeVerifyStatus function_status;
        size_t function_work = 0;
        size_t remaining_work = 0;
        size_t call_index = 0;
        if (function == NULL || !ruda_bytecode_program_consume_work(
                &work,
                1,
                program->options.max_verify_work
            )) {
            return function == NULL ? RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION
                : RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (index != 0) {
            const RudaBytecodeProgramFunction *previous =
                ruda_bytecode_program_function(program, index - 1);
            if (previous == NULL) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
            }
            if (ruda_bytecode_program_compare_symbols(
                    previous->symbol,
                    function->symbol
                ) == 0) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_DUPLICATE_SYMBOL;
            }
            if (ruda_bytecode_program_compare_symbols(
                    previous->symbol,
                    function->symbol
                ) > 0) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT;
            }
        }
        if (function->body.source_count != program->source_count
            || !ruda_bytecode_program_function_options_within(
                function->body.options,
                program->options.function_options
            )
            || !ruda_bytecode_program_accumulate_function(
                &totals,
                &function->body,
                program->options
            ) || !ruda_bytecode_program_consume_work(
                &work,
                function->body.register_types.length,
                program->options.max_verify_work
            ) || !ruda_bytecode_program_consume_work(
                &work,
                function->body.blocks.length,
                program->options.max_verify_work
            ) || !ruda_bytecode_program_consume_work(
                &work,
                function->body.instruction_count,
                program->options.max_verify_work
            ) || !ruda_bytecode_program_consume_work(
                &work,
                function->body.call_arguments.length,
                program->options.max_verify_work
            ) || !ruda_bytecode_program_consume_work(
                &work,
                function->body.array_arguments.length,
                program->options.max_verify_work
            ) || !ruda_bytecode_program_consume_nominal_work(
                &function->body,
                &work,
                program->options.max_verify_work
            )) {
            return function->body.source_count != program->source_count
                    || !ruda_bytecode_program_function_options_within(
                        function->body.options,
                        program->options.function_options
                    )
                ? RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION
                : RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        remaining_work = program->options.max_verify_work - work;
        function_status = ruda_bytecode_verify_with_work_limit(
            &function->body,
            remaining_work,
            &function_work
        );
        if (function_status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED;
        }
        if (function_status == RUDA_BYTECODE_VERIFY_LIMIT_REACHED) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (function_status != RUDA_BYTECODE_VERIFY_OK) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
        }
        if (!ruda_bytecode_program_consume_work(
                &work,
                function_work,
                program->options.max_verify_work
            )) {
            return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        for (call_index = 0; call_index < function->body.calls.length;
            call_index += 1) {
            const RudaBytecodeCall *call = ruda_bytecode_call(
                &function->body,
                (RudaBytecodeCallId)call_index
            );
            const RudaBytecodeProgramFunction *callee = NULL;
            size_t argument_index = 0;
            if (call == NULL || !ruda_bytecode_program_consume_work(
                    &work,
                    1,
                    program->options.max_verify_work
                )) {
                return call == NULL
                    ? RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION
                    : RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
            }
            if (call->interface_slot != UINT32_MAX) {
                RudaBytecodeProgramVerifyStatus interface_status =
                    ruda_bytecode_program_verify_interface_call(
                        program,
                        function,
                        call,
                        &work
                    );
                if (interface_status != RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
                    return interface_status;
                }
                continue;
            }
            callee = ruda_bytecode_program_function_by_symbol(
                program,
                call->callee
            );
            if (callee == NULL) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE;
            }
            if (!ruda_bytecode_program_same_type(
                    &function->body,
                    call->result_type,
                    call->result_layout,
                    &callee->body,
                    callee->body.result_type,
                    callee->body.result_layout
                )
                || call->argument_count != callee->body.parameter_count) {
                return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const RudaBytecodeCallArgument *argument =
                    ruda_bytecode_call_argument(
                        &function->body,
                        call->argument_start + argument_index
                    );
                const RudaBytecodeType *parameter = ruda_vector_at_const(
                    &callee->body.register_types,
                    argument_index
                );
                if (!ruda_bytecode_program_consume_work(
                        &work,
                        1,
                        program->options.max_verify_work
                    )) {
                    return RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
                }
                if (argument == NULL || parameter == NULL
                    || !ruda_bytecode_program_same_type(
                        &function->body,
                        argument->type,
                        argument->layout,
                        &callee->body,
                        *parameter,
                        ruda_bytecode_register_layout(
                            &callee->body,
                            (uint32_t)argument_index
                        )
                    )) {
                    return RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
                }
            }
        }
    }
    return ruda_bytecode_program_verify_nominal_schemas(program, &work);
}

RudaBytecodeCodecStatus ruda_bytecode_encode_program(
    const RudaBytecodeProgram *program,
    RudaAllocator allocator,
    RudaOwnedBytes *output
) {
    RudaBytecodeProgramBuffer payload;
    RudaBytecodeProgramBuffer envelope;
    RudaBytecodeProgramVerifyStatus verify_status;
    RudaBytecodeCodecStatus status = RUDA_BYTECODE_CODEC_OK;
    unsigned char digest[RUDA_BYTECODE_PROGRAM_DIGEST_LENGTH];
    size_t index = 0;
    uint64_t value = 0;
    if (output == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    if (program == NULL || !program->initialized || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    verify_status = ruda_bytecode_program_verify(program);
    status = ruda_bytecode_program_codec_from_verify(verify_status);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    memset(&payload, 0, sizeof(payload));
    memset(&envelope, 0, sizeof(envelope));
    if (!ruda_bytecode_program_buffer_init(
            &payload,
            allocator,
            program->options.max_encoded_bytes
        ) || !ruda_bytecode_program_buffer_init(
            &envelope,
            allocator,
            program->options.max_encoded_bytes
        )) {
        ruda_bytecode_program_buffer_dispose(&payload);
        ruda_bytecode_program_buffer_dispose(&envelope);
        return RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
    }
    if (!ruda_bytecode_program_size_to_u64(program->source_count, &value)
        || !ruda_bytecode_program_buffer_u64(&payload, value)
        || !ruda_bytecode_program_size_to_u64(program->functions.length, &value)
        || !ruda_bytecode_program_buffer_u64(&payload, value)
        || !ruda_bytecode_program_buffer_u64(&payload, program->entry_function)) {
        status = payload.limit_reached ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
            : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function =
            ruda_bytecode_program_function(program, index);
        RudaOwnedBytes encoded;
        memset(&encoded, 0, sizeof(encoded));
        if (function == NULL) {
            status = RUDA_BYTECODE_CODEC_VERIFY_FAILED;
            goto cleanup;
        }
        status = ruda_bytecode_encode_function(&function->body, allocator, &encoded);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!ruda_bytecode_program_size_to_u64(encoded.length, &value)
            || !ruda_bytecode_program_buffer_append(
                &payload,
                function->symbol.bytes,
                sizeof(function->symbol.bytes)
            ) || !ruda_bytecode_program_buffer_u64(&payload, value)
            || !ruda_bytecode_program_buffer_append(
                &payload,
                encoded.data,
                encoded.length
            )) {
            ruda_owned_bytes_dispose(&encoded);
            status = payload.limit_reached ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
                : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        ruda_owned_bytes_dispose(&encoded);
    }
    if (!ruda_sha256(payload.bytes.data, payload.bytes.length, digest)
        || !ruda_bytecode_program_buffer_append(
            &envelope,
            ruda_bytecode_program_magic,
            sizeof(ruda_bytecode_program_magic)
        ) || !ruda_bytecode_program_buffer_u64(
            &envelope,
            RUDA_BYTECODE_PROGRAM_FORMAT_VERSION
        ) || !ruda_bytecode_program_buffer_u64(&envelope, 0)
        || !ruda_bytecode_program_size_to_u64(payload.bytes.length, &value)
        || !ruda_bytecode_program_buffer_u64(&envelope, value)
        || !ruda_bytecode_program_buffer_append(
            &envelope,
            payload.bytes.data,
            payload.bytes.length
        ) || !ruda_bytecode_program_buffer_append(
            &envelope,
            digest,
            sizeof(digest)
        )) {
        status = envelope.limit_reached ? RUDA_BYTECODE_CODEC_LIMIT_REACHED
            : RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (!ruda_owned_bytes_copy(
            output,
            allocator,
            envelope.bytes.data,
            envelope.bytes.length
        )) {
        status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
    }

cleanup:
    ruda_bytecode_program_buffer_dispose(&payload);
    ruda_bytecode_program_buffer_dispose(&envelope);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        ruda_owned_bytes_dispose(output);
    }
    return status;
}

RudaBytecodeCodecStatus ruda_bytecode_decode_program(
    const unsigned char *bytes,
    size_t length,
    RudaAllocator allocator,
    RudaBytecodeProgramOptions options,
    RudaBytecodeProgram *output
) {
    RudaBytecodeProgramCursor envelope;
    RudaBytecodeProgramCursor payload;
    RudaBytecodeProgram program;
    RudaBytecodeCodecStatus status = RUDA_BYTECODE_CODEC_OK;
    unsigned char digest[RUDA_BYTECODE_PROGRAM_DIGEST_LENGTH];
    uint64_t value = 0;
    size_t payload_length = 0;
    size_t payload_end = 0;
    size_t envelope_end = 0;
    size_t index = 0;
    RudaBytecodeProgramTotals totals;
    if (output == NULL) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    memset(&program, 0, sizeof(program));
    memset(&totals, 0, sizeof(totals));
    if (bytes == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || !ruda_bytecode_program_valid_options(options)) {
        return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    if (length > options.max_encoded_bytes) {
        return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
    }
    if (length < RUDA_BYTECODE_PROGRAM_MAGIC_LENGTH) {
        return RUDA_BYTECODE_CODEC_TRUNCATED;
    }
    if (memcmp(bytes, ruda_bytecode_program_magic,
            RUDA_BYTECODE_PROGRAM_MAGIC_LENGTH) != 0) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    envelope.bytes = bytes;
    envelope.length = length;
    envelope.offset = RUDA_BYTECODE_PROGRAM_MAGIC_LENGTH;
    status = ruda_bytecode_program_decode_u64(&envelope, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value != RUDA_BYTECODE_PROGRAM_FORMAT_VERSION) {
        return RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    status = ruda_bytecode_program_decode_u64(&envelope, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value != 0) {
        return RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    status = ruda_bytecode_program_decode_u64(&envelope, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    if (!ruda_bytecode_program_u64_to_size(value, &payload_length)
        || !ruda_size_add(envelope.offset, payload_length, &payload_end)
        || !ruda_size_add(
            payload_end,
            RUDA_BYTECODE_PROGRAM_DIGEST_LENGTH,
            &envelope_end
        )) {
        return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
    }
    if (payload_end > length || envelope_end > length) {
        return RUDA_BYTECODE_CODEC_TRUNCATED;
    }
    if (envelope_end != length) {
        return RUDA_BYTECODE_CODEC_TRAILING_DATA;
    }
    if (!ruda_sha256(bytes + envelope.offset, payload_length, digest)
        || memcmp(digest, bytes + payload_end, sizeof(digest)) != 0) {
        return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
    }
    payload.bytes = bytes + envelope.offset;
    payload.length = payload_length;
    payload.offset = 0;
    status = ruda_bytecode_program_decode_u64(&payload, &value);
    if (status != RUDA_BYTECODE_CODEC_OK) {
        return status;
    }
    {
        size_t source_count = 0;
        size_t function_count = 0;
        if (!ruda_bytecode_program_u64_to_size(value, &source_count)
            || source_count == 0
            || source_count > options.function_options.max_source_files) {
            return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        }
        status = ruda_bytecode_program_decode_u64(&payload, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!ruda_bytecode_program_u64_to_size(value, &function_count)
            || function_count == 0 || function_count > options.max_functions
            || function_count > UINT32_MAX) {
            return RUDA_BYTECODE_CODEC_LIMIT_REACHED;
        }
        status = ruda_bytecode_program_decode_u64(&payload, &value);
        if (status != RUDA_BYTECODE_CODEC_OK) {
            return status;
        }
        if (value > UINT32_MAX) {
            return RUDA_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (!ruda_bytecode_program_init(&program, allocator, source_count, options)) {
            return RUDA_BYTECODE_CODEC_INVALID_ARGUMENT;
        }
        program.entry_function = (uint32_t)value;
        for (index = 0; index < function_count; index += 1) {
            RudaSymbolId symbol;
            RudaBytecodeFunction function;
            size_t function_length = 0;
            uint32_t function_id = 0;
            memset(&function, 0, sizeof(function));
            if (payload.length - payload.offset < sizeof(symbol.bytes)) {
                status = RUDA_BYTECODE_CODEC_TRUNCATED;
                goto cleanup;
            }
            memcpy(symbol.bytes, payload.bytes + payload.offset,
                sizeof(symbol.bytes));
            payload.offset += sizeof(symbol.bytes);
            status = ruda_bytecode_program_decode_u64(&payload, &value);
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!ruda_bytecode_program_u64_to_size(value, &function_length)) {
                status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (function_length > options.function_options.max_encoded_bytes) {
                status = RUDA_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (function_length > payload.length - payload.offset) {
                status = RUDA_BYTECODE_CODEC_TRUNCATED;
                goto cleanup;
            }
            status = ruda_bytecode_decode_function(
                payload.bytes + payload.offset,
                function_length,
                allocator,
                options.function_options,
                &function
            );
            if (status != RUDA_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            {
                bool source_mismatch = function.source_count != source_count;
                if (source_mismatch
                    || !ruda_bytecode_program_accumulate_function(
                        &totals,
                        &function,
                        options
                    )) {
                    ruda_bytecode_function_dispose(&function);
                    status = source_mismatch
                        ? RUDA_BYTECODE_CODEC_VERIFY_FAILED
                        : RUDA_BYTECODE_CODEC_LIMIT_REACHED;
                    goto cleanup;
                }
            }
            payload.offset += function_length;
            if (index != 0) {
                const RudaBytecodeProgramFunction *previous =
                    ruda_bytecode_program_function(&program, index - 1);
                if (previous == NULL
                    || ruda_bytecode_program_compare_symbols(
                        previous->symbol,
                        symbol
                    ) >= 0) {
                    ruda_bytecode_function_dispose(&function);
                    status = RUDA_BYTECODE_CODEC_INVALID_ENCODING;
                    goto cleanup;
                }
            }
            if (!ruda_bytecode_program_take_function(
                    &program,
                    symbol,
                    &function,
                    &function_id
                )) {
                ruda_bytecode_function_dispose(&function);
                status = RUDA_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if (payload.offset != payload.length) {
        status = RUDA_BYTECODE_CODEC_TRAILING_DATA;
        goto cleanup;
    }
    status = ruda_bytecode_program_codec_from_verify(
        ruda_bytecode_program_verify(&program)
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    *output = program;
    memset(&program, 0, sizeof(program));
    return RUDA_BYTECODE_CODEC_OK;

cleanup:
    ruda_bytecode_program_dispose(&program);
    memset(output, 0, sizeof(*output));
    return status;
}
