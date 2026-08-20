#include "test_imported_results_support.h"

#include "ruda/generated/diagnostic_codes.h"

static bool ruda_test_imported_result_name(
    const RudaOwnedBytes *name,
    const char *text
) {
    size_t length = strlen(text);
    return name != NULL && name->length == length
        && memcmp(name->data, text, length) == 0;
}

static void ruda_test_checker_imported_result_metadata(
    const RudaTestImportedResults *test
) {
    const RudaCheckedEnum *choice = NULL;
    bool saw_choice_array = false;
    bool saw_choice_payload_array = false;
    size_t index = 0;
    for (index = 0; index < test->checked.enums.length; index += 1) {
        const RudaCheckedEnum *enum_value = ruda_checked_enum(
            &test->checked, index
        );
        if (enum_value != NULL && ruda_test_imported_result_name(
                &enum_value->name, "Choice"
            )) {
            choice = enum_value;
            break;
        }
    }
    RUDA_EXPECT(choice != NULL);
    if (choice != NULL) {
        RUDA_EXPECT(choice->is_public);
        RUDA_EXPECT(choice->type_parameters.length == 1);
        RUDA_EXPECT(choice->variants.length == 2);
    }
    for (index = 0; index < test->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &test->checked, index
        );
        size_t enum_index = 0;
        if (function == NULL) {
            continue;
        }
        for (enum_index = 0; enum_index < function->mir.enums.length;
            enum_index += 1) {
            const RudaMirEnum *layout = ruda_vector_at_const(
                &function->mir.enums, enum_index
            );
            const RudaTypeDescriptor *descriptor = layout == NULL
                ? NULL : ruda_type_descriptor(
                    &test->checked.types, layout->type
                );
            if (choice != NULL && descriptor != NULL
                && ruda_symbol_id_equal(
                    descriptor->symbol, choice->symbol
                ) && descriptor->argument_count == 1
                && ruda_type_kind(
                    &test->checked.types, descriptor->arguments[0]
                ) == RUDA_TYPE_KIND_ARRAY) {
                const RudaMirEnumPayload *payload = layout->payload_count == 1
                    ? ruda_vector_at_const(
                        &function->mir.enum_payloads,
                        layout->payload_start
                    ) : NULL;
                saw_choice_array = true;
                saw_choice_payload_array = payload != NULL
                    && payload->type == descriptor->arguments[0];
            }
        }
    }
    RUDA_EXPECT(saw_choice_array);
    RUDA_EXPECT(saw_choice_payload_array);
}

