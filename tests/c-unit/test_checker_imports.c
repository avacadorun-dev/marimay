#include "test_checker_imports_support.h"

#include "ruda/generated/diagnostic_codes.h"

static const char ruda_import_store_types[] =
    "public type User { public value: int; }\n"
    "public enum Level { Basic, Advanced(int), }\n";

static const char ruda_import_store_functions[] =
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

static const char ruda_import_application[] =
    "import { makeUser, makeLevel } from \"acme/store\";\n"
    "import { evaluateUser, evaluateLevel } from \"acme/eval\";\n"
    "fn main(): int {\n"
    "  return evaluateUser(makeUser(3)) + evaluateLevel(makeLevel(4));\n"
    "}\n";

static const char ruda_import_evaluator_type_only[] =
    "import { User, Level } from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: User): int {\n"
    "  return through(user) + user.score();\n"
    "}\n"
    "public fn evaluateLevel(level: Level): int {\n"
    "  return through(level) + level.score();\n"
    "}\n";

static const char ruda_import_evaluator_named[] =
    "import { User, Level, score } from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: User): int {\n"
    "  return through(user) + score(user);\n"
    "}\n"
    "public fn evaluateLevel(level: Level): int {\n"
    "  return through(level) + score(level);\n"
    "}\n";

static const char ruda_import_evaluator_namespace[] =
    "import * as store from \"acme/store\";\n"
    "interface Scored { score(): int; }\n"
    "fn through(value: Scored): int { return value.score(); }\n"
    "public fn evaluateUser(user: store.User): int {\n"
    "  return through(user) + store.score(user);\n"
    "}\n"
    "public fn evaluateLevel(level: store.Level): int {\n"
    "  return through(level) + store.score(level);\n"
    "}\n";

static bool ruda_checker_import_build(
    RudaCheckerImportFixture *fixture,
    const char *evaluator
) {
    RudaTestImportSource sources[] = {
        {"acme/app", "app/main.ruda", ruda_import_application},
        {"acme/store", "store/functions.ruda",
            ruda_import_store_functions},
        {"acme/eval", "eval/evaluate.ruda", evaluator},
        {"acme/store", "store/types.ruda", ruda_import_store_types}
    };
    return ruda_checker_import_fixture_init(
        fixture, sources, sizeof(sources) / sizeof(sources[0])
    );
}

static bool ruda_checker_import_symbol_equal(
    RudaSymbolId left,
    RudaSymbolId right
) {
    return memcmp(left.bytes, right.bytes, sizeof(left.bytes)) == 0;
}

