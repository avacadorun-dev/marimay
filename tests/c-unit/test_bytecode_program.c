#include "test_bytecode_support.h"

void ruda_test_bytecode_calls_and_program(void) {
    const RudaSymbolId callee_symbol = {{0x10}};
    const RudaSymbolId caller_symbol = {{0x20}};
    RudaBytecodeFunction caller;
    RudaBytecodeFunction callee;
    RudaBytecodeFunction decoded_function;
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded_program;
    RudaOwnedBytes function_bytes;
    RudaOwnedBytes program_bytes;
    RudaOwnedBytes program_again;
    RudaTestBuffer buffer;
    RudaAllocator allocator = ruda_system_allocator();
    size_t index = 0;

    memset(&caller, 0, sizeof(caller));
    memset(&callee, 0, sizeof(callee));
    memset(&decoded_function, 0, sizeof(decoded_function));
    memset(&program, 0, sizeof(program));
    memset(&decoded_program, 0, sizeof(decoded_program));
    memset(&function_bytes, 0, sizeof(function_bytes));
    memset(&program_bytes, 0, sizeof(program_bytes));
    memset(&program_again, 0, sizeof(program_again));
    RUDA_EXPECT(ruda_test_bytecode_build_direct_caller(
        &caller, ruda_system_allocator(), callee_symbol));
    RUDA_EXPECT(ruda_bytecode_verify(&caller) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(caller.calls.length == 1 && caller.call_arguments.length == 2);
    RUDA_EXPECT(ruda_bytecode_encode_function(&caller, ruda_system_allocator(),
        &function_bytes) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(function_bytes.length > 10 && function_bytes.data[9] == 1);
    for (index = 0; index < function_bytes.length; index += 1) {
        RUDA_EXPECT(ruda_bytecode_decode_function(function_bytes.data, index,
            ruda_system_allocator(), ruda_bytecode_function_default_options(),
            &decoded_function) == RUDA_BYTECODE_CODEC_TRUNCATED);
        RUDA_EXPECT(!decoded_function.initialized);
    }
    RUDA_EXPECT(ruda_bytecode_decode_function(function_bytes.data,
        function_bytes.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded_function)
        == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(decoded_function.calls.length == 1
        && decoded_function.call_arguments.length == 2);
    ruda_bytecode_function_dispose(&decoded_function);
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_bytecode_disassemble(&caller, ruda_test_buffer_writer(&buffer))
        == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strstr(buffer.bytes, "call_direct r2, c0 @s0:30..31\n") != NULL);

    {
        RudaBytecodeInstruction *call_instruction =
            ruda_test_bytecode_at(&caller, 0, 0);
        RudaBytecodeCall *call = ruda_vector_at(&caller.calls, 0);
        RudaBytecodeCallArgument *argument = ruda_vector_at(
            &caller.call_arguments, 0);
        RUDA_EXPECT(call_instruction != NULL && call != NULL && argument != NULL);
        if (call_instruction != NULL) {
            call_instruction->operands[1].unsigned_value = 1;
            RUDA_EXPECT(ruda_bytecode_verify(&caller)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            call_instruction->operands[1].unsigned_value = 0;
        }
        if (argument != NULL) {
            argument->register_id = 9;
            RUDA_EXPECT(ruda_bytecode_verify(&caller)
                == RUDA_BYTECODE_VERIFY_INVALID_REGISTER);
            argument->register_id = 1;
            argument->type = RUDA_BYTECODE_TYPE_BOOL;
            RUDA_EXPECT(ruda_bytecode_verify(&caller)
                == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
            argument->type = RUDA_BYTECODE_TYPE_I64;
        }
        if (call != NULL) {
            call->argument_start = 1;
            RUDA_EXPECT(ruda_bytecode_verify(&caller)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            call->argument_start = 0;
        }
    }

    RUDA_EXPECT(ruda_test_bytecode_build_direct_callee(
        &callee, ruda_system_allocator()));
    RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(), 1,
        ruda_bytecode_program_default_options()));
    {
        uint32_t function_id = UINT32_MAX;
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, callee_symbol,
            &callee, &function_id));
        RUDA_EXPECT(function_id == 0);
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, caller_symbol,
            &caller, &function_id));
        RUDA_EXPECT(function_id == 1);
    }
    RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, caller_symbol));
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_verify_work = 29;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_verify_work = 30;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_verify_work =
        ruda_bytecode_program_default_options().max_verify_work;
    RUDA_EXPECT(ruda_bytecode_program_function_by_symbol(&program, callee_symbol)
        == ruda_bytecode_program_function(&program, 0));
    RUDA_EXPECT(ruda_bytecode_encode_program(&program, ruda_system_allocator(),
        &program_bytes) == RUDA_BYTECODE_CODEC_OK);
    for (index = 0; index < program_bytes.length; index += 1) {
        RUDA_EXPECT(ruda_bytecode_decode_program(program_bytes.data, index,
            ruda_system_allocator(), ruda_bytecode_program_default_options(),
            &decoded_program) == RUDA_BYTECODE_CODEC_TRUNCATED);
        RUDA_EXPECT(!decoded_program.initialized);
    }
    RUDA_EXPECT(ruda_bytecode_decode_program(program_bytes.data, program_bytes.length,
        ruda_system_allocator(), ruda_bytecode_program_default_options(),
        &decoded_program) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_encode_program(&decoded_program,
        ruda_system_allocator(), &program_again) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(program_bytes.length == program_again.length
        && memcmp(program_bytes.data, program_again.data, program_bytes.length) == 0);
    {
        RudaBytecodeProgramOptions limited =
            ruda_bytecode_program_default_options();
        limited.max_total_registers = 5;
        ruda_bytecode_program_dispose(&decoded_program);
        RUDA_EXPECT(ruda_bytecode_decode_program(
            program_bytes.data,
            program_bytes.length,
            ruda_system_allocator(),
            limited,
            &decoded_program
        ) == RUDA_BYTECODE_CODEC_LIMIT_REACHED);
        RUDA_EXPECT(!decoded_program.initialized);
        RUDA_EXPECT(ruda_bytecode_decode_program(
            program_bytes.data,
            program_bytes.length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &decoded_program
        ) == RUDA_BYTECODE_CODEC_OK);
    }
    ruda_bytecode_program_dispose(&decoded_program);

    {
        RudaBytecodeProgramFunction *callee_function = ruda_vector_at(&program.functions, 0);
        RudaBytecodeProgramFunction *caller_function = ruda_vector_at(&program.functions, 1);
        RudaBytecodeCall *call = caller_function != NULL
            ? ruda_vector_at(&caller_function->body.calls, 0) : NULL;
        RudaBytecodeInstruction *instruction = caller_function != NULL
            ? ruda_test_bytecode_at(&caller_function->body, 0, 0) : NULL;
        uint32_t entry = program.entry_function;
        RUDA_EXPECT(callee_function != NULL && caller_function != NULL
            && call != NULL && instruction != NULL);
        program.entry_function = 2;
        RUDA_EXPECT(ruda_bytecode_program_verify(&program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT);
        program.entry_function = entry;
        if (call != NULL) {
            call->callee.bytes[0] = 0x30;
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_UNKNOWN_CALLEE);
            call->callee = callee_symbol;
            RudaBytecodeType *parameter = ruda_vector_at(
                &callee_function->body.register_types, 0);
            RUDA_EXPECT(parameter != NULL);
            if (parameter != NULL) {
                *parameter = RUDA_BYTECODE_TYPE_BOOL;
            }
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
            if (parameter != NULL) {
                *parameter = RUDA_BYTECODE_TYPE_I64;
            }
        }
        if (instruction != NULL) {
            instruction->operands[0].unsigned_value = 99;
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
            instruction->operands[0].unsigned_value = 2;
        }
        if (callee_function != NULL && caller_function != NULL) {
            RudaSymbolId saved = callee_function->symbol;
            callee_function->symbol = caller_function->symbol;
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_DUPLICATE_SYMBOL);
            callee_function->symbol = saved;
            callee_function->symbol.bytes[0] = 0x30;
            RUDA_EXPECT(ruda_bytecode_program_verify(&program)
                == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_ARGUMENT);
            callee_function->symbol = saved;
        }
    }
    {
        unsigned char *mutated = ruda_allocate_array(&allocator,
            program_bytes.length + 1, sizeof(unsigned char));
        RUDA_EXPECT(mutated != NULL);
        if (mutated != NULL) {
            size_t payload_offset = 8;
            uint64_t ignored = 0;
            uint64_t payload_length = 0;
            RUDA_EXPECT(ruda_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset, &ignored
            ));
            RUDA_EXPECT(ruda_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset, &ignored
            ));
            RUDA_EXPECT(ruda_test_bytecode_read_u64(
                program_bytes.data, program_bytes.length, &payload_offset,
                &payload_length
            ));
            RUDA_EXPECT(payload_length != 0
                && payload_length <= program_bytes.length - payload_offset - 32);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[0] = 'X';
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            if (payload_length != 0
                && payload_length <= program_bytes.length - payload_offset - 32) {
                /* Keep the envelope digest intact: only payload integrity changes. */
                mutated[payload_offset] ^= 1;
                RUDA_EXPECT(ruda_bytecode_decode_program(mutated,
                    program_bytes.length, ruda_system_allocator(),
                    ruda_bytecode_program_default_options(), &decoded_program)
                    == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            }
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[8] = 2;
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[9] = 1;
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_UNSUPPORTED_VERSION);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[program_bytes.length - 1] ^= 1;
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, program_bytes.data, program_bytes.length);
            mutated[program_bytes.length] = 0;
            RUDA_EXPECT(ruda_bytecode_decode_program(mutated, program_bytes.length + 1,
                ruda_system_allocator(), ruda_bytecode_program_default_options(),
                &decoded_program) == RUDA_BYTECODE_CODEC_TRAILING_DATA);
            ruda_deallocate(&allocator, mutated,
                program_bytes.length + 1);
        }
    }
    ruda_owned_bytes_dispose(&program_again);
    ruda_bytecode_program_dispose(&decoded_program);
    ruda_owned_bytes_dispose(&program_bytes);
    ruda_owned_bytes_dispose(&function_bytes);
    ruda_bytecode_program_dispose(&program);
}

