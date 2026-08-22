#include "test_native_c_emitter_support.h"

#include "test_imported_results_support.h"

static MarimayNativeCProgram mmay_test_native_imported_result_program(
    const MarimayTestImportedResults *test
) {
    MarimayNativeCProgram program;
    program.functions = test->functions;
    program.function_count = test->checked.functions.length;
    program.entry_symbol = test->entry->mir.symbol;
    return program;
}

static void mmay_test_native_imported_result_allocations(
    const MarimayNativeCProgram *program
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes generated;
        MarimayNativeCEmitStatus status;
        memset(&generated, 0, sizeof(generated));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_native_c_emit_program(
            program,
            mmay_fail_allocator(&state),
            mmay_native_c_emit_default_options(),
            &generated
        );
        if (status == MMAY_NATIVE_C_EMIT_OK) {
            reached_success = true;
        } else {
            MMAY_EXPECT(status == MMAY_NATIVE_C_EMIT_ALLOCATION_FAILED);
        }
        mmay_owned_bytes_dispose(&generated);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_native_imported_results(void) {
    MarimayTestImportedResults test;
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_test_imported_results_init(
        &test, mmay_system_allocator()
    ));
    if (!test.initialized) {
        return;
    }
    program = mmay_test_native_imported_result_program(&test);
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program,
        mmay_system_allocator(),
        mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
    if (generated.data != NULL) {
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "MMAY_MANAGED_OBJECT_ENUM"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "MMAY_MANAGED_OBJECT_ARRAY"
        ));
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&generated, "ok:i64:119\n", 0);
        mmay_test_native_compile_and_run_configured(
            &generated,
            "trap:5:0:0\n",
            1,
            "-DMMAY_G_FAIL_ALLOCATION_AT=0"
        );
        mmay_test_native_compile_and_run_configured(
            &generated,
            "trap:5:0:0\n",
            1,
            "-DMMAY_G_FAIL_ALLOCATION_AT=1"
        );
#endif
    }
    mmay_test_native_imported_result_allocations(&program);
    mmay_owned_bytes_dispose(&generated);
    mmay_test_imported_results_dispose(&test);
}
