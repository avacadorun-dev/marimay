#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

#include "ruda/check/checker.h"
#include "ruda/native/c_emitter.h"

static void ruda_test_native_array_mutation_allocations(
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
        if (status == RUDA_NATIVE_C_EMIT_OK) {
            reached_success = true;
        } else {
            RUDA_EXPECT(status == RUDA_NATIVE_C_EMIT_ALLOCATION_FAILED);
        }
        ruda_owned_bytes_dispose(&generated);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_native_c_emitter_array_mutation(void) {
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
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaMirFunction *functions[16];
    RudaNativeCProgram program;
    RudaOwnedBytes generated;
    size_t index = 0;
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/native-array-mutation",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.functions.length == 1);
    for (index = 0; index < checked.functions.length && index < 16;
        index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, index
        );
        functions[index] = function == NULL ? NULL : &function->mir;
    }
    if (checked.functions.length == 1 && functions[0] != NULL) {
        program.functions = functions;
        program.function_count = checked.functions.length;
        program.entry_symbol = functions[0]->symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &program,
            ruda_system_allocator(),
            ruda_native_c_emit_default_options(),
            &generated
        ) == RUDA_NATIVE_C_EMIT_OK);
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "ruda_managed_array_pop"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "ruda_managed_array_truncate"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "ruda_managed_array_clear"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "ruda_managed_array_reserve"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "ruda_managed_array_check_mutation_generation"
        ));
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&generated, "ok:i64:415\n", 0);
#endif
        ruda_test_native_array_mutation_allocations(&program);
    }
    ruda_owned_bytes_dispose(&generated);
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
}
