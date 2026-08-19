#include "test_native_c_emitter_support.h"

#include "test_imported_arrays_support.h"

static RudaNativeCProgram ruda_test_native_imported_array_program(
    const RudaTestImportedArrays *test
) {
    RudaNativeCProgram program;
    program.functions = test->functions;
    program.function_count = test->checked.functions.length;
    program.entry_symbol = test->entry->mir.symbol;
    return program;
}

static void ruda_test_native_imported_array_allocations(
    const RudaNativeCProgram *program
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaOwnedBytes generated;
        RudaNativeCEmitStatus status;
        memset(&generated, 0, sizeof(generated));
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_native_c_emit_program(
            program,
            ruda_fail_allocator(&state),
            ruda_native_c_emit_default_options(),
            &generated
        );
        if (status == RUDA_NATIVE_C_EMIT_OK) reached_success = true;
        else RUDA_EXPECT(status == RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED);
        ruda_owned_bytes_dispose(&generated);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_native_imported_arrays(void) {
    RudaTestImportedArrays test;
    RudaNativeCProgram program;
    RudaOwnedBytes generated;
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_test_imported_arrays_init(
        &test, ruda_system_allocator()
    ));
    if (!test.initialized) return;
    program = ruda_test_native_imported_array_program(&test);
    RUDA_EXPECT(ruda_native_c_emit_program(
        &program,
        ruda_system_allocator(),
        ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
    if (generated.data != NULL) {
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "RUDA_MANAGED_OBJECT_RECORD"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "RUDA_MANAGED_OBJECT_ENUM"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "RUDA_MANAGED_OBJECT_STRING"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "RUDA_MANAGED_OBJECT_ARRAY"
        ));
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&generated, "ok:i64:51\n", 0);
        ruda_test_native_compile_and_run_configured(
            &generated,
            "trap:5:0:0\n",
            1,
            "-DRUDA_G_FAIL_ALLOCATION_AT=0"
        );
        ruda_test_native_compile_and_run_configured(
            &generated,
            "trap:5:0:0\n",
            1,
            "-DRUDA_G_FAIL_ALLOCATION_AT=1"
        );
#endif
    }
    ruda_test_native_imported_array_allocations(&program);
    ruda_owned_bytes_dispose(&generated);
    ruda_test_imported_arrays_dispose(&test);
}
