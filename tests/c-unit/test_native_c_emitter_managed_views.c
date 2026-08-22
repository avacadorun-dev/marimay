#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

static const RudaCheckedFunction *ruda_test_native_view_function(
    const RudaCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    for (index = 0; checked != NULL && name != NULL
        && index < checked->functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == strlen(name)
            && memcmp(function->name.data, name, function->name.length) == 0) {
            return function;
        }
    }
    return NULL;
}

void ruda_test_native_c_emitter_managed_views(void) {
    static const char source[] =
        "type Bag { mutable values: []int; }\n"
        "fn first(bag: Bag): int { return bag.values[0]; }\n"
        "fn replace(bag: Bag): void { bag.values = [9]; }\n"
        "fn afterReplace(bag: Bag): int {\n"
        "  const before = bag.values[0];\n"
        "  replace(bag);\n"
        "  return before + bag.values[0];\n"
        "}\n"
        "fn main(): int { return afterReplace(Bag { values: [7] }); }\n";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *main_function = NULL;
    const RudaMirFunction *functions[8];
    RudaNativeCProgram program;
    RudaOwnedBytes generated;
    size_t function_count = 0;
    size_t index = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized) {
        ruda_checker_fixture_dispose(&fixture);
        return;
    }
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast, "test/native-managed-views", ruda_system_allocator(),
        ruda_check_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (!checked.initialized) {
        ruda_checker_fixture_dispose(&fixture);
        return;
    }
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.functions.length <= 8);
    for (index = 0; index < checked.functions.length && index < 8;
        index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, index
        );
        RUDA_EXPECT(function != NULL);
        if (function != NULL) {
            functions[function_count] = &function->mir;
            function_count += 1;
        }
    }
    main_function = ruda_test_native_view_function(&checked, "main");
    RUDA_EXPECT(main_function != NULL);
    if (main_function != NULL) {
        program.functions = functions;
        program.function_count = function_count;
        program.entry_symbol = main_function->mir.symbol;
        RUDA_EXPECT(ruda_native_c_emit_program(
            &program, ruda_system_allocator(),
            ruda_native_c_emit_default_options(), &generated
        ) == RUDA_NATIVE_C_EMIT_OK);
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "RudaManagedRecordView ruda_g_rv_0 = {0};"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "RudaManagedArrayView ruda_g_rv_0_f_0 = {0};"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "ruda_g_rv_0.fields[0].reference"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "&ruda_g_rv_0_f_0"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated,
            "ruda_managed_reference_equal(ruda_g_rv_0.fields[0].reference"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "= ruda_g_rv_0_f_0;"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated, "memcpy(&ruda_g_bits, *ruda_g_av_"
        ));
        RUDA_EXPECT(ruda_test_native_contains(
            &generated,
            "#if RUDA_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
            "#if RUDA_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
        ));
#if !defined(_WIN32)
        ruda_test_native_compile_and_run(&generated, "ok:i64:16\n", 0);
#endif
    }
    ruda_owned_bytes_dispose(&generated);
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
}
