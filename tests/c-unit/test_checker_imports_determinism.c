#include "test_checker_imports_support.h"

#include "ruda/bytecode/mir_lower.h"

static const char ruda_determinism_types[] =
    "public type User { public value: int; }\n"
    "public enum Level { Basic, Advanced(int), }\n";

static const char ruda_determinism_functions[] =
    "public fn makeUser(value: int): User {\n"
    "  return User { value: value };\n"
    "}\n"
    "public fn makeLevel(value: int): Level {\n"
    "  return Level.Advanced(value);\n"
    "}\n"
    "public fn score(user: User): int { return user.value; }\n"
    "public fn score(level: Level): int {\n"
    "  return match level {\n"
    "    case Basic => 0;\n"
    "    case Advanced(value) => value;\n"
    "  };\n"
    "}\n";

static const char ruda_determinism_evaluator[] =
    "import { User, Level, score } from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: User): int {\n"
    "  return through(user) + score(user);\n"
    "}\n"
    "public fn evaluateLevel(level: Level): int {\n"
    "  return through(level) + score(level);\n"
    "}\n";

static const char ruda_determinism_app_first[] =
    "import { makeUser, makeLevel } from \"acme/store\";\n"
    "import { evaluateUser, evaluateLevel } from \"acme/eval\";\n"
    "fn main(): int {\n"
    "  return evaluateUser(makeUser(3)) + evaluateLevel(makeLevel(4));\n"
    "}\n";

static const char ruda_determinism_app_second[] =
    "import { evaluateUser, evaluateLevel } from \"acme/eval\";\n"
    "import { makeUser, makeLevel } from \"acme/store\";\n"
    "fn main(): int {\n"
    "  return evaluateUser(makeUser(3)) + evaluateLevel(makeLevel(4));\n"
    "}\n";

