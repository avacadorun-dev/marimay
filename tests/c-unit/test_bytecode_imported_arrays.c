#include "test_imported_arrays_support.h"

#include "mmay/bytecode/vm.h"

static void mmay_test_bytecode_imported_array_tables(
    MarimayBytecodeProgram *program
) {
    size_t function_index = 0;
    bool saw_record = false;
    bool saw_enum = false;
    bool saw_string = false;
    bool saw_nested = false;
    bool corrupted = false;
    for (function_index = 0; function_index < program->functions.length;
        function_index += 1) {
        MarimayBytecodeProgramFunction *program_function = mmay_vector_at(
            &program->functions, function_index
        );
        MarimayBytecodeFunction *function = program_function != NULL
            ? &program_function->body
            : NULL;
        size_t layout_index = 0;
        if (function == NULL) continue;
        for (layout_index = 0; layout_index < function->array_types.length;
            layout_index += 1) {
            MarimayBytecodeArrayType *layout = mmay_vector_at(
                &function->array_types, layout_index
            );
            if (layout == NULL) continue;
            if (layout->element_type == MMAY_BYTECODE_TYPE_RECORD) {
                uint32_t saved = layout->element_layout;
                saw_record = true;
                MMAY_EXPECT(saved != MMAY_BYTECODE_NO_LAYOUT);
                MMAY_EXPECT((size_t)saved < function->record_layouts.length);
                if (!corrupted) {
                    layout->element_layout = MMAY_BYTECODE_NO_LAYOUT;
                    MMAY_EXPECT(mmay_bytecode_program_verify(program)
                        == MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
                    layout->element_layout = UINT32_MAX - 1;
                    MMAY_EXPECT(mmay_bytecode_program_verify(program)
                        == MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
                    layout->element_layout = saved;
                    corrupted = true;
                }
            } else if (layout->element_type == MMAY_BYTECODE_TYPE_ENUM) {
                saw_enum = true;
                MMAY_EXPECT(layout->element_layout
                    != MMAY_BYTECODE_NO_LAYOUT);
                MMAY_EXPECT((size_t)layout->element_layout
                    < function->enum_layouts.length);
            } else if (layout->element_type
                    == MMAY_BYTECODE_TYPE_STRING) {
                saw_string = true;
                MMAY_EXPECT(layout->element_layout
                    == MMAY_BYTECODE_NO_LAYOUT);
            } else if (layout->element_type
                    == MMAY_BYTECODE_TYPE_ARRAY) {
                saw_nested = true;
                MMAY_EXPECT(layout->element_layout
                    != MMAY_BYTECODE_NO_LAYOUT);
                MMAY_EXPECT((size_t)layout->element_layout < layout_index);
            }
        }
    }
    MMAY_EXPECT(saw_record);
    MMAY_EXPECT(saw_enum);
    MMAY_EXPECT(saw_string);
    MMAY_EXPECT(saw_nested);
    MMAY_EXPECT(corrupted);
    MMAY_EXPECT(mmay_bytecode_program_verify(program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static void mmay_test_bytecode_imported_array_execute(
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
    if (!execution.initialized) return;
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        program, NULL, 0, mmay_system_allocator(), run_options, &value
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(value.type == MMAY_BYTECODE_TYPE_I64);
    MMAY_EXPECT(value.as.i64 == 51);
    mmay_bytecode_execution_dispose(&execution);
}

static void mmay_test_bytecode_imported_array_lower_allocations(
    const MarimayTestImportedArrays *test
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
        status = mmay_test_imported_arrays_lower(
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
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_test_bytecode_imported_array_codec_allocations(
    const MarimayBytecodeProgram *program,
    const MarimayOwnedBytes *encoded
) {
    size_t failure_index = 0;
    bool reached_encode = false;
    bool reached_decode = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes bytes;
        MarimayBytecodeCodecStatus status;
        memset(&bytes, 0, sizeof(bytes));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_bytecode_encode_program(
            program, mmay_fail_allocator(&state), &bytes
        );
        if (status == MMAY_BYTECODE_CODEC_OK) reached_encode = true;
        else MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
        mmay_owned_bytes_dispose(&bytes);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_encode) break;
    }
    MMAY_EXPECT(reached_encode);
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
            reached_decode = true;
            mmay_bytecode_program_dispose(&decoded);
        } else {
            MMAY_EXPECT(status == MMAY_BYTECODE_CODEC_ALLOCATION_FAILED);
            MMAY_EXPECT(!decoded.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_decode) break;
    }
    MMAY_EXPECT(reached_decode);
}

static void mmay_test_bytecode_imported_array_vm_allocations(
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
                &execution, mmay_fail_allocator(&state), execution_options
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
                MMAY_EXPECT(value.as.i64 == 51);
            } else {
                MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED);
            }
            mmay_bytecode_execution_dispose(&execution);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_bytecode_imported_arrays(void) {
    MarimayTestImportedArrays test;
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayBytecodeProgram corrupt_output;
    MarimayOwnedBytes encoded;
    unsigned char *corrupt = NULL;
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&corrupt_output, 0, sizeof(corrupt_output));
    memset(&encoded, 0, sizeof(encoded));
    MMAY_EXPECT(mmay_test_imported_arrays_init(
        &test, mmay_system_allocator()
    ));
    if (!test.initialized) return;
    MMAY_EXPECT(mmay_test_imported_arrays_lower(
        &test, mmay_system_allocator(), &program
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    if (!program.initialized) goto cleanup;
    MMAY_EXPECT(mmay_bytecode_program_verify(&program)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    mmay_test_bytecode_imported_array_tables(&program);
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
        mmay_test_bytecode_imported_array_execute(&decoded);
    }
    {
        MarimayAllocator allocator = mmay_system_allocator();
        corrupt = mmay_allocate(&allocator, encoded.length);
    }
    MMAY_EXPECT(corrupt != NULL);
    if (corrupt != NULL && encoded.length != 0) {
        memcpy(corrupt, encoded.data, encoded.length);
        corrupt[encoded.length - 1] ^= 0x01U;
        MMAY_EXPECT(mmay_bytecode_decode_program(
            corrupt,
            encoded.length,
            mmay_system_allocator(),
            mmay_bytecode_program_default_options(),
            &corrupt_output
        ) != MMAY_BYTECODE_CODEC_OK);
        MMAY_EXPECT(!corrupt_output.initialized);
    }
    mmay_test_bytecode_imported_array_lower_allocations(&test);
    mmay_test_bytecode_imported_array_codec_allocations(&program, &encoded);
    mmay_test_bytecode_imported_array_vm_allocations(&program);
cleanup:
    {
        MarimayAllocator allocator = mmay_system_allocator();
        mmay_deallocate(&allocator, corrupt, encoded.length);
    }
    mmay_owned_bytes_dispose(&encoded);
    if (decoded.initialized) mmay_bytecode_program_dispose(&decoded);
    if (program.initialized) mmay_bytecode_program_dispose(&program);
    mmay_test_imported_arrays_dispose(&test);
}
