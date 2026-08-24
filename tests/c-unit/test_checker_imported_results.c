#include "test_imported_results_support.h"

#include "mmay/generated/diagnostic_codes.h"

static bool mmay_test_imported_result_name(
    const MarimayOwnedBytes *name,
    const char *text
) {
    size_t length = strlen(text);
    return name != NULL && name->length == length
        && memcmp(name->data, text, length) == 0;
}

static void mmay_test_checker_imported_result_metadata(
    const MarimayTestImportedResults *test
) {
    const MarimayCheckedEnum *choice = NULL;
    bool saw_choice_array = false;
    bool saw_choice_payload_array = false;
    size_t index = 0;
    for (index = 0; index < test->checked.enums.length; index += 1) {
        const MarimayCheckedEnum *enum_value = mmay_checked_enum(
            &test->checked, index
        );
        if (enum_value != NULL && mmay_test_imported_result_name(
                &enum_value->name, "Choice"
            )) {
            choice = enum_value;
            break;
        }
    }
    MMAY_EXPECT(choice != NULL);
    if (choice != NULL) {
        MMAY_EXPECT(choice->is_public);
        MMAY_EXPECT(choice->type_parameters.length == 1);
        MMAY_EXPECT(choice->variants.length == 2);
    }
    for (index = 0; index < test->checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &test->checked, index
        );
        size_t enum_index = 0;
        if (function == NULL) {
            continue;
        }
        for (enum_index = 0; enum_index < function->mir.enums.length;
            enum_index += 1) {
            const MarimayMirEnum *layout = mmay_vector_at_const(
                &function->mir.enums, enum_index
            );
            const MarimayTypeDescriptor *descriptor = layout == NULL
                ? NULL : mmay_type_descriptor(
                    &test->checked.types, layout->type
                );
            if (choice != NULL && descriptor != NULL
                && mmay_symbol_id_equal(
                    descriptor->symbol, choice->symbol
                ) && descriptor->argument_count == 1
                && mmay_type_kind(
                    &test->checked.types, descriptor->arguments[0]
                ) == MMAY_TYPE_KIND_ARRAY) {
                const MarimayMirEnumPayload *payload = layout->payload_count == 1
                    ? mmay_vector_at_const(
                        &function->mir.enum_payloads,
                        layout->payload_start
                    ) : NULL;
                saw_choice_array = true;
                saw_choice_payload_array = payload != NULL
                    && payload->type == descriptor->arguments[0];
            }
        }
    }
    MMAY_EXPECT(saw_choice_array);
    MMAY_EXPECT(saw_choice_payload_array);
}

static void mmay_test_checker_imported_result_negative(
    const MarimayTestImportSource *sources,
    size_t source_count,
    const char *code,
    const char *message
) {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckDiagnostic *diagnostic = NULL;
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture, sources, source_count
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture,
        "acme/app",
        mmay_system_allocator(),
        mmay_check_graph_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    MMAY_EXPECT(checked.diagnostics.length == 1);
    diagnostic = mmay_check_diagnostic(&checked, 0);
    MMAY_EXPECT(diagnostic != NULL);
    if (diagnostic != NULL) {
        MMAY_EXPECT(strcmp(diagnostic->code, code) == 0);
        MMAY_EXPECT(strstr(diagnostic->message, message) != NULL);
    }
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_test_checker_imported_result_negatives(void) {
    static const MarimayTestImportSource private_sources[] = {
        {"acme/app", "app/main.my",
            "import { Hidden } from \"acme/results\";\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.my",
            "enum Hidden<T> { Value(T), }\n"}
    };
    static const MarimayTestImportSource arity_sources[] = {
        {"acme/app", "app/main.my",
            "import { Choice } from \"acme/results\";\n"
            "fn bad(value: Choice<int, string>): int { return 0; }\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.my",
            "public enum Choice<T> { Value(T), }\n"}
    };
    static const MarimayTestImportSource payload_sources[] = {
        {"acme/app", "app/main.my",
            "import { Choice } from \"acme/results\";\n"
            "fn bad(): Choice<int> { return Choice.Value(\"wrong\"); }\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.my",
            "public enum Choice<T> { Value(T), }\n"}
    };
    static const MarimayTestImportSource ambiguous_sources[] = {
        {"acme/app", "app/main.my",
            "import { Choice } from \"acme/alpha\";\n"
            "import { Choice } from \"acme/beta\";\n"
            "fn main(): int { return 0; }\n"},
        {"acme/alpha", "alpha/api.my",
            "public enum Choice<T> { Alpha(T), }\n"},
        {"acme/beta", "beta/api.my",
            "public enum Choice<T> { Beta(T), }\n"}
    };
    static const MarimayTestImportSource exhaustive_sources[] = {
        {"acme/app", "app/main.my",
            "import { Choice } from \"acme/results\";\n"
            "fn bad(value: Choice<int>): int {\n"
            "  return match value { Empty => 0, };\n"
            "}\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.my",
            "public enum Choice<T> { Empty, Value(T), }\n"}
    };
    static const MarimayTestImportSource wrong_namespace_sources[] = {
        {"acme/app", "app/main.my",
            "import { Choice } from \"acme/results\";\n"
            "import * as other from \"acme/other\";\n"
            "fn bad(value: Choice<int>): int {\n"
            "  return match value {\n"
            "    other.Value(_) => 1, _ => 0,\n"
            "  };\n"
            "}\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.my",
            "public enum Choice<T> { Empty, Value(T), }\n"},
        {"acme/other", "other/api.my",
            "public enum Other<T> { Value(T), }\n"}
    };
    mmay_test_checker_imported_result_negative(
        private_sources,
        sizeof(private_sources) / sizeof(private_sources[0]),
        MMAY_DIAGNOSTIC_PRIVATE_IMPORT,
        "package-private"
    );
    mmay_test_checker_imported_result_negative(
        arity_sources,
        sizeof(arity_sources) / sizeof(arity_sources[0]),
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "argument count"
    );
    mmay_test_checker_imported_result_negative(
        payload_sources,
        sizeof(payload_sources) / sizeof(payload_sources[0]),
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "string literal"
    );
    mmay_test_checker_imported_result_negative(
        ambiguous_sources,
        sizeof(ambiguous_sources) / sizeof(ambiguous_sources[0]),
        MMAY_DIAGNOSTIC_IMPORT_CONFLICT,
        "conflicts"
    );
    mmay_test_checker_imported_result_negative(
        exhaustive_sources,
        sizeof(exhaustive_sources) / sizeof(exhaustive_sources[0]),
        MMAY_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "Value(_)"
    );
    mmay_test_checker_imported_result_negative(
        wrong_namespace_sources,
        sizeof(wrong_namespace_sources) / sizeof(wrong_namespace_sources[0]),
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "does not expose"
    );
}

static void mmay_test_checker_imported_result_allocations(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayTestImportedResults test;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        if (mmay_test_imported_results_init(
                &test, mmay_fail_allocator(&state)
            )) {
            reached_success = true;
            mmay_test_imported_results_dispose(&test);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_checker_imported_results(void) {
    MarimayTestImportedResults test;
    MMAY_EXPECT(mmay_test_imported_results_init(
        &test, mmay_system_allocator()
    ));
    if (test.initialized) {
        MMAY_EXPECT(test.checked.diagnostics.length == 0);
        mmay_test_checker_imported_result_metadata(&test);
        mmay_test_imported_results_dispose(&test);
    }
    mmay_test_checker_imported_result_negatives();
    mmay_test_checker_imported_result_allocations();
}
