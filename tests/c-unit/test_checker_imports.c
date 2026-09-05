#include "test_checker_imports_support.h"

#include "mmay/generated/diagnostic_codes.h"

static const char mmay_import_store_types[] =
    "public type User { public value: int, }\n"
    "public enum Level { Basic, Advanced(int), }\n";

static const char mmay_import_store_functions[] =
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

static const char mmay_import_application[] =
    "import { makeUser, makeLevel } from \"acme/store\";\n"
    "import { evaluateUser, evaluateLevel } from \"acme/eval\";\n"
    "fn main(): int {\n"
    "  return evaluateUser(makeUser(3)) + evaluateLevel(makeLevel(4));\n"
    "}\n";

static const char mmay_import_evaluator_type_only[] =
    "import { User, Level } from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: User): int {\n"
    "  return through(user) + user.score();\n"
    "}\n"
    "public fn evaluateLevel(level: Level): int {\n"
    "  return through(level) + level.score();\n"
    "}\n";

static const char mmay_import_evaluator_named[] =
    "import { User, Level, score as namedScore } from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: User): int {\n"
    "  return through(user) + namedScore(user);\n"
    "}\n"
    "public fn evaluateLevel(level: Level): int {\n"
    "  return through(level) + namedScore(level);\n"
    "}\n";

static const char mmay_import_evaluator_namespace[] =
    "import * as store from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: store.User): int {\n"
    "  return through(user) + store.score(user);\n"
    "}\n"
    "public fn evaluateLevel(level: store.Level): int {\n"
    "  return through(level) + store.score(level);\n"
    "}\n";

static bool mmay_checker_import_build(
    MarimayCheckerImportFixture *fixture,
    const char *evaluator
) {
    MarimayTestImportSource sources[] = {
        {"acme/app", "app/main.my", mmay_import_application},
        {"acme/store", "store/functions.my",
            mmay_import_store_functions},
        {"acme/eval", "eval/evaluate.my", evaluator},
        {"acme/store", "store/types.my", mmay_import_store_types}
    };
    return mmay_checker_import_fixture_init(
        fixture, sources, sizeof(sources) / sizeof(sources[0])
    );
}

static bool mmay_checker_import_symbol_equal(
    MarimaySymbolId left,
    MarimaySymbolId right
) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

