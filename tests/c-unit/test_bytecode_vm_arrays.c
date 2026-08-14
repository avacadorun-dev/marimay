#include "test_bytecode_vm_support.h"

static bool ruda_test_vm_build_array_function(
    RudaBytecodeFunction *function,
    RudaAllocator allocator,
    int64_t access_index,
    bool nested
) {
    static const RudaBytecodeType flat_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64
    };
    static const RudaBytecodeType nested_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_F64
    };
    RudaBytecodeArrayArgument inner_arguments[2];
    RudaBytecodeArrayArgument outer_argument;
    RudaBytecodeArrayConstructionId inner =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    RudaBytecodeArrayConstructionId outer =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint32_t block = UINT32_MAX;
    uint64_t span = 700;
    const RudaBytecodeType *types = nested ? nested_types : flat_types;
    size_t type_count = nested
        ? sizeof(nested_types) / sizeof(nested_types[0])
        : sizeof(flat_types) / sizeof(flat_types[0]);
    memset(function, 0, sizeof(*function));
    if (!ruda_bytecode_function_init(
            function, allocator, RUDA_BYTECODE_TYPE_F64, 1, NULL, 0,
            ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(function, types, type_count)
        || !ruda_bytecode_add_block(function, &block)) {
        goto failure;
    }
    inner_arguments[0].register_id = 0;
    inner_arguments[0].type = RUDA_BYTECODE_TYPE_F64;
    inner_arguments[1].register_id = nested ? 0 : 1;
    inner_arguments[1].type = RUDA_BYTECODE_TYPE_F64;
    if (!ruda_bytecode_add_array_construction(
            function, RUDA_BYTECODE_TYPE_ARRAY_F64, inner_arguments,
            nested ? 1 : 2, &inner
        )) {
        goto failure;
    }
    if (nested) {
        outer_argument.register_id = 1;
        outer_argument.type = RUDA_BYTECODE_TYPE_ARRAY_F64;
        if (!ruda_bytecode_add_array_construction(
                function, RUDA_BYTECODE_TYPE_ARRAY_ARRAY_F64,
                &outer_argument, 1, &outer
            )) {
            goto failure;
        }
    }
    if (!ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff8000000000000), 0, &span
        )) {
        goto failure;
    }
    if (!nested && !ruda_test_vm_append(
            function, block, RUDA_OPCODE_CONST_F64, 1,
            UINT64_C(0x4004000000000000), 0, &span
        )) {
        goto failure;
    }
    if (!ruda_test_vm_append(
            function, block, RUDA_OPCODE_ARRAY_NEW, nested ? 1 : 2,
            inner, 0, &span
        )) {
        goto failure;
    }
    if (nested) {
        if (!ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_NEW, 2, outer, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_CONST_I64, 3, 0, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_GET, 4, 2, 3, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_GET, 5, 4, 3, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_RETURN, 5, 0, 0, &span
            )) {
            goto failure;
        }
    } else {
        if (!ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_LENGTH, 3, 2, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_CONST_I64, 4,
                (uint64_t)access_index, 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_REQUIRE_INDEX, 7, 2, 4,
                &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_CONST_F64, 5,
                UINT64_C(0x4022000000000000), 0, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_SET, 7, 4, 5, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_ARRAY_GET, 6, 2, 4, &span
            ) || !ruda_test_vm_append(
                function, block, RUDA_OPCODE_RETURN, 6, 0, 0, &span
            )) {
            goto failure;
        }
    }
    return true;

failure:
    ruda_bytecode_function_dispose(function);
    return false;
}

