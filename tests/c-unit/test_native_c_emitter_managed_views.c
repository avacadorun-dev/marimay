#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

static const MarimayCheckedFunction *mmay_test_native_view_function(
    const MarimayCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    for (index = 0; checked != NULL && name != NULL
        && index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == strlen(name)
            && memcmp(function->name.data, name, function->name.length) == 0) {
            return function;
        }
    }
    return NULL;
}

void mmay_test_native_c_emitter_managed_views(void) {
    static const char source[] =
        "type Bag { mutable values: []int, }\n"
        "fn first(bag: Bag): int { return bag.values[0]; }\n"
        "fn replace(bag: Bag): void { bag.values = [9]; }\n"
        "fn afterReplace(bag: Bag): int {\n"
        "  const before = bag.values[0];\n"
        "  replace(bag);\n"
        "  return before + first(bag);\n"
        "}\n"
        "type StableBag { values: []int, }\n"
        "fn stableFirst(bag: StableBag): int { return bag.values[0]; }\n"
        "fn makeStable(): StableBag { return StableBag { values: [5] }; }\n"
        "fn copiedStable(): int {\n"
        "  const original = makeStable();\n"
        "  const copied = original;\n"
        "  return stableFirst(copied);\n"
        "}\n"
        "fn recordFirst(values: []StableBag): int { return values[0].values[0]; }\n"
        "fn arrayFirst(values: [][]int): int { return values[0][0]; }\n"
        "fn children(): int {\n"
        "  const records = [StableBag { values: [2] }];\n"
        "  const arrays = [[3]];\n"
        "  const before = recordFirst(records) + arrayFirst(arrays);\n"
        "  records[0] = StableBag { values: [11] };\n"
        "  arrays[0] = [13];\n"
        "  for i in 0..32 {\n"
        "    const _ = push(records, StableBag { values: [i] });\n"
        "    const _ = push(arrays, [i]);\n"
        "  }\n"
        "  return before + records[0].values[0] + arrays[0][0]\n"
        "    + records[32].values[0] + arrays[32][0];\n"
        "}\n"
        "fn main(): int {\n"
        "  return afterReplace(Bag { values: [7] }) + copiedStable() + children();\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *main_function = NULL;
    const MarimayMirFunction *functions[16];
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    size_t function_count = 0;
    size_t index = 0;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&program, 0, sizeof(program));
    memset(&generated, 0, sizeof(generated));
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    if (!fixture.ast_initialized) {
        mmay_checker_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast, "test/native-managed-views", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (!checked.initialized) {
        mmay_checker_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length <= 16);
    for (index = 0; index < checked.functions.length && index < 16;
        index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        MMAY_EXPECT(function != NULL);
        if (function != NULL) {
            functions[function_count] = &function->mir;
            function_count += 1;
        }
    }
    main_function = mmay_test_native_view_function(&checked, "main");
    MMAY_EXPECT(main_function != NULL);
    if (main_function != NULL) {
        program.functions = functions;
        program.function_count = function_count;
        program.entry_symbol = main_function->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &generated
        ) == MMAY_NATIVE_C_EMIT_OK);
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "MarimayManagedRecordGeneratedView mmay_g_rv_0 = {0};"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated,
            "MarimayManagedArrayGeneratedView mmay_g_rv_0_f_0 = {0};"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_g_rv_0.object->fields[0].reference"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "&mmay_g_rv_0_f_0"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated,
            "mmay_managed_array_generated_bind(&mmay_g_x->heap, mmay_g_rv_0.object->fields[0].reference"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated,
            "if (mmay_g_ro_0 != NULL) { mmay_g_rv_0.reference = mmay_g_v_0; mmay_g_rv_0.object = mmay_g_ro_0; } else {"
        ));
        MMAY_EXPECT(!mmay_test_native_contains(
            &generated, "mmay_g_ro_0->descriptor =="
        ));
        MMAY_EXPECT(!mmay_test_native_contains(
            &generated, "mmay_g_ro_0_f_0->descriptor =="
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "= mmay_g_rv_0_f_0;"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "_f_0 = mmay_g_rv_"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_array_generated_get_scalar("
        ));
        /* Keep proven parameter prewarming, but never rescan a mutable
         * parent or consult its possibly stale child cache at an access. */
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_array_generated_cache_children("
        ));
        MMAY_EXPECT(!mmay_test_native_contains(
            &generated, "generated_children->generation !="
        ));
        MMAY_EXPECT(!mmay_test_native_contains(
            &generated, "generated_children != NULL ?"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_record_generated_child_unchecked(&mmay_g_x->heap, mmay_g_cell.reference)"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_array_generated_child_unchecked(&mmay_g_x->heap, mmay_g_cell.reference)"
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated,
            "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
            "#if MMAY_G_FAIL_ALLOCATION_AT != SIZE_MAX\n"
        ));
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&generated, "ok:i64:112\n", 0);
#endif
    }
    mmay_owned_bytes_dispose(&generated);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}
