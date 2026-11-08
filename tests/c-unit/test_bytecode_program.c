#include "test_bytecode_support.h"

void mmay_test_bytecode_calls_and_program(void) {
    const MarimaySymbolId callee_symbol = {{0x10}};
    const MarimaySymbolId caller_symbol = {{0x20}};
    MarimayBytecodeFunction caller;
    MarimayBytecodeFunction callee;
    MarimayBytecodeFunction decoded_function;
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded_program;
    MarimayOwnedBytes function_bytes;
    MarimayOwnedBytes program_bytes;
    MarimayOwnedBytes program_again;
    MarimayTestBuffer buffer;
    MarimayAllocator allocator = mmay_system_allocator();
    size_t index = 0;

    memset(&caller, 0, sizeof(caller));
    memset(&callee, 0, sizeof(callee));
    memset(&decoded_function, 0, sizeof(decoded_function));
    memset(&program, 0, sizeof(program));
    memset(&decoded_program, 0, sizeof(decoded_program));
    memset(&function_bytes, 0, sizeof(function_bytes));
    memset(&program_bytes, 0, sizeof(program_bytes));
    memset(&program_again, 0, sizeof(program_again));
    MMAY_EXPECT(mmay_test_bytecode_build_direct_caller(
        &caller, mmay_system_allocator(), callee_symbol));
    MMAY_EXPECT(mmay_bytecode_verify(&caller) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(caller.calls.length == 1 && caller.call_arguments.length == 2);
    MMAY_EXPECT(mmay_bytecode_encode_function(&caller, mmay_system_allocator(),
        &function_bytes) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(function_bytes.length > 10 && function_bytes.data[9] == 1);
    for (index = 0; index < function_bytes.length; index += 1) {
        MMAY_EXPECT(mmay_bytecode_decode_function(function_bytes.data, index,
            mmay_system_allocator(), mmay_bytecode_function_default_options(),
            &decoded_function) == MMAY_BYTECODE_CODEC_TRUNCATED);
        MMAY_EXPECT(!decoded_function.initialized);
    }
    MMAY_EXPECT(mmay_bytecode_decode_function(function_bytes.data,
        function_bytes.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded_function)
        == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(decoded_function.calls.length == 1
        && decoded_function.call_arguments.length == 2);
    mmay_bytecode_function_dispose(&decoded_function);
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_bytecode_disassemble(&caller, mmay_test_buffer_writer(&buffer))
        == MMAY_BYTECODE_DISASSEMBLE_OK);
    MMAY_EXPECT(strstr(buffer.bytes, "call_direct r2, c0 @s0:30..31\n") != NULL);

    {
        MarimayBytecodeInstruction *call_instruction =
            mmay_test_bytecode_at(&caller, 0, 0);
        MarimayBytecodeCall *call = mmay_vector_at(&caller.calls, 0);
        MarimayBytecodeCallArgument *argument = mmay_vector_at(
            &caller.call_arguments, 0);
        MMAY_EXPECT(call_instruction != NULL && call != NULL && argument != NULL);
        if (call_instruction != NULL) {
            call_instruction->operands[1].unsigned_value = 1;
            MMAY_EXPECT(mmay_bytecode_verify(&caller)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            call_instruction->operands[1].unsigned_value = 0;
        }
        if (argument != NULL) {
            argument->register_id = 9;
            MMAY_EXPECT(mmay_bytecode_verify(&caller)
                == MMAY_BYTECODE_VERIFY_INVALID_REGISTER);
            argument->register_id = 1;
            argument->type = MMAY_BYTECODE_TYPE_BOOL;
            MMAY_EXPECT(mmay_bytecode_verify(&caller)
                == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
            argument->type = MMAY_BYTECODE_TYPE_I64;
        }
        if (call != NULL) {
            call->argument_start = 1;
            MMAY_EXPECT(mmay_bytecode_verify(&caller)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            call->argument_start = 0;
        }
    }

    MMAY_EXPECT(mmay_test_bytecode_build_direct_callee(
        &callee, mmay_system_allocator()));
    MMAY_EXPECT(mmay_bytecode_program_init(&program, mmay_system_allocator(), 1,
        mmay_bytecode_program_default_options()));
    {
        uint32_t function_id = UINT32_MAX;
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, callee_symbol,
            &callee, &function_id));
        MMAY_EXPECT(function_id == 0);
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, caller_symbol,
            &caller, &function_id));
        MMAY_EXPECT(function_id == 1);
    }
    MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, caller_symbol));
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_verify_work = 35;
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_verify_work = 36;
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_verify_work =
        mmay_bytecode_program_default_options().max_verify_work;
    MMAY_EXPECT(mmay_bytecode_program_function_by_symbol(&program, callee_symbol)
        == mmay_bytecode_program_function(&program, 0));
    MMAY_EXPECT(mmay_bytecode_encode_program(&program, mmay_system_allocator(),
        &program_bytes) == MMAY_BYTECODE_CODEC_OK);
    for (index = 0; index < program_bytes.length; index += 1) {
        MMAY_EXPECT(mmay_bytecode_decode_program(program_bytes.data, index,
            mmay_system_allocator(), mmay_bytecode_program_default_options(),
            &decoded_program) == MMAY_BYTECODE_CODEC_TRUNCATED);
        MMAY_EXPECT(!decoded_program.initialized);
    }
    MMAY_EXPECT(mmay_bytecode_decode_program(program_bytes.data, program_bytes.length,
        mmay_system_allocator(), mmay_bytecode_program_default_options(),
        &decoded_program) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_encode_program(&decoded_program,
        mmay_system_allocator(), &program_again) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(program_bytes.length == program_again.length
        && memcmp(program_bytes.data, program_again.data, program_bytes.length) == 0);
    {
        MarimayBytecodeProgramOptions limited =
            mmay_bytecode_program_default_options();
        limited.max_total_registers = 5;
        mmay_bytecode_program_dispose(&decoded_program);
        MMAY_EXPECT(mmay_bytecode_decode_program(
            program_bytes.data,
            program_bytes.length,
            mmay_system_allocator(),
            limited,
            &decoded_program
        ) == MMAY_BYTECODE_CODEC_LIMIT_REACHED);
        MMAY_EXPECT(!decoded_program.initialized);
        MMAY_EXPECT(mmay_bytecode_decode_program(
            program_bytes.data,
            program_bytes.length,
            mmay_system_allocator(),
            mmay_bytecode_program_default_options(),
            &decoded_program
        ) == MMAY_BYTECODE_CODEC_OK);
    }
    mmay_bytecode_program_dispose(&decoded_program);

    {
        MarimayBytecodeProgramFunction *callee_function = mmay_vector_at(&program.functions, 0);
        MarimayBytecodeProgramFunction *caller_function = mmay_vector_at(&program.functions, 1);
        MarimayBytecodeCall *call = caller_function != NULL
            ? mmay_vector_at(&caller_function->body.calls, 0) : NULL;
        MarimayBytecodeInstruction *instruction = caller_function != NULL
            ? mmay_test_bytecode_at(&caller_function->body, 0, 0) : NULL;
        uint32_t entry = program.entry_function;
        MMAY_EXPECT(callee_function != NULL && caller_function != NULL
            && call != NULL && instruction != NULL);
        program.entry_function = 2;
        MMAY_EXPECT(mmay_bytecode_program_verify(&program)
            == MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT);
        program.entry_function = entry;
        if (call != NULL) {
            call->callee.bytes[0] = 0x30;
            MMAY_EXPECT(mmay_bytecode_program_verify(&program)
                == MMAY_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE);
            call->callee = callee_symbol;
            MarimayBytecodeType *parameter = mmay_vector_at(
                &callee_function->body.register_types, 0);
            MMAY_EXPECT(parameter != NULL);
            if (parameter != NULL) {
                *parameter = MMAY_BYTECODE_TYPE_BOOL;
            }
            MMAY_EXPECT(mmay_bytecode_program_verify(&program)
                == MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
            if (parameter != NULL) {
                *parameter = MMAY_BYTECODE_TYPE_I64;
            }
        }
        if (instruction != NULL) {
            instruction->operands[0].unsigned_value = 99;
            MMAY_EXPECT(mmay_bytecode_program_verify(&program)
                == MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
            instruction->operands[0].unsigned_value = 2;
        }
        if (callee_function != NULL && caller_function != NULL) {
            MarimaySymbolId saved = callee_function->symbol;
            callee_function->symbol = caller_function->symbol;
            MMAY_EXPECT(mmay_bytecode_program_verify(&program)
                == MMAY_BYTECODE_PROGRAM_VERIFY_DUPLICATE_SYMBOL);
            callee_function->symbol = saved;
            callee_function->symbol.bytes[0] = 0x30;
            MMAY_EXPECT(mmay_bytecode_program_verify(&program)
                == MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT);
            callee_function->symbol = saved;
        }
    }
    {
        unsigned char *mutated = mmay_allocate_array(&allocator,
            program_bytes.length + 1, sizeof(unsigned char));
        MMAY_EXPECT(mutated != NULL);
        if (mutated != NULL) {
            size_t payload_offset = 8;
            uint64_t ignored = 0;
            uint64_t payload_length = 0;
            MMAY_EXPECT(mmay_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset, &ignored
            ));
            MMAY_EXPECT(mmay_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset, &ignored
            ));
            MMAY_EXPECT(mmay_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset,
                &payload_length
            ));
            MMAY_EXPECT(payload_length != 0
                && payload_length <= program_bytes.length - payload_offset - 32);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[0] = 'X';
            MMAY_EXPECT(mmay_bytecode_decode_program(mutated, program_bytes.length,
                mmay_system_allocator(), mmay_bytecode_program_default_options(),
                &decoded_program) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            if (payload_length != 0
                && payload_length <= program_bytes.length - payload_offset - 32) {
                /* Keep the envelope digest intact: only payload integrity changes. */
                mutated[payload_offset] ^= 1;
                MMAY_EXPECT(mmay_bytecode_decode_program(mutated,
                    program_bytes.length, mmay_system_allocator(),
                    mmay_bytecode_program_default_options(), &decoded_program)
                    == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
            }
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[8] = 2;
            MMAY_EXPECT(mmay_bytecode_decode_program(mutated, program_bytes.length,
                mmay_system_allocator(), mmay_bytecode_program_default_options(),
                &decoded_program) == MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[9] = 1;
            MMAY_EXPECT(mmay_bytecode_decode_program(mutated, program_bytes.length,
                mmay_system_allocator(), mmay_bytecode_program_default_options(),
                &decoded_program) == MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[program_bytes.length - 1] ^= 1;
            MMAY_EXPECT(mmay_bytecode_decode_program(mutated, program_bytes.length,
                mmay_system_allocator(), mmay_bytecode_program_default_options(),
                &decoded_program) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[program_bytes.length] = 0;
            MMAY_EXPECT(mmay_bytecode_decode_program(mutated, program_bytes.length + 1,
                mmay_system_allocator(), mmay_bytecode_program_default_options(),
                &decoded_program) == MMAY_BYTECODE_CODEC_TRAILING_DATA);
            mmay_deallocate(&allocator, mutated,
                program_bytes.length + 1);
        }
    }
    mmay_owned_bytes_dispose(&program_again);
    mmay_bytecode_program_dispose(&decoded_program);
    mmay_owned_bytes_dispose(&program_bytes);
    mmay_owned_bytes_dispose(&function_bytes);
    mmay_bytecode_program_dispose(&program);
}

void mmay_test_bytecode_string_pool(void) {
    static const unsigned char expected_feature_strings[] = {
        'R', 'U', 'D', 'A', 'B', 'C', 'F', 0,
        1, 2, 4, 1, 0, 1, 4, 1, 2,
        27, 0, 0, 0, 70, 71,
        17, 0, 0, 71, 72,
        3, 3, 'a', 0, 'b', 0, 3, 0xe2, 0x98, 0x83
    };
    static const unsigned char empty[] = "";
    static const unsigned char embedded[] = {'a', 0, 'b'};
    static const unsigned char unicode[] = {0xe2, 0x98, 0x83};
    static const unsigned char invalid_utf8[] = {0xc0, 0x80};
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayOwnedBytes encoded;
    MarimayOwnedBytes encoded_again;
    MarimayTestBuffer buffer;
    size_t index = 0;

    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    MMAY_EXPECT(mmay_test_bytecode_build_string_constant(&function,
        mmay_system_allocator(), embedded, sizeof(embedded)));
    MMAY_EXPECT(mmay_bytecode_add_string_constant(&function, empty, 0, &(MarimayBytecodeStringId){0}));
    MMAY_EXPECT(mmay_bytecode_add_string_constant(&function, unicode, sizeof(unicode),
        &(MarimayBytecodeStringId){0}));
    MMAY_EXPECT(!mmay_bytecode_add_string_constant(&function, invalid_utf8,
        sizeof(invalid_utf8), &(MarimayBytecodeStringId){0}));
    MMAY_EXPECT(!mmay_bytecode_add_string_constant(&function, NULL, 1,
        &(MarimayBytecodeStringId){0}));
    MMAY_EXPECT(function.string_constants.length == 3
        && function.string_bytes == sizeof(embedded) + sizeof(unicode));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_buffer_init(&buffer);
    MMAY_EXPECT(mmay_bytecode_disassemble(&function,
        mmay_test_buffer_writer(&buffer)) == MMAY_BYTECODE_DISASSEMBLE_OK);
    MMAY_EXPECT(strcmp(buffer.bytes,
        "bytecode function v1 result=string sources=1 parameters=0 registers=1 blocks=1\n"
        "registers r0:string\n"
        "block b0:\n"
        "  const_string r0, s0 @s0:70..71\n"
        "  return r0 @s0:71..72\n") == 0);
    MMAY_EXPECT(mmay_bytecode_encode_function(&function, mmay_system_allocator(),
        &encoded) == MMAY_BYTECODE_CODEC_OK);
    /* Feature bit 1 and the raw UTF-8/NUL pool bytes are canonical wire data. */
    MMAY_EXPECT(encoded.length == sizeof(expected_feature_strings));
    MMAY_EXPECT(memcmp(encoded.data, expected_feature_strings,
        sizeof(expected_feature_strings)) == 0);
    for (index = 0; index < encoded.length; index += 1) {
        MMAY_EXPECT(mmay_bytecode_decode_function(encoded.data, index,
            mmay_system_allocator(), mmay_bytecode_function_default_options(),
            &decoded) == MMAY_BYTECODE_CODEC_TRUNCATED);
        MMAY_EXPECT(!decoded.initialized);
    }
    MMAY_EXPECT(mmay_bytecode_decode_function(encoded.data, encoded.length,
        mmay_system_allocator(), mmay_bytecode_function_default_options(),
        &decoded) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(decoded.string_constants.length == 3
        && decoded.string_bytes == sizeof(embedded) + sizeof(unicode));
    MMAY_EXPECT(mmay_bytecode_encode_function(&decoded, mmay_system_allocator(),
        &encoded_again) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(encoded_again.length == sizeof(expected_feature_strings)
        && memcmp(encoded_again.data, expected_feature_strings,
            sizeof(expected_feature_strings)) == 0);
    {
        const MarimayBytecodeStringConstant *first = mmay_bytecode_string_constant(
            &decoded, 0);
        const MarimayBytecodeStringConstant *second = mmay_bytecode_string_constant(
            &decoded, 1);
        const MarimayBytecodeStringConstant *third = mmay_bytecode_string_constant(
            &decoded, 2);
        MMAY_EXPECT(first != NULL && second != NULL && third != NULL);
        if (first != NULL) {
            MMAY_EXPECT(first->value.length == sizeof(embedded)
                && memcmp(first->value.data, embedded, sizeof(embedded)) == 0);
        }
        if (second != NULL) {
            MMAY_EXPECT(second->value.length == 0);
        }
        if (third != NULL) {
            MMAY_EXPECT(third->value.length == sizeof(unicode)
                && memcmp(third->value.data, unicode, sizeof(unicode)) == 0);
        }
    }
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded_again);

    {
        unsigned char *mutated = mmay_allocate_array(
            &function.allocator, encoded.length + 1, sizeof(unsigned char)
        );
        MMAY_EXPECT(mutated != NULL);
        if (mutated != NULL) {
            /* The format version is the first one-byte varint after the magic. */
            memcpy(mutated, encoded.data, 8);
            mutated[8] = 0x81;
            mutated[9] = 0;
            memcpy(mutated + 10, encoded.data + 9, encoded.length - 9);
            MMAY_EXPECT(mmay_bytecode_decode_function(mutated,
                encoded.length + 1, mmay_system_allocator(),
                mmay_bytecode_function_default_options(), &decoded)
                == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, encoded.data, encoded.length);
            MMAY_EXPECT(encoded.length >= sizeof(unicode));
            if (encoded.length >= sizeof(unicode)) {
                /* Canonical strings are raw UTF-8 at the end of this fixture. */
                mutated[encoded.length - sizeof(unicode)] = 0xff;
                MMAY_EXPECT(mmay_bytecode_decode_function(mutated, encoded.length,
                    mmay_system_allocator(), mmay_bytecode_function_default_options(),
                    &decoded) == MMAY_BYTECODE_CODEC_INVALID_ENCODING);
            }
            mmay_deallocate(&function.allocator, mutated, encoded.length + 1);
        }
    }
    mmay_owned_bytes_dispose(&encoded);

    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeFunction allocated;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
            if (mmay_test_bytecode_build_string_constant(&allocated,
                    mmay_fail_allocator(&state), embedded, sizeof(embedded))) {
                MarimayBytecodeStringId ignored = MMAY_BYTECODE_NO_STRING;
                if (mmay_bytecode_add_string_constant(&allocated, empty,
                        0, &ignored)) {
                    reached_success = true;
                }
                mmay_bytecode_function_dispose(&allocated);
            }
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
    }
    {
        size_t failure_index = 0;
        bool reached_success = false;
        MMAY_EXPECT(mmay_bytecode_encode_function(&function,
            mmay_system_allocator(), &encoded) == MMAY_BYTECODE_CODEC_OK);
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayOwnedBytes failed_encoded;
            MarimayBytecodeCodecStatus status;
            memset(&failed_encoded, 0, sizeof(failed_encoded));
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
            status = mmay_bytecode_encode_function(&function,
                mmay_fail_allocator(&state), &failed_encoded);
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED
                || status == MMAY_BYTECODE_CODEC_OK);
            if (status == MMAY_BYTECODE_CODEC_OK) {
                reached_success = true;
            }
            mmay_owned_bytes_dispose(&failed_encoded);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
        mmay_owned_bytes_dispose(&encoded);
    }

    {
        MarimayBytecodeInstruction *instruction = mmay_test_bytecode_at(
            &function, 0, 0);
        MMAY_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->operands[1].unsigned_value = 3;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            instruction->operands[1].unsigned_value = 0;
        }
    }
    {
        MarimayBytecodeFunctionOptions limited =
            mmay_bytecode_function_default_options();
        MarimayBytecodeStringId id = MMAY_BYTECODE_NO_STRING;
        limited.max_string_constants = 1;
        MMAY_EXPECT(mmay_bytecode_function_init(&decoded, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_VOID, 1, NULL, 0, limited));
        MMAY_EXPECT(mmay_bytecode_add_string_constant(&decoded, empty, 0, &id));
        MMAY_EXPECT(!mmay_bytecode_add_string_constant(&decoded, unicode,
            sizeof(unicode), &id));
        mmay_bytecode_function_dispose(&decoded);
        limited = mmay_bytecode_function_default_options();
        limited.max_string_bytes = 2;
        MMAY_EXPECT(mmay_bytecode_function_init(&decoded, mmay_system_allocator(),
            MMAY_BYTECODE_TYPE_VOID, 1, NULL, 0, limited));
        MMAY_EXPECT(!mmay_bytecode_add_string_constant(&decoded, unicode,
            sizeof(unicode), &id));
        mmay_bytecode_function_dispose(&decoded);
    }
    {
        const MarimaySymbolId symbol = {{0x10}};
        MarimayBytecodeProgram program;
        MarimayBytecodeProgramOptions options = mmay_bytecode_program_default_options();
        uint32_t id = UINT32_MAX;
        memset(&program, 0, sizeof(program));
        options.max_total_string_bytes = 2;
        MMAY_EXPECT(mmay_bytecode_program_init(&program, mmay_system_allocator(),
            1, options));
        MMAY_EXPECT(mmay_test_bytecode_build_string_constant(&decoded,
            mmay_system_allocator(), embedded, sizeof(embedded)));
        MMAY_EXPECT(mmay_bytecode_program_take_function(&program, symbol,
            &decoded, &id) && id == 0);
        MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, symbol));
        MMAY_EXPECT(mmay_bytecode_program_verify(&program)
            == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
        mmay_bytecode_program_dispose(&program);
    }
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_bytecode_string_verifier_rejections(void) {
    const MarimaySymbolId callee = {{0x10}};
    MarimayBytecodeFunction function;
    MarimayBytecodeInstruction *instruction;
    MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
    uint32_t block = UINT32_MAX;
    size_t span = 100;

    MMAY_EXPECT(mmay_test_bytecode_build_string_equality(&function,
        mmay_system_allocator(), true));
    instruction = mmay_test_bytecode_at(&function, 0,
        ((MarimayBytecodeBlock *)mmay_vector_at(&function.blocks, 0))
            ->instructions.length - 2);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[1].unsigned_value = 3;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_string_equality(&function,
        mmay_system_allocator(), false));
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
    mmay_bytecode_function_dispose(&function);

    MMAY_EXPECT(mmay_test_bytecode_build_panic_string(&function,
        mmay_system_allocator(), MMAY_BYTECODE_TYPE_BOOL, true, false));
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    mmay_bytecode_function_dispose(&function);
    MMAY_EXPECT(mmay_test_bytecode_build_panic_string(&function,
        mmay_system_allocator(), MMAY_BYTECODE_TYPE_STRING, false, false));
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
    mmay_bytecode_function_dispose(&function);
    MMAY_EXPECT(mmay_test_bytecode_build_panic_string(&function,
        mmay_system_allocator(), MMAY_BYTECODE_TYPE_STRING, true, true));
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    mmay_bytecode_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_bytecode_function_init(&function, mmay_system_allocator(),
        MMAY_BYTECODE_TYPE_NEVER, 1, NULL, 0,
        mmay_bytecode_function_default_options()));
    MMAY_EXPECT(mmay_bytecode_add_block(&function, &block));
    MMAY_EXPECT(mmay_bytecode_add_call(&function, callee, MMAY_BYTECODE_TYPE_I64,
        NULL, 0, &call));
    MMAY_EXPECT(mmay_test_bytecode_append(&function, block,
        MMAY_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span));
    MMAY_EXPECT(mmay_bytecode_verify(&function)
        == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_bytecode_allocation_failures(void) {
    MarimayBytecodeFunction function;
    MarimayOwnedBytes bytes;
    size_t failure_index = 0;
    bool reached_success = false;

    for (failure_index = 0; failure_index < 256 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        bool built = false;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        built = mmay_test_bytecode_build_full(
            &function,
            mmay_fail_allocator(&state)
        );
        MMAY_EXPECT(built == (failure_index >= state.attempts));
        if (function.initialized) {
            reached_success = true;
            mmay_bytecode_function_dispose(&function);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

    MMAY_EXPECT(mmay_test_bytecode_build_full(&function, mmay_system_allocator()));
    memset(&bytes, 0, sizeof(bytes));
    MMAY_EXPECT(mmay_bytecode_encode_function(&function, mmay_system_allocator(),
        &bytes) == MMAY_BYTECODE_CODEC_OK);
    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes output;
        MarimayBytecodeCodecStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = mmay_bytecode_encode_function(
            &function,
            mmay_fail_allocator(&state),
            &output
        );
        if (status == MMAY_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            MMAY_EXPECT(output.data == NULL);
            MMAY_EXPECT(output.length == 0);
        }
        mmay_owned_bytes_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeFunction output;
        MarimayBytecodeCodecStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = mmay_bytecode_decode_function(
            bytes.data,
            bytes.length,
            mmay_fail_allocator(&state),
            mmay_bytecode_function_default_options(),
            &output
        );
        if (status == MMAY_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            MMAY_EXPECT(!output.initialized);
        }
        mmay_bytecode_function_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
    mmay_owned_bytes_dispose(&bytes);
    mmay_bytecode_function_dispose(&function);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeVerifyStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_bytecode_build_full(
            &function,
            mmay_fail_allocator(&state)
        ));
        state.failure_index = state.attempts + failure_index;
        status = mmay_bytecode_verify(&function);
        if (status == MMAY_BYTECODE_VERIFY_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_VERIFY_ALLOCATION_FAILED);
        }
        mmay_bytecode_function_dispose(&function);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeProgram program;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        if (mmay_test_bytecode_build_direct_program(
                &program,
                mmay_fail_allocator(&state)
            )) {
            reached_success = true;
        }
        mmay_bytecode_program_dispose(&program);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);

    {
        MarimayBytecodeProgram program;
        MarimayOwnedBytes program_bytes;
        memset(&program_bytes, 0, sizeof(program_bytes));
        MMAY_EXPECT(mmay_test_bytecode_build_direct_program(
            &program,
            mmay_system_allocator()
        ));
        MMAY_EXPECT(mmay_bytecode_encode_program(
            &program,
            mmay_system_allocator(),
            &program_bytes
        ) == MMAY_BYTECODE_CODEC_OK);
        for (failure_index = 0, reached_success = false;
            failure_index < 256 && !reached_success; failure_index += 1) {
            MarimayFailAllocator state;
            MarimayOwnedBytes output;
            MarimayBytecodeCodecStatus status;
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            memset(&output, 0, sizeof(output));
            status = mmay_bytecode_encode_program(
                &program,
                mmay_fail_allocator(&state),
                &output
            );
            if (status == MMAY_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
                MMAY_EXPECT(output.data == NULL && output.length == 0);
            }
            mmay_owned_bytes_dispose(&output);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
        for (failure_index = 0, reached_success = false;
            failure_index < 256 && !reached_success; failure_index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeProgram output;
            MarimayBytecodeCodecStatus status;
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            memset(&output, 0, sizeof(output));
            status = mmay_bytecode_decode_program(
                program_bytes.data,
                program_bytes.length,
                mmay_fail_allocator(&state),
                mmay_bytecode_program_default_options(),
                &output
            );
            if (status == MMAY_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
                MMAY_EXPECT(!output.initialized);
            }
            mmay_bytecode_program_dispose(&output);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
        mmay_owned_bytes_dispose(&program_bytes);
        mmay_bytecode_program_dispose(&program);
    }

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeProgram program;
        MarimayBytecodeProgramVerifyStatus status;
        size_t live_allocations = 0;
        size_t live_bytes = 0;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_bytecode_build_direct_program(
            &program,
            mmay_fail_allocator(&state)
        ));
        live_allocations = state.live_allocations;
        live_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = mmay_bytecode_program_verify(&program);
        if (status == MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(
                status == MMAY_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED
            );
        }
        MMAY_EXPECT(state.live_allocations == live_allocations);
        MMAY_EXPECT(state.live_bytes == live_bytes);
        mmay_bytecode_program_dispose(&program);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}
