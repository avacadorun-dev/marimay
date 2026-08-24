#include "test_checker_support.h"

#include <string.h>

#include "mmay/check/checker.h"
#include "mmay/mir/mir.h"

static size_t mmay_test_mir_array_mutation_function(
    const MarimayCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; checked != NULL && index < checked->functions.length;
        index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

void mmay_test_mir_array_mutation(void) {
    static const char source[] =
        "fn probe(): int {\n"
        "  let values = [1, 2, 3]; let alias = values;\n"
        "  const _ = alias.reserve(8); const _ = alias.push(4);\n"
        "  const popped = match values.pop() {\n"
        "    Some(value) => value, None => 0,\n"
        "  };\n"
        "  const _ = alias.truncate(1); const shortened = values.length();\n"
        "  const _ = values.clear();\n"
        "  return popped * 100 + shortened * 10 + alias.length();\n"
        "}\n"
        "fn invalidated(): void {\n"
        "  let values = [1, 2];\n"
        "  for value in values { const _ = values.clear(); }\n"
        "}\n";
    static const unsigned char panic_message[] =
        "array iterator invalidated by mutation";
    static const char loop_header[] = "for value in values";
    MarimayCheckerFixture fixture;
    MarimayCheckResult checked;
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirPanic panic;
    MarimayMirValue result;
    size_t probe = SIZE_MAX;
    size_t invalidated = SIZE_MAX;
    size_t index = 0;
    const char *header = strstr(source, loop_header);
    memset(&checked, 0, sizeof(checked));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    MMAY_EXPECT(header != NULL);
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, source));
    MMAY_EXPECT(mmay_check_ast(
        &fixture.ast,
        "test/mir-array-mutation",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    for (index = 0; index < checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        MMAY_EXPECT(function != NULL);
        if (function != NULL) {
            MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
        }
    }
    probe = mmay_test_mir_array_mutation_function(&checked, "probe");
    invalidated = mmay_test_mir_array_mutation_function(
        &checked, "invalidated"
    );
    MMAY_EXPECT(probe != SIZE_MAX && invalidated != SIZE_MAX);
    if (probe != SIZE_MAX) {
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, probe, NULL, 0, mmay_system_allocator(),
            options, &result
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(result.type == MMAY_TYPE_I64 && result.as.i64 == 410);
    }
    options.panic = &panic;
    if (invalidated != SIZE_MAX) {
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, invalidated, NULL, 0, mmay_system_allocator(),
            options, &result
        ) == MMAY_MIR_INTERPRET_PANIC);
        MMAY_EXPECT(panic.code != NULL && strcmp(panic.code, "G4005") == 0);
        MMAY_EXPECT(panic.message.length == sizeof(panic_message) - 1);
        MMAY_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data, panic_message, sizeof(panic_message) - 1
        ) == 0);
        if (header != NULL) {
            MMAY_EXPECT(panic.span.start == (size_t)(header - source));
            MMAY_EXPECT(
                panic.span.end == panic.span.start + strlen(loop_header)
            );
        }
    }
    mmay_mir_panic_dispose(&panic);
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}
