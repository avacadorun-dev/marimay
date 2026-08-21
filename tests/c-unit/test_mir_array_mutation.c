#include "test_checker_support.h"

#include <string.h>

#include "ruda/check/checker.h"
#include "ruda/mir/mir.h"

static size_t ruda_test_mir_array_mutation_function(
    const RudaCheckResult *checked,
    const char *name
) {
    size_t index = 0;
    size_t length = strlen(name);
    for (index = 0; checked != NULL && index < checked->functions.length;
        index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            checked, index
        );
        if (function != NULL && function->name.length == length
            && memcmp(function->name.data, name, length) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

void ruda_test_mir_array_mutation(void) {
    static const char source[] =
        "fn probe(): int {\n"
        "  let values = [1, 2, 3]; let alias = values;\n"
        "  alias.reserve(8); alias.push(4);\n"
        "  const popped = match values.pop() {\n"
        "    case Some(value) => value; case None => 0;\n"
        "  };\n"
        "  alias.truncate(1); const shortened = values.length();\n"
        "  values.clear();\n"
        "  return popped * 100 + shortened * 10 + alias.length();\n"
        "}\n"
        "fn invalidated(): void {\n"
        "  let values = [1, 2];\n"
        "  for const value of values { values.clear(); }\n"
        "}\n";
    static const unsigned char panic_message[] =
        "array iterator invalidated by mutation";
    static const char loop_header[] = "for const value of values";
    RudaCheckerFixture fixture;
    RudaCheckResult checked;
    RudaCheckInterpretOptions options = ruda_check_interpret_default_options();
    RudaMirPanic panic;
    RudaMirValue result;
    size_t probe = SIZE_MAX;
    size_t invalidated = SIZE_MAX;
    size_t index = 0;
    const char *header = strstr(source, loop_header);
    memset(&checked, 0, sizeof(checked));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    RUDA_EXPECT(header != NULL);
    RUDA_EXPECT(ruda_checker_fixture_init(&fixture, source));
    RUDA_EXPECT(ruda_check_ast(
        &fixture.ast,
        "test/mir-array-mutation",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized && checked.diagnostics.length == 0);
    for (index = 0; index < checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, index
        );
        RUDA_EXPECT(function != NULL);
        if (function != NULL) {
            RUDA_EXPECT(ruda_mir_verify(&function->mir) == RUDA_MIR_VERIFY_OK);
        }
    }
    probe = ruda_test_mir_array_mutation_function(&checked, "probe");
    invalidated = ruda_test_mir_array_mutation_function(
        &checked, "invalidated"
    );
    RUDA_EXPECT(probe != SIZE_MAX && invalidated != SIZE_MAX);
    if (probe != SIZE_MAX) {
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, probe, NULL, 0, ruda_system_allocator(),
            options, &result
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(result.type == RUDA_TYPE_I64 && result.as.i64 == 410);
    }
    options.panic = &panic;
    if (invalidated != SIZE_MAX) {
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, invalidated, NULL, 0, ruda_system_allocator(),
            options, &result
        ) == RUDA_MIR_INTERPRET_PANIC);
        RUDA_EXPECT(panic.code != NULL && strcmp(panic.code, "G4005") == 0);
        RUDA_EXPECT(panic.message.length == sizeof(panic_message) - 1);
        RUDA_EXPECT(panic.message.data != NULL && memcmp(
            panic.message.data, panic_message, sizeof(panic_message) - 1
        ) == 0);
        if (header != NULL) {
            RUDA_EXPECT(panic.span.start == (size_t)(header - source));
            RUDA_EXPECT(
                panic.span.end == panic.span.start + strlen(loop_header)
            );
        }
    }
    ruda_mir_panic_dispose(&panic);
    ruda_check_result_dispose(&checked);
    ruda_checker_fixture_dispose(&fixture);
}
