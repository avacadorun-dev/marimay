#include "test_bytecode_vm_support.h"

static bool mmay_test_vm_build_array_function(
    MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    int64_t access_index,
    bool nested
) {
    static const MarimayBytecodeType flat_types[] = {
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64
    };
    static const MarimayBytecodeType nested_types[] = {
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_F64
    };
    MarimayBytecodeArrayArgument inner_arguments[2];
    MarimayBytecodeArrayArgument outer_argument;
    MarimayBytecodeArrayConstructionId inner =
        MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    MarimayBytecodeArrayConstructionId outer =
        MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    uint32_t block = UINT32_MAX;
    uint64_t span = 700;
    const MarimayBytecodeType *types = nested ? nested_types : flat_types;
    size_t type_count = nested
        ? sizeof(nested_types) / sizeof(nested_types[0])
        : sizeof(flat_types) / sizeof(flat_types[0]);
    memset(function, 0, sizeof(*function));
    if (!mmay_bytecode_function_init(
            function, allocator, MMAY_BYTECODE_TYPE_F64, 1, NULL, 0,
            mmay_bytecode_function_default_options()
        ) || !mmay_test_vm_add_registers(function, types, type_count)
        || !mmay_bytecode_add_block(function, &block)) {
        goto failure;
    }
    inner_arguments[0].register_id = 0;
    inner_arguments[0].type = MMAY_BYTECODE_TYPE_F64;
    inner_arguments[1].register_id = nested ? 0 : 1;
    inner_arguments[1].type = MMAY_BYTECODE_TYPE_F64;
    if (!mmay_bytecode_add_array_construction(
            function, MMAY_BYTECODE_TYPE_ARRAY_F64, inner_arguments,
            nested ? 1 : 2, &inner
        )) {
        goto failure;
    }
    if (nested) {
        outer_argument.register_id = 1;
        outer_argument.type = MMAY_BYTECODE_TYPE_ARRAY_F64;
        if (!mmay_bytecode_add_array_construction(
                function, MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64,
                &outer_argument, 1, &outer
            )) {
            goto failure;
        }
    }
    if (!mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff8000000000000), 0, &span
        )) {
        goto failure;
    }
    if (!nested && !mmay_test_vm_append(
            function, block, MMAY_OPCODE_CONST_F64, 1,
            UINT64_C(0x4004000000000000), 0, &span
        )) {
        goto failure;
    }
    if (!mmay_test_vm_append(
            function, block, MMAY_OPCODE_ARRAY_NEW, nested ? 1 : 2,
            inner, 0, &span
        )) {
        goto failure;
    }
    if (nested) {
        if (!mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_NEW, 2, outer, 0, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_CONST_I64, 3, 0, 0, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_GET, 4, 2, 3, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_GET, 5, 4, 3, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_RETURN, 5, 0, 0, &span
            )) {
            goto failure;
        }
    } else {
        if (!mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_LENGTH, 3, 2, 0, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_CONST_I64, 4,
                (uint64_t)access_index, 0, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 7, 2, 4,
                &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_CONST_F64, 5,
                UINT64_C(0x4022000000000000), 0, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_SET, 7, 4, 5, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_ARRAY_GET, 6, 2, 4, &span
            ) || !mmay_test_vm_append(
                function, block, MMAY_OPCODE_RETURN, 6, 0, 0, &span
            )) {
            goto failure;
        }
    }
    return true;

failure:
    mmay_bytecode_function_dispose(function);
    return false;
}

