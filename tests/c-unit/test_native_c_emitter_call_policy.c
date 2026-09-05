#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

#include <stdio.h>
#include <string.h>

static const MarimayCheckedFunction *mmay_test_native_policy_function(
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

static bool mmay_test_native_policy_signature(
    char *output,
    size_t capacity,
    const char *prefix,
    MarimaySymbolId symbol
) {
    size_t index = 0;
    int written = 0;
    size_t used = 0;
    if (output == NULL || capacity == 0 || prefix == NULL) return false;
    written = snprintf(output, capacity, "%s", prefix);
    if (written < 0 || (size_t)written >= capacity) return false;
    used = (size_t)written;
    for (index = 0; index < sizeof(symbol.bytes); index += 1) {
        written = snprintf(
            output + used, capacity - used, "%02x", symbol.bytes[index]
        );
        if (written != 2 || (size_t)written >= capacity - used) return false;
        used += (size_t)written;
    }
    return true;
}

void mmay_test_native_c_emitter_call_policy(void) {
    static const char source[] =
        "type Box { values: []int, }\n"
        "fn tiny(box: Box): int { return box.values[0]; }\n"
        "fn large(box: Box): int {\n"
        "  let total = tiny(box);\n"
        "  total += 1; total += 2; total += 3; total += 4;\n"
        "  total += 5; total += 6; total += 7; total += 8;\n"
        "  total += 9; total += 10; total += 11; total += 12;\n"
        "  total += 13; total += 14; total += 15; total += 16;\n"
        "  return total;\n"
        "}\n"
        "fn pressured(box: Box): int {\n"
        "  let total = box.values[0];\n"
        "  total += 1; total += 2; total += 3; total += 4;\n"
        "  total += 5; total += 6; total += 7; total += 8;\n"
        "  total += 9; total += 10; total += 11; total += 12;\n"
        "  total += 13; total += 14; total += 15; total += 16;\n"
        "  return total;\n"
        "}\n"
        "fn preserveCaller(box: Box): int {\n"
        "  let total = pressured(box);\n"
        "  total += 17; total += 18; total += 19; total += 20;\n"
        "  total += 21; total += 22; total += 23; total += 24;\n"
        "  return total;\n"
        "}\n"
        "fn half(value: int): int { return value / 2; }\n"
        "fn mod3(value: int): int { return value % 3; }\n"
        "fn divide(value: int, divisor: int): int { return value / divisor; }\n"
        "fn main(): int {\n"
        "  const box = Box { values: [1] };\n"
        "  return large(box) + preserveCaller(box)\n"
        "    + half(10) + mod3(8) + divide(6, 3);\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry = NULL;
    const MarimayCheckedFunction *large = NULL;
    const MarimayCheckedFunction *pressured = NULL;
    const MarimayCheckedFunction *preserve = NULL;
    const MarimayMirFunction *functions[12];
    MarimayNativeCProgram program;
    MarimayOwnedBytes generated;
    char large_signature[96];
    char pressured_signature[96];
    char preserve_signature[96];
    char forbidden_signature[96];
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
        &fixture.ast, "test/native-call-policy", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (!checked.initialized) {
        mmay_checker_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length <= 12);
    for (index = 0; index < checked.functions.length && index < 12;
        index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        MMAY_EXPECT(function != NULL);
        if (function != NULL) functions[function_count++] = &function->mir;
    }
    entry = mmay_test_native_policy_function(&checked, "main");
    large = mmay_test_native_policy_function(&checked, "large");
    pressured = mmay_test_native_policy_function(&checked, "pressured");
    preserve = mmay_test_native_policy_function(&checked, "preserveCaller");
    MMAY_EXPECT(entry != NULL && large != NULL && pressured != NULL
        && preserve != NULL);
    if (entry != NULL && large != NULL && pressured != NULL
        && preserve != NULL) {
        program.functions = functions;
        program.function_count = function_count;
        program.entry_symbol = entry->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &generated
        ) == MMAY_NATIVE_C_EMIT_OK);
        MMAY_EXPECT(mmay_test_native_policy_signature(
            large_signature, sizeof(large_signature),
            "static MMAY_G_NOINLINE int mmay_g_f_", large->mir.symbol
        ));
        MMAY_EXPECT(mmay_test_native_policy_signature(
            pressured_signature, sizeof(pressured_signature),
            "static MMAY_G_ALWAYS_INLINE int mmay_g_f_",
            pressured->mir.symbol
        ));
        MMAY_EXPECT(mmay_test_native_policy_signature(
            preserve_signature, sizeof(preserve_signature),
            "static int mmay_g_f_", preserve->mir.symbol
        ));
        MMAY_EXPECT(mmay_test_native_policy_signature(
            forbidden_signature, sizeof(forbidden_signature),
            "static MMAY_G_NOINLINE int mmay_g_f_", preserve->mir.symbol
        ));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "#define MMAY_G_NOINLINE __attribute__((noinline))"
        ));
        MMAY_EXPECT(mmay_test_native_contains(&generated, large_signature));
        MMAY_EXPECT(mmay_test_native_contains(&generated, pressured_signature));
        MMAY_EXPECT(mmay_test_native_contains(&generated, preserve_signature));
        MMAY_EXPECT(!mmay_test_native_contains(
            &generated, forbidden_signature
        ));
        MMAY_EXPECT(mmay_test_native_contains(&generated, " / mmay_g_v_"));
        MMAY_EXPECT(mmay_test_native_contains(&generated, " % mmay_g_v_"));
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_checked_i64_divide_fast("
        ));
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&generated, "ok:i64:447\n", 0);
#endif
    }
    mmay_owned_bytes_dispose(&generated);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}
