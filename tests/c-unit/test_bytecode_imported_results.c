#include "test_imported_results_support.h"

#include "ruda/bytecode/vm.h"

static bool ruda_test_bytecode_imported_result_choice_symbol(
    const RudaTestImportedResults *test,
    RudaSymbolId *symbol
) {
    size_t index = 0;
    if (test == NULL || symbol == NULL) {
        return false;
    }
    for (index = 0; index < test->checked.enums.length; index += 1) {
        const RudaCheckedEnum *enum_value = ruda_checked_enum(
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

static void ruda_test_bytecode_imported_result_provenance(
    const RudaTestImportedResults *test,
    RudaBytecodeProgram *program
) {
    RudaBytecodeEnumLayout *candidate = NULL;
    RudaSymbolId imported_symbol;
    size_t function_index = 0;
    memset(&imported_symbol, 0, sizeof(imported_symbol));
    RUDA_EXPECT(ruda_test_bytecode_imported_result_choice_symbol(
        test, &imported_symbol
    ));
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        RudaBytecodeProgramFunction *function = ruda_vector_at(
            &program->functions, function_index
        );
        const RudaMirFunction *mir = function_index
                < test->checked.functions.length
            ? test->functions[function_index]
            : NULL;
        size_t enum_index = 0;
        if (function == NULL || mir == NULL) {
            continue;
        }
        for (enum_index = 0; enum_index < mir->enums.length;
            enum_index += 1) {
            const RudaMirEnum *mir_layout = ruda_vector_at_const(
                &mir->enums, enum_index
            );
            if (mir_layout != NULL && ruda_symbol_id_equal(
                    mir_layout->symbol, imported_symbol
                )) {
                candidate = ruda_vector_at(
                    &function->body.enum_layouts, enum_index
                );
                break;
            }
        }
        if (candidate != NULL) {
            break;
        }
    }
    RUDA_EXPECT(candidate != NULL);
    if (candidate != NULL) {
        RudaSymbolId saved = candidate->symbol;
        RUDA_EXPECT(candidate->variant_count == 2);
        RUDA_EXPECT(candidate->payload_count == 1);
        candidate->symbol.bytes[0] ^= UINT8_C(0x80);
        RUDA_EXPECT(ruda_bytecode_program_verify(program)
            == RUDA_BYTECODE_PROGRAM_VERIFY_SIGNATURE_MISMATCH);
        candidate->symbol = saved;
    }
    RUDA_EXPECT(ruda_bytecode_program_verify(program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
}

static void ruda_test_bytecode_imported_result_execute(
    const RudaBytecodeProgram *program
) {
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options =
        ruda_bytecode_execution_default_options();
    RudaBytecodeRunOptions run_options = ruda_bytecode_run_default_options();
    RudaBytecodeValue value;
    memset(&execution, 0, sizeof(execution));
    memset(&value, 0, sizeof(value));
    execution_options.managed_heap.collect_before_allocation = true;
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(), execution_options
    ));
    if (!execution.initialized) {
        return;
    }
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run_program(
        program, NULL, 0, ruda_system_allocator(), run_options, &value
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(value.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(value.as.i64 == 119);
    ruda_bytecode_execution_dispose(&execution);
}

static void ruda_test_bytecode_imported_result_lower_allocations(
    const RudaTestImportedResults *test
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeProgram program;
        RudaMirBytecodeLowerStatus status;
        memset(&program, 0, sizeof(program));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_test_imported_results_lower(
            test, ruda_fail_allocator(&state), &program
        );
        if (status == RUDA_MIR_BYTECODE_LOWER_OK) {
            reached_success = true;
            RUDA_EXPECT(program.initialized);
            ruda_bytecode_program_dispose(&program);
        } else {
            RUDA_EXPECT(status == RUDA_MIR_BYTECODE_LOWER_ALLOCATION_FAILED);
            RUDA_EXPECT(!program.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_bytecode_imported_result_encode_allocations(
    const RudaBytecodeProgram *program
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes encoded;
        RudaBytecodeCodecStatus status;
        memset(&encoded, 0, sizeof(encoded));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_bytecode_encode_program(
            program, ruda_fail_allocator(&state), &encoded
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
        }
        ruda_owned_bytes_dispose(&encoded);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_bytecode_imported_result_decode_allocations(
    const RudaOwnedBytes *encoded
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeProgram decoded;
        RudaBytecodeCodecStatus status;
        memset(&decoded, 0, sizeof(decoded));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_bytecode_decode_program(
            encoded->data,
            encoded->length,
            ruda_fail_allocator(&state),
            ruda_bytecode_program_default_options(),
            &decoded
        );
        if (status == RUDA_BYTECODE_CODEC_OK) {
            reached_success = true;
            ruda_bytecode_program_dispose(&decoded);
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(!decoded.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_bytecode_imported_result_vm_allocations(
    const RudaBytecodeProgram *program
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeExecution execution;
        RudaBytecodeExecutionOptions execution_options =
            ruda_bytecode_execution_default_options();
        RudaBytecodeRunOptions run_options =
            ruda_bytecode_run_default_options();
        RudaBytecodeValue value;
        RudaBytecodeRunStatus status = RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&value, 0, sizeof(value));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        execution_options.managed_heap.collect_before_allocation = true;
        if (ruda_bytecode_execution_init(
                &execution,
                ruda_fail_allocator(&state),
                execution_options
            )) {
            run_options.execution = &execution;
            status = ruda_bytecode_run_program(
                program,
                NULL,
                0,
                ruda_fail_allocator(&state),
                run_options,
                &value
            );
            if (status == RUDA_BYTECODE_RUN_OK) {
                reached_success = true;
                RUDA_EXPECT(value.type == RUDA_BYTECODE_TYPE_I64);
                RUDA_EXPECT(value.as.i64 == 119);
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            ruda_bytecode_execution_dispose(&execution);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_bytecode_imported_results(void) {
    RudaTestImportedResults test;
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaBytecodeProgram corrupt_output;
    RudaOwnedBytes encoded;
    unsigned char *corrupt = NULL;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&corrupt_output, 0, sizeof(corrupt_output));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_imported_results_init(
        &test, ruda_system_allocator()
    ));
    if (!test.initialized) {
        return;
    }
    RUDA_EXPECT(ruda_test_imported_results_lower(
        &test, ruda_system_allocator(), &program
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    if (!program.initialized) {
        goto cleanup;
    }
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    ruda_test_bytecode_imported_result_provenance(
        &test, &program
    );
    RUDA_EXPECT(ruda_bytecode_encode_program(
        &program, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_program(
        encoded.data,
        encoded.length,
        ruda_system_allocator(),
        ruda_bytecode_program_default_options(),
        &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    if (decoded.initialized) {
        RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
            == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
        ruda_test_bytecode_imported_result_execute(&decoded);
    }
    {
        RudaAllocator allocator = ruda_system_allocator();
        corrupt = ruda_allocate(&allocator, encoded.length);
    }
    RUDA_EXPECT(corrupt != NULL);
    if (corrupt != NULL && encoded.length != 0) {
        memcpy(corrupt, encoded.data, encoded.length);
        corrupt[encoded.length - 1] ^= UINT8_C(0x01);
        RUDA_EXPECT(ruda_bytecode_decode_program(
            corrupt,
            encoded.length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &corrupt_output
        ) != RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(!corrupt_output.initialized);
    }
    ruda_test_bytecode_imported_result_lower_allocations(&test);
    ruda_test_bytecode_imported_result_encode_allocations(&program);
    ruda_test_bytecode_imported_result_decode_allocations(&encoded);
    ruda_test_bytecode_imported_result_vm_allocations(&program);

cleanup:
    {
        RudaAllocator allocator = ruda_system_allocator();
        ruda_deallocate(&allocator, corrupt, encoded.length);
    }
    ruda_owned_bytes_dispose(&encoded);
    if (decoded.initialized) {
        ruda_bytecode_program_dispose(&decoded);
    }
    if (program.initialized) {
        ruda_bytecode_program_dispose(&program);
    }
    ruda_test_imported_results_dispose(&test);
}
