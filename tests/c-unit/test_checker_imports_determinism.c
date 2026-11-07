#include "test_checker_imports_support.h"

#include "mmay/bytecode/mir_lower.h"

static const char mmay_determinism_types[] =
    "public type User { public value: int, }\n"
    "public enum Level { Basic, Advanced(int), }\n";

static const char mmay_determinism_functions[] =
    "public fn makeUser(value: int): User {\n"
    "  return User { value: value };\n"
    "}\n"
    "public fn makeLevel(value: int): Level {\n"
    "  return Level.Advanced(value);\n"
    "}\n"
    "public fn score(user: User): int { return user.value; }\n"
    "public fn score(level: Level): int {\n"
    "  return match level {\n"
    "    Basic => 0,\n"
    "    Advanced(value) => value,\n"
    "  };\n"
    "}\n";

static const char mmay_determinism_evaluator[] =
    "import { User, Level, score as namedScore } from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: User): int {\n"
    "  return through(user) + namedScore(user);\n"
    "}\n"
    "public fn evaluateLevel(level: Level): int {\n"
    "  return through(level) + namedScore(level);\n"
    "}\n";

static const char mmay_determinism_app_first[] =
    "import { makeUser, makeLevel } from \"acme/store\";\n"
    "import { evaluateUser, evaluateLevel } from \"acme/eval\";\n"
    "fn main(): int {\n"
    "  return evaluateUser(makeUser(3)) + evaluateLevel(makeLevel(4));\n"
    "}\n";

static const char mmay_determinism_app_second[] =
    "import { evaluateUser, evaluateLevel } from \"acme/eval\";\n"
    "import { makeUser, makeLevel } from \"acme/store\";\n"
    "fn main(): int {\n"
    "  return evaluateUser(makeUser(3)) + evaluateLevel(makeLevel(4));\n"
    "}\n";

static bool mmay_determinism_encode(
    const MarimayTestImportSource *sources,
    size_t source_count,
    MarimayOwnedBytes *encoded
) {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    MarimayBytecodeProgram program;
    const MarimayMirFunction *functions[16] = {0};
    const MarimayCheckedFunction *entry = NULL;
    size_t entry_index = SIZE_MAX;
    size_t index = 0;
    bool accepted = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&program, 0, sizeof(program));
    memset(encoded, 0, sizeof(*encoded));
    if (!mmay_checker_import_fixture_init(&fixture, sources, source_count)
        || mmay_checker_import_fixture_check(
            &fixture, "acme/app", mmay_system_allocator(),
            mmay_check_graph_default_options(), &checked
        ) != MMAY_CHECK_OK || !checked.initialized
        || checked.diagnostics.length != 0 || checked.functions.length > 16) {
        goto cleanup;
    }
    entry = mmay_checker_import_function(&checked, "main", &entry_index);
    if (entry == NULL) {
        goto cleanup;
    }
    for (index = 0; index < checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked, index
        );
        if (function == NULL) {
            goto cleanup;
        }
        functions[index] = &function->mir;
    }
    if (mmay_mir_lower_bytecode_program(
            functions, checked.functions.length, entry->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(), &program
        ) != MMAY_MIR_BYTECODE_LOWER_OK
        || mmay_bytecode_encode_program(
            &program, mmay_system_allocator(), encoded
        ) != MMAY_BYTECODE_CODEC_OK) {
        goto cleanup;
    }
    accepted = true;
cleanup:
    if (program.initialized) {
        mmay_bytecode_program_dispose(&program);
    }
    if (checked.initialized) {
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_import_fixture_dispose(&fixture);
    if (!accepted) {
        mmay_owned_bytes_dispose(encoded);
    }
    return accepted;
}

