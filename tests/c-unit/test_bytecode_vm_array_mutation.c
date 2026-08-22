#include "test_bytecode_vm_support.h"

static bool mmay_test_vm_build_array_mutation(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    bool invalidate_generation
) {
    static const MarimayBytecodeType types[] = {
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_U64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_I64
    };
    MarimayBytecodeArrayArgument argument;
    MarimayBytecodeArrayConstructionId construction =
        MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint32_t block = UINT32_MAX;
    uint64_t span = 4200;
    memset(function, 0, sizeof(*function));
    argument.register_id = 0;
    argument.type = MMAY_BYTECODE_TYPE_F64;
    if (!mmay_bytecode_function_init(
            function, allocator, MMAY_BYTECODE_TYPE_F64, 1, NULL, 0,
            mmay_bytecode_function_default_options()
        ) || !mmay_test_vm_add_registers(
            function, types, sizeof(types) / sizeof(types[0])
        ) || !mmay_bytecode_add_block(function, &block)
        || !mmay_bytecode_add_array_construction(
            function, MMAY_BYTECODE_TYPE_ARRAY_F64, &argument, 1,
            &construction
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff0000000000000), 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64, 2, 1, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_RESERVE, 1, 2, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_GENERATION, 3, 1, 0, &span
        )) {
        goto failure;
    }
    if (invalidate_generation) {
        if (!mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_CLEAR, 1, 0, 0, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_REQUIRE_GENERATION,
                4, 1, 3, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_RETURN, 0, 0, 0, &span
            )) {
            goto failure;
        }
    } else if (!mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_POP, 5, 1, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_GENERATION, 3, 1, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_REQUIRE_GENERATION,
            4, 1, 3, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_I64, 6, 0, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_TRUNCATE, 4, 6, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_CLEAR, 4, 0, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_RESERVE, 4, 2, 0, &span
        ) || !mmay_test_vm_append(
            function, block, MMAY_OPCODE_RETURN, 5, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

static void mmay_test_vm_array_mutation_malformed(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeInstruction *instruction;
    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_test_vm_build_array_mutation(
        &function, mmay_system_allocator(), false
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    instruction = mmay_test_vm_at(&function, 0, 5);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 2;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        instruction->operands[0].unsigned_value = 5;
    }
    instruction = mmay_test_vm_at(&function, 0, 9);
    MMAY_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[1].unsigned_value = 0;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
        instruction->operands[1].unsigned_value = 6;
    }
    mmay_bytecode_function_dispose(&function);
}

static void mmay_test_vm_array_pop_empty_is_invalid_bytecode(void) {
    static const MarimayBytecodeType types[] = {
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_F64
    };
    MarimayBytecodeFunction function;
    MarimayBytecodeArrayConstructionId construction =
        MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions options;
    MarimayBytecodeValue result;
    uint32_t block = UINT32_MAX;
    uint64_t span = 4300;
    memset(&function, 0, sizeof(function));
    memset(&execution, 0, sizeof(execution));
    MMAY_EXPECT(mmay_bytecode_function_init(
        &function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_F64, 1, NULL, 0,
        mmay_bytecode_function_default_options()
    ) && mmay_test_vm_add_registers(
        &function, types, sizeof(types) / sizeof(types[0])
    ) && mmay_bytecode_add_block(&function, &block)
        && mmay_bytecode_add_array_construction(
            &function, MMAY_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &construction
        ) && mmay_test_vm_append(
            &function, block, MMAY_OPCODE_ARRAY_NEW, 0, construction, 0, &span
        ) && mmay_test_vm_append(
            &function, block, MMAY_OPCODE_ARRAY_POP, 1, 0, 0, &span
        ) && mmay_test_vm_append(
            &function, block, MMAY_OPCODE_RETURN, 1, 0, 0, &span
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    options = mmay_bytecode_run_default_options();
    options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(
        &function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_INVALID_BYTECODE);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_function_dispose(&function);
}

static void mmay_test_vm_array_mutation_panic_allocations(
    const MarimayBytecodeFunction *function
) {
    size_t failure_index = 0;
    bool reached_panic = false;
    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution execution;
        MarimayBytecodePanic panic;
        MarimayBytecodeRunOptions options;
        MarimayBytecodeValue result;
        MarimayBytecodeRunStatus status;
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        MMAY_EXPECT(mmay_bytecode_execution_init(
            &execution, mmay_system_allocator(),
            mmay_bytecode_execution_default_options()
        ));
        MMAY_EXPECT(mmay_bytecode_panic_init(
            &panic, mmay_fail_allocator(&state)
        ));
        options = mmay_bytecode_run_default_options();
        options.execution = &execution;
        options.panic = &panic;
        status = mmay_bytecode_run(
            function, NULL, 0, mmay_fail_allocator(&state), options, &result
        );
        MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            || status == MMAY_BYTECODE_RUN_PANIC);
        reached_panic = status == MMAY_BYTECODE_RUN_PANIC;
        if (reached_panic) {
            MMAY_EXPECT(panic.code != NULL && strcmp(
                panic.code, "G4005"
            ) == 0);
        }
        mmay_bytecode_panic_dispose(&panic);
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_panic) break;
    }
    MMAY_EXPECT(reached_panic);
}

void mmay_test_vm_array_mutation(void) {
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodePanic panic;
    MarimayBytecodeValue result;
    MarimayBytecodeRunOptions options;
    MarimayOwnedBytes encoded;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&encoded, 0, sizeof(encoded));
    mmay_test_vm_array_mutation_malformed();
    mmay_test_vm_array_pop_empty_is_invalid_bytecode();
    MMAY_EXPECT(mmay_test_vm_build_array_mutation(
        &function, mmay_system_allocator(), false
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &function, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_verify(&decoded) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    options = mmay_bytecode_run_default_options();
    options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x3ff0000000000000));
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);
    MMAY_EXPECT(mmay_test_vm_build_array_mutation(
        &function, mmay_system_allocator(), true
    ));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
    options.panic = &panic;
    MMAY_EXPECT(mmay_bytecode_run(
        &function, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4005") == 0);
    MMAY_EXPECT(panic.message.length
        == strlen("array iterator invalidated by mutation"));
    MMAY_EXPECT(memcmp(
        panic.message.data, "array iterator invalidated by mutation",
        panic.message.length
    ) == 0);
    mmay_test_vm_array_mutation_panic_allocations(&function);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_function_dispose(&function);
    mmay_bytecode_execution_dispose(&execution);
}
