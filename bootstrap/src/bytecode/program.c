#include "mmay/bytecode/program.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/hash.h"
#include "bytecode_internal.h"
#include "program_internal.h"


enum {
    MMAY_BYTECODE_PROGRAM_MAGIC_LENGTH = 8,
    MMAY_BYTECODE_PROGRAM_DIGEST_LENGTH = 32
};

static const unsigned char mmay_bytecode_program_magic[
    MMAY_BYTECODE_PROGRAM_MAGIC_LENGTH
] = { 'R', 'U', 'D', 'A', 'B', 'C', 'P', 0 };

typedef struct MarimayBytecodeProgramBuffer {
    MarimayVector bytes;
    size_t limit;
    bool limit_reached;
} MarimayBytecodeProgramBuffer;

typedef struct MarimayBytecodeProgramCursor {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
} MarimayBytecodeProgramCursor;

static int mmay_bytecode_program_compare_symbols(
    MarimaySymbolId left,
    MarimaySymbolId right
) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes));
}

static bool mmay_bytecode_program_size_to_u64(size_t value, uint64_t *result) {
    uint64_t converted = (uint64_t)value;
    if (result == NULL || (size_t)converted != value) {
        return false;
    }
    *result = converted;
    return true;
}

static bool mmay_bytecode_program_u64_to_size(uint64_t value, size_t *result) {
    size_t converted = (size_t)value;
    if (result == NULL || (uint64_t)converted != value) {
        return false;
    }
    *result = converted;
    return true;
}

static bool mmay_bytecode_program_add_bounded(
    size_t *total,
    size_t amount,
    size_t limit
) {
    size_t next = 0;
    if (total == NULL || !mmay_size_add(*total, amount, &next) || next > limit) {
        return false;
    }
    *total = next;
    return true;
}

static bool mmay_bytecode_program_consume_work(
    size_t *work,
    size_t amount,
    size_t limit
) {
    return mmay_bytecode_program_add_bounded(work, amount, limit);
}

static bool mmay_bytecode_program_buffer_init(
    MarimayBytecodeProgramBuffer *buffer,
    MarimayAllocator allocator,
    size_t limit
) {
    if (buffer == NULL) {
        return false;
    }
    memset(buffer, 0, sizeof(*buffer));
    buffer->limit = limit;
    return mmay_vector_init(&buffer->bytes, allocator, sizeof(unsigned char));
}

static void mmay_bytecode_program_buffer_dispose(
    MarimayBytecodeProgramBuffer *buffer
) {
    if (buffer != NULL) {
        mmay_vector_dispose(&buffer->bytes);
        memset(buffer, 0, sizeof(*buffer));
    }
}

