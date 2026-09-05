#include "test_checker_imports_support.h"

/* Owns exact enum-pattern qualification, diagnostics and allocation teardown. */
static const char pattern_library[] =
    "public enum Choice<T> { Empty, Value(T), Named { value: T, }, }\n"
    "public enum Other { Empty, Value(int), Named { value: int, }, }\n"
    "enum Hidden { Empty, }\n"
    "public type IntChoice = Choice<int>;\n"
    "public type StringChoice = Choice<string>;\n";

static void mmay_checker_enum_pattern_case_body(
    const char *arms,
    const char *expected_diagnostic,
    const char *prologue
) {
    char application[4096];
    MarimayCheckerImportFixture fixture;
    MarimayTestImportSource sources[2];
    size_t attempts = 0;
    size_t iteration = 0;
    int length = snprintf(
        application, sizeof(application),
        "import * as m from \"test/model\";\n"
        "import { Choice as Selected, IntChoice } from \"test/model\";\n"
        "type Alias = m.Choice<int>;\n"
        "type WrongAlias = m.Choice<string>;\n"
        "type Absent = m.Choice<int>;\n"
        "enum Local { Empty, Value(int), Named { value: int, }, }\n"
        "fn inspect(value: m.Choice<int>): int { %s return match value { %s }; }\n",
        prologue, arms
    );
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(application));
    if (length <= 0 || (size_t)length >= sizeof(application)) {
        return;
    }
    sources[0] = (MarimayTestImportSource){"test/app", "app/main.my", application};
    sources[1] = (MarimayTestImportSource){"test/model", "model/api.my", pattern_library};
    if (!mmay_checker_import_fixture_init(&fixture, sources, 2)) {
        MMAY_EXPECT(false);
        return;
    }
    do {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        const size_t failure_index = iteration == 0 ? SIZE_MAX : iteration - 1;
        memset(&checked, 0, sizeof(checked));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        status = mmay_checker_import_fixture_check(
            &fixture, "test/app", mmay_fail_allocator(&state),
            mmay_check_graph_default_options(), &checked
        );
        if (iteration == 0) {
            attempts = state.attempts;
            MMAY_EXPECT(status == MMAY_CHECK_OK);
            MMAY_EXPECT(checked.initialized);
            if (checked.initialized && expected_diagnostic == NULL) {
                const MarimayCheckedFunction *inspect =
                    mmay_checker_import_function(&checked, "inspect", NULL);
                MMAY_EXPECT(checked.diagnostics.length == 0);
                MMAY_EXPECT(inspect != NULL);
                if (inspect != NULL) {
                    MMAY_EXPECT(mmay_mir_verify(&inspect->mir) == MMAY_MIR_VERIFY_OK);
                }
            } else if (checked.initialized) {
                const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(&checked, 0);
                MMAY_EXPECT(checked.diagnostics.length == 1);
                MMAY_EXPECT(diagnostic != NULL);
                if (diagnostic != NULL) {
                    if (strcmp(diagnostic->code, expected_diagnostic) != 0) {
                        fprintf(stderr, "pattern %s: expected %s, got %s: %s\n",
                            arms, expected_diagnostic, diagnostic->code, diagnostic->message);
                    }
                    MMAY_EXPECT(strcmp(diagnostic->code, expected_diagnostic) == 0);
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
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_checker_enum_pattern_case(
    const char *arms,
    const char *expected_diagnostic
) {
    mmay_checker_enum_pattern_case_body(arms, expected_diagnostic, "");
}

static void mmay_checker_enum_pattern_qualifier(
    const char *qualifier,
    const char *expected_diagnostic
) {
    char arms[512];
    int length = snprintf(arms, sizeof(arms),
        "%s.Empty => 0, %s.Value(item) => item, %s.Named { value: item } => item,",
        qualifier, qualifier, qualifier);
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(arms));
    if (length > 0 && (size_t)length < sizeof(arms)) {
        mmay_checker_enum_pattern_case(arms, expected_diagnostic);
    }
}

void mmay_test_checker_enum_pattern_names(void) {
    mmay_checker_enum_pattern_qualifier("m.Choice", NULL);
    mmay_checker_enum_pattern_qualifier("Selected", NULL);
    mmay_checker_enum_pattern_qualifier("IntChoice", NULL);
    mmay_checker_enum_pattern_qualifier("Alias", NULL);
    mmay_checker_enum_pattern_qualifier("m.IntChoice", NULL);
    mmay_checker_enum_pattern_qualifier("m", NULL);
    mmay_checker_enum_pattern_qualifier("m.Other", MMAY_DIAGNOSTIC_TYPE_MISMATCH);
    mmay_checker_enum_pattern_qualifier("Local", MMAY_DIAGNOSTIC_TYPE_MISMATCH);
    mmay_checker_enum_pattern_qualifier("WrongAlias", MMAY_DIAGNOSTIC_TYPE_MISMATCH);
    mmay_checker_enum_pattern_qualifier("m.StringChoice", MMAY_DIAGNOSTIC_TYPE_MISMATCH);
    mmay_checker_enum_pattern_qualifier("m.Hidden", MMAY_DIAGNOSTIC_PRIVATE_IMPORT);
    mmay_checker_enum_pattern_qualifier("m.Absent", MMAY_DIAGNOSTIC_UNKNOWN_IMPORT);
    mmay_checker_enum_pattern_qualifier("absent.Choice", MMAY_DIAGNOSTIC_UNKNOWN_IMPORT);
    mmay_checker_enum_pattern_qualifier("m.Choice.Extra", MMAY_DIAGNOSTIC_TYPE_MISMATCH);
    mmay_checker_enum_pattern_case(
        "m.Choice.Absent => 0, _ => 1,", MMAY_DIAGNOSTIC_UNKNOWN_VARIANT
    );
    mmay_checker_enum_pattern_case(
        "m.Choice.Value(item) => item,", MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH
    );
    mmay_checker_enum_pattern_case(
        "m.Choice.Empty => 0, Selected.Empty => 1, _ => 2,",
        MMAY_DIAGNOSTIC_REDUNDANT_PATTERN
    );
    mmay_checker_enum_pattern_case_body(
        "Selected.Empty => 0, _ => 1,", MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "const Selected = 42;"
    );
    mmay_checker_enum_pattern_case(
        "m.Choice.Value(m.Choice.Empty) => 0, _ => 1,",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
    mmay_checker_enum_pattern_case(
        "m.Choice.Value(m.Choice.Value(item)) => 0, _ => 1,",
        MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
}
