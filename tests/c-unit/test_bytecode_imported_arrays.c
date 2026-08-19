#include "test_imported_arrays_support.h"

#include "ruda/bytecode/vm.h"

static void ruda_test_bytecode_imported_array_tables(
    RudaBytecodeProgram *program
) {
    size_t function_index = 0;
    bool saw_record = false;
    bool saw_enum = false;
    bool saw_string = false;
    bool saw_nested = false;
    bool corrupted = false;
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        RudaBytecodeProgramFunction *program_function = ruda_vector_at(
            &program->functions, function_index
        );
        RudaBytecodeFunction *function = program_function != NULL
            ? &program_function->body
            : NULL;
        size_t layout_index = 0;
        if (function == NULL) continue;
        for (layout_index = 0; layout_index < function->array_types.length;
            layout_index += 1) {
            RudaBytecodeArrayType *layout = ruda_vector_at(
                &function->array_types, layout_index
            );
            if (layout == NULL) continue;
            if (layout->element_type == RUDA_BYTECODE_TYPE_RECORD) {
                uint32_t saved = layout->element_layout;
                saw_record = true;
                RUDA_EXPECT(saved != RUDA_BYTECODE_NO_LAYOUT);
                RUDA_EXPECT((size_t)saved < function->record_layouts.length);
                if (!corrupted) {
                    layout->element_layout = RUDA_BYTECODE_NO_LAYOUT;
                    RUDA_EXPECT(ruda_bytecode_program_verify(program)
                        == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
                    layout->element_layout = UINT32_MAX - 1;
                    RUDA_EXPECT(ruda_bytecode_program_verify(program)
                        == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
                    layout->element_layout = saved;
                    corrupted = true;
                }
            } else if (layout->element_type == RUDA_BYTECODE_TYPE_ENUM) {
                saw_enum = true;
                RUDA_EXPECT(layout->element_layout
                    != RUDA_BYTECODE_NO_LAYOUT);
                RUDA_EXPECT((size_t)layout->element_layout
                    < function->enum_layouts.length);
            } else if (layout->element_type
                    == RUDA_BYTECODE_TYPE_STRING) {
                saw_string = true;
                RUDA_EXPECT(layout->element_layout
                    == RUDA_BYTECODE_NO_LAYOUT);
            } else if (layout->element_type
                    == RUDA_BYTECODE_TYPE_ARRAY) {
                saw_nested = true;
                RUDA_EXPECT(layout->element_layout
                    != RUDA_BYTECODE_NO_LAYOUT);
                RUDA_EXPECT((size_t)layout->element_layout < layout_index);
            }
        }
    }
    RUDA_EXPECT(saw_record);
    RUDA_EXPECT(saw_enum);
    RUDA_EXPECT(saw_string);
    RUDA_EXPECT(saw_nested);
    RUDA_EXPECT(corrupted);
    RUDA_EXPECT(ruda_bytecode_program_verify(program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
}

static void ruda_test_bytecode_imported_array_execute(
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
    if (!execution.initialized) return;
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run_program(
        program, NULL, 0, ruda_system_allocator(), run_options, &value
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(value.type == RUDA_BYTECODE_TYPE_I64);
    RUDA_EXPECT(value.as.i64 == 51);
    ruda_bytecode_execution_dispose(&execution);
}

static void ruda_test_bytecode_imported_array_lower_allocations(
    const RudaTestImportedArrays *test
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
        status = ruda_test_imported_arrays_lower(
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
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_test_bytecode_imported_array_codec_allocations(
    const RudaBytecodeProgram *program,
    const RudaOwnedBytes *encoded
) {
    size_t failure_index = 0;
    bool reached_encode = false;
    bool reached_decode = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes bytes;
        RudaBytecodeCodecStatus status;
        memset(&bytes, 0, sizeof(bytes));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_bytecode_encode_program(
            program, ruda_fail_allocator(&state), &bytes
        );
        if (status == RUDA_BYTECODE_CODEC_OK) reached_encode = true;
        else RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
        ruda_owned_bytes_dispose(&bytes);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_encode) break;
    }
    RUDA_EXPECT(reached_encode);
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
            reached_decode = true;
            ruda_bytecode_program_dispose(&decoded);
        } else {
            RUDA_EXPECT(status == RUDA_BYTECODE_CODEC_ALLOCATION_FAILED);
            RUDA_EXPECT(!decoded.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_decode) break;
    }
    RUDA_EXPECT(reached_decode);
}

static void ruda_test_bytecode_imported_array_vm_allocations(
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
                &execution, ruda_fail_allocator(&state), execution_options
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
                RUDA_EXPECT(value.as.i64 == 51);
            } else {
                RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            ruda_bytecode_execution_dispose(&execution);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_bytecode_imported_arrays(void) {
    RudaTestImportedArrays test;
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaBytecodeProgram corrupt_output;
    RudaOwnedBytes encoded;
    unsigned char *corrupt = NULL;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&corrupt_output, 0, sizeof(corrupt_output));
    memset(&encoded, 0, sizeof(encoded));
    RUDA_EXPECT(ruda_test_imported_arrays_init(
        &test, ruda_system_allocator()
    ));
    if (!test.initialized) return;
    RUDA_EXPECT(ruda_test_imported_arrays_lower(
        &test, ruda_system_allocator(), &program
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    if (!program.initialized) goto cleanup;
    RUDA_EXPECT(ruda_bytecode_program_verify(&program)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    ruda_test_bytecode_imported_array_tables(&program);
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
        ruda_test_bytecode_imported_array_execute(&decoded);
    }
    {
        RudaAllocator allocator = ruda_system_allocator();
        corrupt = ruda_allocate(&allocator, encoded.length);
    }
    RUDA_EXPECT(corrupt != NULL);
    if (corrupt != NULL && encoded.length != 0) {
        memcpy(corrupt, encoded.data, encoded.length);
        corrupt[encoded.length - 1] ^= 0x01U;
        RUDA_EXPECT(ruda_bytecode_decode_program(
            corrupt,
            encoded.length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &corrupt_output
        ) != RUDA_BYTECODE_CODEC_OK);
        RUDA_EXPECT(!corrupt_output.initialized);
    }
    ruda_test_bytecode_imported_array_lower_allocations(&test);
    ruda_test_bytecode_imported_array_codec_allocations(&program, &encoded);
    ruda_test_bytecode_imported_array_vm_allocations(&program);
cleanup:
    {
        RudaAllocator allocator = ruda_system_allocator();
        ruda_deallocate(&allocator, corrupt, encoded.length);
    }
    ruda_owned_bytes_dispose(&encoded);
    if (decoded.initialized) ruda_bytecode_program_dispose(&decoded);
    if (program.initialized) ruda_bytecode_program_dispose(&program);
    ruda_test_imported_arrays_dispose(&test);
}
