#include "test_imported_results_support.h"

#include "mmay/bytecode/vm.h"

static bool mmay_test_bytecode_imported_result_choice_symbol(
    const MarimayTestImportedResults *test,
    MarimaySymbolId *symbol
) {
    size_t index = 0;
    if (test == NULL || symbol == NULL) {
        return false;
    }
    for (index = 0; index < test->checked.enums.length; index += 1) {
        const MarimayCheckedEnum *enum_value = mmay_checked_enum(
            &test->checked, index
        );
        if (enum_value != NULL && enum_value->name.length == 6
            && memcmp(enum_value->name.data, "Choice", 6) == 0) {
            *symbol = enum_value->symbol;
            return true;
        }
    }
    return false;
}

static void mmay_test_bytecode_imported_result_provenance(
    const MarimayTestImportedResults *test,
    MarimayBytecodeProgram *program
) {
    MarimayBytecodeEnumLayout *candidate = NULL;
    MarimaySymbolId imported_symbol;
    size_t function_index = 0;
    memset(&imported_symbol, 0, sizeof(imported_symbol));
    MMAY_EXPECT(mmay_test_bytecode_imported_result_choice_symbol(
        test, &imported_symbol
    ));
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        MarimayBytecodeProgramFunction *function = mmay_vector_at(
            &program->functions, function_index
        );
        const MarimayMirFunction *mir = function_index
                < test->checked.functions.length
            ? test->functions[function_index]
            : NULL;
        size_t enum_index = 0;
        if (function == NULL || mir == NULL) {
            continue;
        }
        for (enum_index = 0; enum_index < mir->enums.length;
            enum_index += 1) {
            const MarimayMirEnum *mir_layout = mmay_vector_at_const(
                &mir->enums, enum_index
            );
            if (mir_layout != NULL && mmay_symbol_id_equal(
                    mir_layout->symbol, imported_symbol
                )) {
                candidate = mmay_vector_at(
                    &function->body.enum_layouts, enum_index
                );
                break;
            }
        }
        if (candidate != NULL) {
            break;
        }
    }
    MMAY_EXPECT(candidate != NULL);
    if (candidate != NULL) {
        MarimaySymbolId saved = candidate->symbol;
        MMAY_EXPECT(candidate->variant_count == 2);
        MMAY_EXPECT(candidate->payload_count == 1);
        candidate->symbol.bytes[0] ^= UINT8_C(0x80);
        MMAY_EXPECT(mmay_bytecode_program_verify(program)
            == MMAY_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
        candidate->symbol = saved;
    }
    MMAY_EXPECT(mmay_bytecode_program_verify(program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static void mmay_test_bytecode_imported_result_execute(
    const MarimayBytecodeProgram *program
) {
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options =
        mmay_bytecode_execution_default_options();
    MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue value;
    memset(&execution, 0, sizeof(execution));
    memset(&value, 0, sizeof(value));
    execution_options.managed_heap.collect_before_allocation = true;
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(), execution_options
    ));
    if (!execution.initialized) {
        return;
    }
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        program, NULL, 0, mmay_system_allocator(), run_options, &value
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(value.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(value.as.i64 == 119);
    mmay_bytecode_execution_dispose(&execution);
}

static void mmay_test_bytecode_imported_result_lower_allocations(
    const MarimayTestImportedResults *test
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeProgram program;
        MarimayMirBytecodeLowerStatus status;
        memset(&program, 0, sizeof(program));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_test_imported_results_lower(
            test, mmay_fail_allocator(&state), &program
        );
        if (status == MMAY_MIR_BYTECODE_LOWER_OK) {
            reached_success = true;
            MMAY_EXPECT(program.initialized);
            mmay_bytecode_program_dispose(&program);
        } else {
            MMAY_EXPECT(status == MMAY_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
            MMAY_EXPECT(!program.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_bytecode_imported_result_encode_allocations(
    const MarimayBytecodeProgram *program
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes encoded;
        MarimayBytecodeCodecStatus status;
        memset(&encoded, 0, sizeof(encoded));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_bytecode_encode_program(
            program, mmay_fail_allocator(&state), &encoded
        );
        if (status == MMAY_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
        }
        mmay_owned_bytes_dispose(&encoded);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_bytecode_imported_result_decode_allocations(
    const MarimayOwnedBytes *encoded
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeProgram decoded;
        MarimayBytecodeCodecStatus status;
        memset(&decoded, 0, sizeof(decoded));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_bytecode_decode_program(
            encoded->data,
            encoded->length,
            mmay_fail_allocator(&state),
            mmay_bytecode_program_default_options(),
            &decoded
        );
        if (status == MMAY_BYTECODE_CODEC_OK) {
            reached_success = true;
            mmay_bytecode_program_dispose(&decoded);
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            MMAY_EXPECT(!decoded.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_bytecode_imported_result_vm_allocations(
    const MarimayBytecodeProgram *program
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution execution;
        MarimayBytecodeExecutionOptions execution_options =
            mmay_bytecode_execution_default_options();
        MarimayBytecodeRunOptions run_options =
            mmay_bytecode_run_default_options();
        MarimayBytecodeValue value;
        MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&value, 0, sizeof(value));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        execution_options.managed_heap.collect_before_allocation = true;
        if (mmay_bytecode_execution_init(
                &execution,
                mmay_fail_allocator(&state),
                execution_options
            )) {
            run_options.execution = &execution;
            status = mmay_bytecode_run_program(
                program,
                NULL,
                0,
                mmay_fail_allocator(&state),
                run_options,
                &value
            );
            if (status == MMAY_BYTECODE_RUN_OK) {
                reached_success = true;
                MMAY_EXPECT(value.type == MMAY_BYTECODE_TYPE_I64);
                MMAY_EXPECT(value.as.i64 == 119);
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            mmay_bytecode_execution_dispose(&execution);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_bytecode_imported_results(void) {
    MarimayTestImportedResults test;
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayBytecodeProgram corrupt_output;
    MarimayOwnedBytes encoded;
    unsigned char *corrupt = NULL;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&corrupt_output, 0, sizeof(corrupt_output));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_imported_results_init(
        &test, mmay_system_allocator()
    ));
    if (!test.initialized) {
        return;
    }
    MMAY_EXPECT(mmay_test_imported_results_lower(
        &test, mmay_system_allocator(), &program
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    if (!program.initialized) {
        goto cleanup;
    }
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    mmay_test_bytecode_imported_result_provenance(
        &test, &program
    );
    MMAY_EXPECT(mmay_bytecode_encode_program(
        &program, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(
        encoded.data,
        encoded.length,
        mmay_system_allocator(),
        mmay_bytecode_program_default_options(),
        &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    if (decoded.initialized) {
        MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
            == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
        mmay_test_bytecode_imported_result_execute(&decoded);
    }
    {
        MarimayAllocator allocator = mmay_system_allocator();
        corrupt = mmay_allocate(&allocator, encoded.length);
    }
    MMAY_EXPECT(corrupt != NULL);
    if (corrupt != NULL && encoded.length != 0) {
        memcpy(corrupt, encoded.data, encoded.length);
        corrupt[encoded.length - 1] ^= UINT8_C(0x01);
        MMAY_EXPECT(mmay_bytecode_decode_program(
            corrupt,
            encoded.length,
            mmay_system_allocator(),
            mmay_bytecode_program_default_options(),
            &corrupt_output
        ) != MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(!corrupt_output.initialized);
    }
    mmay_test_bytecode_imported_result_lower_allocations(&test);
    mmay_test_bytecode_imported_result_encode_allocations(&program);
    mmay_test_bytecode_imported_result_decode_allocations(&encoded);
    mmay_test_bytecode_imported_result_vm_allocations(&program);

cleanup:
    {
        MarimayAllocator allocator = mmay_system_allocator();
        mmay_deallocate(&allocator, corrupt, encoded.length);
    }
    mmay_owned_bytes_dispose(&encoded);
    if (decoded.initialized) {
        mmay_bytecode_program_dispose(&decoded);
    }
    if (program.initialized) {
        mmay_bytecode_program_dispose(&program);
    }
    mmay_test_imported_results_dispose(&test);
}