static bool ruda_determinism_encode(
    const RudaTestImportSource *sources,
    size_t source_count,
    RudaOwnedBytes *encoded
) {
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    RudaBytecodeProgram program;
    const RudaMirFunction *functions[16];
    const RudaCheckedFunction *entry = NULL;
    size_t entry_index = SIZE_MAX;
    size_t index = 0;
    bool accepted = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&program, 0, sizeof(program));
    memset(functions, 0, sizeof(functions));
    memset(encoded, 0, sizeof(*encoded));
    if (!ruda_checker_import_fixture_init(&fixture, sources, source_count)
        || ruda_checker_import_fixture_check(
            &fixture, "acme/app", ruda_system_allocator(),
            ruda_check_graph_default_options(), &checked
        ) != RUDA_CHECK_OK || !checked.initialized
        || checked.diagnostics.length != 0 || checked.functions.length > 16) {
        goto cleanup;
    }
    entry = ruda_checker_import_function(&checked, "main", &entry_index);
    if (entry == NULL) {
        goto cleanup;
    }
    for (index = 0; index < checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked, index
        );
        if (function == NULL) {
            goto cleanup;
        }
        functions[index] = &function->mir;
    }
    if (ruda_mir_lower_bytecode_program(
            functions, checked.functions.length, entry->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(), &program
        ) != RUDA_MIR_BYTECODE_LOWER_OK
        || ruda_bytecode_encode_program(
            &program, ruda_system_allocator(), encoded
        ) != RUDA_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    accepted = true;
cleanup:
    if (program.initialized) {
        ruda_bytecode_program_dispose(&program);
    }
    if (checked.initialized) {
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_import_fixture_dispose(&fixture);
    if (!accepted) {
        ruda_owned_bytes_dispose(encoded);
    }
    return accepted;
}

void ruda_test_checker_import_determinism(void) {
    RudaTestImportSource first[] = {
        {"acme/app", "app/main.ruda", ruda_determinism_app_first},
        {"acme/store", "store/functions.ruda", ruda_determinism_functions},
        {"acme/eval", "eval/evaluate.ruda", ruda_determinism_evaluator},
        {"acme/store", "store/types.ruda", ruda_determinism_types}
    };
    RudaTestImportSource second[] = {
        {"acme/store", "store/types.ruda", ruda_determinism_types},
        {"acme/eval", "eval/evaluate.ruda", ruda_determinism_evaluator},
        {"acme/store", "store/functions.ruda", ruda_determinism_functions},
        {"acme/app", "app/main.ruda", ruda_determinism_app_second}
    };
    RudaOwnedBytes first_encoded;
    RudaOwnedBytes second_encoded;
    memset(&first_encoded, 0, sizeof(first_encoded));
    memset(&second_encoded, 0, sizeof(second_encoded));
    RUDA_EXPECT(ruda_determinism_encode(
        first, sizeof(first) / sizeof(first[0]), &first_encoded
    ));
    RUDA_EXPECT(ruda_determinism_encode(
        second, sizeof(second) / sizeof(second[0]), &second_encoded
    ));
    RUDA_EXPECT(first_encoded.length == second_encoded.length);
    if (first_encoded.length == second_encoded.length) {
        RUDA_EXPECT(first_encoded.length == 0 || memcmp(
            first_encoded.data, second_encoded.data, first_encoded.length
        ) == 0);
    }
    ruda_owned_bytes_dispose(&second_encoded);
    ruda_owned_bytes_dispose(&first_encoded);

    {
        static const RudaTestImportSource diagnostic_first[] = {
            {"app", "app/main.ruda",
                "import { Missing } from \"lib\";\n"},
            {"lib", "lib/declarations.ruda",
                "public fn present(): int { return 1; }\n"}
        };
        static const RudaTestImportSource diagnostic_second[] = {
            {"lib", "lib/declarations.ruda",
                "public fn present(): int { return 1; }\n"},
            {"app", "app/main.ruda",
                "import { Missing } from \"lib\";\n"}
        };
        RudaCheckerImportFixture first_fixture;
        RudaCheckerImportFixture second_fixture;
        RudaCheckResult first_checked;
        RudaCheckResult second_checked;
        const RudaCheckDiagnostic *first_diagnostic = NULL;
        const RudaCheckDiagnostic *second_diagnostic = NULL;
        memset(&first_fixture, 0, sizeof(first_fixture));
        memset(&second_fixture, 0, sizeof(second_fixture));
        memset(&first_checked, 0, sizeof(first_checked));
        memset(&second_checked, 0, sizeof(second_checked));
        RUDA_EXPECT(ruda_checker_import_fixture_init(
            &first_fixture, diagnostic_first, 2
        ));
        RUDA_EXPECT(ruda_checker_import_fixture_init(
            &second_fixture, diagnostic_second, 2
        ));
        RUDA_EXPECT(ruda_checker_import_fixture_check(
            &first_fixture, "app", ruda_system_allocator(),
            ruda_check_graph_default_options(), &first_checked
        ) == RUDA_CHECK_OK);
        RUDA_EXPECT(ruda_checker_import_fixture_check(
            &second_fixture, "app", ruda_system_allocator(),
            ruda_check_graph_default_options(), &second_checked
        ) == RUDA_CHECK_OK);
        first_diagnostic = ruda_check_diagnostic(&first_checked, 0);
        second_diagnostic = ruda_check_diagnostic(&second_checked, 0);
        RUDA_EXPECT(first_checked.diagnostics.length == 1);
        RUDA_EXPECT(second_checked.diagnostics.length == 1);
        RUDA_EXPECT(first_diagnostic != NULL);
        RUDA_EXPECT(second_diagnostic != NULL);
        if (first_diagnostic != NULL && second_diagnostic != NULL) {
            RUDA_EXPECT(strcmp(
                first_diagnostic->code, second_diagnostic->code
            ) == 0);
            RUDA_EXPECT(strcmp(
                first_diagnostic->message, second_diagnostic->message
            ) == 0);
            RUDA_EXPECT(first_diagnostic->start == second_diagnostic->start);
            RUDA_EXPECT(first_diagnostic->end == second_diagnostic->end);
            RUDA_EXPECT(strcmp(
                ruda_source_path(first_diagnostic->snapshot),
                ruda_source_path(second_diagnostic->snapshot)
            ) == 0);
        }
        ruda_check_result_dispose(&second_checked);
        ruda_check_result_dispose(&first_checked);
        ruda_checker_import_fixture_dispose(&second_fixture);
        ruda_checker_import_fixture_dispose(&first_fixture);
    }
}