static bool mmay_bytecode_program_buffer_append(
    MarimayBytecodeProgramBuffer *buffer,
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

static bool mmay_bytecode_program_buffer_u64(
    MarimayBytecodeProgramBuffer *buffer,
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
    return mmay_bytecode_program_buffer_append(buffer, encoded, count);
}

static MarimayBytecodeCodecStatus mmay_bytecode_program_decode_u64(
    MarimayBytecodeProgramCursor *cursor,
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

static MarimayBytecodeCodecStatus mmay_bytecode_program_codec_from_verify(
    MarimayBytecodeProgramVerifyStatus status
) {
    if (status == MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
        return MMAY_BYTECODE_CODEC_OK;
    }
    if (status == MMAY_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
    }
    if (status == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED) {
        return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
    }
    return MMAY_BYTECODE_CODEC_VERIFY_FAILED;
}

MarimayBytecodeProgramOptions mmay_bytecode_program_default_options(void) {
    MarimayBytecodeProgramOptions options;
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
    options.max_total_collection_layouts = 1000000;
    options.max_total_interface_layouts = 1000000;
    options.max_total_interface_constructions = 1000000;
    options.max_total_interface_methods = 10000000;
    options.max_encoded_bytes = 256U * 1024U * 1024U;
    options.max_verify_work = 64000000;
    options.function_options = mmay_bytecode_function_default_options();
    return options;
}

bool mmay_bytecode_program_init(
    MarimayBytecodeProgram *program,
    MarimayAllocator allocator,
    size_t source_count,
    MarimayBytecodeProgramOptions options
) {
    if (program == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || source_count == 0
        || source_count > options.function_options.max_source_files
        || !mmay_bytecode_program_valid_options(options)) {
        return false;
    }
    memset(program, 0, sizeof(*program));
    if (!mmay_vector_init(
            &program->functions,
            allocator,
            sizeof(MarimayBytecodeProgramFunction)
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

void mmay_bytecode_program_dispose(MarimayBytecodeProgram *program) {
    size_t index = 0;
    if (program == NULL) {
        return;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        MarimayBytecodeProgramFunction *function = mmay_vector_at(
            &program->functions,
            index
        );
        if (function != NULL) {
            mmay_bytecode_function_dispose(&function->body);
        }
    }
    mmay_vector_dispose(&program->functions);
    memset(program, 0, sizeof(*program));
}

const MarimayBytecodeProgramFunction *mmay_bytecode_program_function(
    const MarimayBytecodeProgram *program,
    size_t index
) {
    return program != NULL && program->initialized
        ? mmay_vector_at_const(&program->functions, index)
        : NULL;
}

const MarimayBytecodeProgramFunction *mmay_bytecode_program_function_by_symbol(
    const MarimayBytecodeProgram *program,
    MarimaySymbolId symbol
) {
    size_t low = 0;
    size_t high = 0;
    if (program == NULL || !program->initialized) {
        return NULL;
    }
    high = program->functions.length;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const MarimayBytecodeProgramFunction *candidate =
            mmay_bytecode_program_function(program, middle);
        int comparison = candidate == NULL ? 1
            : mmay_bytecode_program_compare_symbols(candidate->symbol, symbol);
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

bool mmay_bytecode_program_take_function(
    MarimayBytecodeProgram *program,
    MarimaySymbolId symbol,
    MarimayBytecodeFunction *function,
    uint32_t *result
) {
    MarimayBytecodeProgramFunction owned;
    const MarimayBytecodeProgramFunction *previous = NULL;
    if (program == NULL || !program->initialized || function == NULL
        || !function->initialized || result == NULL
        || program->functions.length >= program->options.max_functions
        || program->functions.length >= UINT32_MAX) {
        return false;
    }
    previous = program->functions.length == 0 ? NULL
        : mmay_bytecode_program_function(
            program,
            program->functions.length - 1
        );
    if (previous != NULL && mmay_bytecode_program_compare_symbols(
            previous->symbol,
            symbol
        ) >= 0) {
        return false;
    }
    owned.symbol = symbol;
    owned.body = *function;
    if (!mmay_vector_push(&program->functions, &owned)) {
        return false;
    }
    *result = (uint32_t)(program->functions.length - 1);
    memset(function, 0, sizeof(*function));
    return true;
}

bool mmay_bytecode_program_set_entry(
    MarimayBytecodeProgram *program,
    MarimaySymbolId symbol
) {
    const MarimayBytecodeProgramFunction *function =
        mmay_bytecode_program_function_by_symbol(program, symbol);
    if (program == NULL || !program->initialized || function == NULL) {
        return false;
    }
    program->entry_function = (uint32_t)(function - (const MarimayBytecodeProgramFunction *)
        program->functions.data);
    return true;
}

MarimayBytecodeProgramVerifyStatus mmay_bytecode_program_verify(
    const MarimayBytecodeProgram *program
) {
    MarimayBytecodeProgramTotals totals;
    size_t work = 0;
    size_t index = 0;
    memset(&totals, 0, sizeof(totals));
    if (program == NULL || !program->initialized
        || !mmay_bytecode_program_valid_options(program->options)
        || program->source_count == 0
        || program->source_count > program->options.function_options.max_source_files
        || program->functions.length == 0
        || program->functions.length > program->options.max_functions
        || program->functions.length > UINT32_MAX
        || program->entry_function >= program->functions.length) {
        return MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function =
            mmay_bytecode_program_function(program, index);
        MarimayBytecodeVerifyStatus function_status;
        size_t function_work = 0;
        size_t remaining_work = 0;
        size_t call_index = 0;
        if (function == NULL || !mmay_bytecode_program_consume_work(
                &work,
                1,
                program->options.max_verify_work
            )) {
            return function == NULL ? MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION
                : MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (index != 0) {
            const MarimayBytecodeProgramFunction *previous =
                mmay_bytecode_program_function(program, index - 1);
            if (previous == NULL) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
            }
            if (mmay_bytecode_program_compare_symbols(
                    previous->symbol,
                    function->symbol
                ) == 0) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_DUPLICATE_SYMBOL;
            }
            if (mmay_bytecode_program_compare_symbols(
                    previous->symbol,
                    function->symbol
                ) > 0) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT;
            }
        }
        if (function->body.source_count != program->source_count
            || !mmay_bytecode_program_function_options_within(
                function->body.options,
                program->options.function_options
            )
            || !mmay_bytecode_program_accumulate_function(
                &totals,
                &function->body,
                program->options
            ) || !mmay_bytecode_program_consume_work(
                &work,
                function->body.register_types.length,
                program->options.max_verify_work
            ) || !mmay_bytecode_program_consume_work(
                &work,
                function->body.blocks.length,
                program->options.max_verify_work
            ) || !mmay_bytecode_program_consume_work(
                &work,
                function->body.instruction_count,
                program->options.max_verify_work
            ) || !mmay_bytecode_program_consume_work(
                &work,
                function->body.call_arguments.length,
                program->options.max_verify_work
            ) || !mmay_bytecode_program_consume_work(
                &work,
                function->body.array_arguments.length,
                program->options.max_verify_work
            ) || !mmay_bytecode_program_consume_nominal_work(
                &function->body,
                &work,
                program->options.max_verify_work
            )) {
            return function->body.source_count != program->source_count
                    || !mmay_bytecode_program_function_options_within(
                        function->body.options,
                        program->options.function_options
                    )
                ? MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION
                : MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        remaining_work = program->options.max_verify_work - work;
        function_status = mmay_bytecode_verify_with_work_limit(
            &function->body,
            remaining_work,
            &function_work
        );
        if (function_status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED;
        }
        if (function_status == MMAY_BYTECODE_VERIFY_LIMIT_REACHED) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        if (function_status != MMAY_BYTECODE_VERIFY_OK) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION;
        }
        if (!mmay_bytecode_program_consume_work(
                &work,
                function_work,
                program->options.max_verify_work
            )) {
            return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
        }
        for (call_index = 0; call_index < function->body.calls.length;
            call_index += 1) {
            const MarimayBytecodeCall *call = mmay_bytecode_call(
                &function->body,
                (MarimayBytecodeCallId)call_index
            );
            const MarimayBytecodeProgramFunction *callee = NULL;
            size_t argument_index = 0;
            if (call == NULL || !mmay_bytecode_program_consume_work(
                    &work,
                    1,
                    program->options.max_verify_work
                )) {
                return call == NULL
                    ? MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION
                    : MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
            }
            if (call->interface_slot != UINT32_MAX) {
                MarimayBytecodeProgramVerifyStatus interface_status =
                    mmay_bytecode_program_verify_interface_call(
                        program,
                        function,
                        call,
                        &work
                    );
                if (interface_status != MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
                    return interface_status;
                }
                continue;
            }
            callee = mmay_bytecode_program_function_by_symbol(
                program,
                call->callee
            );
            if (callee == NULL) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE;
            }
            if (!mmay_bytecode_program_same_type(
                    &function->body,
                    call->result_type,
                    call->result_layout,
                    &callee->body,
                    callee->body.result_type,
                    callee->body.result_layout
                )
                || call->argument_count != callee->body.parameter_count) {
                return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
            }
            for (argument_index = 0;
                argument_index < call->argument_count;
                argument_index += 1) {
                const MarimayBytecodeCallArgument *argument =
                    mmay_bytecode_call_argument(
                        &function->body,
                        call->argument_start + argument_index
                    );
                const MarimayBytecodeType *parameter = mmay_vector_at_const(
                    &callee->body.register_types,
                    argument_index
                );
                if (!mmay_bytecode_program_consume_work(
                        &work,
                        1,
                        program->options.max_verify_work
                    )) {
                    return MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED;
                }
                if (argument == NULL || parameter == NULL
                    || !mmay_bytecode_program_same_type(
                        &function->body,
                        argument->type,
                        argument->layout,
                        &callee->body,
                        *parameter,
                        mmay_bytecode_register_layout(
                            &callee->body,
                            (uint32_t)argument_index
                        )
                    )) {
                    return MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH;
                }
            }
        }
    }
    return mmay_bytecode_program_verify_nominal_schemas(program, &work);
}

MarimayBytecodeCodecStatus mmay_bytecode_encode_program(
    const MarimayBytecodeProgram *program,
    MarimayAllocator allocator,
    MarimayOwnedBytes *output
) {
    MarimayBytecodeProgramBuffer payload;
    MarimayBytecodeProgramBuffer envelope;
    MarimayBytecodeProgramVerifyStatus verify_status;
    MarimayBytecodeCodecStatus status = MMAY_BYTECODE_CODEC_OK;
    unsigned char digest[MMAY_BYTECODE_PROGRAM_DIGEST_LENGTH];
    size_t index = 0;
    uint64_t value = 0;
    if (output == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    if (program == NULL || !program->initialized || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    verify_status = mmay_bytecode_program_verify(program);
    status = mmay_bytecode_program_codec_from_verify(verify_status);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    memset(&payload, 0, sizeof(payload));
    memset(&envelope, 0, sizeof(envelope));
    if (!mmay_bytecode_program_buffer_init(
            &payload,
            allocator,
            program->options.max_encoded_bytes
        ) || !mmay_bytecode_program_buffer_init(
            &envelope,
            allocator,
            program->options.max_encoded_bytes
        )) {
        mmay_bytecode_program_buffer_dispose(&payload);
        mmay_bytecode_program_buffer_dispose(&envelope);
        return MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
    }
    if (!mmay_bytecode_program_size_to_u64(program->source_count, &value)
        || !mmay_bytecode_program_buffer_u64(&payload, value)
        || !mmay_bytecode_program_size_to_u64(program->functions.length, &value)
        || !mmay_bytecode_program_buffer_u64(&payload, value)
        || !mmay_bytecode_program_buffer_u64(&payload, program->entry_function)) {
        status = payload.limit_reached ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
            : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function =
            mmay_bytecode_program_function(program, index);
        MarimayOwnedBytes encoded;
        memset(&encoded, 0, sizeof(encoded));
        if (function == NULL) {
            status = MMAY_BYTECODE_CODEC_VERIFY_FAILED;
            goto cleanup;
        }
        status = mmay_bytecode_encode_function(&function->body, allocator, &encoded);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            goto cleanup;
        }
        if (!mmay_bytecode_program_size_to_u64(encoded.length, &value)
            || !mmay_bytecode_program_buffer_append(
                &payload,
                function->symbol.bytes,
                sizeof(function->symbol.bytes)
            ) || !mmay_bytecode_program_buffer_u64(&payload, value)
            || !mmay_bytecode_program_buffer_append(
                &payload,
                encoded.data,
                encoded.length
            )) {
            mmay_owned_bytes_dispose(&encoded);
            status = payload.limit_reached ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
                : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
            goto cleanup;
        }
        mmay_owned_bytes_dispose(&encoded);
    }
    if (!mmay_sha256(payload.bytes.data, payload.bytes.length, digest)
        || !mmay_bytecode_program_buffer_append(
            &envelope,
            mmay_bytecode_program_magic,
            sizeof(mmay_bytecode_program_magic)
        ) || !mmay_bytecode_program_buffer_u64(
            &envelope,
            MMAY_BYTECODE_PROGRAM_FORMAT_VERSION
        ) || !mmay_bytecode_program_buffer_u64(&envelope, 0)
        || !mmay_bytecode_program_size_to_u64(payload.bytes.length, &value)
        || !mmay_bytecode_program_buffer_u64(&envelope, value)
        || !mmay_bytecode_program_buffer_append(
            &envelope,
            payload.bytes.data,
            payload.bytes.length
        ) || !mmay_bytecode_program_buffer_append(
            &envelope,
            digest,
            sizeof(digest)
        )) {
        status = envelope.limit_reached ? MMAY_BYTECODE_CODEC_LIMIT_REACHED
            : MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
        goto cleanup;
    }
    if (!mmay_owned_bytes_copy(
            output,
            allocator,
            envelope.bytes.data,
            envelope.bytes.length
        )) {
        status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
    }

cleanup:
    mmay_bytecode_program_buffer_dispose(&payload);
    mmay_bytecode_program_buffer_dispose(&envelope);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        mmay_owned_bytes_dispose(output);
    }
    return status;
}

MarimayBytecodeCodecStatus mmay_bytecode_decode_program(
    const unsigned char *bytes,
    size_t length,
    MarimayAllocator allocator,
    MarimayBytecodeProgramOptions options,
    MarimayBytecodeProgram *output
) {
    MarimayBytecodeProgramCursor envelope;
    MarimayBytecodeProgramCursor payload;
    MarimayBytecodeProgram program;
    MarimayBytecodeCodecStatus status = MMAY_BYTECODE_CODEC_OK;
    unsigned char digest[MMAY_BYTECODE_PROGRAM_DIGEST_LENGTH];
    uint64_t value = 0;
    size_t payload_length = 0;
    size_t payload_end = 0;
    size_t envelope_end = 0;
    size_t index = 0;
    MarimayBytecodeProgramTotals totals;
    if (output == NULL) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    memset(&program, 0, sizeof(program));
    memset(&totals, 0, sizeof(totals));
    if (bytes == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || !mmay_bytecode_program_valid_options(options)) {
        return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
    }
    if (length > options.max_encoded_bytes) {
        return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
    }
    if (length < MMAY_BYTECODE_PROGRAM_MAGIC_LENGTH) {
        return MMAY_BYTECODE_CODEC_TRUNCATED;
    }
    if (memcmp(bytes, mmay_bytecode_program_magic,
            MMAY_BYTECODE_PROGRAM_MAGIC_LENGTH) != 0) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    envelope.bytes = bytes;
    envelope.length = length;
    envelope.offset = MMAY_BYTECODE_PROGRAM_MAGIC_LENGTH;
    status = mmay_bytecode_program_decode_u64(&envelope, &value);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value != MMAY_BYTECODE_PROGRAM_FORMAT_VERSION) {
        return MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    status = mmay_bytecode_program_decode_u64(&envelope, &value);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if (value != 0) {
        return MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION;
    }
    status = mmay_bytecode_program_decode_u64(&envelope, &value);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    if (!mmay_bytecode_program_u64_to_size(value, &payload_length)
        || !mmay_size_add(envelope.offset, payload_length, &payload_end)
        || !mmay_size_add(
            payload_end,
            MMAY_BYTECODE_PROGRAM_DIGEST_LENGTH,
            &envelope_end
        )) {
        return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
    }
    if (payload_end > length || envelope_end > length) {
        return MMAY_BYTECODE_CODEC_TRUNCATED;
    }
    if (envelope_end != length) {
        return MMAY_BYTECODE_CODEC_TRAILING_DATA;
    }
    if (!mmay_sha256(bytes + envelope.offset, payload_length, digest)
        || memcmp(digest, bytes + payload_end, sizeof(digest)) != 0) {
        return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
    }
    payload.bytes = bytes + envelope.offset;
    payload.length = payload_length;
    payload.offset = 0;
    status = mmay_bytecode_program_decode_u64(&payload, &value);
    if (status != MMAY_BYTECODE_CODEC_OK) {
        return status;
    }
    {
        size_t source_count = 0;
        size_t function_count = 0;
        if (!mmay_bytecode_program_u64_to_size(value, &source_count)
            || source_count == 0
            || source_count > options.function_options.max_source_files) {
            return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        }
        status = mmay_bytecode_program_decode_u64(&payload, &value);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if (!mmay_bytecode_program_u64_to_size(value, &function_count)
            || function_count == 0 || function_count > options.max_functions
            || function_count > UINT32_MAX) {
            return MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        }
        status = mmay_bytecode_program_decode_u64(&payload, &value);
        if (status != MMAY_BYTECODE_CODEC_OK) {
            return status;
        }
        if (value > UINT32_MAX) {
            return MMAY_BYTECODE_CODEC_INVALID_ENCODING;
        }
        if (!mmay_bytecode_program_init(&program, allocator, source_count, options)) {
            return MMAY_BYTECODE_CODEC_INVALID_ARGUMENT;
        }
        program.entry_function = (uint32_t)value;
        for (index = 0; index < function_count; index += 1) {
            MarimaySymbolId symbol;
            MarimayBytecodeFunction function;
            size_t function_length = 0;
            uint32_t function_id = 0;
            memset(&function, 0, sizeof(function));
            if (payload.length - payload.offset < sizeof(symbol.bytes)) {
                status = MMAY_BYTECODE_CODEC_TRUNCATED;
                goto cleanup;
            }
            memcpy(symbol.bytes, payload.bytes + payload.offset,
                sizeof(symbol.bytes));
            payload.offset += sizeof(symbol.bytes);
            status = mmay_bytecode_program_decode_u64(&payload, &value);
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            if (!mmay_bytecode_program_u64_to_size(value, &function_length)) {
                status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (function_length > options.function_options.max_encoded_bytes) {
                status = MMAY_BYTECODE_CODEC_LIMIT_REACHED;
                goto cleanup;
            }
            if (function_length > payload.length - payload.offset) {
                status = MMAY_BYTECODE_CODEC_TRUNCATED;
                goto cleanup;
            }
            status = mmay_bytecode_decode_function(
                payload.bytes + payload.offset,
                function_length,
                allocator,
                options.function_options,
                &function
            );
            if (status != MMAY_BYTECODE_CODEC_OK) {
                goto cleanup;
            }
            {
                bool source_mismatch = function.source_count != source_count;
                if (source_mismatch
                    || !mmay_bytecode_program_accumulate_function(
                        &totals,
                        &function,
                        options
                    )) {
                    mmay_bytecode_function_dispose(&function);
                    status = source_mismatch
                        ? MMAY_BYTECODE_CODEC_VERIFY_FAILED
                        : MMAY_BYTECODE_CODEC_LIMIT_REACHED;
                    goto cleanup;
                }
            }
            payload.offset += function_length;
            if (index != 0) {
                const MarimayBytecodeProgramFunction *previous =
                    mmay_bytecode_program_function(&program, index - 1);
                if (previous == NULL
                    || mmay_bytecode_program_compare_symbols(
                        previous->symbol,
                        symbol
                    ) >= 0) {
                    mmay_bytecode_function_dispose(&function);
                    status = MMAY_BYTECODE_CODEC_INVALID_ENCODING;
                    goto cleanup;
                }
            }
            if (!mmay_bytecode_program_take_function(
                    &program,
                    symbol,
                    &function,
                    &function_id
                )) {
                mmay_bytecode_function_dispose(&function);
                status = MMAY_BYTECODE_CODEC_ALLOCATION_FAILED;
                goto cleanup;
            }
        }
    }
    if (payload.offset != payload.length) {
        status = MMAY_BYTECODE_CODEC_TRAILING_DATA;
        goto cleanup;
    }
    status = mmay_bytecode_program_codec_from_verify(
        mmay_bytecode_program_verify(&program)
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    *output = program;
    memset(&program, 0, sizeof(program));
    return MMAY_BYTECODE_CODEC_OK;

cleanup:
    mmay_bytecode_program_dispose(&program);
    memset(output, 0, sizeof(*output));
    return status;
}