void ruda_test_vm_managed_arrays(void) {
    static const int64_t invalid_indices[] = {
        -1, 2, INT64_MIN, INT64_MAX
    };
    static const char *expected_messages[] = {
        "array index -1 is outside length 2",
        "array index 2 is outside length 2",
        "array index -9223372036854775808 is outside length 2",
        "array index 9223372036854775807 is outside length 2"
    };
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeValue result;
    RudaOwnedBytes encoded;
    RudaTestBuffer disassembly;
    RudaBytecodeRunOptions run_options;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    size_t index = 0;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&encoded, 0, sizeof(encoded));
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_test_vm_build_array_function(
        &function, ruda_system_allocator(), 1, false
    ));
    {
        RudaBytecodeArrayConstruction *construction = ruda_vector_at(
            &function.array_constructions, 0
        );
        RudaBytecodeArrayArgument *argument = ruda_vector_at(
            &function.array_arguments, 0
        );
        RudaBytecodeInstruction *array_new = ruda_test_vm_at(&function, 0, 2);
        RUDA_EXPECT(construction != NULL && argument != NULL
            && array_new != NULL);
        if (construction != NULL) {
            construction->array_type = RUDA_BYTECODE_TYPE_F64;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            construction->array_type = RUDA_BYTECODE_TYPE_ARRAY_F64;
        }
        if (argument != NULL) {
            argument->type = RUDA_BYTECODE_TYPE_BOOL;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_TYPE_MISMATCH);
            argument->type = RUDA_BYTECODE_TYPE_F64;
        }
        if (array_new != NULL) {
            array_new->operands[1].unsigned_value = UINT64_MAX;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            array_new->operands[1].unsigned_value = 0;
        }
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    {
        RudaBytecodeInstruction *set = ruda_test_vm_at(&function, 0, 7);
        RudaBytecodeInstruction *rhs = ruda_test_vm_at(&function, 0, 6);
        RudaBytecodeInstruction saved_rhs;
        uint64_t original_array = set != NULL
            ? set->operands[0].unsigned_value
            : UINT64_MAX;
        uint64_t original_index = set != NULL
            ? set->operands[1].unsigned_value
            : UINT64_MAX;
        RUDA_EXPECT(set != NULL && rhs != NULL);
        memset(&saved_rhs, 0, sizeof(saved_rhs));
        if (rhs != NULL) {
            saved_rhs = *rhs;
        }
        if (set != NULL && rhs != NULL) {
            set->operands[0].unsigned_value = 2;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            RUDA_EXPECT(ruda_bytecode_run(
                &function, NULL, 0, ruda_system_allocator(),
                ruda_bytecode_run_default_options(), &result
            ) == RUDA_BYTECODE_RUN_INVALID_BYTECODE);
            set->operands[0].unsigned_value = original_array;
            set->operands[1].unsigned_value = 3;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            set->operands[1].unsigned_value = original_index;
            *rhs = ruda_test_vm_instruction(
                RUDA_OPCODE_COPY, 7, 2, 0, saved_rhs.span.start
            );
            set->operands[2].unsigned_value = 0;
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            *rhs = ruda_test_vm_instruction(
                RUDA_OPCODE_CONST_I64, 4, 0, 0, saved_rhs.span.start
            );
            RUDA_EXPECT(ruda_bytecode_verify(&function)
                == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
            *rhs = saved_rhs;
            set->operands[2].unsigned_value = 5;
        }
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_test_buffer_init(&disassembly);
    RUDA_EXPECT(ruda_bytecode_disassemble(
        &function, ruda_test_buffer_writer(&disassembly)
    ) == RUDA_BYTECODE_DISASSEMBLE_OK);
    RUDA_EXPECT(strstr(disassembly.bytes, "array_new r2, a0") != NULL);
    RUDA_EXPECT(strstr(disassembly.bytes, "array_set r7, r4, r5") != NULL);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &function, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeFunction attempted;
            RudaBytecodeCodecStatus decode_status;
            memset(&attempted, 0, sizeof(attempted));
            ruda_fail_allocator_init(
                &state, ruda_system_allocator(), failure_index
            );
            decode_status = ruda_bytecode_decode_function(
                encoded.data, encoded.length, ruda_fail_allocator(&state),
                ruda_bytecode_function_default_options(), &attempted
            );
            if (decode_status == RUDA_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(decode_status
                    == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            }
            ruda_bytecode_function_dispose(&attempted);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
    }
    RUDA_EXPECT(ruda_bytecode_decode_function(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_function_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), execution_options
    ));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_bytecode_run(
        &decoded, NULL, 0, ruda_system_allocator(), run_options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x4022000000000000));
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);

    {
        bool reached_success = false;
        size_t failure_index = 0;
        RUDA_EXPECT(ruda_test_vm_build_array_function(
            &function, ruda_system_allocator(), 1, false
        ));
        for (failure_index = 0; failure_index < 32 && !reached_success;
            failure_index += 1) {
            RudaFailAllocator state;
            RudaBytecodeExecution failed_execution;
            RudaBytecodeRunStatus status;
            memset(&failed_execution, 0, sizeof(failed_execution));
            ruda_fail_allocator_init(
                &state, ruda_system_allocator(), SIZE_MAX
            );
            RUDA_EXPECT(ruda_bytecode_execution_init(
                &failed_execution, ruda_fail_allocator(&state),
                ruda_bytecode_execution_default_options()
            ));
            state.failure_index = state.attempts + failure_index;
            run_options = ruda_bytecode_run_default_options();
            run_options.execution = &failed_execution;
            status = ruda_bytecode_run(
                &function, NULL, 0, ruda_fail_allocator(&state),
                run_options, &result
            );
            if (status == RUDA_BYTECODE_RUN_OK) {
                reached_success = true;
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            ruda_bytecode_execution_dispose(&failed_execution);
            RUDA_EXPECT(state.live_allocations == 0);
            RUDA_EXPECT(state.live_bytes == 0);
        }
        RUDA_EXPECT(reached_success);
        ruda_bytecode_function_dispose(&function);
    }
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;

    RUDA_EXPECT(ruda_test_vm_build_array_function(
        &function, ruda_system_allocator(), 0, true
    ));
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_bytecode_run(
        &function, NULL, 0, ruda_system_allocator(), run_options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x3ff8000000000000));
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_function_dispose(&function);

    for (index = 0;
        index < sizeof(invalid_indices) / sizeof(invalid_indices[0]);
        index += 1) {
        RUDA_EXPECT(ruda_test_vm_build_array_function(
            &function, ruda_system_allocator(), invalid_indices[index], false
        ));
        memset(&result, 0, sizeof(result));
        RUDA_EXPECT(ruda_bytecode_run(
            &function, NULL, 0, ruda_system_allocator(), run_options, &result
        ) == RUDA_BYTECODE_RUN_PANIC);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
        RUDA_EXPECT(panic.message.length == strlen(expected_messages[index]));
        RUDA_EXPECT(memcmp(
            panic.message.data, expected_messages[index], panic.message.length
        ) == 0);
        RUDA_EXPECT(panic.span.start == 705 && panic.span.end == 706);
        RUDA_EXPECT(execution.managed_heap.roots == NULL);
        if (index == 0) {
            run_options.panic = NULL;
            RUDA_EXPECT(ruda_bytecode_run(
                &function, NULL, 0, ruda_system_allocator(), run_options,
                &result
            ) == RUDA_BYTECODE_RUN_PANIC);
            RUDA_EXPECT(execution.managed_heap.roots == NULL);
            run_options.panic = &panic;
        }
        ruda_bytecode_function_dispose(&function);
    }
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);
}

