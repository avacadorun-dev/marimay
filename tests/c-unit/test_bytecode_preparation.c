#include "test_bytecode_support.h"

/* Compare every embedded body with the independently verified public encoder. */
static void preparation_compare_bodies(const MarimayBytecodeProgram *program,
    const MarimayOwnedBytes *encoded) {
    size_t offset = 8;
    uint64_t value = 0, payload_size = 0;
    MMAY_EXPECT(mmay_test_bytecode_read_u64(encoded->data, encoded->length,
        &offset, &value) && value == MMAY_BYTECODE_PROGRAM_FORMAT_VERSION);
    MMAY_EXPECT(mmay_test_bytecode_read_u64(encoded->data, encoded->length,
        &offset, &value) && value == 0);
    MMAY_EXPECT(mmay_test_bytecode_read_u64(encoded->data, encoded->length,
        &offset, &payload_size));
    size_t payload_start = offset;
    MMAY_EXPECT(mmay_test_bytecode_read_u64(encoded->data, encoded->length,
        &offset, &value) && value == program->source_count);
    MMAY_EXPECT(mmay_test_bytecode_read_u64(encoded->data, encoded->length,
        &offset, &value) && value == program->functions.length);
    MMAY_EXPECT(mmay_test_bytecode_read_u64(encoded->data, encoded->length,
        &offset, &value) && value == program->entry_function);
    for (size_t index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function =
            mmay_bytecode_program_function(program, index);
        MarimayOwnedBytes standalone = {0};
        MMAY_EXPECT(function != NULL);
        if (function == NULL || offset > encoded->length
            || sizeof(function->symbol.bytes) > encoded->length - offset) return;
        MMAY_EXPECT(memcmp(encoded->data + offset, function->symbol.bytes,
            sizeof(function->symbol.bytes)) == 0);
        offset += sizeof(function->symbol.bytes);
        MMAY_EXPECT(mmay_test_bytecode_read_u64(encoded->data, encoded->length,
            &offset, &value));
        MMAY_EXPECT(mmay_bytecode_encode_function(&function->body,
            mmay_system_allocator(), &standalone) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(value == standalone.length);
        if (value == standalone.length && offset <= encoded->length
            && value <= encoded->length - offset) {
            MMAY_EXPECT(memcmp(encoded->data + offset, standalone.data,
                standalone.length) == 0);
            offset += standalone.length;
        }
        mmay_owned_bytes_dispose(&standalone);
    }
    MMAY_EXPECT(offset - payload_start == payload_size);
    MMAY_EXPECT(offset + 32 == encoded->length);
}

static void preparation_codec_boundaries(void) {
    MarimayFailAllocator state;
    MarimayBytecodeProgram program = {0};
    MarimayOwnedBytes encoded = {0};
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator allocator = mmay_fail_allocator(&state);
    MMAY_EXPECT(mmay_test_bytecode_build_direct_program(&program, allocator));
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, allocator, &encoded)
        == MMAY_BYTECODE_CODEC_OK);
    preparation_compare_bodies(&program, &encoded);
    size_t encoded_size = encoded.length;
    mmay_owned_bytes_dispose(&encoded);

    /* Verification is fresh on each invocation, including after a success. */
    MarimayBytecodeProgramFunction *entry = mmay_vector_at(&program.functions,
        program.entry_function);
    MMAY_EXPECT(entry != NULL);
    if (entry != NULL) {
        MarimayBytecodeInstruction *instruction = mmay_test_bytecode_at(
            &entry->body, 0, 0);
        MMAY_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            MarimayOpcode saved = instruction->opcode;
            instruction->opcode = MMAY_OPCODE_INVALID;
            MMAY_EXPECT(mmay_bytecode_encode_program(&program, allocator,
                &encoded) == MMAY_BYTECODE_CODEC_VERIFY_FAILED);
            MMAY_EXPECT(encoded.data == NULL && encoded.length == 0);
            MMAY_EXPECT(mmay_bytecode_encode_function(&entry->body, allocator,
                &encoded) == MMAY_BYTECODE_CODEC_VERIFY_FAILED);
            MMAY_EXPECT(encoded.data == NULL && encoded.length == 0);
            instruction->opcode = saved;
        }
    }
    bool saw_limit = false, saw_success = false;
    for (size_t limit = 1; limit < 128; limit += 1) {
        program.options.max_verify_work = limit;
        MarimayBytecodeProgramVerifyStatus verified =
            mmay_bytecode_program_verify(&program);
        MarimayBytecodeCodecStatus status = mmay_bytecode_encode_program(
            &program, allocator, &encoded);
        MMAY_EXPECT((verified == MMAY_BYTECODE_PROGRAM_VERIFY_OK
                && status == MMAY_BYTECODE_CODEC_OK)
            || (verified == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED
                && status == MMAY_BYTECODE_CODEC_LIMIT_REACHED));
        saw_limit |= status == MMAY_BYTECODE_CODEC_LIMIT_REACHED;
        saw_success |= status == MMAY_BYTECODE_CODEC_OK;
        mmay_owned_bytes_dispose(&encoded);
    }
    MMAY_EXPECT(saw_limit && saw_success);
    program.options.max_verify_work =
        mmay_bytecode_program_default_options().max_verify_work;
    if (entry != NULL) {
        size_t saved = entry->body.options.max_verify_work;
        entry->body.options.max_verify_work = 1;
        MMAY_EXPECT(mmay_bytecode_encode_program(&program, allocator,
            &encoded) == MMAY_BYTECODE_CODEC_LIMIT_REACHED);
        MMAY_EXPECT(encoded.data == NULL && encoded.length == 0);
        entry->body.options.max_verify_work = saved;
        saved = entry->body.options.max_encoded_bytes;
        entry->body.options.max_encoded_bytes = 8;
        MMAY_EXPECT(mmay_bytecode_encode_program(&program, allocator,
            &encoded) == MMAY_BYTECODE_CODEC_LIMIT_REACHED);
        MMAY_EXPECT(encoded.data == NULL && encoded.length == 0);
        entry->body.options.max_encoded_bytes = saved;
    }
    program.options.max_encoded_bytes = encoded_size - 1;
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, allocator,
        &encoded) == MMAY_BYTECODE_CODEC_LIMIT_REACHED);
    MMAY_EXPECT(encoded.data == NULL && encoded.length == 0);
    program.options.max_encoded_bytes = encoded_size;
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, allocator,
        &encoded) == MMAY_BYTECODE_CODEC_OK);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
}

static void preparation_codec_allocation_failures(void) {
    bool complete = false;
    for (size_t failure = 0; failure < 512 && !complete; failure += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeProgram program = {0};
        MarimayOwnedBytes encoded = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MarimayAllocator allocator = mmay_fail_allocator(&state);
        MMAY_EXPECT(mmay_test_bytecode_build_direct_program(&program, allocator));
        state.failure_index = state.attempts + failure;
        MarimayBytecodeCodecStatus status = mmay_bytecode_encode_program(
            &program, allocator, &encoded);
        MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_OK
            || status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
        if (status == MMAY_BYTECODE_CODEC_OK) {
            complete = true;
            state.failure_index = SIZE_MAX;
            preparation_compare_bodies(&program, &encoded);
        } else {
            MMAY_EXPECT(encoded.data == NULL && encoded.length == 0);
        }
        mmay_owned_bytes_dispose(&encoded);
        mmay_bytecode_program_dispose(&program);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(complete);
}

void mmay_test_bytecode_preparation(void) {
    preparation_codec_boundaries();
    preparation_codec_allocation_failures();
}
