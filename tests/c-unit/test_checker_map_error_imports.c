#include "test_checker_imports_support.h"

/* Owns qualified mapError mapper resolution and failure-atomic publication. */
static const char mapper_library[] =
    "public enum Input { Bad, }\n"
    "public enum Wrapped { Failure(Input), }\n"
    "public fn message(error: Input): string { return \"mapped\"; }\n"
    "fn hidden(error: Input): string { return \"private\"; }\n"
    "public fn wrong(error: int): string { return \"wrong\"; }\n"
    "public fn pair(error: Input, extra: int): string { return \"pair\"; }\n"
    "public const answer: int = 42;\n";

static void mmay_checker_map_error_import_case(
    const char *mapper,
    const char *output_type,
    const char *expected_diagnostic
) {
    char application[2048];
    MarimayCheckerImportFixture fixture;
    MarimayTestImportSource sources[2];
    size_t attempts = 0;
    size_t iteration = 0;
    int length = snprintf(
        application, sizeof(application),
        "import * as m from \"test/mappers\";\n"
        "import { message as describe } from \"test/mappers\";\n"
        "fn missing(error: m.Input): string { return \"not imported\"; }\n"
        "fn mapped(value: Result<int, m.Input>): Result<int, %s> {\n"
        "  return value.mapError(%s);\n"
        "}\n", output_type, mapper
    );
    MMAY_EXPECT(length > 0 && (size_t)length < sizeof(application));
    if (length <= 0 || (size_t)length >= sizeof(application)) {
        return;
    }
    sources[0] = (MarimayTestImportSource){
        "test/app", "app/main.my", application
    };
    sources[1] = (MarimayTestImportSource){
        "test/mappers", "mappers/api.my", mapper_library
    };
    if (!mmay_checker_import_fixture_init(&fixture, sources, 2)) {
        MMAY_EXPECT(false);
        return;
    }
    do {
        MarimayFailAllocator state;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        size_t failure_index = iteration == 0 ? SIZE_MAX : iteration - 1;
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
                const MarimayCheckedFunction *mapped =
                    mmay_checker_import_function(&checked, "mapped", NULL);
                MMAY_EXPECT(checked.diagnostics.length == 0);
                MMAY_EXPECT(mapped != NULL);
                if (mapped != NULL) {
                    MMAY_EXPECT(mmay_mir_verify(&mapped->mir) == MMAY_MIR_VERIFY_OK);
                }
            } else if (checked.initialized) {
                const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(
                    &checked, 0
                );
                MMAY_EXPECT(checked.diagnostics.length == 1);
                MMAY_EXPECT(diagnostic != NULL);
                if (diagnostic != NULL) {
                    if (strcmp(diagnostic->code, expected_diagnostic) != 0) {
                        fprintf(stderr, "mapper %s: expected %s, got %s: %s\n",
                            mapper, expected_diagnostic, diagnostic->code,
                            diagnostic->message);
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

void mmay_test_checker_map_error_imports(void) {
    mmay_checker_map_error_import_case("m.message", "string", NULL);
    mmay_checker_map_error_import_case("describe", "string", NULL);
    mmay_checker_map_error_import_case("m.Wrapped.Failure", "m.Wrapped", NULL);
    mmay_checker_map_error_import_case(
        "m.hidden", "string", MMAY_DIAGNOSTIC_PRIVATE_IMPORT
    );
    mmay_checker_map_error_import_case(
        "m.missing", "string", MMAY_DIAGNOSTIC_UNKNOWN_IMPORT
    );
    mmay_checker_map_error_import_case(
        "m.wrong", "string", MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
    mmay_checker_map_error_import_case(
        "m.pair", "string", MMAY_DIAGNOSTIC_TYPE_MISMATCH
    );
    mmay_checker_map_error_import_case(
        "m.answer", "string", MMAY_DIAGNOSTIC_UNKNOWN_IMPORT
    );
}