void ruda_test_vm_array_feature_call_metadata(void) {
    const RudaSymbolId callee = {{0x10}};
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded;
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    RudaOwnedBytes encoded;
    uint32_t block = UINT32_MAX;
    uint64_t span = 880;
    const size_t feature_offset = 9;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_bytecode_function_init(
        &function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_VOID,
        1, NULL, 0, ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_add_call(
        &function, callee, RUDA_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &call
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &block));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, block, RUDA_OPCODE_RETURN_VOID, 0, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_function(
        &function, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(encoded.length > feature_offset);
    if (encoded.length > feature_offset) {
        /* v1 and the current feature mask are one-byte canonical varints. */
        RUDA_EXPECT(encoded.data[feature_offset] == ((1U << 0) | (1U << 3)));
        RUDA_EXPECT(ruda_bytecode_decode_function(
            encoded.data, encoded.length, ruda_system_allocator(),
            ruda_bytecode_function_default_options(), &decoded
        ) == RUDA_BYTECODE_CODEC_OK);
        ruda_bytecode_function_dispose(&decoded);
        encoded.data[feature_offset] = 1U << 0;
        RUDA_EXPECT(ruda_bytecode_decode_function(
            encoded.data, encoded.length, ruda_system_allocator(),
            ruda_bytecode_function_default_options(), &decoded
        ) != RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(!decoded.initialized);
    }
    ruda_bytecode_function_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_vm_checked_index_cfg(void) {
    static const RudaBytecodeType parameters[] = {
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_BOOL
    };
    RudaBytecodeFunction function;
    RudaBytecodeInstruction *second_require;
    uint32_t blocks[4] = {
        UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
    };
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint64_t span = 1080;
    memset(&function, 0, sizeof(function));
    RUDA_EXPECT(ruda_bytecode_function_init(
        &function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_F64,
        1, parameters, sizeof(parameters) / sizeof(parameters[0]),
        ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_add_register(
        &function, RUDA_BYTECODE_TYPE_ARRAY_F64, &checked
    ));
    RUDA_EXPECT(ruda_bytecode_add_register(
        &function, RUDA_BYTECODE_TYPE_ARRAY_F64, &alternate
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &blocks[0]));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &blocks[1]));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &blocks[2]));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &blocks[3]));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[0], RUDA_OPCODE_BRANCH_BOOL, 3, blocks[1],
        blocks[2], &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[1], RUDA_OPCODE_ARRAY_REQUIRE_INDEX, checked, 0, 1,
        &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[1], RUDA_OPCODE_JUMP, blocks[3], 0, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[2], RUDA_OPCODE_ARRAY_REQUIRE_INDEX, checked, 0, 1,
        &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[2], RUDA_OPCODE_JUMP, blocks[3], 0, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[3], RUDA_OPCODE_ARRAY_SET, checked, 1, 2, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, blocks[3], RUDA_OPCODE_RETURN, 2, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    second_require = ruda_test_vm_at(&function, blocks[2], 0);
    RUDA_EXPECT(second_require != NULL);
    if (second_require != NULL) {
        second_require->operands[0].unsigned_value = alternate;
        RUDA_EXPECT(ruda_bytecode_verify(&function)
            == RUDA_BYTECODE_VERIFY_INVALID_ARGUMENT);
        second_require->operands[0].unsigned_value = checked;
    }
    RUDA_EXPECT(ruda_bytecode_verify(&function) == RUDA_BYTECODE_VERIFY_OK);
    ruda_bytecode_function_dispose(&function);
}