static void ruda_checker_import_expect_form(
    const char *evaluator,
    RudaSymbolId *user_score,
    RudaSymbolId *level_score,
    RudaSymbolId *evaluate_user,
    RudaSymbolId *evaluate_level
) {
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    const RudaCheckedFunction *main_function = NULL;
    const RudaCheckedFunction *user_function = NULL;
    const RudaCheckedFunction *level_function = NULL;
    RudaCheckInterpretOptions interpret_options;
    RudaMirValue value;
    RudaCheckStatus status;
    size_t main_index = SIZE_MAX;
    size_t user_index = SIZE_MAX;
    size_t level_index = SIZE_MAX;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&value, 0, sizeof(value));
    RUDA_EXPECT(ruda_checker_import_build(&fixture, evaluator));
    status = ruda_checker_import_fixture_check(
        &fixture,
        "acme/app",
        ruda_system_allocator(),
        ruda_check_graph_default_options(),
        &checked
    );
    if (status != RUDA_CHECK_OK) {
        fprintf(stderr, "import graph check status: %d\n", (int)status);
    }
    RUDA_EXPECT(status == RUDA_CHECK_OK);
    if (!checked.initialized) {
        ruda_checker_import_fixture_dispose(&fixture);
        return;
    }
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.records.length == 1);
    RUDA_EXPECT(checked.enums.length == 1);
    RUDA_EXPECT(checked.interfaces.length == 1);
    RUDA_EXPECT(checked.functions.length == 8);
    main_function = ruda_checker_import_function(
        &checked, "main", &main_index
    );
    user_function = ruda_checker_import_function(
        &checked, "evaluateUser", &user_index
    );
    level_function = ruda_checker_import_function(
        &checked, "evaluateLevel", &level_index
    );
    RUDA_EXPECT(main_function != NULL);
    RUDA_EXPECT(user_function != NULL);
    RUDA_EXPECT(level_function != NULL);
    if (main_function != NULL && user_function != NULL
        && level_function != NULL) {
        const RudaMirCall *user_direct = ruda_vector_at_const(
            &user_function->mir.calls, 1
        );
        const RudaMirCall *level_direct = ruda_vector_at_const(
            &level_function->mir.calls, 1
        );
        RUDA_EXPECT(user_function->mir.calls.length == 2);
        RUDA_EXPECT(level_function->mir.calls.length == 2);
        RUDA_EXPECT(user_direct != NULL);
        RUDA_EXPECT(level_direct != NULL);
        if (user_direct != NULL && level_direct != NULL) {
            if (user_score->bytes[0] == 0) {
                *user_score = user_direct->callee;
                *level_score = level_direct->callee;
                *evaluate_user = user_function->mir.symbol;
                *evaluate_level = level_function->mir.symbol;
            } else {
                RUDA_EXPECT(ruda_checker_import_symbol_equal(
                    *user_score, user_direct->callee
                ));
                RUDA_EXPECT(ruda_checker_import_symbol_equal(
                    *level_score, level_direct->callee
                ));
                RUDA_EXPECT(ruda_checker_import_symbol_equal(
                    *evaluate_user, user_function->mir.symbol
                ));
                RUDA_EXPECT(ruda_checker_import_symbol_equal(
                    *evaluate_level, level_function->mir.symbol
                ));
            }
        }
        interpret_options = ruda_check_interpret_default_options();
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked,
            main_index,
            NULL,
            0,
            ruda_system_allocator(),
            interpret_options,
            &value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(value.type == RUDA_TYPE_I64);
        RUDA_EXPECT(value.as.i64 == 14);
    }
    ruda_check_result_dispose(&checked);
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_checker_import_forms(void) {
    RudaSymbolId user_score;
    RudaSymbolId level_score;
    RudaSymbolId evaluate_user;
    RudaSymbolId evaluate_level;
    memset(&user_score, 0, sizeof(user_score));
    memset(&level_score, 0, sizeof(level_score));
    memset(&evaluate_user, 0, sizeof(evaluate_user));
    memset(&evaluate_level, 0, sizeof(evaluate_level));
    ruda_checker_import_expect_form(
        ruda_import_evaluator_type_only,
        &user_score,
        &level_score,
        &evaluate_user,
        &evaluate_level
    );
    ruda_checker_import_expect_form(
        ruda_import_evaluator_named,
        &user_score,
        &level_score,
        &evaluate_user,
        &evaluate_level
    );
    ruda_checker_import_expect_form(
        ruda_import_evaluator_namespace,
        &user_score,
        &level_score,
        &evaluate_user,
        &evaluate_level
    );
}