static void ruda_test_checker_imported_result_negative(
    const RudaTestImportSource *sources,
    size_t source_count,
    const char *code,
    const char *message
) {
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    const RudaCheckDiagnostic *diagnostic = NULL;
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_import_fixture_init(
        &fixture, sources, source_count
    ));
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture,
        "acme/app",
        ruda_system_allocator(),
        ruda_check_graph_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized);
    RUDA_EXPECT(checked.diagnostics.length == 1);
    diagnostic = ruda_check_diagnostic(&checked, 0);
    RUDA_EXPECT(diagnostic != NULL);
    if (diagnostic != NULL) {
        RUDA_EXPECT(strcmp(diagnostic->code, code) == 0);
        RUDA_EXPECT(strstr(diagnostic->message, message) != NULL);
    }
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_test_checker_imported_result_negatives(void) {
    static const RudaTestImportSource private_sources[] = {
        {"acme/app", "app/main.ruda",
            "import { Hidden } from \"acme/results\";\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.ruda",
            "enum Hidden<T> { Value(T), }\n"}
    };
    static const RudaTestImportSource arity_sources[] = {
        {"acme/app", "app/main.ruda",
            "import { Choice } from \"acme/results\";\n"
            "fn bad(value: Choice<int, string>): int { return 0; }\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.ruda",
            "public enum Choice<T> { Value(T), }\n"}
    };
    static const RudaTestImportSource payload_sources[] = {
        {"acme/app", "app/main.ruda",
            "import { Choice } from \"acme/results\";\n"
            "fn bad(): Choice<int> { return Choice.Value(\"wrong\"); }\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.ruda",
            "public enum Choice<T> { Value(T), }\n"}
    };
    static const RudaTestImportSource ambiguous_sources[] = {
        {"acme/app", "app/main.ruda",
            "import { Choice } from \"acme/alpha\";\n"
            "import { Choice } from \"acme/beta\";\n"
            "fn main(): int { return 0; }\n"},
        {"acme/alpha", "alpha/api.ruda",
            "public enum Choice<T> { Alpha(T), }\n"},
        {"acme/beta", "beta/api.ruda",
            "public enum Choice<T> { Beta(T), }\n"}
    };
    static const RudaTestImportSource exhaustive_sources[] = {
        {"acme/app", "app/main.ruda",
            "import { Choice } from \"acme/results\";\n"
            "fn bad(value: Choice<int>): int {\n"
            "  return match value { case Empty => 0; };\n"
            "}\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.ruda",
            "public enum Choice<T> { Empty, Value(T), }\n"}
    };
    static const RudaTestImportSource wrong_namespace_sources[] = {
        {"acme/app", "app/main.ruda",
            "import { Choice } from \"acme/results\";\n"
            "import * as other from \"acme/other\";\n"
            "fn bad(value: Choice<int>): int {\n"
            "  return match value {\n"
            "    case other.Value(_) => 1; case _ => 0;\n"
            "  };\n"
            "}\n"
            "fn main(): int { return 0; }\n"},
        {"acme/results", "results/api.ruda",
            "public enum Choice<T> { Empty, Value(T), }\n"},
        {"acme/other", "other/api.ruda",
            "public enum Other<T> { Value(T), }\n"}
    };
    ruda_test_checker_imported_result_negative(
        private_sources,
        sizeof(private_sources) / sizeof(private_sources[0]),
        RUDA_DIAGNOSTIC_PRIVATE_IMPORT,
        "package-private"
    );
    ruda_test_checker_imported_result_negative(
        arity_sources,
        sizeof(arity_sources) / sizeof(arity_sources[0]),
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "argument count"
    );
    ruda_test_checker_imported_result_negative(
        payload_sources,
        sizeof(payload_sources) / sizeof(payload_sources[0]),
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "string literal"
    );
    ruda_test_checker_imported_result_negative(
        ambiguous_sources,
        sizeof(ambiguous_sources) / sizeof(ambiguous_sources[0]),
        RUDA_DIAGNOSTIC_IMPORT_CONFLICT,
        "conflicts"
    );
    ruda_test_checker_imported_result_negative(
        exhaustive_sources,
        sizeof(exhaustive_sources) / sizeof(exhaustive_sources[0]),
        RUDA_DIAGNOSTIC_NON_EXHAUSTIVE_MATCH,
        "Value(_)"
    );
    ruda_test_checker_imported_result_negative(
        wrong_namespace_sources,
        sizeof(wrong_namespace_sources) / sizeof(wrong_namespace_sources[0]),
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "does not expose"
    );
}

static void ruda_test_checker_imported_result_allocations(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaTestImportedResults test;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        if (ruda_test_imported_results_init(
                &test, ruda_fail_allocator(&state)
            )) {
            reached_success = true;
            ruda_test_imported_results_dispose(&test);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_checker_imported_results(void) {
    RudaTestImportedResults test;
    RUDA_EXPECT(ruda_test_imported_results_init(
        &test, ruda_system_allocator()
    ));
    if (test.initialized) {
        RUDA_EXPECT(test.checked.diagnostics.length == 0);
        ruda_test_checker_imported_result_metadata(&test);
        ruda_test_imported_results_dispose(&test);
    }
    ruda_test_checker_imported_result_negatives();
    ruda_test_checker_imported_result_allocations();
}