static bool ruda_test_vm_build_array_rhs_panic_program(
    RudaBytecodeProgram *program,
    int64_t access_index
) {
    const RudaSymbolId rhs_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    static const unsigned char rhs_message[] = "rhs evaluated";
    static const RudaBytecodeType entry_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_ARRAY_F64
    };
    RudaBytecodeFunction rhs;
    RudaBytecodeFunction entry;
    RudaBytecodeArrayArgument argument;
    RudaBytecodeArrayConstructionId construction =
        RUDA_BYTECODE_NO_ARRAY_CONSTRUCTION;
    RudaBytecodeCallId call = RUDA_BYTECODE_NO_CALL;
    RudaBytecodeInstruction *require_index;
    uint32_t block = UINT32_MAX;
    uint32_t function_id = UINT32_MAX;
    uint64_t span = 1100;
    memset(program, 0, sizeof(*program));
    memset(&rhs, 0, sizeof(rhs));
    memset(&entry, 0, sizeof(entry));
    if (!ruda_bytecode_program_init(
            program, ruda_system_allocator(), 1,
            ruda_bytecode_program_default_options()
        ) || !ruda_test_vm_build_string_literal(
            &rhs, ruda_system_allocator(), rhs_message,
            sizeof(rhs_message) - 1, true
        ) || !ruda_bytecode_program_take_function(
            program, rhs_symbol, &rhs, &function_id
        ) || !ruda_bytecode_function_init(
            &entry, ruda_system_allocator(), RUDA_BYTECODE_TYPE_NEVER,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(
            &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
        ) || !ruda_bytecode_add_block(&entry, &block)) {
        goto failure;
    }
    argument.register_id = 0;
    argument.type = RUDA_BYTECODE_TYPE_F64;
    if (!ruda_bytecode_add_array_construction(
            &entry, RUDA_BYTECODE_TYPE_ARRAY_F64, &argument, 1,
            &construction
        ) || !ruda_bytecode_add_call(
            &entry, rhs_symbol, RUDA_BYTECODE_TYPE_NEVER, NULL, 0, &call
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff0000000000000), 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CONST_I64, 2,
            (uint64_t)access_index, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_REQUIRE_INDEX, 3, 1, 2, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span
        )) {
        goto failure;
    }
    require_index = ruda_test_vm_at(&entry, 0, 3);
    if (require_index == NULL) {
        goto failure;
    }
    require_index->span.start = 1120;
    require_index->span.end = 1134;
    if (!ruda_bytecode_program_take_function(
            program, entry_symbol, &entry, &function_id
        ) || !ruda_bytecode_program_set_entry(program, entry_symbol)) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(&entry);
    ruda_bytecode_function_dispose(&rhs);
    ruda_bytecode_program_dispose(program);
    return false;
}