void mmay_test_checker_import_determinism(void) {
    MarimayTestImportSource first[] = {
        {"acme/app", "app/main.my", mmay_determinism_app_first},
        {"acme/store", "store/functions.my", mmay_determinism_functions},
        {"acme/eval", "eval/evaluate.my", mmay_determinism_evaluator},
        {"acme/store", "store/types.my", mmay_determinism_types}
    };
    MarimayTestImportSource second[] = {
        {"acme/store", "store/types.my", mmay_determinism_types},
        {"acme/eval", "eval/evaluate.my", mmay_determinism_evaluator},
        {"acme/store", "store/functions.my", mmay_determinism_functions},
        {"acme/app", "app/main.my", mmay_determinism_app_second}
    };
    MarimayOwnedBytes first_encoded;
    MarimayOwnedBytes second_encoded;
    memset(&first_encoded, 0, sizeof(first_encoded));
    memset(&second_encoded, 0, sizeof(second_encoded));
    MMAY_EXPECT(mmay_determinism_encode(
        first, sizeof(first) / sizeof(first[0]), &first_encoded
    ));
    MMAY_EXPECT(mmay_determinism_encode(
        second, sizeof(second) / sizeof(second[0]), &second_encoded
    ));
    MMAY_EXPECT(first_encoded.length == second_encoded.length);
    if (first_encoded.length == second_encoded.length) {
        MMAY_EXPECT(first_encoded.length == 0 || memcmp(
            first_encoded.data, second_encoded.data, first_encoded.length
        ) == 0);
    }
    mmay_owned_bytes_dispose(&second_encoded);
    mmay_owned_bytes_dispose(&first_encoded);

    {
        static const MarimayTestImportSource diagnostic_first[] = {
            {"app", "app/main.my",
                "import { Missing } from \"lib\";\n"},
            {"lib", "lib/declarations.my",
                "public fn present(): int { return 1; }\n"}
        };
        static const MarimayTestImportSource diagnostic_second[] = {
            {"lib", "lib/declarations.my",
                "public fn present(): int { return 1; }\n"},
            {"app", "app/main.my",
                "import { Missing } from \"lib\";\n"}
        };
        MarimayCheckerImportFixture first_fixture;
        MarimayCheckerImportFixture second_fixture;
        MarimayCheckResult first_checked;
        MarimayCheckResult second_checked;
        const MarimayCheckDiagnostic *first_diagnostic = NULL;
        const MarimayCheckDiagnostic *second_diagnostic = NULL;
        memset(&first_fixture, 0, sizeof(first_fixture));
        memset(&second_fixture, 0, sizeof(second_fixture));
        memset(&first_checked, 0, sizeof(first_checked));
        memset(&second_checked, 0, sizeof(second_checked));
        MMAY_EXPECT(mmay_checker_import_fixture_init(
            &first_fixture, diagnostic_first, 2
        ));
        MMAY_EXPECT(mmay_checker_import_fixture_init(
            &second_fixture, diagnostic_second, 2
        ));
        MMAY_EXPECT(mmay_checker_import_fixture_check(
            &first_fixture, "app", mmay_system_allocator(),
            mmay_check_graph_default_options(), &first_checked
        ) == MMAY_CHECK_OK);
        MMAY_EXPECT(mmay_checker_import_fixture_check(
            &second_fixture, "app", mmay_system_allocator(),
            mmay_check_graph_default_options(), &second_checked
        ) == MMAY_CHECK_OK);
        first_diagnostic = mmay_check_diagnostic(&first_checked, 0);
        second_diagnostic = mmay_check_diagnostic(&second_checked, 0);
        MMAY_EXPECT(first_checked.diagnostics.length == 1);
        MMAY_EXPECT(second_checked.diagnostics.length == 1);
        MMAY_EXPECT(first_diagnostic != NULL);
        MMAY_EXPECT(second_diagnostic != NULL);
        if (first_diagnostic != NULL && second_diagnostic != NULL) {
            MMAY_EXPECT(strcmp(
                first_diagnostic->code, second_diagnostic->code
            ) == 0);
            MMAY_EXPECT(strcmp(
                first_diagnostic->message, second_diagnostic->message
            ) == 0);
            MMAY_EXPECT(first_diagnostic->start == second_diagnostic->start);
            MMAY_EXPECT(first_diagnostic->end == second_diagnostic->end);
            MMAY_EXPECT(strcmp(
                mmay_source_path(first_diagnostic->snapshot),
                mmay_source_path(second_diagnostic->snapshot)
            ) == 0);
        }
        mmay_check_result_dispose(&second_checked);
        mmay_check_result_dispose(&first_checked);
        mmay_checker_import_fixture_dispose(&second_fixture);
        mmay_checker_import_fixture_dispose(&first_fixture);
    }
}
