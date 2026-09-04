#include "test_checker_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

#include "mmay/mir/range.h"

static const MarimayCheckedFunction *mmay_test_native_relations_function(
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

static size_t mmay_test_native_relations_count(
    const MarimayOwnedBytes *bytes,
    const char *needle
) {
    size_t count = 0;
    size_t needle_length = needle != NULL ? strlen(needle) : 0;
    size_t index = 0;
    if (bytes == NULL || needle == NULL || needle_length == 0
        || needle_length > bytes->length) return 0;
    for (index = 0; index + needle_length <= bytes->length; index += 1) {
        if (memcmp(bytes->data + index, needle, needle_length) == 0) {
            count += 1;
            index += needle_length - 1;
        }
    }
    return count;
}

void mmay_test_native_c_emitter_relations(void) {
    static const char source[] =
        "fn selected(values: []int): int {\n"
        "  let root = 0;\n"
        "  while root * 2 + 1 < values.length() {\n"
        "    const left = root * 2 + 1;\n"
        "    const right = left + 1;\n"
        "    let selected = left;\n"
        "    if right < values.length() && values[right] < values[left] {\n"
        "      selected = right;\n"
        "    }\n"
        "    return values[selected];\n"
        "  }\n"
        "  return 0;\n"
        "}\n"
        "fn uncheckedChoice(values: []int, chooseEnd: bool): int {\n"
        "  let selected = 0;\n"
        "  if chooseEnd { selected = values.length(); }\n"
        "  return values[selected];\n"
        "}\n"
        "fn changedOperand(values: []int): int {\n"
        "  let root = 0;\n"
        "  if root * 2 + 1 < values.length() {\n"
        "    root = values.length();\n"
        "    const changed = root * 2 + 1;\n"
        "    return values[changed];\n"
        "  }\n"
        "  return 0;\n"
        "}\n"
        "fn delayedChangedOperand(values: []int): int {\n"
        "  let root = 0;\n"
        "  const original = root * 2 + 1;\n"
        "  root = values.length();\n"
        "  if original < values.length() {\n"
        "    const changed = root * 2 + 1;\n"
        "    return values[changed];\n"
        "  }\n"
        "  return 0;\n"
        "}\n"
        "fn main(): int {\n"
        "  return selected([9, 7, 5]) + selected([9, 5, 7]);\n"
        "}\n";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry = NULL;
    const MarimayMirFunction *functions[8];
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
        &fixture.ast, "test/native-relations", mmay_system_allocator(),
        mmay_check_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (!checked.initialized) {
        mmay_checker_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length <= 8);
    for (index = 0; index < checked.functions.length && index < 8;
        index += 1) {
        MarimayCheckedFunction *function = mmay_vector_at(
            &checked.functions, index
        );
        MMAY_EXPECT(function != NULL);
        if (function != NULL && !function->is_external) {
            MMAY_EXPECT(mmay_mir_optimize_ranges(&function->mir)
                == MMAY_MIR_RANGE_OPTIMIZE_OK);
            functions[function_count++] = &function->mir;
        }
    }
    entry = mmay_test_native_relations_function(&checked, "main");
    MMAY_EXPECT(entry != NULL);
    if (entry != NULL) {
        program.functions = functions;
        program.function_count = function_count;
        program.entry_symbol = entry->mir.symbol;
        MMAY_EXPECT(mmay_native_c_emit_program(
            &program, mmay_system_allocator(),
            mmay_native_c_emit_default_options(), &generated
        ) == MMAY_NATIVE_C_EMIT_OK);
        MMAY_EXPECT(mmay_test_native_relations_count(
            &generated, "mmay_g_begin_bounds("
        ) == 4);
        MMAY_EXPECT(mmay_test_native_contains(
            &generated, "mmay_managed_array_generated_get_scalar("
        ));
#if !defined(_WIN32)
        mmay_test_native_compile_and_run(&generated, "ok:i64:10\n", 0);
#endif
    }
    mmay_owned_bytes_dispose(&generated);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}
