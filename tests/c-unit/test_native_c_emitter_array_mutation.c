#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

#include "mmay/check/checker.h"
#include "mmay/native/c_emitter.h"

static void mmay_test_native_array_mutation_allocations(
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
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_native_c_emitter_array_mutation(void) {
    static const char source[] =
        "fn probe(): int {\n"
        "  let values = [1, 2, 3]; let alias = values;\n"
        "  alias.reserve(8); alias.push(4);\n"
        "  const popped = match values.pop() {\n"
        "    case Some(value) => value; case None => 0;\n"
        "  };\n"
        "  alias.truncate(1); const shortened = values.length();\n"
        "  values.clear();\n"
        "  let stable = [5]; let total = 0;\n"
        "  for const value of stable { stable.reserve(2); total += value; }\n"
        "  return popped * 100 + shortened * 10 + alias.length() + total;\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayMirFunction *functions[16];
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    size_t index = 0;
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/native-array-mutation",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length == 1);
    for (index = 0; index < checked.functions.length && index < 16;
        index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        functions[index] = function == NULL ? NULL : &function->mir;
    }
    if (checked.functions.length == 1 && functions[0] != NULL) {
        program.functions = functions;
        program.function_count = checked.functions.length;
        program.entry_symbol = functions[0]->symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program,
            mmay_system_allocator(),
            mmay_native_c_emit_default_options(),
            &generated
        ) == MMAY_NATIVE_C_EMIT_OK);
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_array_generated_pop_raw64"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_array_truncate"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_array_clear"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_array_reserve"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "->mutation_generation"
        ));
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&generated, "ok:i64:415\n", 0);
#endif
        mmay_test_native_array_mutation_allocations(&program);
    }
    mmay_owned_bytes_dispose(&generated);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}
