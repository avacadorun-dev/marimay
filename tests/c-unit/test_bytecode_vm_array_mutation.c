#include "test_bytecode_vm_support.h"

static bool ruda_test_vm_build_array_mutation(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    bool invalidate_generation
) {
    static const RudaBytecodeType types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_U64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_I64
    };
    RudaBytecodeArrayArgument argument;
    RudaBytecodeArrayConstructionId construction =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint32_t block = UINT32_MAX;
    uint64_t span = 4200;
    memset(function, 0, sizeof(*function));
    argument.register_id = 0;
    argument.type = RUDA_BYTECODE_TYPE_F64;
    if (!ruda_bytecode_function_init(
            function, allocator, RUDA_BYTECODE_TYPE_F64, 1, NULL, 0,
            ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(
            function, types, sizeof(types) / sizeof(types[0])
        ) || !ruda_bytecode_add_block(function, &block)
        || !ruda_bytecode_add_array_construction(
            function, RUDA_BYTECODE_TYPE_ARRAY_F64, &argument, 1,
            &construction
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff0000000000000), 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64, 2, 1, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_RESERVE, 1, 2, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_GENERATION, 3, 1, 0, &span
        )) {
        goto failure;
    }
    if (invalidate_generation) {
        if (!ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_CLEAR, 1, 0, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_REQUIRE_GENERATION,
                4, 1, 3, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_RETURN, 0, 0, 0, &span
            )) {
            goto failure;
        }
    } else if (!ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_POP, 5, 1, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_GENERATION, 3, 1, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_REQUIRE_GENERATION,
            4, 1, 3, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_I64, 6, 0, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_TRUNCATE, 4, 6, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_CLEAR, 4, 0, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_RESERVE, 4, 2, 0, &span
        ) || !ruda_test_vm_append(
            function, block, RUDA_OPCODE_RETURN, 5, 0, 0, &span
        )) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

static void ruda_test_vm_array_mutation_malformed(void) {
    RudaBytecodeFunction function;
    RudaBytecodeInstruction *instruction;
    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_test_vm_build_array_mutation(
        &function, ruda_system_allocator(), false
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    instruction = ruda_test_vm_at(&function, 0, 5);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[0].unsigned_value = 2;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        instruction->operands[0].unsigned_value = 5;
    }
    instruction = ruda_test_vm_at(&function, 0, 9);
    RUDA_EXPECT(instruction != NULL);
    if (instruction != NULL) {
        instruction->operands[1].unsigned_value = 0;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
        instruction->operands[1].unsigned_value = 6;
    }
    ruda_bytecode_function_dispose(&function);
}

static void ruda_test_vm_array_pop_empty_is_invalid_bytecode(void) {
    static const RudaBytecodeType types[] = {
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_F64
    };
    RudaBytecodeFunction function;
    RudaBytecodeArrayConstructionId construction =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    RudaBytecodeExecution execution;
    RudaBytecodeRunOptions options;
    RudaBytecodeValue result;
    uint32_t block = UINT32_MAX;
    uint64_t span = 4300;
    memset(&function, 0, sizeof(function));
    memset(&execution, 0, sizeof(execution));
    RUDA_EXPECT(ruda_bytecode_function_init(
        &function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_F64, 1, NULL, 0,
        ruda_bytecode_function_default_options()
    ) && ruda_test_vm_add_registers(
        &function, types, sizeof(types) / sizeof(types[0])
    ) && ruda_bytecode_add_block(&function, &block)
        && ruda_bytecode_add_array_construction(
            &function, RUDA_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &construction
        ) && ruda_test_vm_append(
            &function, block, RUDA_OPCODE_ARRAY_NEW, 0, construction, 0, &span
        ) && ruda_test_vm_append(
            &function, block, RUDA_OPCODE_ARRAY_POP, 1, 0, 0, &span
        ) && ruda_test_vm_append(
            &function, block, RUDA_OPCODE_RETURN, 1, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(
        &function, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_INVALID_BYTECODE);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_function_dispose(&function);
}

static void ruda_test_vm_array_mutation_panic_allocations(
    const RudaBytecodeFunction *function
) {
    size_t failure_index = 0;
    bool reached_panic = false;
    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeExecution execution;
        RudaBytecodePanic panic;
        RudaBytecodeRunOptions options;
        RudaBytecodeValue result;
        RudaBytecodeRunStatus status;
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &execution, ruda_system_allocator(),
            ruda_bytecode_execution_default_options()
        ));
        RUDA_EXPECT(ruda_bytecode_panic_init(
            &panic, ruda_fail_allocator(&state)
        ));
        options = ruda_bytecode_run_default_options();
        options.execution = &execution;
        options.panic = &panic;
        status = ruda_bytecode_run(
            function, NULL, 0, ruda_fail_allocator(&state), options, &result
        );
        RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED
            || status == RUDA_BYTECODE_RUN_PANIC);
        reached_panic = status == RUDA_BYTECODE_RUN_PANIC;
        if (reached_panic) {
            RUDA_EXPECT(panic.code != NULL && strcmp(
                panic.code, "G4005"
            ) == 0);
        }
        ruda_bytecode_panic_dispose(&panic);
        ruda_bytecode_execution_dispose(&execution);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_panic) break;
    }
    RUDA_EXPECT(reached_panic);
}

void ruda_test_vm_array_mutation(void) {
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeValue result;
    RudaBytecodeRunOptions options;
    RudaOwnedBytes encoded;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&encoded, 0, sizeof(encoded));
    ruda_test_vm_array_mutation_malformed();
    ruda_test_vm_array_pop_empty_is_invalid_bytecode();
    RUDA_EXPECT(ruda_test_vm_build_array_mutation(
        &function, ruda_system_allocator(), false
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &function, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_verify(&decoded) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x3ff0000000000000));
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);
    RUDA_EXPECT(ruda_test_vm_build_array_mutation(
        &function, ruda_system_allocator(), true
    ));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    options.panic = &panic;
    RUDA_EXPECT(ruda_bytecode_run(
        &function, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4005") == 0);
    RUDA_EXPECT(panic.message.length
        == strlen("array iterator invalidated by mutation"));
    RUDA_EXPECT(memcmp(
        panic.message.data, "array iterator invalidated by mutation",
        panic.message.length
    ) == 0);
    ruda_test_vm_array_mutation_panic_allocations(&function);
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_function_dispose(&function);
    ruda_bytecode_execution_dispose(&execution);
}
