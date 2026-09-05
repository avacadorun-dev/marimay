#include "test_checker_support.h"

/* Owns declaration relocation, borrowed generic bindings and name teardown. */
static const char generic_declarations[] =
    "fn identity<T>(value: T): T { return value; }\n"
    "fn forward<T>(value: T): T {\n"
    "  const first = identity(value);\n"
    "  const typed: T = first;\n"
    "  return typed;\n"
    "}\n";

/* Grow the declaration vector in the concrete body and again in forward<T>.
 * No pre-reservation or dependence on an allocator moving in place is needed
 * under ASan. Each type must create its own forward and identity instance. */
static const char generic_calls[] =
    "  const a = forward(7);\n"
    "  const b = forward(true);\n"
    "  const c = forward(\"text\");\n"
    "  const d: i8 = 7; const dr = forward(d);\n"
    "  const e: u8 = 7; const er = forward(e);\n"
    "  const f: i16 = 7; const fr = forward(identity(f));\n"
    "  const g: u16 = 7; const gr = forward(g);\n"
    "  const h: i32 = 7; const hr = forward(h);\n"
    "  const i: u32 = 7; const ir = forward(i);\n";

static void mmay_checker_generic_lifetime_publication(
    const MarimayCheckResult *checked,
    bool is_test
) {
    size_t index = 0;
    size_t forwards = 0;
    size_t identities = 0;
    MMAY_EXPECT(checked->functions.length == 19);
    for (index = 0; index < checked->functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            checked, index
        );
        const char *expected = index == 0 ? "relocates" : NULL;
        MMAY_EXPECT(function != NULL);
        if (function == NULL) {
            continue;
        }
        if (index != 0 && function->name.length == strlen("forward")
            && memcmp(function->name.data, "forward", strlen("forward")) == 0) {
            expected = "forward";
            forwards += 1;
        } else if (index != 0) {
            expected = "identity";
            identities += 1;
        }
        MMAY_EXPECT(function->name.length == strlen(expected));
        MMAY_EXPECT(function->name.data != NULL);
        if (function->name.data != NULL
            && function->name.length == strlen(expected)) {
            MMAY_EXPECT(memcmp(
                function->name.data, expected, strlen(expected)
            ) == 0);
        }
        MMAY_EXPECT(function->is_test == (index == 0 && is_test));
        MMAY_EXPECT(!function->is_fuzz);
        MMAY_EXPECT(mmay_mir_verify(&function->mir) == MMAY_MIR_VERIFY_OK);
    }
    MMAY_EXPECT(forwards == 9);
    MMAY_EXPECT(identities == 9);
}

static void mmay_checker_generic_lifetime_case(
    const char *header,
    const char *tail,
    const char *diagnostic,
    bool is_test
) {
    char source[4096];
    MarimayCheckerFixture fixture;
    size_t attempts = 0;
    size_t iteration = 0;
    int length = snprintf(
        source, sizeof(source), "%s%s {\n%s%s}\n",
        generic_declarations, header, generic_calls, tail
    );
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(source));
    if (length <= 0 || (size_t)length >= sizeof(source)) {
        return;
    }
    if (!mmay_checker_fixture_init_path(
            &fixture, "generic_lifetime_test.my", source
        )) {
        MMAY_EXPECT(false);
        return;
    }
    /* Establish the exact successful allocation trace, then fail each index.
     * Diagnostic exits get the same complete teardown sweep as publication. */
    do {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        const size_t failure_index = iteration == 0 ? SIZE_MAX : iteration - 1;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        status = mmay_check_ast(
            &fixture.ast, "test/generic-lifetime", mmay_fail_allocator(&state),
            mmay_check_default_options(), &checked
        );
        if (iteration == 0) {
            attempts = state.attempts;
            MMAY_EXPECT(status == MMAY_CHECK_OK);
            MMAY_EXPECT(checked.initialized);
            if (checked.initialized) {
                MMAY_EXPECT(checked.diagnostics.length == (diagnostic == NULL ? 0 : 1));
                if (diagnostic == NULL) {
                    mmay_checker_generic_lifetime_publication(&checked, is_test);
                } else {
                    const MarimayCheckDiagnostic *actual = mmay_check_diagnostic(
                        &checked, 0
                    );
                    MMAY_EXPECT(actual != NULL);
                    if (actual != NULL) {
                        MMAY_EXPECT(strcmp(actual->code, diagnostic) == 0);
                    }
                }
            }
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        if (checked.initialized) {
            mmay_check_result_dispose(&checked);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        iteration += 1;
    } while (iteration <= attempts);
    MMAY_EXPECT(attempts > 0);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_generic_lifetime(void) {
    mmay_checker_generic_lifetime_case(
        "@test fn relocates(): void", "", NULL, true
    );
    mmay_checker_generic_lifetime_case(
        "fn relocates(): int", "return 42;\n", NULL, false
    );
    mmay_checker_generic_lifetime_case(
        "fn relocates(): int", "", MMAY_DIAGNOSTIC_MISSING_RETURN, false
    );
    mmay_checker_generic_lifetime_case(
        "fn relocates(): void", "const wrong: bool = 7;\n",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH, false
    );
}