static void ruda_checker_import_expect_diagnostic(
    const RudaTestImportSource *sources,
    size_t source_count,
    const char *root,
    const char *code,
    const char *marked
) {
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_import_fixture_init(
        &fixture, sources, source_count
    ));
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture,
        root,
        ruda_system_allocator(),
        ruda_check_graph_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        const RudaCheckDiagnostic *diagnostic = ruda_check_diagnostic(
            &checked, 0
        );
        RUDA_EXPECT(checked.diagnostics.length == 1);
        RUDA_EXPECT(checked.functions.length == 0);
        RUDA_EXPECT(checked.constants.length == 0);
        RUDA_EXPECT(checked.type_aliases.length == 0);
        RUDA_EXPECT(checked.records.length == 0);
        RUDA_EXPECT(checked.enums.length == 0);
        RUDA_EXPECT(checked.interfaces.length == 0);
        RUDA_EXPECT(checked.symbols.length == 0);
        RUDA_EXPECT(diagnostic != NULL);
        if (diagnostic != NULL) {
            const unsigned char *bytes = ruda_source_bytes(
                diagnostic->snapshot
            );
            RUDA_EXPECT(strcmp(diagnostic->code, code) == 0);
            RUDA_EXPECT(diagnostic->end >= diagnostic->start);
            if (diagnostic->end < diagnostic->start
                || diagnostic->end - diagnostic->start != strlen(marked)) {
                fprintf(stderr,
                    "import diagnostic span mismatch: code=%s marked=%s "
                    "start=%zu end=%zu\n",
                    code, marked, diagnostic->start, diagnostic->end);
            }
            RUDA_EXPECT(diagnostic->end - diagnostic->start
                == strlen(marked));
            if (bytes != NULL && diagnostic->end >= diagnostic->start
                && diagnostic->end - diagnostic->start == strlen(marked)) {
                RUDA_EXPECT(memcmp(
                    bytes + diagnostic->start, marked, strlen(marked)
                ) == 0);
            }
            if (strcmp(code, RUDA_DIAGNOSTIC_PACKAGE_CYCLE) == 0) {
                RUDA_EXPECT(strstr(
                    diagnostic->message, "a -> b -> a"
                ) != NULL);
            }
        }
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_checker_import_diagnostics(void) {
    static const RudaTestImportSource unknown_package[] = {
        {"app", "app.ruda", "import * as missing from \"missing\";\n"}
    };
    static const RudaTestImportSource unknown_item[] = {
        {"app", "app.ruda", "import { Missing } from \"lib\";\n"},
        {"lib", "lib.ruda", "public fn present(): int { return 1; }\n"}
    };
    static const RudaTestImportSource private_item[] = {
        {"app", "app.ruda", "import { hidden } from \"lib\";\n"},
        {"lib", "lib.ruda", "fn hidden(): int { return 1; }\n"}
    };
    static const RudaTestImportSource conflict[] = {
        {"app", "app.ruda",
            "import { first as same, second as same } from \"lib\";\n"},
        {"lib", "lib.ruda",
            "public fn first(): int { return 1; }\n"
            "public fn second(): int { return 2; }\n"}
    };
    static const RudaTestImportSource cycle[] = {
        {"a", "a.ruda", "import * as b from \"b\";\n"},
        {"b", "b.ruda", "import * as a from \"a\";\n"}
    };
    static const RudaTestImportSource local_conflict[] = {
        {"app", "app.ruda",
            "import { present as local } from \"lib\";\n"
            "fn local(): int { return 1; }\n"},
        {"lib", "lib.ruda", "public fn present(): int { return 1; }\n"}
    };
    static const RudaTestImportSource multi_file_local_conflict[] = {
        {"app", "app/imports.ruda",
            "import { present as local } from \"lib\";\n"},
        {"app", "app/declaration.ruda",
            "fn local(): int { return 1; }\n"},
        {"lib", "lib.ruda", "public fn present(): int { return 1; }\n"}
    };
    static const RudaTestImportSource private_field[] = {
        {"app", "app.ruda",
            "import { User, makeUser } from \"lib\";\n"
            "fn read(): int { const user = makeUser(1); return user.hidden; }\n"},
        {"lib", "lib.ruda",
            "public type User { public visible: int; hidden: int; }\n"
            "public fn makeUser(value: int): User {\n"
            "  return User { visible: value, hidden: value };\n"
            "}\n"}
    };
    static const RudaTestImportSource type_only_free[] = {
        {"app", "app.ruda",
            "import { User } from \"lib\";\n"
            "fn bad(user: User): int { return score(user); }\n"},
        {"lib", "lib.ruda",
            "public type User { public value: int; }\n"
            "public fn score(user: User): int { return user.value; }\n"}
    };
    static const RudaTestImportSource unknown_namespace_member[] = {
        {"app", "app.ruda",
            "import * as library from \"lib\";\n"
            "fn bad(): int { return library.missing(); }\n"},
        {"lib", "lib.ruda", "public fn present(): int { return 1; }\n"}
    };
    static const RudaTestImportSource private_namespace_member[] = {
        {"app", "app.ruda",
            "import * as library from \"lib\";\n"
            "fn bad(): int { return library.hidden(); }\n"},
        {"lib", "lib.ruda", "fn hidden(): int { return 1; }\n"}
    };
    static const RudaTestImportSource namespace_free_absent[] = {
        {"app", "app.ruda",
            "import * as library from \"lib\";\n"
            "fn bad(user: library.User): int { return score(user); }\n"},
        {"lib", "lib.ruda",
            "public type User { public value: int; }\n"
            "public fn score(user: User): int { return user.value; }\n"}
    };
    static const RudaTestImportSource private_namespace_type[] = {
        {"app", "app.ruda",
            "import * as library from \"lib\";\n"
            "fn bad(user: library.Hidden): int { return 0; }\n"},
        {"lib", "lib.ruda", "type Hidden { public value: int; }\n"}
    };
    ruda_checker_import_expect_diagnostic(
        unknown_package, 1, "app", RUDA_DIAGNOSTIC_UNKNOWN_PACKAGE,
        "\"missing\""
    );
    ruda_checker_import_expect_diagnostic(
        unknown_item, 2, "app", RUDA_DIAGNOSTIC_UNKNOWN_IMPORT, "Missing"
    );
    ruda_checker_import_expect_diagnostic(
        private_item, 2, "app", RUDA_DIAGNOSTIC_PRIVATE_IMPORT, "hidden"
    );
    ruda_checker_import_expect_diagnostic(
        conflict, 2, "app", RUDA_DIAGNOSTIC_IMPORT_CONFLICT, "same"
    );
    ruda_checker_import_expect_diagnostic(
        cycle, 2, "a", RUDA_DIAGNOSTIC_PACKAGE_CYCLE, "\"a\""
    );
    ruda_checker_import_expect_diagnostic(
        local_conflict, 2, "app", RUDA_DIAGNOSTIC_IMPORT_CONFLICT, "local"
    );
    ruda_checker_import_expect_diagnostic(
        multi_file_local_conflict, 3, "app",
        RUDA_DIAGNOSTIC_IMPORT_CONFLICT, "local"
    );
    ruda_checker_import_expect_diagnostic(
        private_field, 2, "app", RUDA_DIAGNOSTIC_PRIVATE_IMPORT, "hidden"
    );
    ruda_checker_import_expect_diagnostic(
        type_only_free, 2, "app", RUDA_DIAGNOSTIC_UNKNOWN_NAME, "score"
    );
    ruda_checker_import_expect_diagnostic(
        unknown_namespace_member, 2, "app",
        RUDA_DIAGNOSTIC_UNKNOWN_IMPORT, "missing"
    );
    ruda_checker_import_expect_diagnostic(
        private_namespace_member, 2, "app",
        RUDA_DIAGNOSTIC_PRIVATE_IMPORT, "hidden"
    );
    ruda_checker_import_expect_diagnostic(
        namespace_free_absent, 2, "app",
        RUDA_DIAGNOSTIC_UNKNOWN_NAME, "score"
    );
    ruda_checker_import_expect_diagnostic(
        private_namespace_type, 2, "app",
        RUDA_DIAGNOSTIC_PRIVATE_IMPORT, "Hidden"
    );
}