static void mmay_checker_import_expect_form(
    const char *evaluator,
    MarimaySymbolId *user_score,
    MarimaySymbolId *level_score,
    MarimaySymbolId *evaluate_user,
    MarimaySymbolId *evaluate_level,
    MarimayOwnedBytes *canonical_mir
) {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *main_function = NULL;
    const MarimayCheckedFunction *user_function = NULL;
    const MarimayCheckedFunction *level_function = NULL;
    MarimayCheckInterpretOptions interpret_options;
    MarimayMirValue value;
    MarimayOwnedBytes mir_bytes;
    MarimayCheckStatus status;
    size_t main_index = SIZE_MAX;
    size_t user_index = SIZE_MAX;
    size_t level_index = SIZE_MAX;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&value, 0, sizeof(value));
    memset(&mir_bytes, 0, sizeof(mir_bytes));
    MMAY_EXPECT(mmay_checker_import_build(&fixture, evaluator));
    status = mmay_checker_import_fixture_check(
        &fixture,
        "acme/app",
        mmay_system_allocator(),
        mmay_check_graph_default_options(),
        &checked
    );
    if (status != MMAY_CHECK_OK) {
        fprintf(stderr, "import graph check status: %d\n", (int)status);
    }
    MMAY_EXPECT(status == MMAY_CHECK_OK);
    if (!checked.initialized) {
        mmay_checker_import_fixture_dispose(&fixture);
        return;
    }
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.records.length == 1);
    MMAY_EXPECT(checked.enums.length == 1);
    MMAY_EXPECT(checked.interfaces.length == 1);
    MMAY_EXPECT(checked.functions.length == 8);
    MMAY_EXPECT(mmay_checker_import_mir_bytes(
        &checked, mmay_system_allocator(), &mir_bytes
    ));
    if (canonical_mir->data == NULL) {
        *canonical_mir = mir_bytes;
        memset(&mir_bytes, 0, sizeof(mir_bytes));
    } else {
        MMAY_EXPECT(mir_bytes.length == canonical_mir->length);
        if (mir_bytes.length == canonical_mir->length) {
            size_t difference = 0;
            while (difference < mir_bytes.length
                && mir_bytes.data[difference]
                    == canonical_mir->data[difference]) {
                difference += 1;
            }
            if (difference != mir_bytes.length) {
                fprintf(stderr,
                    "import MIR differs at byte %zu: %u != %u\n",
                    difference,
                    (unsigned int)mir_bytes.data[difference],
                    (unsigned int)canonical_mir->data[difference]);
            }
            MMAY_EXPECT(mir_bytes.length == 0 || memcmp(
                mir_bytes.data, canonical_mir->data, mir_bytes.length
            ) == 0);
        }
    }
    main_function = mmay_checker_import_function(
        &checked, "main", &main_index
    );
    user_function = mmay_checker_import_function(
        &checked, "evaluateUser", &user_index
    );
    level_function = mmay_checker_import_function(
        &checked, "evaluateLevel", &level_index
    );
    MMAY_EXPECT(main_function != NULL);
    MMAY_EXPECT(user_function != NULL);
    MMAY_EXPECT(level_function != NULL);
    if (main_function != NULL && user_function != NULL
        && level_function != NULL) {
        const MarimayMirCall *user_direct = mmay_vector_at_const(
            &user_function->mir.calls, 1
        );
        const MarimayMirCall *level_direct = mmay_vector_at_const(
            &level_function->mir.calls, 1
        );
        MMAY_EXPECT(user_function->mir.calls.length == 2);
        MMAY_EXPECT(level_function->mir.calls.length == 2);
        MMAY_EXPECT(user_direct != NULL);
        MMAY_EXPECT(level_direct != NULL);
        if (user_direct != NULL && level_direct != NULL) {
            if (user_score->bytes[0] == 0) {
                *user_score = user_direct->callee;
                *level_score = level_direct->callee;
                *evaluate_user = user_function->mir.symbol;
                *evaluate_level = level_function->mir.symbol;
            } else {
                MMAY_EXPECT(mmay_checker_import_symbol_equal(
                    *user_score, user_direct->callee
                ));
                MMAY_EXPECT(mmay_checker_import_symbol_equal(
                    *level_score, level_direct->callee
                ));
                MMAY_EXPECT(mmay_checker_import_symbol_equal(
                    *evaluate_user, user_function->mir.symbol
                ));
                MMAY_EXPECT(mmay_checker_import_symbol_equal(
                    *evaluate_level, level_function->mir.symbol
                ));
            }
        }
        interpret_options = mmay_check_interpret_default_options();
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked,
            main_index,
            NULL,
            0,
            mmay_system_allocator(),
            interpret_options,
            &value
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(value.type == MMAY_TYPE_I64);
        MMAY_EXPECT(value.as.i64 == 14);
    }
    mmay_owned_bytes_dispose(&mir_bytes);
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_checker_import_forms(void) {
    MarimaySymbolId user_score;
    MarimaySymbolId level_score;
    MarimaySymbolId evaluate_user;
    MarimaySymbolId evaluate_level;
    MarimayOwnedBytes canonical_mir;
    memset(&user_score, 0, sizeof(user_score));
    memset(&level_score, 0, sizeof(level_score));
    memset(&evaluate_user, 0, sizeof(evaluate_user));
    memset(&evaluate_level, 0, sizeof(evaluate_level));
    memset(&canonical_mir, 0, sizeof(canonical_mir));
    mmay_checker_import_expect_form(
        mmay_import_evaluator_type_only,
        &user_score,
        &level_score,
        &evaluate_user,
        &evaluate_level,
        &canonical_mir
    );
    mmay_checker_import_expect_form(
        mmay_import_evaluator_named,
        &user_score,
        &level_score,
        &evaluate_user,
        &evaluate_level,
        &canonical_mir
    );
    mmay_checker_import_expect_form(
        mmay_import_evaluator_namespace,
        &user_score,
        &level_score,
        &evaluate_user,
        &evaluate_level,
        &canonical_mir
    );
    mmay_owned_bytes_dispose(&canonical_mir);
}