void ruda_test_vm_array_rhs_ordering(void) {
    static const struct {
        int64_t index;
        const char *code;
    } cases[] = {
        {1, "G4002"},
        {0, "G4000"}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        RudaBytecodeProgram program;
        RudaBytecodeProgram decoded;
        RudaBytecodeExecution execution;
        RudaBytecodePanic panic;
        RudaBytecodeRunOptions options;
        RudaBytecodeValue result;
        RudaOwnedBytes encoded;
        memset(&program, 0, sizeof(program));
        memset(&decoded, 0, sizeof(decoded));
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        memset(&encoded, 0, sizeof(encoded));
        RUDA_EXPECT(ruda_test_vm_build_array_rhs_panic_program(
            &program, cases[index].index
        ));
        RUDA_EXPECT(ruda_bytecode_program_verify(&program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
        RUDA_EXPECT(ruda_bytecode_encode_program(
            &program, ruda_system_allocator(), &encoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_decode_program(
            encoded.data, encoded.length, ruda_system_allocator(),
            ruda_bytecode_program_default_options(), &decoded
        ) == RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(ruda_bytecode_execution_init(
            &execution, ruda_system_allocator(),
            ruda_bytecode_execution_default_options()
        ));
        RUDA_EXPECT(ruda_bytecode_panic_init(
            &panic, ruda_system_allocator()
        ));
        options = ruda_bytecode_run_default_options();
        options.execution = &execution;
        options.panic = &panic;
        RUDA_EXPECT(ruda_bytecode_run_program(
            &decoded, NULL, 0, ruda_system_allocator(), options, &result
        ) == RUDA_BYTECODE_RUN_PANIC);
        RUDA_EXPECT(panic.code != NULL
            && strcmp(panic.code, cases[index].code) == 0);
        if (cases[index].index == 1) {
            RUDA_EXPECT(panic.span.start == 1120 && panic.span.end == 1134);
            RUDA_EXPECT(panic.message.length
                == strlen("array index 1 is outside length 1"));
            RUDA_EXPECT(memcmp(
                panic.message.data, "array index 1 is outside length 1",
                panic.message.length
            ) == 0);
        } else {
            RUDA_EXPECT(panic.message.length == strlen("rhs evaluated"));
            RUDA_EXPECT(memcmp(
                panic.message.data, "rhs evaluated", panic.message.length
            ) == 0);
        }
        RUDA_EXPECT(execution.managed_heap.roots == NULL);
        ruda_bytecode_panic_dispose(&panic);
        ruda_bytecode_execution_dispose(&execution);
        ruda_bytecode_program_dispose(&decoded);
        ruda_owned_bytes_dispose(&encoded);
        ruda_bytecode_program_dispose(&program);
    }
}

static bool ruda_test_vm_build_array_call_program(
    RudaBytecodeProgram *program
) {
    const RudaSymbolId leaf_symbol = {{0x10}};
    const RudaSymbolId entry_symbol = {{0x20}};
    static const RudaBytecodeType leaf_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64
    };
    static const RudaBytecodeType entry_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64
    };
    RudaBytecodeFunction leaf;
    RudaBytecodeFunction entry;
    RudaBytecodeArrayArgument array_argument;
    RudaBytecodeArrayConstructionId construction;
    RudaBytecodeCallId call;
    uint32_t block;
    uint32_t function_id;
    uint64_t span = 900;
    memset(program, 0, sizeof(*program));
    memset(&leaf, 0, sizeof(leaf));
    memset(&entry, 0, sizeof(entry));
    if (!ruda_bytecode_program_init(
            program, ruda_system_allocator(), 1,
            ruda_bytecode_program_default_options()
        ) || !ruda_bytecode_function_init(
            &leaf, ruda_system_allocator(), RUDA_BYTECODE_TYPE_ARRAY_F64,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(
            &leaf, leaf_types, sizeof(leaf_types) / sizeof(leaf_types[0])
        ) || !ruda_bytecode_add_block(&leaf, &block)) {
        goto failure;
    }
    array_argument.register_id = 0;
    array_argument.type = RUDA_BYTECODE_TYPE_F64;
    if (!ruda_bytecode_add_array_construction(
            &leaf, RUDA_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
            &construction
        ) || !ruda_test_vm_append(
            &leaf, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x4008000000000000), 0, &span
        ) || !ruda_test_vm_append(
            &leaf, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !ruda_test_vm_append(
            &leaf, block, RUDA_OPCODE_RETURN, 1, 0, 0, &span
        ) || !ruda_bytecode_program_take_function(
            program, leaf_symbol, &leaf, &function_id
        )) {
        goto failure;
    }
    span = 920;
    if (!ruda_bytecode_function_init(
            &entry, ruda_system_allocator(), RUDA_BYTECODE_TYPE_ARRAY_F64,
            1, NULL, 0, ruda_bytecode_function_default_options()
        ) || !ruda_test_vm_add_registers(
            &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
        ) || !ruda_bytecode_add_block(&entry, &block)) {
        goto failure;
    }
    array_argument.register_id = 0;
    if (!ruda_bytecode_add_array_construction(
            &entry, RUDA_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
            &construction
        ) || !ruda_bytecode_add_call(
            &entry, leaf_symbol, RUDA_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &call
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff8000000000000), 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CALL_DIRECT, 2, call, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_NEW, 5, construction, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_CONST_I64, 3, 0, 0, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_ARRAY_GET, 4, 2, 3, &span
        ) || !ruda_test_vm_append(
            &entry, block, RUDA_OPCODE_RETURN, 2, 0, 0, &span
        ) || !ruda_bytecode_program_take_function(
            program, entry_symbol, &entry, &function_id
        ) || !ruda_bytecode_program_set_entry(program, entry_symbol)) {
        goto failure;
    }
    return true;

failure:
    ruda_bytecode_function_dispose(&entry);
    ruda_bytecode_function_dispose(&leaf);
    ruda_bytecode_program_dispose(program);
    return false;
}

void ruda_test_vm_managed_array_calls(void) {
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaBytecodeExecution execution;
    RudaBytecodeValue result;
    RudaBytecodeRunOptions run_options;
    RudaManagedRootFrame host_root;
    RudaManagedReference *host_locations[1];
    RudaManagedCell cell;
    size_t length = 0;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    RudaOwnedBytes encoded;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&encoded, 0, sizeof(encoded));
    memset(&host_root, 0, sizeof(host_root));
    memset(&cell, 0, sizeof(cell));
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_test_vm_build_array_call_program(&program));
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_total_array_constructions = 1;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_total_array_constructions = 1000000;
    program.options.max_total_array_arguments = 1;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_total_array_arguments = 10000000;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_program(
        &program, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    {
        RudaBytecodeProgramOptions limited =
            ruda_bytecode_program_default_options();
        RudaFailAllocator tracking;
        limited.max_total_array_constructions = 1;
        ruda_fail_allocator_init(
            &tracking, ruda_system_allocator(), SIZE_MAX
        );
        RUDA_EXPECT(ruda_bytecode_decode_program(
            encoded.data, encoded.length, ruda_fail_allocator(&tracking),
            limited, &decoded
        ) == RUDA_BYTECODE_CODEC_LIMIT_REACHED);
        RUDA_EXPECT(!decoded.initialized);
        ruda_bytecode_program_dispose(&decoded);
        RUDA_EXPECT(tracking.live_allocations == 0);
        RUDA_EXPECT(tracking.live_bytes == 0);
    }
    {
        RudaBytecodeProgramOptions limited =
            ruda_bytecode_program_default_options();
        RudaFailAllocator tracking;
        limited.max_total_array_arguments = 1;
        ruda_fail_allocator_init(
            &tracking, ruda_system_allocator(), SIZE_MAX
        );
        RUDA_EXPECT(ruda_bytecode_decode_program(
            encoded.data, encoded.length, ruda_fail_allocator(&tracking),
            limited, &decoded
        ) == RUDA_BYTECODE_CODEC_LIMIT_REACHED);
        RUDA_EXPECT(!decoded.initialized);
        ruda_bytecode_program_dispose(&decoded);
        RUDA_EXPECT(tracking.live_allocations == 0);
        RUDA_EXPECT(tracking.live_bytes == 0);
    }
    RUDA_EXPECT(ruda_bytecode_decode_program(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_program_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), execution_options
    ));
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_bytecode_run_program(
        &decoded, NULL, 0, ruda_system_allocator(), run_options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_ARRAY_F64);
    RUDA_EXPECT(ruda_managed_array_length(
        &execution.managed_heap, result.as.managed_reference, &length
    ) == RUDA_MANAGED_OK && length == 1);
    host_locations[0] = &result.as.managed_reference;
    RUDA_EXPECT(ruda_managed_root_push(
        &execution.managed_heap, &host_root, host_locations, 1
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&execution.managed_heap)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_get(
        &execution.managed_heap, result.as.managed_reference, 0, &cell, &length
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(cell.f64_bits == UINT64_C(0x4008000000000000));
    RUDA_EXPECT(ruda_managed_root_pop(
        &execution.managed_heap, &host_root
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_heap_collect(&execution.managed_heap)
        == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_length(
        &execution.managed_heap, result.as.managed_reference, &length
    ) == RUDA_MANAGED_INVALID_REFERENCE);
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&program);
}

void ruda_test_vm_managed_array_provenance(void) {
    const RudaBytecodeType parameter = RUDA_BYTECODE_TYPE_ARRAY_F64;
    RudaBytecodeFunction function;
    RudaBytecodeExecution first;
    RudaBytecodeExecution second;
    RudaBytecodeValue argument;
    RudaBytecodeValue result;
    RudaBytecodeRunOptions options;
    RudaManagedCell cell;
    RudaManagedReference first_reference;
    RudaManagedReference second_reference;
    RudaManagedReference wrong_descriptor_reference;
    RudaManagedReference stale_reference;
    RudaManagedCell nested_cell;
    uint32_t length_register = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 950;
    memset(&function, 0, sizeof(function));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&cell, 0, sizeof(cell));
    memset(&nested_cell, 0, sizeof(nested_cell));
    cell.f64_bits = UINT64_C(0x3ff0000000000000);
    RUDA_EXPECT(ruda_bytecode_function_init(
        &function, ruda_system_allocator(), RUDA_BYTECODE_TYPE_I64,
        1, &parameter, 1, ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_add_register(
        &function, RUDA_BYTECODE_TYPE_I64, &length_register
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&function, &block));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, block, RUDA_OPCODE_ARRAY_LENGTH, length_register, 0, 0,
        &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &function, block, RUDA_OPCODE_RETURN, length_register, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &first, ruda_system_allocator(), ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &second, ruda_system_allocator(), ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &first.managed_heap, &ruda_managed_array_f64_descriptor,
        &cell, 1, &first_reference
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(ruda_managed_array_new(
        &second.managed_heap, &ruda_managed_array_f64_descriptor,
        &cell, 1, &second_reference
    ) == RUDA_MANAGED_OK);
    nested_cell.reference = second_reference;
    RUDA_EXPECT(ruda_managed_array_new(
        &second.managed_heap, &ruda_managed_array_array_f64_descriptor,
        &nested_cell, 1, &wrong_descriptor_reference
    ) == RUDA_MANAGED_OK);
    argument.type = RUDA_BYTECODE_TYPE_ARRAY_F64;
    argument.as.managed_reference = first_reference;
    options = ruda_bytecode_run_default_options();
    options.execution = &second;
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(ruda_bytecode_run(
        &function, &argument, 1, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    argument.as.managed_reference = wrong_descriptor_reference;
    RUDA_EXPECT(ruda_bytecode_run(
        &function, &argument, 1, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    stale_reference = second_reference;
    ruda_bytecode_execution_dispose(&second);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &second, ruda_system_allocator(), ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_managed_array_new(
        &second.managed_heap, &ruda_managed_array_f64_descriptor,
        &cell, 1, &second_reference
    ) == RUDA_MANAGED_OK);
    argument.as.managed_reference = stale_reference;
    options.execution = &second;
    RUDA_EXPECT(ruda_bytecode_run(
        &function, &argument, 1, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_INVALID_REFERENCE);
    ruda_bytecode_execution_dispose(&second);
    ruda_bytecode_execution_dispose(&first);
    ruda_bytecode_function_dispose(&function);
}

void ruda_test_vm_managed_array_panic_frames(void) {
    const RudaSymbolId leaf_symbol = {{0x30}};
    const RudaSymbolId entry_symbol = {{0x40}};
    static const RudaBytecodeType leaf_types[] = {
        RUDA_BYTECODE_TYPE_F64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64,
        RUDA_BYTECODE_TYPE_ARRAY_F64,
        RUDA_BYTECODE_TYPE_I64
    };
    static const RudaBytecodeType entry_types[] = {
        RUDA_BYTECODE_TYPE_I64
    };
    RudaBytecodeFunction leaf;
    RudaBytecodeFunction entry;
    RudaBytecodeProgram program;
    RudaBytecodeExecution execution;
    RudaBytecodePanic panic;
    RudaBytecodeValue result;
    RudaBytecodeRunOptions options;
    RudaBytecodeArrayArgument array_argument;
    RudaBytecodeArrayConstructionId construction;
    RudaBytecodeCallId call;
    const RudaBytecodePanicFrame *frame;
    uint32_t block;
    uint32_t function_id;
    uint64_t span = 980;
    memset(&leaf, 0, sizeof(leaf));
    memset(&entry, 0, sizeof(entry));
    memset(&program, 0, sizeof(program));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    RUDA_EXPECT(ruda_bytecode_program_init(
        &program, ruda_system_allocator(), 1,
        ruda_bytecode_program_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_function_init(
        &leaf, ruda_system_allocator(), RUDA_BYTECODE_TYPE_I64,
        1, NULL, 0, ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_test_vm_add_registers(
        &leaf, leaf_types, sizeof(leaf_types) / sizeof(leaf_types[0])
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&leaf, &block));
    array_argument.register_id = 0;
    array_argument.type = RUDA_BYTECODE_TYPE_F64;
    RUDA_EXPECT(ruda_bytecode_add_array_construction(
        &leaf, RUDA_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
        &construction
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_CONST_F64, 0,
        UINT64_C(0x3ff0000000000000), 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_ARRAY_NEW, 1, construction, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_CONST_I64, 2, (uint64_t)-1, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_ARRAY_REQUIRE_INDEX, 3, 1, 2, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_ARRAY_LENGTH, 4, 3, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &leaf, block, RUDA_OPCODE_RETURN, 4, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program, leaf_symbol, &leaf, &function_id
    ));
    span = 1000;
    RUDA_EXPECT(ruda_bytecode_function_init(
        &entry, ruda_system_allocator(), RUDA_BYTECODE_TYPE_I64,
        1, NULL, 0, ruda_bytecode_function_default_options()
    ));
    RUDA_EXPECT(ruda_test_vm_add_registers(
        &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
    ));
    RUDA_EXPECT(ruda_bytecode_add_call(
        &entry, leaf_symbol, RUDA_BYTECODE_TYPE_I64, NULL, 0, &call
    ));
    RUDA_EXPECT(ruda_bytecode_add_block(&entry, &block));
    RUDA_EXPECT(ruda_test_vm_append(
        &entry, block, RUDA_OPCODE_CALL_DIRECT, 0, call, 0, &span
    ));
    RUDA_EXPECT(ruda_test_vm_append(
        &entry, block, RUDA_OPCODE_RETURN, 0, 0, 0, &span
    ));
    RUDA_EXPECT(ruda_bytecode_program_take_function(
        &program, entry_symbol, &entry, &function_id
    ));
    RUDA_EXPECT(ruda_bytecode_program_set_entry(&program, entry_symbol));
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), ruda_bytecode_execution_default_options()
    ));
    RUDA_EXPECT(ruda_bytecode_panic_init(&panic, ruda_system_allocator()));
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;
    options.panic = &panic;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &program, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
    RUDA_EXPECT(panic.message.length
        == strlen("array index -1 is outside length 1"));
    RUDA_EXPECT(memcmp(
        panic.message.data, "array index -1 is outside length 1",
        panic.message.length
    ) == 0);
    RUDA_EXPECT(memcmp(&panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && panic.span.start == 983 && panic.span.end == 984
        && panic.frames.length == 2);
    frame = ruda_bytecode_panic_frame(&panic, 1);
    RUDA_EXPECT(frame != NULL
        && memcmp(&frame->symbol, &entry_symbol, sizeof(entry_symbol)) == 0
        && frame->call_span.start == 1000 && frame->call_span.end == 1001);
    RUDA_EXPECT(execution.managed_heap.roots == NULL);
    ruda_bytecode_panic_dispose(&panic);
    ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&program);
}