void mmay_test_vm_managed_arrays(void) {
    static const int64_t invalid_indices[] = {
        -1, 2, INT64_MIN, INT64_MAX
    };
    static const char *expected_messages[] = {
        "array index -1 is outside length 2",
        "array index 2 is outside length 2",
        "array index -9223372036854775808 is outside length 2",
        "array index 9223372036854775807 is outside length 2"
    };
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodePanic panic;
    MarimayBytecodeValue result;
    MarimayOwnedBytes encoded;
    MarimayTestBuffer disassembly;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodeExecutionOptions execution_options =
        mmay_bytecode_execution_default_options();
    size_t index = 0;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&encoded, 0, sizeof(encoded));
    execution_options.managed_heap.collect_before_allocation = true;
    MMAY_EXPECT(mmay_test_vm_build_array_function(
        &function, mmay_system_allocator(), 1, false
    ));
    {
        MarimayBytecodeArrayConstruction *construction = mmay_vector_at(
            &function.array_constructions, 0
        );
        MarimayBytecodeArrayArgument *argument = mmay_vector_at(
            &function.array_arguments, 0
        );
        MarimayBytecodeInstruction *array_new = mmay_test_vm_at(&function, 0, 2);
        MMAY_EXPECT(construction != NULL && argument != NULL
            && array_new != NULL);
        if (construction != NULL) {
            construction->array_type = MMAY_BYTECODE_TYPE_F64;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            construction->array_type = MMAY_BYTECODE_TYPE_ARRAY_F64;
        }
        if (argument != NULL) {
            argument->type = MMAY_BYTECODE_TYPE_BOOL;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_TYPE_MISMATCH);
            argument->type = MMAY_BYTECODE_TYPE_F64;
        }
        if (array_new != NULL) {
            array_new->operands[1].unsigned_value = UINT64_MAX;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            array_new->operands[1].unsigned_value = 0;
        }
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    {
        MarimayBytecodeInstruction *set = mmay_test_vm_at(&function, 0, 7);
        MarimayBytecodeInstruction *rhs = mmay_test_vm_at(&function, 0, 6);
        MarimayBytecodeInstruction saved_rhs;
        uint64_t original_array = set != NULL
            ? set->operands[0].unsigned_value
            : UINT64_MAX;
        uint64_t original_index = set != NULL
            ? set->operands[1].unsigned_value
            : UINT64_MAX;
        MMAY_EXPECT(set != NULL && rhs != NULL);
        memset(&saved_rhs, 0, sizeof(saved_rhs));
        if (rhs != NULL) {
            saved_rhs = *rhs;
        }
        if (set != NULL && rhs != NULL) {
            set->operands[0].unsigned_value = 2;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            MMAY_EXPECT(mmay_bytecode_run(
                &function, NULL, 0, mmay_system_allocator(),
                mmay_bytecode_run_default_options(), &result
            ) == MMAY_BYTECODE_RUN_INVALID_BYTECODE);
            set->operands[0].unsigned_value = original_array;
            set->operands[1].unsigned_value = 3;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            set->operands[1].unsigned_value = original_index;
            *rhs = mmay_test_vm_instruction(
                MMAY_OPCODE_COPY, 7, 2, 0, saved_rhs.span.start
            );
            set->operands[2].unsigned_value = 0;
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            *rhs = mmay_test_vm_instruction(
                MMAY_OPCODE_CONST_I64, 4, 0, 0, saved_rhs.span.start
            );
            MMAY_EXPECT(mmay_bytecode_verify(&function)
                == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
            *rhs = saved_rhs;
            set->operands[2].unsigned_value = 5;
        }
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_test_buffer_init(&disassembly);
    MMAY_EXPECT(mmay_bytecode_disassemble(
        &function, mmay_test_buffer_writer(&disassembly)
    ) == MMAY_BYTECODE_DISASSEMBLE_OK);
    MMAY_EXPECT(strstr(disassembly.bytes, "array_new r2, a0") != NULL);
    MMAY_EXPECT(strstr(disassembly.bytes, "array_set r7, r4, r5") != NULL);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &function, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    {
        bool reached_success = false;
        size_t failure_index = 0;
        for (failure_index = 0; failure_index < 64 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeFunction attempted;
            MarimayBytecodeCodecStatus decode_status;
            memset(&attempted, 0, sizeof(attempted));
            mmay_fail_allocator_init(
                &state, mmay_system_allocator(), failure_index
            );
            decode_status = mmay_bytecode_decode_function(
                encoded.data, encoded.length, mmay_fail_allocator(&state),
                mmay_bytecode_function_default_options(), &attempted
            );
            if (decode_status == MMAY_BYTECODE_CODEC_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(decode_status
                    == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            }
            mmay_bytecode_function_dispose(&attempted);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
    }
    MMAY_EXPECT(mmay_bytecode_decode_function(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_function_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(), execution_options
    ));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_bytecode_run(
        &decoded, NULL, 0, mmay_system_allocator(), run_options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x4022000000000000));
    MMAY_EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);

    {
        bool reached_success = false;
        size_t failure_index = 0;
        MMAY_EXPECT(mmay_test_vm_build_array_function(
            &function, mmay_system_allocator(), 1, false
        ));
        for (failure_index = 0; failure_index < 32 && !reached_success;
            failure_index += 1) {
            MarimayFailAllocator state;
            MarimayBytecodeExecution failed_execution;
            MarimayBytecodeRunStatus status;
            memset(&failed_execution, 0, sizeof(failed_execution));
            mmay_fail_allocator_init(
                &state, mmay_system_allocator(), SIZE_MAX
            );
            MMAY_EXPECT(mmay_bytecode_execution_init(
                &failed_execution, mmay_fail_allocator(&state),
                mmay_bytecode_execution_default_options()
            ));
            state.failure_index = state.attempts + failure_index;
            run_options = mmay_bytecode_run_default_options();
            run_options.execution = &failed_execution;
            status = mmay_bytecode_run(
                &function, NULL, 0, mmay_fail_allocator(&state),
                run_options, &result
            );
            if (status == MMAY_BYTECODE_RUN_OK) {
                reached_success = true;
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            mmay_bytecode_execution_dispose(&failed_execution);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
        MMAY_EXPECT(reached_success);
        mmay_bytecode_function_dispose(&function);
    }
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &panic;

    MMAY_EXPECT(mmay_test_vm_build_array_function(
        &function, mmay_system_allocator(), 0, true
    ));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_bytecode_run(
        &function, NULL, 0, mmay_system_allocator(), run_options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_F64
        && result.as.f64_bits == UINT64_C(0x3ff8000000000000));
    MMAY_EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_function_dispose(&function);

    for (index = 0;
        index < sizeof(invalid_indices) / sizeof(invalid_indices[0]);
        index += 1) {
        MMAY_EXPECT(mmay_test_vm_build_array_function(
            &function, mmay_system_allocator(), invalid_indices[index], false
        ));
        memset(&result, 0, sizeof(result));
        MMAY_EXPECT(mmay_bytecode_run(
            &function, NULL, 0, mmay_system_allocator(), run_options, &result
        ) == MMAY_BYTECODE_RUN_PANIC);
        MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
        MMAY_EXPECT(panic.message.length == strlen(expected_messages[index]));
        MMAY_EXPECT(memcmp(
            panic.message.data, expected_messages[index], panic.message.length
        ) == 0);
        MMAY_EXPECT(panic.span.start == 705 && panic.span.end == 706);
        MMAY_EXPECT(execution.managed_heap.roots == NULL);
        if (index == 0) {
            run_options.panic = NULL;
            MMAY_EXPECT(mmay_bytecode_run(
                &function, NULL, 0, mmay_system_allocator(), run_options,
                &result
            ) == MMAY_BYTECODE_RUN_PANIC);
            MMAY_EXPECT(execution.managed_heap.roots == NULL);
            run_options.panic = &panic;
        }
        mmay_bytecode_function_dispose(&function);
    }
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
}

void mmay_test_vm_array_feature_call_metadata(void) {
    const MarimaySymbolId callee = {{0x10}};
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded;
    MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
    MarimayOwnedBytes encoded;
    uint32_t block = UINT32_MAX;
    uint64_t span = 880;
    const size_t feature_offset = 9;
    memset(&function, 0, sizeof(function));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_bytecode_function_init(
        &function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_VOID,
        1, NULL, 0, mmay_bytecode_function_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_add_call(
        &function, callee, MMAY_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &call
    ));
    MMAY_EXPECT(mmay_bytecode_add_block(&function, &block));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, block, MMAY_OPCODE_RETURN_VOID, 0, 0, 0, &span
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_function(
        &function, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(encoded.length > feature_offset);
    if (encoded.length > feature_offset) {
        /* v1 and the current feature mask are one-byte canonical varints. */
        MMAY_EXPECT(encoded.data[feature_offset] == ((1U << 0) | (1U << 3)));
        MMAY_EXPECT(mmay_bytecode_decode_function(
            encoded.data, encoded.length, mmay_system_allocator(),
            mmay_bytecode_function_default_options(), &decoded
        ) == MMAY_BYTECODE_CODEC_OK);
        mmay_bytecode_function_dispose(&decoded);
        encoded.data[feature_offset] = 1U << 0;
        MMAY_EXPECT(mmay_bytecode_decode_function(
            encoded.data, encoded.length, mmay_system_allocator(),
            mmay_bytecode_function_default_options(), &decoded
        ) != MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(!decoded.initialized);
    }
    mmay_bytecode_function_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_vm_checked_index_cfg(void) {
    static const MarimayBytecodeType parameters[] = {
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_BOOL
    };
    MarimayBytecodeFunction function;
    MarimayBytecodeInstruction *second_require;
    uint32_t blocks[4] = {
        UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
    };
    uint32_t checked = UINT32_MAX;
    uint32_t alternate = UINT32_MAX;
    uint64_t span = 1080;
    memset(&function, 0, sizeof(function));
    MMAY_EXPECT(mmay_bytecode_function_init(
        &function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_F64,
        1, parameters, sizeof(parameters) / sizeof(parameters[0]),
        mmay_bytecode_function_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_add_register(
        &function, MMAY_BYTECODE_TYPE_ARRAY_F64, &checked
    ));
    MMAY_EXPECT(mmay_bytecode_add_register(
        &function, MMAY_BYTECODE_TYPE_ARRAY_F64, &alternate
    ));
    MMAY_EXPECT(mmay_bytecode_add_block(&function, &blocks[0]));
    MMAY_EXPECT(mmay_bytecode_add_block(&function, &blocks[1]));
    MMAY_EXPECT(mmay_bytecode_add_block(&function, &blocks[2]));
    MMAY_EXPECT(mmay_bytecode_add_block(&function, &blocks[3]));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, blocks[0], MMAY_OPCODE_BRANCH_BOOL, 3, blocks[1],
        blocks[2], &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, blocks[1], MMAY_OPCODE_ARRAY_REQUIRE_INDEX, checked, 0, 1,
        &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, blocks[1], MMAY_OPCODE_JUMP, blocks[3], 0, 0, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, blocks[2], MMAY_OPCODE_ARRAY_REQUIRE_INDEX, checked, 0, 1,
        &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, blocks[2], MMAY_OPCODE_JUMP, blocks[3], 0, 0, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, blocks[3], MMAY_OPCODE_ARRAY_SET, checked, 1, 2, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, blocks[3], MMAY_OPCODE_RETURN, 2, 0, 0, &span
    ));
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    second_require = mmay_test_vm_at(&function, blocks[2], 0);
    MMAY_EXPECT(second_require != NULL);
    if (second_require != NULL) {
        second_require->operands[0].unsigned_value = alternate;
        MMAY_EXPECT(mmay_bytecode_verify(&function)
            == MMAY_BYTECODE_VERIFY_INVALID_ARGUMENT);
        second_require->operands[0].unsigned_value = checked;
    }
    MMAY_EXPECT(mmay_bytecode_verify(&function) == MMAY_BYTECODE_VERIFY_OK);
    mmay_bytecode_function_dispose(&function);
}

static bool mmay_test_vm_build_array_rhs_panic_program(
    MarimayBytecodeProgram *program,
    int64_t access_index
) {
    const MarimaySymbolId rhs_symbol = {{0x10}};
    const MarimaySymbolId entry_symbol = {{0x20}};
    static const unsigned char rhs_message[] = "rhs evaluated";
    static const MarimayBytecodeType entry_types[] = {
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_ARRAY_F64
    };
    MarimayBytecodeFunction rhs;
    MarimayBytecodeFunction entry;
    MarimayBytecodeArrayArgument argument;
    MarimayBytecodeArrayConstructionId construction =
        MMAY_BYTECODE_NO_ARRAY_CONSTRUCTION;
    MarimayBytecodeCallId call = MMAY_BYTECODE_NO_CALL;
    MarimayBytecodeInstruction *require_index;
    uint32_t block = UINT32_MAX;
    uint32_t function_id = UINT32_MAX;
    uint64_t span = 1100;
    memset(program, 0, sizeof(*program));
    memset(&rhs, 0, sizeof(rhs));
    memset(&entry, 0, sizeof(entry));
    if (!mmay_bytecode_program_init(
            program, mmay_system_allocator(), 1,
            mmay_bytecode_program_default_options()
        ) || !mmay_test_vm_build_string_literal(
            &rhs, mmay_system_allocator(), rhs_message,
            sizeof(rhs_message) - 1, true
        ) || !mmay_bytecode_program_take_function(
            program, rhs_symbol, &rhs, &function_id
        ) || !mmay_bytecode_function_init(
            &entry, mmay_system_allocator(), MMAY_BYTECODE_TYPE_NEVER,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_test_vm_add_registers(
            &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
        ) || !mmay_bytecode_add_block(&entry, &block)) {
        goto failure;
    }
    argument.register_id = 0;
    argument.type = MMAY_BYTECODE_TYPE_F64;
    if (!mmay_bytecode_add_array_construction(
            &entry, MMAY_BYTECODE_TYPE_ARRAY_F64, &argument, 1,
            &construction
        ) || !mmay_bytecode_add_call(
            &entry, rhs_symbol, MMAY_BYTECODE_TYPE_NEVER, NULL, 0, &call
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff0000000000000), 0, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_CONST_I64, 2,
            (uint64_t)access_index, 0, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 3, 1, 2, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_CALL_DIRECT_NEVER, call, 0, 0, &span
        )) {
        goto failure;
    }
    require_index = mmay_test_vm_at(&entry, 0, 3);
    if (require_index == NULL) {
        goto failure;
    }
    require_index->span.start = 1120;
    require_index->span.end = 1134;
    if (!mmay_bytecode_program_take_function(
            program, entry_symbol, &entry, &function_id
        ) || !mmay_bytecode_program_set_entry(program, entry_symbol)) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(&entry);
    mmay_bytecode_function_dispose(&rhs);
    mmay_bytecode_program_dispose(program);
    return false;
}

void mmay_test_vm_array_rhs_ordering(void) {
    static const struct {
        int64_t index;
        const char *code;
    } cases[] = {
        {1, "G4002"},
        {0, "G4000"}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        MarimayBytecodeProgram program;
        MarimayBytecodeProgram decoded;
        MarimayBytecodeExecution execution;
        MarimayBytecodePanic panic;
        MarimayBytecodeRunOptions options;
        MarimayBytecodeValue result;
        MarimayOwnedBytes encoded;
        memset(&program, 0, sizeof(program));
        memset(&decoded, 0, sizeof(decoded));
        memset(&execution, 0, sizeof(execution));
        memset(&panic, 0, sizeof(panic));
        memset(&encoded, 0, sizeof(encoded));
        MMAY_EXPECT(mmay_test_vm_build_array_rhs_panic_program(
            &program, cases[index].index
        ));
        MMAY_EXPECT(mmay_bytecode_program_verify(&program)
            == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
        MMAY_EXPECT(mmay_bytecode_encode_program(
            &program, mmay_system_allocator(), &encoded
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_decode_program(
            encoded.data, encoded.length, mmay_system_allocator(),
            mmay_bytecode_program_default_options(), &decoded
        ) == MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(mmay_bytecode_execution_init(
            &execution, mmay_system_allocator(),
            mmay_bytecode_execution_default_options()
        ));
        MMAY_EXPECT(mmay_bytecode_panic_init(
            &panic, mmay_system_allocator()
        ));
        options = mmay_bytecode_run_default_options();
        options.execution = &execution;
        options.panic = &panic;
        MMAY_EXPECT(mmay_bytecode_run_program(
            &decoded, NULL, 0, mmay_system_allocator(), options, &result
        ) == MMAY_BYTECODE_RUN_PANIC);
        MMAY_EXPECT(panic.code != NULL
            && strcmp(panic.code, cases[index].code) == 0);
        if (cases[index].index == 1) {
            MMAY_EXPECT(panic.span.start == 1120 && panic.span.end == 1134);
            MMAY_EXPECT(panic.message.length
                == strlen("array index 1 is outside length 1"));
            MMAY_EXPECT(memcmp(
                panic.message.data, "array index 1 is outside length 1",
                panic.message.length
            ) == 0);
        } else {
            MMAY_EXPECT(panic.message.length == strlen("rhs evaluated"));
            MMAY_EXPECT(memcmp(
                panic.message.data, "rhs evaluated", panic.message.length
            ) == 0);
        }
        MMAY_EXPECT(execution.managed_heap.roots == NULL);
        mmay_bytecode_panic_dispose(&panic);
        mmay_bytecode_execution_dispose(&execution);
        mmay_bytecode_program_dispose(&decoded);
        mmay_owned_bytes_dispose(&encoded);
        mmay_bytecode_program_dispose(&program);
    }
}

static bool mmay_test_vm_build_array_call_program(
    MarimayBytecodeProgram *program
) {
    const MarimaySymbolId leaf_symbol = {{0x10}};
    const MarimaySymbolId entry_symbol = {{0x20}};
    static const MarimayBytecodeType leaf_types[] = {
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64
    };
    static const MarimayBytecodeType entry_types[] = {
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64
    };
    MarimayBytecodeFunction leaf;
    MarimayBytecodeFunction entry;
    MarimayBytecodeArrayArgument array_argument;
    MarimayBytecodeArrayConstructionId construction;
    MarimayBytecodeCallId call;
    uint32_t block;
    uint32_t function_id;
    uint64_t span = 900;
    memset(program, 0, sizeof(*program));
    memset(&leaf, 0, sizeof(leaf));
    memset(&entry, 0, sizeof(entry));
    if (!mmay_bytecode_program_init(
            program, mmay_system_allocator(), 1,
            mmay_bytecode_program_default_options()
        ) || !mmay_bytecode_function_init(
            &leaf, mmay_system_allocator(), MMAY_BYTECODE_TYPE_ARRAY_F64,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_test_vm_add_registers(
            &leaf, leaf_types, sizeof(leaf_types) / sizeof(leaf_types[0])
        ) || !mmay_bytecode_add_block(&leaf, &block)) {
        goto failure;
    }
    array_argument.register_id = 0;
    array_argument.type = MMAY_BYTECODE_TYPE_F64;
    if (!mmay_bytecode_add_array_construction(
            &leaf, MMAY_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
            &construction
        ) || !mmay_test_vm_append(
            &leaf, block, MMAY_OPCODE_CONST_F64, 0,
            UINT64_C(0x4008000000000000), 0, &span
        ) || !mmay_test_vm_append(
            &leaf, block, MMAY_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !mmay_test_vm_append(
            &leaf, block, MMAY_OPCODE_RETURN, 1, 0, 0, &span
        ) || !mmay_bytecode_program_take_function(
            program, leaf_symbol, &leaf, &function_id
        )) {
        goto failure;
    }
    span = 920;
    if (!mmay_bytecode_function_init(
            &entry, mmay_system_allocator(), MMAY_BYTECODE_TYPE_ARRAY_F64,
            1, NULL, 0, mmay_bytecode_function_default_options()
        ) || !mmay_test_vm_add_registers(
            &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
        ) || !mmay_bytecode_add_block(&entry, &block)) {
        goto failure;
    }
    array_argument.register_id = 0;
    if (!mmay_bytecode_add_array_construction(
            &entry, MMAY_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
            &construction
        ) || !mmay_bytecode_add_call(
            &entry, leaf_symbol, MMAY_BYTECODE_TYPE_ARRAY_F64, NULL, 0, &call
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_CONST_F64, 0,
            UINT64_C(0x3ff8000000000000), 0, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_ARRAY_NEW, 1, construction, 0, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_CALL_DIRECT, 2, call, 0, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_ARRAY_NEW, 5, construction, 0, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_CONST_I64, 3, 0, 0, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_ARRAY_GET, 4, 2, 3, &span
        ) || !mmay_test_vm_append(
            &entry, block, MMAY_OPCODE_RETURN, 2, 0, 0, &span
        ) || !mmay_bytecode_program_take_function(
            program, entry_symbol, &entry, &function_id
        ) || !mmay_bytecode_program_set_entry(program, entry_symbol)) {
        goto failure;
    }
    return true;

failure:
    mmay_bytecode_function_dispose(&entry);
    mmay_bytecode_function_dispose(&leaf);
    mmay_bytecode_program_dispose(program);
    return false;
}

void mmay_test_vm_managed_array_calls(void) {
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeValue result;
    MarimayBytecodeRunOptions run_options;
    MarimayManagedRootFrame host_root;
    MarimayManagedReference *host_locations[1];
    MarimayManagedCell cell;
    size_t length = 0;
    MarimayBytecodeExecutionOptions execution_options =
        mmay_bytecode_execution_default_options();
    MarimayOwnedBytes encoded;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&execution, 0, sizeof(execution));
    memset(&encoded, 0, sizeof(encoded));
    memset(&host_root, 0, sizeof(host_root));
    memset(&cell, 0, sizeof(cell));
    execution_options.managed_heap.collect_before_allocation = true;
    MMAY_EXPECT(mmay_test_vm_build_array_call_program(&program));
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    program.options.max_total_array_constructions = 1;
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_total_array_constructions = 1000000;
    program.options.max_total_array_arguments = 1;
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_LIMIT_REACHED);
    program.options.max_total_array_arguments = 10000000;
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_program(
        &program, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    {
        MarimayBytecodeProgramOptions limited =
            mmay_bytecode_program_default_options();
        MarimayFailAllocator tracking;
        limited.max_total_array_constructions = 1;
        mmay_fail_allocator_init(
            &tracking, mmay_system_allocator(), SIZE_MAX
        );
        MMAY_EXPECT(mmay_bytecode_decode_program(
            encoded.data, encoded.length, mmay_fail_allocator(&tracking),
            limited, &decoded
        ) == MMAY_BYTECODE_CODEC_LIMIT_REACHED);
        MMAY_EXPECT(!decoded.initialized);
        mmay_bytecode_program_dispose(&decoded);
        MMAY_EXPECT(tracking.live_allocations == 0);
        MMAY_EXPECT(tracking.live_bytes == 0);
    }
    {
        MarimayBytecodeProgramOptions limited =
            mmay_bytecode_program_default_options();
        MarimayFailAllocator tracking;
        limited.max_total_array_arguments = 1;
        mmay_fail_allocator_init(
            &tracking, mmay_system_allocator(), SIZE_MAX
        );
        MMAY_EXPECT(mmay_bytecode_decode_program(
            encoded.data, encoded.length, mmay_fail_allocator(&tracking),
            limited, &decoded
        ) == MMAY_BYTECODE_CODEC_LIMIT_REACHED);
        MMAY_EXPECT(!decoded.initialized);
        mmay_bytecode_program_dispose(&decoded);
        MMAY_EXPECT(tracking.live_allocations == 0);
        MMAY_EXPECT(tracking.live_bytes == 0);
    }
    MMAY_EXPECT(mmay_bytecode_decode_program(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_program_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(), execution_options
    ));
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_bytecode_run_program(
        &decoded, NULL, 0, mmay_system_allocator(), run_options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_ARRAY_F64);
    MMAY_EXPECT(mmay_managed_array_length(
        &execution.managed_heap, result.as.managed_reference, &length
    ) == MMAY_MANAGED_OK && length == 1);
    host_locations[0] = &result.as.managed_reference;
    MMAY_EXPECT(mmay_managed_root_push(
        &execution.managed_heap, &host_root, host_locations, 1
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&execution.managed_heap)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_get(
        &execution.managed_heap, result.as.managed_reference, 0, &cell, &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(cell.f64_bits == UINT64_C(0x4008000000000000));
    MMAY_EXPECT(mmay_managed_root_pop(
        &execution.managed_heap, &host_root
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_heap_collect(&execution.managed_heap)
        == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(
        &execution.managed_heap, result.as.managed_reference, &length
    ) == MMAY_MANAGED_INVALID_REFERENCE);
    MMAY_EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
}

void mmay_test_vm_managed_array_provenance(void) {
    const MarimayBytecodeType parameter = MMAY_BYTECODE_TYPE_ARRAY_F64;
    MarimayBytecodeFunction function;
    MarimayBytecodeExecution first;
    MarimayBytecodeExecution second;
    MarimayBytecodeValue argument;
    MarimayBytecodeValue result;
    MarimayBytecodeRunOptions options;
    MarimayManagedCell cell;
    MarimayManagedReference first_reference;
    MarimayManagedReference second_reference;
    MarimayManagedReference wrong_descriptor_reference;
    MarimayManagedReference stale_reference;
    MarimayManagedCell nested_cell;
    uint32_t length_register = UINT32_MAX;
    uint32_t block = UINT32_MAX;
    uint64_t span = 950;
    memset(&function, 0, sizeof(function));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    memset(&cell, 0, sizeof(cell));
    memset(&nested_cell, 0, sizeof(nested_cell));
    cell.f64_bits = UINT64_C(0x3ff0000000000000);
    MMAY_EXPECT(mmay_bytecode_function_init(
        &function, mmay_system_allocator(), MMAY_BYTECODE_TYPE_I64,
        1, &parameter, 1, mmay_bytecode_function_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_add_register(
        &function, MMAY_BYTECODE_TYPE_I64, &length_register
    ));
    MMAY_EXPECT(mmay_bytecode_add_block(&function, &block));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, block, MMAY_OPCODE_ARRAY_LENGTH, length_register, 0, 0,
        &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &function, block, MMAY_OPCODE_RETURN, length_register, 0, 0, &span
    ));
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &first, mmay_system_allocator(), mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &second, mmay_system_allocator(), mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &first.managed_heap, &mmay_managed_array_f64_descriptor,
        &cell, 1, &first_reference
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_new(
        &second.managed_heap, &mmay_managed_array_f64_descriptor,
        &cell, 1, &second_reference
    ) == MMAY_MANAGED_OK);
    nested_cell.reference = second_reference;
    MMAY_EXPECT(mmay_managed_array_new(
        &second.managed_heap, &mmay_managed_array_array_f64_descriptor,
        &nested_cell, 1, &wrong_descriptor_reference
    ) == MMAY_MANAGED_OK);
    argument.type = MMAY_BYTECODE_TYPE_ARRAY_F64;
    argument.as.managed_reference = first_reference;
    options = mmay_bytecode_run_default_options();
    options.execution = &second;
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(mmay_bytecode_run(
        &function, &argument, 1, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    argument.as.managed_reference = wrong_descriptor_reference;
    MMAY_EXPECT(mmay_bytecode_run(
        &function, &argument, 1, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    stale_reference = second_reference;
    mmay_bytecode_execution_dispose(&second);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &second, mmay_system_allocator(), mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_managed_array_new(
        &second.managed_heap, &mmay_managed_array_f64_descriptor,
        &cell, 1, &second_reference
    ) == MMAY_MANAGED_OK);
    argument.as.managed_reference = stale_reference;
    options.execution = &second;
    MMAY_EXPECT(mmay_bytecode_run(
        &function, &argument, 1, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_INVALID_REFERENCE);
    mmay_bytecode_execution_dispose(&second);
    mmay_bytecode_execution_dispose(&first);
    mmay_bytecode_function_dispose(&function);
}

void mmay_test_vm_managed_array_panic_frames(void) {
    const MarimaySymbolId leaf_symbol = {{0x30}};
    const MarimaySymbolId entry_symbol = {{0x40}};
    static const MarimayBytecodeType leaf_types[] = {
        MMAY_BYTECODE_TYPE_F64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64,
        MMAY_BYTECODE_TYPE_ARRAY_F64,
        MMAY_BYTECODE_TYPE_I64
    };
    static const MarimayBytecodeType entry_types[] = {
        MMAY_BYTECODE_TYPE_I64
    };
    MarimayBytecodeFunction leaf;
    MarimayBytecodeFunction entry;
    MarimayBytecodeProgram program;
    MarimayBytecodeExecution execution;
    MarimayBytecodePanic panic;
    MarimayBytecodeValue result;
    MarimayBytecodeRunOptions options;
    MarimayBytecodeArrayArgument array_argument;
    MarimayBytecodeArrayConstructionId construction;
    MarimayBytecodeCallId call;
    const MarimayBytecodePanicFrame *frame;
    uint32_t block;
    uint32_t function_id;
    uint64_t span = 980;
    memset(&leaf, 0, sizeof(leaf));
    memset(&entry, 0, sizeof(entry));
    memset(&program, 0, sizeof(program));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    MMAY_EXPECT(mmay_bytecode_program_init(
        &program, mmay_system_allocator(), 1,
        mmay_bytecode_program_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_function_init(
        &leaf, mmay_system_allocator(), MMAY_BYTECODE_TYPE_I64,
        1, NULL, 0, mmay_bytecode_function_default_options()
    ));
    MMAY_EXPECT(mmay_test_vm_add_registers(
        &leaf, leaf_types, sizeof(leaf_types) / sizeof(leaf_types[0])
    ));
    MMAY_EXPECT(mmay_bytecode_add_block(&leaf, &block));
    array_argument.register_id = 0;
    array_argument.type = MMAY_BYTECODE_TYPE_F64;
    MMAY_EXPECT(mmay_bytecode_add_array_construction(
        &leaf, MMAY_BYTECODE_TYPE_ARRAY_F64, &array_argument, 1,
        &construction
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &leaf, block, MMAY_OPCODE_CONST_F64, 0,
        UINT64_C(0x3ff0000000000000), 0, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &leaf, block, MMAY_OPCODE_ARRAY_NEW, 1, construction, 0, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &leaf, block, MMAY_OPCODE_CONST_I64, 2, (uint64_t)-1, 0, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &leaf, block, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 3, 1, 2, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &leaf, block, MMAY_OPCODE_ARRAY_LENGTH, 4, 3, 0, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &leaf, block, MMAY_OPCODE_RETURN, 4, 0, 0, &span
    ));
    MMAY_EXPECT(mmay_bytecode_program_take_function(
        &program, leaf_symbol, &leaf, &function_id
    ));
    span = 1000;
    MMAY_EXPECT(mmay_bytecode_function_init(
        &entry, mmay_system_allocator(), MMAY_BYTECODE_TYPE_I64,
        1, NULL, 0, mmay_bytecode_function_default_options()
    ));
    MMAY_EXPECT(mmay_test_vm_add_registers(
        &entry, entry_types, sizeof(entry_types) / sizeof(entry_types[0])
    ));
    MMAY_EXPECT(mmay_bytecode_add_call(
        &entry, leaf_symbol, MMAY_BYTECODE_TYPE_I64, NULL, 0, &call
    ));
    MMAY_EXPECT(mmay_bytecode_add_block(&entry, &block));
    MMAY_EXPECT(mmay_test_vm_append(
        &entry, block, MMAY_OPCODE_CALL_DIRECT, 0, call, 0, &span
    ));
    MMAY_EXPECT(mmay_test_vm_append(
        &entry, block, MMAY_OPCODE_RETURN, 0, 0, 0, &span
    ));
    MMAY_EXPECT(mmay_bytecode_program_take_function(
        &program, entry_symbol, &entry, &function_id
    ));
    MMAY_EXPECT(mmay_bytecode_program_set_entry(&program, entry_symbol));
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(), mmay_bytecode_execution_default_options()
    ));
    MMAY_EXPECT(mmay_bytecode_panic_init(&panic, mmay_system_allocator()));
    options = mmay_bytecode_run_default_options();
    options.execution = &execution;
    options.panic = &panic;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &program, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4002") == 0);
    MMAY_EXPECT(panic.message.length
        == strlen("array index -1 is outside length 1"));
    MMAY_EXPECT(memcmp(
        panic.message.data, "array index -1 is outside length 1",
        panic.message.length
    ) == 0);
    MMAY_EXPECT(memcmp(&panic.symbol, &leaf_symbol, sizeof(leaf_symbol)) == 0
        && panic.span.start == 983 && panic.span.end == 984
        && panic.frames.length == 2);
    frame = mmay_bytecode_panic_frame(&panic, 1);
    MMAY_EXPECT(frame != NULL
        && memcmp(&frame->symbol, &entry_symbol, sizeof(entry_symbol)) == 0
        && frame->call_span.start == 1000 && frame->call_span.end == 1001);
    MMAY_EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&program);
}