static void mmay_checker_import_expect_diagnostic(
    const MarimayTestImportSource *sources,
    size_t source_count,
    const char *root,
    const char *code,
    const char *marked
) {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture, sources, source_count
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture,
        root,
        mmay_system_allocator(),
        mmay_check_graph_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(
            &checked, 0
        );
        MMAY_EXPECT(checked.diagnostics.length == 1);
        MMAY_EXPECT(checked.functions.length == 0);
        MMAY_EXPECT(checked.constants.length == 0);
        MMAY_EXPECT(checked.type_aliases.length == 0);
        MMAY_EXPECT(checked.records.length == 0);
        MMAY_EXPECT(checked.enums.length == 0);
        MMAY_EXPECT(checked.interfaces.length == 0);
        MMAY_EXPECT(checked.symbols.length == 0);
        MMAY_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            const unsigned char *bytes = mmay_source_bytes(
                diagnostic->snapshot
            );
            MMAY_EXPECT(strcmp(diagnostic->code, code) == 0);
            MMAY_EXPECT(diagnostic->end >= diagnostic->start);
            if (diagnostic->end < diagnostic->start
                || diagnostic->end - diagnostic->start != strlen(marked)) {
                fprintf(stderr,
                    "import diagnostic span mismatch: code=%s marked=%s "
                    "start=%zu end=%zu\n",
                    code, marked, diagnostic->start, diagnostic->end);
            }
            MMAY_EXPECT(diagnostic->end - diagnostic->start
                == strlen(marked));
            if (bytes != NULL && diagnostic->end >= diagnostic->start
                && diagnostic->end - diagnostic->start == strlen(marked)) {
                MMAY_EXPECT(memcmp(
                    bytes + diagnostic->start, marked, strlen(marked)
                ) == 0);
            }
            if (strcmp(code, MMAY_DIAGNOSTIC_PACKAGE_CYCLE) == 0) {
                MMAY_EXPECT(strstr(
                    diagnostic->message, "a -> b -> a"
                ) != NULL);
            }
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_checker_import_diagnostics(void) {
    static const MarimayTestImportSource unknown_package[] = {
        {"app", "app.my", "import * as missing from \"missing\";\n"}
    };
    static const MarimayTestImportSource unknown_item[] = {
        {"app", "app.my", "import { Missing } from \"lib\";\n"},
        {"lib", "lib.my", "public fn present(): int { return 1; }\n"}
    };
    static const MarimayTestImportSource private_item[] = {
        {"app", "app.my", "import { hidden } from \"lib\";\n"},
        {"lib", "lib.my", "fn hidden(): int { return 1; }\n"}
    };
    static const MarimayTestImportSource conflict[] = {
        {"app", "app.my",
            "import { first as same, second as same } from \"lib\";\n"},
        {"lib", "lib.my",
            "public fn first(): int { return 1; }\n"
            "public fn second(): int { return 2; }\n"}
    };
    static const MarimayTestImportSource cycle[] = {
        {"a", "a.my", "import * as b from \"b\";\n"},
        {"b", "b.my", "import * as a from \"a\";\n"}
    };
    static const MarimayTestImportSource local_conflict[] = {
        {"app", "app.my",
            "import { present as local } from \"lib\";\n"
            "fn local(): int { return 1; }\n"},
        {"lib", "lib.my", "public fn present(): int { return 1; }\n"}
    };
    static const MarimayTestImportSource multi_file_local_conflict[] = {
        {"app", "app/imports.my",
            "import { present as local } from \"lib\";\n"},
        {"app", "app/declaration.my",
            "fn local(): int { return 1; }\n"},
        {"lib", "lib.my", "public fn present(): int { return 1; }\n"}
    };
    static const MarimayTestImportSource private_field[] = {
        {"app", "app.my",
            "import { User, makeUser } from \"lib\";\n"
            "fn read(): int { const user = makeUser(1); return user.hidden; }\n"},
        {"lib", "lib.my",
            "public type User { public visible: int, hidden: int, }\n"
            "public fn makeUser(value: int): User {\n"
            "  return User { visible: value, hidden: value };\n"
            "}\n"}
    };
    static const MarimayTestImportSource type_only_free[] = {
        {"app", "app.my",
            "import { User } from \"lib\";\n"
            "fn bad(user: User): int { return score(user); }\n"},
        {"lib", "lib.my",
            "public type User { public value: int, }\n"
            "public fn score(user: User): int { return user.value; }\n"}
    };
    static const MarimayTestImportSource unknown_namespace_member[] = {
        {"app", "app.my",
            "import * as library from \"lib\";\n"
            "fn bad(): int { return library.missing(); }\n"},
        {"lib", "lib.my", "public fn present(): int { return 1; }\n"}
    };
    static const MarimayTestImportSource private_namespace_member[] = {
        {"app", "app.my",
            "import * as library from \"lib\";\n"
            "fn bad(): int { return library.hidden(); }\n"},
        {"lib", "lib.my", "fn hidden(): int { return 1; }\n"}
    };
    static const MarimayTestImportSource namespace_free_absent[] = {
        {"app", "app.my",
            "import * as library from \"lib\";\n"
            "fn bad(user: library.User): int { return score(user); }\n"},
        {"lib", "lib.my",
            "public type User { public value: int, }\n"
            "public fn score(user: User): int { return user.value; }\n"}
    };
    static const MarimayTestImportSource private_namespace_type[] = {
        {"app", "app.my",
            "import * as library from \"lib\";\n"
            "fn bad(user: library.Hidden): int { return 0; }\n"},
        {"lib", "lib.my", "type Hidden { public value: int, }\n"}
    };
    mmay_checker_import_expect_diagnostic(
        unknown_package, 1, "app", MMAY_DIAGNOSTIC_UNKNOWN_PACKAGE,
        "\"missing\""
    );
    mmay_checker_import_expect_diagnostic(
        unknown_item, 2, "app", MMAY_DIAGNOSTIC_UNKNOWN_IMPORT, "Missing"
    );
    mmay_checker_import_expect_diagnostic(
        private_item, 2, "app", MMAY_DIAGNOSTIC_PRIVATE_IMPORT, "hidden"
    );
    mmay_checker_import_expect_diagnostic(
        conflict, 2, "app", MMAY_DIAGNOSTIC_IMPORT_CONFLICT, "same"
    );
    mmay_checker_import_expect_diagnostic(
        cycle, 2, "a", MMAY_DIAGNOSTIC_PACKAGE_CYCLE, "\"a\""
    );
    mmay_checker_import_expect_diagnostic(
        local_conflict, 2, "app", MMAY_DIAGNOSTIC_IMPORT_CONFLICT, "local"
    );
    mmay_checker_import_expect_diagnostic(
        multi_file_local_conflict, 3, "app",
        MMAY_DIAGNOSTIC_IMPORT_CONFLICT, "local"
    );
    mmay_checker_import_expect_diagnostic(
        private_field, 2, "app", MMAY_DIAGNOSTIC_PRIVATE_IMPORT, "hidden"
    );
    mmay_checker_import_expect_diagnostic(
        type_only_free, 2, "app", MMAY_DIAGNOSTIC_UNKNOWN_NAME, "score"
    );
    mmay_checker_import_expect_diagnostic(
        unknown_namespace_member, 2, "app",
        MMAY_DIAGNOSTIC_UNKNOWN_IMPORT, "missing"
    );
    mmay_checker_import_expect_diagnostic(
        private_namespace_member, 2, "app",
        MMAY_DIAGNOSTIC_PRIVATE_IMPORT, "hidden"
    );
    mmay_checker_import_expect_diagnostic(
        namespace_free_absent, 2, "app",
        MMAY_DIAGNOSTIC_UNKNOWN_NAME, "score"
    );
    mmay_checker_import_expect_diagnostic(
        private_namespace_type, 2, "app",
        MMAY_DIAGNOSTIC_PRIVATE_IMPORT, "Hidden"
    );
}

static void mmay_checker_import_aliases_and_public_field(void) {
    static const MarimayTestImportSource sources[] = {
        {"app", "app.my",
            "import { User as StoredUser, makeUser as create, score as value } "
            "from \"lib\";\n"
            "import * as library from \"lib\";\n"
            "fn named(user: StoredUser): int { return value(user); }\n"
            "fn qualified(user: library.User): int {\n"
            "  return library.score(user);\n"
            "}\n"
            "fn main(): int {\n"
            "  const user = create(5);\n"
            "  return named(user) + qualified(user) + user.visible;\n"
            "}\n"},
        {"lib", "lib.my",
            "public type User { public visible: int, hidden: int, }\n"
            "public fn makeUser(value: int): User {\n"
            "  return User { visible: value, hidden: value };\n"
            "}\n"
            "public fn score(user: User): int { return user.visible; }\n"}
    };
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    MarimayMirValue value;
    size_t main_index = SIZE_MAX;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&value, 0, sizeof(value));
    MMAY_EXPECT(mmay_checker_import_fixture_init(&fixture, sources, 2));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked
    ) == MMAY_CHECK_OK);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(mmay_checker_import_function(
            &checked, "main", &main_index
        ) != NULL);
        MMAY_EXPECT(mmay_check_interpret_function(
            &checked, main_index, NULL, 0, mmay_system_allocator(),
            mmay_check_interpret_default_options(), &value
        ) == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(value.type == MMAY_TYPE_I64);
        MMAY_EXPECT(value.as.i64 == 15);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_checker_import_foreign_extension_excluded(void) {
    static const MarimayTestImportSource sources[] = {
        {"app", "app.my",
            "import { User } from \"lib\";\n"
            "fn extra(user: User): int { return 9; }\n"
            "fn bad(user: User): int { return user.extra(); }\n"},
        {"lib", "lib.my", "public type User { public value: int, }\n"}
    };
    mmay_checker_import_expect_diagnostic(
        sources, 2, "app", MMAY_DIAGNOSTIC_TYPE_MISMATCH, "extra"
    );
}

static void mmay_checker_import_nominal_ownership_is_exact(void) {
    static const MarimayTestImportSource sources[] = {
        {"app", "app.my",
            "import * as left from \"left\";\n"
            "import * as right from \"right\";\n"
            "fn bad(user: left.User): int { return user.score(); }\n"},
        {"left", "left.my", "public type User { public value: int, }\n"},
        {"right", "right.my",
            "public type User { public value: int, }\n"
            "public fn score(user: User): int { return user.value; }\n"}
    };
    mmay_checker_import_expect_diagnostic(
        sources, 3, "app", MMAY_DIAGNOSTIC_TYPE_MISMATCH, "user"
    );
}

static void mmay_checker_import_interface_mismatch(void) {
    static const MarimayTestImportSource sources[] = {
        {"app", "app.my",
            "import { User } from \"lib\";\n"
            "interface Scored { score(): int; }\n"
            "fn through(value: Scored): int { return value.score(); }\n"
            "fn bad(user: User): int { return through(user); }\n"},
        {"lib", "lib.my", "public type User { public value: int, }\n"}
    };
    mmay_checker_import_expect_diagnostic(
        sources, 2, "app", MMAY_DIAGNOSTIC_TYPE_MISMATCH, "user"
    );
}

static void mmay_checker_import_limits(void) {
    static const MarimayTestImportSource sources[] = {
        {"app", "app.my",
            "import * as library from \"lib\";\n"
            "import { value, other } from \"lib\";\n"
            "fn local(): int { return 0; }\n"},
        {"lib", "lib.my",
            "public fn value(): int { return 1; }\n"
            "public fn other(): int { return 2; }\n"}
    };
    MarimayCheckerImportFixture fixture;
    MarimayCheckGraphOptions options = mmay_check_graph_default_options();
    MarimayCheckResult checked;
    MMAY_EXPECT(mmay_checker_import_fixture_init(&fixture, sources, 2));
    options.max_packages = 1;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_graph_default_options();
    options.max_dependency_depth = 1;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_graph_default_options();
    options.max_total_imports = 1;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_graph_default_options();
    options.max_imports_per_file = 1;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_graph_default_options();
    options.max_items_per_import = 1;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_graph_default_options();
    options.max_total_bindings = 1;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_graph_default_options();
    options.package_options.max_package_name_bytes = 2;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_graph_default_options();
    options.package_options.max_functions = 1;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_graph_default_options();
    options.package_options.max_symbols = 2;
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(), options, &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_checker_import_diagnostic_allocation_case(
    const MarimayTestImportSource *sources,
    size_t source_count,
    const char *expected_code
) {
    MarimayCheckerImportFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture, sources, source_count
    ));
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_checker_import_fixture_check(
            &fixture, "app", allocator,
            mmay_check_graph_default_options(), &checked
        );
        if (status == MMAY_CHECK_OK) {
            const MarimayCheckDiagnostic *diagnostic =
                mmay_check_diagnostic(&checked, 0);
            reached_success = true;
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == 1);
            MMAY_EXPECT(diagnostic != NULL);
            if (diagnostic != NULL) {
                MMAY_EXPECT(strcmp(diagnostic->code, expected_code) == 0);
            }
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_checker_import_diagnostic_allocation_failures(void) {
    static const MarimayTestImportSource cycle[] = {
        {"app", "app.my", "import * as lib from \"lib\";\n"},
        {"lib", "lib.my", "import * as app from \"app\";\n"}
    };
    static const MarimayTestImportSource private_field[] = {
        {"app", "app.my",
            "import { User, makeUser } from \"lib\";\n"
            "fn read(): int { const user = makeUser(1); return user.hidden; }\n"},
        {"lib", "lib.my",
            "public type User { public visible: int, hidden: int, }\n"
            "public fn makeUser(value: int): User {\n"
            "  return User { visible: value, hidden: value };\n"
            "}\n"}
    };
    mmay_checker_import_diagnostic_allocation_case(
        cycle, 2, MMAY_DIAGNOSTIC_PACKAGE_CYCLE
    );
    mmay_checker_import_diagnostic_allocation_case(
        private_field, 2, MMAY_DIAGNOSTIC_PRIVATE_IMPORT
    );
}

static void mmay_checker_import_allocation_failures(void) {
    MarimayCheckerImportFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_checker_import_build(
        &fixture, mmay_import_evaluator_named
    ));
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        allocator = mmay_fail_allocator(&state);
        status = mmay_checker_import_fixture_check(
            &fixture, "acme/app", allocator,
            mmay_check_graph_default_options(), &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == 0);
            mmay_check_result_dispose(&checked);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_checker_import_mir_allocation_failures(void) {
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_build(
        &fixture, mmay_import_evaluator_named
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "acme/app", mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked
    ) == MMAY_CHECK_OK);
    for (failure_index = 0; checked.initialized && failure_index < 4096;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayOwnedBytes bytes;
        bool accepted = false;
        memset(&bytes, 0, sizeof(bytes));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        accepted = mmay_checker_import_mir_bytes(
            &checked, mmay_fail_allocator(&state), &bytes
        );
        if (accepted) {
            reached_success = true;
            MMAY_EXPECT(bytes.length != 0);
        }
        mmay_owned_bytes_dispose(&bytes);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void mmay_checker_import_std_convert(void) {
    static const MarimayTestImportSource sources[] = {
        {
            "app", "app.my",
            "import { formatF64, formatF32 as binary32 } from \"std/convert\";\n"
            "fn rendered(): string { return formatF64(415.0); }\n"
            "fn rendered32(value: f32): string { return binary32(value); }\n"
        },
        {
            "std/convert", "std/convert/convert.my",
            "public extern \"c\" fn formatF32(value: f32): string;\n"
            "public extern \"c\" fn formatF64(value: f64): string;\n"
        }
    };
    MarimayCheckerImportFixture fixture;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *rendered = NULL;
    const MarimayCheckedFunction *rendered32 = NULL;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &fixture, sources, sizeof(sources) / sizeof(sources[0])
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &fixture, "app", mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        rendered = mmay_checker_import_function(&checked, "rendered", NULL);
        rendered32 = mmay_checker_import_function(&checked, "rendered32", NULL);
        MMAY_EXPECT(rendered != NULL);
        MMAY_EXPECT(rendered32 != NULL);
        if (rendered != NULL) {
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &rendered->mir, MMAY_MIR_FORMAT_F64
            ) == 1);
            MMAY_EXPECT(rendered->mir.calls.length == 0);
            MMAY_EXPECT(mmay_mir_verify(&rendered->mir) == MMAY_MIR_VERIFY_OK);
        }
        if (rendered32 != NULL) {
            MMAY_EXPECT(mmay_test_mir_instruction_kind_count(
                &rendered32->mir, MMAY_MIR_FORMAT_F32
            ) == 1);
            MMAY_EXPECT(rendered32->mir.calls.length == 0);
            MMAY_EXPECT(mmay_mir_verify(&rendered32->mir) == MMAY_MIR_VERIFY_OK);
        }
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_import_fixture_dispose(&fixture);
}

void mmay_test_checker_record_imports(void);

void mmay_test_checker_imports(void) {
    mmay_test_checker_record_imports();
    mmay_checker_import_forms();
    mmay_checker_import_diagnostics();
    mmay_checker_import_aliases_and_public_field();
    mmay_checker_import_foreign_extension_excluded();
    mmay_checker_import_nominal_ownership_is_exact();
    mmay_checker_import_interface_mismatch();
    mmay_checker_import_limits();
    mmay_checker_import_allocation_failures();
    mmay_checker_import_diagnostic_allocation_failures();
    mmay_checker_import_mir_allocation_failures();
    mmay_checker_import_std_convert();
}