void ruda_test_bytecode_string_pool(void) {
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
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaOwnedBytes encoded;
    RudaOwnedBytes encoded_again;
    RudaTestBuffer buffer;
    size_t index = 0;

    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&encoded_again, 0, sizeof(encoded_again));
    RUDA_EXPECT(ruda_test_bytecode_build_string_constant(&function,
        ruda_system_allocator(), embedded, sizeof(embedded)));
    RUDA_EXPECT(ruda_bytecode_add_string_constant(&function, empty, 0, &(RudaBytecodeStringId){0}));
    RUDA_EXPECT(ruda_bytecode_add_string_constant(&function, unicode, sizeof(unicode),
        &(RudaBytecodeStringId){0}));
    RUDA_EXPECT(!ruda_bytecode_add_string_constant(&function, invalid_utf8,
        sizeof(invalid_utf8), &(RudaBytecodeStringId){0}));
    RUDA_EXPECT(!ruda_bytecode_add_string_constant(&function, NULL, 1,
        &(RudaBytecodeStringId){0}));
    RUDA_EXPECT(function.string_constants.length == 3
        && function.string_bytes == sizeof(embedded) + sizeof(unicode));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_test_buffer_init(&buffer);
    RUDA_EXPECT(ruda_bytecode_disassemble(&function,
        ruda_test_buffer_writer(&buffer)) == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strcmp(buffer.bytes,
        "bytecode function v1 result=string sources=1 parameters=0 registers=1 blocks=1\n"
        "registers r0:string\n"
        "block b0:\n"
        "  const_string r0, s0 @s0:70..71\n"
        "  return r0 @s0:71..72\n") == 0);
    RUDA_EXPECT(ruda_bytecode_encode_function(&function, ruda_system_allocator(),
        &encoded) == RUDA_BYTECODE_CODEC_OK);
    /* Feature bit 1 and the raw UTF-8/NUL pool bytes are canonical wire data. */
    RUDA_EXPECT(encoded.length == sizeof(expected_feature_strings));
    RUDA_EXPECT(memcmp(encoded.data, expected_feature_strings,
        sizeof(expected_feature_strings)) == 0);
    for (index = 0; index < encoded.length; index += 1) {
        RUDA_EXPECT(ruda_bytecode_decode_function(encoded.data, index,
            ruda_system_allocator(), ruda_bytecode_function_default_options(),
            &decoded) == RUDA_BYTECODE_CODEC_TRUNCATED);
        RUDA_EXPECT(!decoded.initialized);
    }
    RUDA_EXPECT(ruda_bytecode_decode_function(encoded.data, encoded.length,
        ruda_system_allocator(), ruda_bytecode_function_default_options(),
        &decoded) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(decoded.string_constants.length == 3
        && decoded.string_bytes == sizeof(embedded) + sizeof(unicode));
    RUDA_EXPECT(ruda_bytecode_encode_function(&decoded, ruda_system_allocator(),
        &encoded_again) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded_again.length == sizeof(expected_feature_strings)
        && memcmp(encoded_again.data, expected_feature_strings,
            sizeof(expected_feature_strings)) == 0);
    {
        const RudaBytecodeStringConstant *first = ruda_bytecode_string_constant(
            &decoded, 0);
        const RudaBytecodeStringConstant *second = ruda_bytecode_string_constant(
            &decoded, 1);
        const RudaBytecodeStringConstant *third = ruda_bytecode_string_constant(
            &decoded, 2);
        RUDA_EXPECT(first != NULL && second != NULL && third != NULL);
        if (first != NULL) {
            RUDA_EXPECT(first->value.length == sizeof(embedded)
                && memcmp(first->value.data, embedded, sizeof(embedded)) == 0);
        }
        if (second != NULL) {
            RUDA_EXPECT(second->value.length == 0);
        }
        if (third != NULL) {
            RUDA_EXPECT(third->value.length == sizeof(unicode)
                && memcmp(third->value.data, unicode, sizeof(unicode)) == 0);
        }
    }
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded_again);

    {
        unsigned char *mutated = ruda_allocate_array(
            &function.allocator, encoded.length + 1, sizeof(unsigned char)
        );
        RUDA_EXPECT(mutated != NULL);
        if (mutated != NULL) {
            /* The format version is the first one-byte varint after the magic. */
            memcpy(mutated, encoded.data, 8);
            mutated[8] = 0x81;
            mutated[9] = 0;
            memcpy(mutated + 10, encoded.data + 9, encoded.length - 9);
            RUDA_EXPECT(ruda_bytecode_decode_function(mutated,
                encoded.length + 1, ruda_system_allocator(),
                ruda_bytecode_function_default_options(), &decoded)
                == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            memcpy(mutated, encoded.data, encoded.length);
            RUDA_EXPECT(encoded.length >= sizeof(unicode));
            if (encoded.length >= sizeof(unicode)) {
                /* Canonical strings are raw UTF-8 at the end of this fixture. */
                mutated[encoded.length - sizeof(unicode)] = 0xff;
                RUDA_EXPECT(ruda_bytecode_decode_function(mutated, encoded.length,
                    ruda_system_allocator(), ruda_bytecode_function_default_options(),
                    &decoded) == RUDA_BYTECODE_CODEC_INVALID_ENCODING);
            }
            ruda_deallocate(&function.allocator, mutated, encoded.length + 1);
        }
    }
    ruda_owned_bytes_dispose(&encoded);

    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeFunction allocated;
            ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
            if (ruda_test_bytecode_build_string_constant(&allocated,
                    ruda_fail_allocator(&state), embedded, sizeof(embedded))) {
                RudaBytecodeStringId ignored = RUDA_BYTECODE_NO_STRING;
                if (ruda_bytecode_add_string_constant(&allocated, empty,
                        0, &ignored)) {
                    reached_success = true;
                }
                ruda_bytecode_function_dispose(&allocated);
            }
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
    }
    {
        size_t failure_index = 0;
        bool reached_success = false;
        RUDA_EXPECT(ruda_bytecode_encode_function(&function,
            ruda_system_allocator(), &encoded) == RUDA_BYTECODE_CODEC_OK);
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaOwnedBytes failed_encoded;
            RudaBytecodeCodecStatus status;
            memset(&failed_encoded, 0, sizeof(failed_encoded));
            ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
            status = ruda_bytecode_encode_function(&function,
                ruda_fail_allocator(&state), &failed_encoded);
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED
                || status == RUDA_BYTECODE_CODEC_OK);
            if (status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            }
            ruda_owned_bytes_dispose(&failed_encoded);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        ruda_owned_bytes_dispose(&encoded);
    }

    {
        RudaBytecodeInstruction *instruction = ruda_test_bytecode_at(
            &function, 0, 0);
        RUDA_EXPECT(instruction != NULL);
        if (instruction != NULL) {
            instruction->operands[1].unsigned_value = 3;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            instruction->operands[1].unsigned_value = 0;
        }
    }
    {
        RudaBytecodeFunctionOptions limited =
            ruda_bytecode_function_default_options();
        RudaBytecodeStringId id = RUDA_BYTECODE_NO_STRING;
        limited.max_string_constants = 1;
        RUDA_EXPECT(ruda_bytecode_function_init(&decoded, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_VOID, 1, NULL, 0, limited));
        RUDA_EXPECT(ruda_bytecode_add_string_constant(&decoded, empty, 0, &id));
        RUDA_EXPECT(!ruda_bytecode_add_string_constant(&decoded, unicode,
            sizeof(unicode), &id));
        ruda_bytecode_function_dispose(&decoded);
        limited = ruda_bytecode_function_default_options();
        limited.max_string_bytes = 2;
        RUDA_EXPECT(ruda_bytecode_function_init(&decoded, ruda_system_allocator(),
            RUDA_BYTECODE_TYPE_VOID, 1, NULL, 0, limited));
        RUDA_EXPECT(!ruda_bytecode_add_string_constant(&decoded, unicode,
            sizeof(unicode), &id));
        ruda_bytecode_function_dispose(&decoded);
    }
    {
        const RudaSymbolId symbol = {{0x10}};
        RudaBytecodeProgram program;
        RudaBytecodeProgramOptions options = ruda_bytecode_program_default_options();
        uint32_t id = UINT32_MAX;
        memset(&program, 0, sizeof(program));
        options.max_total_string_bytes = 2;
        RUDA_EXPECT(ruda_bytecode_program_init(&program, ruda_system_allocator(),
            1, options));
        RUDA_EXPECT(ruda_test_bytecode_build_string_constant(&decoded,
            ruda_system_allocator(), embedded, sizeof(embedded)));
        RUDA_EXPECT(ruda_bytecode_program_take_function(&program, symbol,
            &decoded, &id) && id == 0);
        RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, symbol));
        RUDA_EXPECT(ruda_bytecode_program_verify(&program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
        ruda_bytecode_program_dispose(&program);
    }
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_bytecode_string_verifier_rejections(void) {
    const RudaSymbolId callee = {{0x10}};
    RudaBytecodeFunction function;
    RudaBytecodeInstruction *instruction;
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    uint32_t block = UINT32_MAX;
    size_t span = 100;

    RUDA_EXPECT(ruda_test_bytecode_build_string_equality(&function,
        ruda_system_allocator(), true));
    instruction = ruda_test_bytecode_at(&function, 0,
        ((RudaBytecodeBlock *)ruda_vector_at(&function.blocks, 0))
            ->instructions.length - 2);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[1].unsigned_value = 3;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_string_equality(&function,
        ruda_system_allocator(), false));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
    ruda_bytecode_function_dispose(&function);

    RUDA_EXPECT(ruda_test_bytecode_build_panic_string(&function,
        ruda_system_allocator(), RUDA_BYTECODE_TYPE_BOOL, true, false));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    ruda_bytecode_function_dispose(&function);
    RUDA_EXPECT(ruda_test_bytecode_build_panic_string(&function,
        ruda_system_allocator(), RUDA_BYTECODE_TYPE_STRING, false, false));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_UNINITIALIZED_REGISTER);
    ruda_bytecode_function_dispose(&function);
    RUDA_EXPECT(ruda_test_bytecode_build_panic_string(&function,
        ruda_system_allocator(), RUDA_BYTECODE_TYPE_STRING, true, true));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_INVALID_CONTROL_FLOW);
    ruda_bytecode_function_dispose(&function);

    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_bytecode_function_init(&function, ruda_system_allocator(),
        RUDA_BYTECODE_TYPE_NEVER, 1, NULL, 0,
        ruda_bytecode_function_default_options()));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &block));
    RUDA_EXPECT(ruda_bytecode_add_call(&function, callee, RUDA_BYTECODE_TYPE_I64,
        NULL, 0, &call));
    RUDA_EXPECT(ruda_test_bytecode_append(&function, block,
        RUDA_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span));
    RUDA_EXPECT(ruda_bytecode_verify(&function)
        == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_bytecode_allocation_failures(void) {
    RudaBytecodeFunction function;
    RudaOwnedBytes bytes;
    size_t failure_index = 0;
    bool reached_success = false;

    for (failure_index = 0; failure_index < 256 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        RUDA_EXPECT(ruda_test_bytecode_build_full(
            &function,
            ruda_fail_allocator(&state)
        ) == (failure_index >= state.attempts));
        if (function.initialized) {
            reached_success = true;
            ruda_bytecode_function_dispose(&function);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    RUDA_EXPECT(ruda_test_bytecode_build_full(&function, ruda_system_allocator()));
    memset(&bytes, 0, sizeof(bytes));
    RUDA_EXPECT(ruda_bytecode_encode_function(&function, ruda_system_allocator(),
        &bytes) == RUDA_BYTECODE_CODEC_OK);
    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes output;
        RudaBytecodeCodecStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_bytecode_encode_function(
            &function,
            ruda_fail_allocator(&state),
            &output
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(output.data == NULL);
            RUDA_EXPECT(output.length == 0);
        }
        ruda_owned_bytes_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeFunction output;
        RudaBytecodeCodecStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        memset(&output, 0, sizeof(output));
        status = ruda_bytecode_decode_function(
            bytes.data,
            bytes.length,
            ruda_fail_allocator(&state),
            ruda_bytecode_function_default_options(),
            &output
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(!output.initialized);
        }
        ruda_bytecode_function_dispose(&output);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
    ruda_owned_bytes_dispose(&bytes);
    ruda_bytecode_function_dispose(&function);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeVerifyStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_bytecode_build_full(
            &function,
            ruda_fail_allocator(&state)
        ));
        state.failure_index = state.attempts + failure_index;
        status = ruda_bytecode_verify(&function);
        if (status == RUDA_BYTECODE_VERIFY_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_VERIFY_ALLOCATION_FAILED);
        }
        ruda_bytecode_function_dispose(&function);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeProgram program;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        if (ruda_test_bytecode_build_direct_program(
                &program,
                ruda_fail_allocator(&state)
            )) {
            reached_success = true;
        }
        ruda_bytecode_program_dispose(&program);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);

    {
        RudaBytecodeProgram program;
        RudaOwnedBytes program_bytes;
        memset(&program_bytes, 0, sizeof(program_bytes));
        RUDA_EXPECT(ruda_test_bytecode_build_direct_program(
            &program,
            ruda_system_allocator()
        ));
        RUDA_EXPECT(ruda_bytecode_encode_program(
            &program,
            ruda_system_allocator(),
            &program_bytes
        ) == RUDA_BYTECODE_CODEC_OK);
        for (failure_index = 0, reached_success = false;
            failure_index < 256 && !reached_success; failure_index += 1) {
            RudaFailAllocator state;
            RudaOwnedBytes output;
            RudaBytecodeCodecStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            memset(&output, 0, sizeof(output));
            status = ruda_bytecode_encode_program(
                &program,
                ruda_fail_allocator(&state),
                &output
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
                RUDA_EXPECT(output.data == NULL && output.length == 0);
            }
            ruda_owned_bytes_dispose(&output);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        for (failure_index = 0, reached_success = false;
            failure_index < 256 && !reached_success; failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeProgram output;
            RudaBytecodeCodecStatus status;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            memset(&output, 0, sizeof(output));
            status = ruda_bytecode_decode_program(
                program_bytes.data,
                program_bytes.length,
                ruda_fail_allocator(&state),
                ruda_bytecode_program_default_options(),
                &output
            );
            if (status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
                RUDA_EXPECT(!output.initialized);
            }
            ruda_bytecode_program_dispose(&output);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        ruda_owned_bytes_dispose(&program_bytes);
        ruda_bytecode_program_dispose(&program);
    }

    for (failure_index = 0, reached_success = false;
        failure_index < 256 && !reached_success; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeProgram program;
        RudaBytecodeProgramVerifyStatus status;
        size_t live_allocations = 0;
        size_t live_bytes = 0;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), SIZE_MAX);
        RUDA_EXPECT(ruda_test_bytecode_build_direct_program(
            &program,
            ruda_fail_allocator(&state)
        ));
        live_allocations = state.live_allocations;
        live_bytes = state.live_bytes;
        state.failure_index = state.attempts + failure_index;
        status = ruda_bytecode_program_verify(&program);
        if (status == RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(
                status == RUDA_BYTECODE_PROGRAM_VERIFY_ALLOCATION_FAILED
            );
        }
        RUDA_EXPECT(state.live_allocations == live_allocations);
        RUDA_EXPECT(state.live_bytes == live_bytes);
        ruda_bytecode_program_dispose(&program);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
    }
    RUDA_EXPECT(reached_success);
}