static void ruda_checker_import_aliases_and_public_field(void) {
    static const RudaTestImportSource sources[] = {
        {"app", "app.ruda",
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
        {"lib", "lib.ruda",
            "public type User { public visible: int; hidden: int; }\n"
            "public fn makeUser(value: int): User {\n"
            "  return User { visible: value, hidden: value };\n"
            "}\n"
            "public fn score(user: User): int { return user.visible; }\n"}
    };
    RudaCheckerImportFixture fixture;
    RudaCheckResult checked;
    RudaMirValue value;
    size_t main_index = SIZE_MAX;
    memset(&fixture, 0, sizeof(fixture));
    memset(&checked, 0, sizeof(checked));
    memset(&value, 0, sizeof(value));
    RUDA_EXPECT(ruda_checker_import_fixture_init(&fixture, sources, 2));
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(),
        ruda_check_graph_default_options(), &checked
    ) == RUDA_CHECK_OK);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(ruda_checker_import_function(
            &checked, "main", &main_index
        ) != NULL);
        RUDA_EXPECT(ruda_check_interpret_function(
            &checked, main_index, NULL, 0, ruda_system_allocator(),
            ruda_check_interpret_default_options(), &value
        ) == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(value.type == RUDA_TYPE_I64);
        RUDA_EXPECT(value.as.i64 == 15);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_checker_import_foreign_extension_excluded(void) {
    static const RudaTestImportSource sources[] = {
        {"app", "app.ruda",
            "import { User } from \"lib\";\n"
            "fn extra(user: User): int { return 9; }\n"
            "fn bad(user: User): int { return user.extra(); }\n"},
        {"lib", "lib.ruda", "public type User { public value: int; }\n"}
    };
    ruda_checker_import_expect_diagnostic(
        sources, 2, "app", RUDA_DIAGNOSTIC_TYPE_MISMATCH, "extra"
    );
}

static void ruda_checker_import_nominal_ownership_is_exact(void) {
    static const RudaTestImportSource sources[] = {
        {"app", "app.ruda",
            "import * as left from \"left\";\n"
            "import * as right from \"right\";\n"
            "fn bad(user: left.User): int { return user.score(); }\n"},
        {"left", "left.ruda", "public type User { public value: int; }\n"},
        {"right", "right.ruda",
            "public type User { public value: int; }\n"
            "public fn score(user: User): int { return user.value; }\n"}
    };
    ruda_checker_import_expect_diagnostic(
        sources, 3, "app", RUDA_DIAGNOSTIC_TYPE_MISMATCH, "user"
    );
}

static void ruda_checker_import_interface_mismatch(void) {
    static const RudaTestImportSource sources[] = {
        {"app", "app.ruda",
            "import { User } from \"lib\";\n"
            "interface Scored { score(): int; }\n"
            "fn through(value: Scored): int { return value.score(); }\n"
            "fn bad(user: User): int { return through(user); }\n"},
        {"lib", "lib.ruda", "public type User { public value: int; }\n"}
    };
    ruda_checker_import_expect_diagnostic(
        sources, 2, "app", RUDA_DIAGNOSTIC_TYPE_MISMATCH, "user"
    );
}

static void ruda_checker_import_limits(void) {
    static const RudaTestImportSource sources[] = {
        {"app", "app.ruda",
            "import * as library from \"lib\";\n"
            "import { value, other } from \"lib\";\n"
            "fn local(): int { return 0; }\n"},
        {"lib", "lib.ruda",
            "public fn value(): int { return 1; }\n"
            "public fn other(): int { return 2; }\n"}
    };
    RudaCheckerImportFixture fixture;
    RudaCheckGraphOptions options = ruda_check_graph_default_options();
    RudaCheckResult checked;
    RUDA_EXPECT(ruda_checker_import_fixture_init(&fixture, sources, 2));
    options.max_packages = 1;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_graph_default_options();
    options.max_dependency_depth = 1;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_graph_default_options();
    options.max_total_imports = 1;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_graph_default_options();
    options.max_imports_per_file = 1;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_graph_default_options();
    options.max_items_per_import = 1;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_graph_default_options();
    options.max_total_bindings = 1;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_graph_default_options();
    options.package_options.max_package_name_bytes = 2;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_graph_default_options();
    options.package_options.max_functions = 1;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_graph_default_options();
    options.package_options.max_symbols = 2;
    RUDA_EXPECT(ruda_checker_import_fixture_check(
        &fixture, "app", ruda_system_allocator(), options, &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_checker_import_diagnostic_allocation_case(
    const RudaTestImportSource *sources,
    size_t source_count,
    const char *expected_code
) {
    RudaCheckerImportFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    memset(&fixture, 0, sizeof(fixture));
    RUDA_EXPECT(ruda_checker_import_fixture_init(
        &fixture, sources, source_count
    ));
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_checker_import_fixture_check(
            &fixture, "app", allocator,
            ruda_check_graph_default_options(), &checked
        );
        if (status == RUDA_CHECK_OK) {
            const RudaCheckDiagnostic *diagnostic =
                ruda_check_diagnostic(&checked, 0);
            reached_success = true;
            RUDA_EXPECT(checked.initialized);
            RUDA_EXPECT(checked.diagnostics.length == 1);
            RUDA_EXPECT(diagnostic != NULL);
            if (diagnostic != NULL) {
                RUDA_EXPECT(strcmp(diagnostic->code, expected_code) == 0);
            }
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_import_fixture_dispose(&fixture);
}

static void ruda_checker_import_diagnostic_allocation_failures(void) {
    static const RudaTestImportSource cycle[] = {
        {"app", "app.ruda", "import * as lib from \"lib\";\n"},
        {"lib", "lib.ruda", "import * as app from \"app\";\n"}
    };
    static const RudaTestImportSource private_field[] = {
        {"app", "app.ruda",
            "import { User, makeUser } from \"lib\";\n"
            "fn read(): int { const user = makeUser(1); return user.hidden; }\n"},
        {"lib", "lib.ruda",
            "public type User { public visible: int; hidden: int; }\n"
            "public fn makeUser(value: int): User {\n"
            "  return User { visible: value, hidden: value };\n"
            "}\n"}
    };
    ruda_checker_import_diagnostic_allocation_case(
        cycle, 2, RUDA_DIAGNOSTIC_PACKAGE_CYCLE
    );
    ruda_checker_import_diagnostic_allocation_case(
        private_field, 2, RUDA_DIAGNOSTIC_PRIVATE_IMPORT
    );
}

static void ruda_checker_import_allocation_failures(void) {
    RudaCheckerImportFixture fixture;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_checker_import_build(
        &fixture, ruda_import_evaluator_named
    ));
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        allocator = ruda_fail_allocator(&state);
        status = ruda_checker_import_fixture_check(
            &fixture, "acme/app", allocator,
            ruda_check_graph_default_options(), &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.initialized);
            RUDA_EXPECT(checked.diagnostics.length == 0);
            ruda_check_result_dispose(&checked);
        } else {
            RUDA_EXPECT(status == RUDA_CHECK_ALLOCATION_FAILED);
            RUDA_EXPECT(!checked.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_checker_import_fixture_dispose(&fixture);
}

void ruda_test_checker_imports(void) {
    ruda_checker_import_forms();
    ruda_checker_import_diagnostics();
    ruda_checker_import_aliases_and_public_field();
    ruda_checker_import_foreign_extension_excluded();
    ruda_checker_import_nominal_ownership_is_exact();
    ruda_checker_import_interface_mismatch();
    ruda_checker_import_limits();
    ruda_checker_import_allocation_failures();
    ruda_checker_import_diagnostic_allocation_failures();
}
