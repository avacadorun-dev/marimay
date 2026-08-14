#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/check/checker.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

typedef struct RudaCheckerFactsFixture {
    RudaSourceManager sources;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaAstTree ast;
    bool sources_initialized;
    bool lexed_initialized;
    bool parsed_initialized;
    bool ast_initialized;
} RudaCheckerFactsFixture;

typedef struct RudaCheckerFactsExpected {
    const char *name;
    RudaCheckedSymbolKind kind;
    bool is_public;
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
    RudaSymbolId symbol;
} RudaCheckerFactsExpected;

static bool ruda_checker_facts_fixture_init_bytes(
    RudaCheckerFactsFixture *fixture,
    const char *path,
    size_t path_length,
    const char *source
) {
    const RudaSourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(
            &fixture->sources,
            ruda_system_allocator()
        )) {
        return false;
    }
    fixture->sources_initialized = true;
    if (ruda_source_manager_add(
            &fixture->sources,
            path,
            path_length,
            source,
            strlen(source),
            &snapshot
        ) != RUDA_SOURCE_OK) {
        return false;
    }
    if (ruda_lex_snapshot(
            snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &fixture->lexed
        ) != RUDA_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0 || ruda_parse_file(
            &fixture->lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &fixture->parsed
        ) != RUDA_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0 || ruda_ast_project(
            &fixture->parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &fixture->ast
        ) != RUDA_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

static bool ruda_checker_facts_fixture_init(
    RudaCheckerFactsFixture *fixture,
    const char *path,
    const char *source
) {
    return ruda_checker_facts_fixture_init_bytes(
        fixture,
        path,
        strlen(path),
        source
    );
}

static void ruda_checker_facts_fixture_dispose(
    RudaCheckerFactsFixture *fixture
) {
    if (fixture->ast_initialized) {
        ruda_ast_tree_dispose(&fixture->ast);
    }
    if (fixture->parsed_initialized) {
        ruda_parse_result_dispose(&fixture->parsed);
    }
    if (fixture->lexed_initialized) {
        ruda_lex_result_dispose(&fixture->lexed);
    }
    if (fixture->sources_initialized) {
        ruda_source_manager_dispose(&fixture->sources);
    }
    memset(fixture, 0, sizeof(*fixture));
}

static bool ruda_checker_facts_bytes_equal(
    const RudaOwnedBytes *actual,
    const char *expected
) {
    size_t length = expected == NULL ? 0 : strlen(expected);
    return actual != NULL && expected != NULL && actual->length == length
        && memcmp(actual->data, expected, length) == 0;
}

static size_t ruda_checker_facts_name_start(
    const char *source,
    const char *name
) {
    const char *found = strstr(source, name);
    RUDA_EXPECT(found != NULL);
    return found == NULL ? 0 : (size_t)(found - source);
}

static bool ruda_checker_facts_type_for_symbol(
    const RudaCheckResult *checked,
    const RudaCheckedSymbol *symbol,
    RudaTypeId *type
) {
    size_t index = 0;
    if (checked == NULL || symbol == NULL || type == NULL) {
        return false;
    }
    if (symbol->kind == RUDA_CHECKED_SYMBOL_FUNCTION) {
        for (index = 0; index < checked->functions.length; index += 1) {
            const RudaCheckedFunction *function = ruda_checked_function(
                checked,
                index
            );
            if (function != NULL && ruda_symbol_id_equal(
                    function->mir.symbol,
                    symbol->symbol
                )) {
                *type = function->signature_type;
                return true;
            }
        }
    } else if (symbol->kind == RUDA_CHECKED_SYMBOL_CONSTANT) {
        for (index = 0; index < checked->constants.length; index += 1) {
            const RudaCheckedConstant *constant = ruda_checked_constant(
                checked,
                index
            );
            if (constant != NULL && ruda_symbol_id_equal(
                    constant->symbol,
                    symbol->symbol
                )) {
                *type = constant->value.type;
                return true;
            }
        }
    } else if (symbol->kind == RUDA_CHECKED_SYMBOL_TYPE_ALIAS) {
        for (index = 0; index < checked->type_aliases.length; index += 1) {
            const RudaCheckedTypeAlias *alias = ruda_checked_type_alias(
                checked,
                index
            );
            if (alias != NULL && ruda_symbol_id_equal(
                    alias->symbol,
                    symbol->symbol
                )) {
                *type = alias->target_type;
                return true;
            }
        }
    } else if (symbol->kind == RUDA_CHECKED_SYMBOL_RECORD) {
        for (index = 0; index < checked->records.length; index += 1) {
            const RudaCheckedRecord *record = ruda_checked_record(
                checked,
                index
            );
            if (record != NULL && ruda_symbol_id_equal(
                    record->symbol,
                    symbol->symbol
                )) {
                *type = record->type;
                return true;
            }
        }
    } else if (symbol->kind == RUDA_CHECKED_SYMBOL_ENUM) {
        for (index = 0; index < checked->enums.length; index += 1) {
            const RudaCheckedEnum *enum_value = ruda_checked_enum(
                checked,
                index
            );
            if (enum_value != NULL && ruda_symbol_id_equal(
                    enum_value->symbol,
                    symbol->symbol
                )) {
                *type = enum_value->type;
                return true;
            }
        }
    }
    return false;
}

static void ruda_checker_facts_expect_symbol(
    const RudaCheckResult *checked,
    size_t index,
    const char *name,
    RudaCheckedSymbolKind kind,
    bool is_public,
    const RudaSourceSnapshot *snapshot,
    size_t start,
    RudaCheckerFactsExpected *expected
) {
    const RudaCheckedSymbol *symbol = ruda_checked_symbol(checked, index);
    RudaTypeId artifact_type = RUDA_TYPE_INVALID;
    size_t end = start + strlen(name);
    RUDA_EXPECT(symbol != NULL);
    if (symbol == NULL) {
        return;
    }
    RUDA_EXPECT(ruda_checker_facts_bytes_equal(&symbol->name, name));
    RUDA_EXPECT(symbol->kind == kind);
    RUDA_EXPECT(symbol->is_public == is_public);
    RUDA_EXPECT(symbol->snapshot == snapshot);
    RUDA_EXPECT(symbol->start == start);
    RUDA_EXPECT(symbol->end == end);
    RUDA_EXPECT(symbol->type != RUDA_TYPE_INVALID);
    RUDA_EXPECT(ruda_checker_facts_type_for_symbol(
        checked,
        symbol,
        &artifact_type
    ));
    RUDA_EXPECT(symbol->type == artifact_type);
    RUDA_EXPECT(ruda_checked_symbol_by_id(checked, symbol->symbol) == symbol);
    RUDA_EXPECT(ruda_checked_symbol_defined_at(checked, snapshot, start)
        == symbol);
    RUDA_EXPECT(ruda_checked_symbol_defined_at(checked, snapshot, start + 1)
        == symbol);
    RUDA_EXPECT(ruda_checked_symbol_defined_at(checked, snapshot, end) == NULL);
    if (start != 0) {
        RUDA_EXPECT(ruda_checked_symbol_defined_at(
            checked,
            snapshot,
            start - 1
        ) == NULL);
    }
    if (expected != NULL) {
        expected->name = name;
        expected->kind = kind;
        expected->is_public = is_public;
        expected->snapshot = snapshot;
        expected->start = start;
        expected->end = end;
        expected->symbol = symbol->symbol;
    }
}

static void ruda_checker_facts_expect_canonical(
    const RudaCheckResult *checked,
    const RudaCheckerFactsExpected *expected,
    size_t expected_count
) {
    size_t index = 0;
    RUDA_EXPECT(checked != NULL && checked->symbols.length == expected_count);
    for (index = 0; index < expected_count; index += 1) {
        const RudaCheckedSymbol *symbol = ruda_checked_symbol(checked, index);
        RudaTypeId artifact_type = RUDA_TYPE_INVALID;
        RUDA_EXPECT(symbol != NULL);
        if (symbol != NULL) {
            RUDA_EXPECT(ruda_checker_facts_bytes_equal(
                &symbol->name,
                expected[index].name
            ));
            RUDA_EXPECT(symbol->kind == expected[index].kind);
            RUDA_EXPECT(symbol->is_public == expected[index].is_public);
            RUDA_EXPECT(symbol->snapshot == expected[index].snapshot);
            RUDA_EXPECT(symbol->start == expected[index].start);
            RUDA_EXPECT(symbol->end == expected[index].end);
            RUDA_EXPECT(ruda_symbol_id_equal(
                symbol->symbol,
                expected[index].symbol
            ));
            RUDA_EXPECT(ruda_checker_facts_type_for_symbol(
                checked,
                symbol,
                &artifact_type
            ));
            RUDA_EXPECT(symbol->type == artifact_type);
            RUDA_EXPECT(ruda_checked_symbol_by_id(checked, symbol->symbol)
                == symbol);
        }
    }
}

static void ruda_test_checker_facts_inventory(void) {
    static const char source_a[] =
        "public type PublicId = int;\n"
        "public type Person { public id: PublicId; }\n"
        "public enum Signal { Ready, }\n"
        "public const answer: PublicId = 42;\n"
        "public fn first(value: PublicId): PublicId { return value; }\n";
    static const char source_b[] =
        "type PrivateId = int;\n"
        "type Secret { value: PrivateId; }\n"
        "enum Hidden { Off, }\n"
        "const hidden: PrivateId = 1;\n"
        "fn second(value: PrivateId): PrivateId { return value + hidden; }\n";
    RudaCheckerFactsFixture first;
    RudaCheckerFactsFixture second;
    const RudaAstTree *asts[2];
    RudaCheckerFactsExpected expected[10];
    RudaCheckResult checked;
    const RudaSourceSnapshot *wrong_snapshot = NULL;
    char package_name[] = "facts/package";

    memset(expected, 0, sizeof(expected));
    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &first,
        "a_facts.ruda",
        source_a
    ));
    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &second,
        "b_facts.ruda",
        source_b
    ));
    asts[0] = &second.ast;
    asts[1] = &first.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        package_name,
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        RUDA_EXPECT(ruda_checker_facts_bytes_equal(
            &checked.package_name,
            "facts/package"
        ));
        package_name[0] = 'X';
        RUDA_EXPECT(ruda_checker_facts_bytes_equal(
            &checked.package_name,
            "facts/package"
        ));
        ruda_checker_facts_expect_symbol(
            &checked, 0, "PublicId", RUDA_CHECKED_SYMBOL_TYPE_ALIAS, true,
            first.ast.snapshot,
            ruda_checker_facts_name_start(source_a, "PublicId"), &expected[0]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 1, "Person", RUDA_CHECKED_SYMBOL_RECORD, true,
            first.ast.snapshot,
            ruda_checker_facts_name_start(source_a, "Person"), &expected[1]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 2, "Signal", RUDA_CHECKED_SYMBOL_ENUM, true,
            first.ast.snapshot,
            ruda_checker_facts_name_start(source_a, "Signal"), &expected[2]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 3, "answer", RUDA_CHECKED_SYMBOL_CONSTANT, true,
            first.ast.snapshot,
            ruda_checker_facts_name_start(source_a, "answer"), &expected[3]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 4, "first", RUDA_CHECKED_SYMBOL_FUNCTION, true,
            first.ast.snapshot,
            ruda_checker_facts_name_start(source_a, "first"), &expected[4]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 5, "PrivateId", RUDA_CHECKED_SYMBOL_TYPE_ALIAS, false,
            second.ast.snapshot,
            ruda_checker_facts_name_start(source_b, "PrivateId"), &expected[5]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 6, "Secret", RUDA_CHECKED_SYMBOL_RECORD, false,
            second.ast.snapshot,
            ruda_checker_facts_name_start(source_b, "Secret"), &expected[6]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 7, "Hidden", RUDA_CHECKED_SYMBOL_ENUM, false,
            second.ast.snapshot,
            ruda_checker_facts_name_start(source_b, "Hidden"), &expected[7]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 8, "hidden", RUDA_CHECKED_SYMBOL_CONSTANT, false,
            second.ast.snapshot,
            ruda_checker_facts_name_start(source_b, "hidden"), &expected[8]
        );
        ruda_checker_facts_expect_symbol(
            &checked, 9, "second", RUDA_CHECKED_SYMBOL_FUNCTION, false,
            second.ast.snapshot,
            ruda_checker_facts_name_start(source_b, "second"), &expected[9]
        );
        RUDA_EXPECT(ruda_source_manager_overlay(
            &first.sources,
            ruda_source_file_id(first.ast.snapshot),
            source_a,
            strlen(source_a),
            &wrong_snapshot
        ) == RUDA_SOURCE_OK);
        RUDA_EXPECT(wrong_snapshot != NULL);
        RUDA_EXPECT(ruda_checked_symbol_defined_at(
            &checked,
            wrong_snapshot,
            expected[0].start
        ) == NULL);
        ruda_check_result_dispose(&checked);
    }

    memset(&checked, 0, sizeof(checked));
    asts[0] = &first.ast;
    asts[1] = &second.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        2,
        "facts/package",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length == 0);
        ruda_checker_facts_expect_canonical(&checked, expected, 10);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_facts_fixture_dispose(&second);
    ruda_checker_facts_fixture_dispose(&first);
}

static void ruda_test_checker_facts_exact_path_identity(void) {
    static const char first_path[] = {'a', '\0', 'x'};
    static const char second_path[] = {'a', '\0', 'y'};
    static const char source[] = "fn stable(): void {}\n";
    RudaCheckerFactsFixture first;
    RudaCheckerFactsFixture second;
    const RudaAstTree *asts[1];
    RudaCheckResult checked_first;
    RudaCheckResult checked_second;
    const RudaCheckedSymbol *first_symbol = NULL;
    const RudaCheckedSymbol *second_symbol = NULL;

    memset(&checked_first, 0, sizeof(checked_first));
    memset(&checked_second, 0, sizeof(checked_second));
    RUDA_EXPECT(ruda_checker_facts_fixture_init_bytes(
        &first,
        first_path,
        sizeof(first_path),
        source
    ));
    RUDA_EXPECT(ruda_checker_facts_fixture_init_bytes(
        &second,
        second_path,
        sizeof(second_path),
        source
    ));
    asts[0] = &first.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        1,
        "facts/path",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked_first
    ) == RUDA_CHECK_OK);
    asts[0] = &second.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        1,
        "facts/path",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked_second
    ) == RUDA_CHECK_OK);
    first_symbol = ruda_checked_symbol(&checked_first, 0);
    second_symbol = ruda_checked_symbol(&checked_second, 0);
    RUDA_EXPECT(first_symbol != NULL && second_symbol != NULL);
    if (first_symbol != NULL && second_symbol != NULL) {
        RUDA_EXPECT(!ruda_symbol_id_equal(
            first_symbol->symbol,
            second_symbol->symbol
        ));
    }
    ruda_check_result_dispose(&checked_second);
    ruda_check_result_dispose(&checked_first);
    ruda_checker_facts_fixture_dispose(&second);
    ruda_checker_facts_fixture_dispose(&first);
}

static const RudaCheckedSymbol *ruda_checker_facts_find_symbol(
    const RudaCheckResult *checked,
    const char *name,
    RudaCheckedSymbolKind kind
) {
    size_t index = 0;
    for (index = 0; checked != NULL && index < checked->symbols.length;
         index += 1) {
        const RudaCheckedSymbol *symbol = ruda_checked_symbol(checked, index);
        if (symbol != NULL && symbol->kind == kind
            && ruda_checker_facts_bytes_equal(&symbol->name, name)) {
            return symbol;
        }
    }
    return NULL;
}

static void ruda_test_checker_facts_receiver_identity(void) {
    static const char nominal_source[] =
        "type Owner {}\n"
        "type Alias = Owner;\n"
        "fn stable(value: Alias): void {}\n";
    static const char primitive_source[] =
        "fn stable(value: int): void {}\n";
    RudaCheckerFactsFixture nominal;
    RudaCheckerFactsFixture primitive;
    const RudaAstTree *asts[1];
    RudaCheckResult checked_nominal;
    RudaCheckResult checked_primitive;
    const RudaCheckedSymbol *owner = NULL;
    const RudaCheckedSymbol *nominal_function = NULL;
    const RudaCheckedSymbol *primitive_function = NULL;
    const RudaCheckedFunction *published_function = NULL;
    RudaSymbolId expected;
    RudaSymbolId ordinary;

    memset(&checked_nominal, 0, sizeof(checked_nominal));
    memset(&checked_primitive, 0, sizeof(checked_primitive));
    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &nominal,
        "receiver_facts.ruda",
        nominal_source
    ));
    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &primitive,
        "receiver_facts.ruda",
        primitive_source
    ));
    asts[0] = &nominal.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        1,
        "facts/receiver",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked_nominal
    ) == RUDA_CHECK_OK);
    asts[0] = &primitive.ast;
    RUDA_EXPECT(ruda_check_package(
        asts,
        1,
        "facts/receiver",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked_primitive
    ) == RUDA_CHECK_OK);
    nominal_function = ruda_checker_facts_find_symbol(
        &checked_nominal,
        "stable",
        RUDA_CHECKED_SYMBOL_FUNCTION
    );
    primitive_function = ruda_checker_facts_find_symbol(
        &checked_primitive,
        "stable",
        RUDA_CHECKED_SYMBOL_FUNCTION
    );
    owner = ruda_checker_facts_find_symbol(
        &checked_nominal,
        "Owner",
        RUDA_CHECKED_SYMBOL_RECORD
    );
    published_function = ruda_checked_function(&checked_nominal, 0);
    RUDA_EXPECT(owner != NULL && nominal_function != NULL
        && primitive_function != NULL && published_function != NULL);
    if (owner != NULL && nominal_function != NULL
        && primitive_function != NULL && published_function != NULL) {
        RUDA_EXPECT(ruda_symbol_id_from_bytes_with_receiver(
            "facts/receiver",
            strlen("facts/receiver"),
            "receiver_facts.ruda",
            strlen("receiver_facts.ruda"),
            "stable",
            strlen("stable"),
            owner->symbol,
            &expected
        ));
        RUDA_EXPECT(ruda_symbol_id_from_bytes(
            "facts/receiver",
            strlen("facts/receiver"),
            "receiver_facts.ruda",
            strlen("receiver_facts.ruda"),
            "stable",
            strlen("stable"),
            0,
            &ordinary
        ));
        RUDA_EXPECT(ruda_symbol_id_equal(nominal_function->symbol, expected));
        RUDA_EXPECT(ruda_symbol_id_equal(
            published_function->mir.symbol,
            expected
        ));
        RUDA_EXPECT(!ruda_symbol_id_equal(expected, ordinary));
        RUDA_EXPECT(!ruda_symbol_id_equal(
            nominal_function->symbol,
            primitive_function->symbol
        ));
    }
    ruda_check_result_dispose(&checked_primitive);
    ruda_check_result_dispose(&checked_nominal);
    ruda_checker_facts_fixture_dispose(&primitive);
    ruda_checker_facts_fixture_dispose(&nominal);
}

static void ruda_test_checker_facts_diagnostics_and_limit(void) {
    static const char invalid_source[] =
        "fn invalid(): int { return missing; }\n";
    static const char limited_source[] =
        "type First = int;\n"
        "type Second = int;\n";
    static const char preflight_source[] =
        "fn first(): int { return missing; }\n"
        "fn second(): int { return missing; }\n";
    RudaCheckerFactsFixture invalid;
    RudaCheckerFactsFixture limited;
    RudaCheckerFactsFixture preflight;
    RudaCheckResult checked;
    RudaCheckOptions options = ruda_check_default_options();

    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &invalid,
        "invalid_facts.ruda",
        invalid_source
    ));
    RUDA_EXPECT(ruda_check_ast(
        &invalid.ast,
        "facts/invalid",
        ruda_system_allocator(),
        ruda_check_default_options(),
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized);
    if (checked.initialized) {
        RUDA_EXPECT(checked.diagnostics.length != 0);
        RUDA_EXPECT(checked.symbols.length == 0);
        RUDA_EXPECT(ruda_checked_symbol(&checked, 0) == NULL);
        ruda_check_result_dispose(&checked);
    }
    ruda_checker_facts_fixture_dispose(&invalid);

    memset(&checked, 0, sizeof(checked));
    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &limited,
        "limited_facts.ruda",
        limited_source
    ));
    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &preflight,
        "preflight_facts.ruda",
        preflight_source
    ));
    options.max_symbols = 1;
    RUDA_EXPECT(ruda_check_ast(
        &limited.ast,
        "facts/limited",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    options = ruda_check_default_options();
    options.max_symbols = 1;
    RUDA_EXPECT(ruda_check_ast(
        &preflight.ast,
        "facts/preflight",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);

    options.max_symbols = 2;
    RUDA_EXPECT(ruda_check_ast(
        &limited.ast,
        "facts/limited",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_OK);
    RUDA_EXPECT(checked.initialized && checked.symbols.length == 2);
    ruda_check_result_dispose(&checked);

    options = ruda_check_default_options();
    options.max_package_name_bytes = 4;
    RUDA_EXPECT(ruda_check_ast(
        &limited.ast,
        "facts",
        ruda_system_allocator(),
        options,
        &checked
    ) == RUDA_CHECK_LIMIT_REACHED);
    RUDA_EXPECT(!checked.initialized);
    ruda_checker_facts_fixture_dispose(&preflight);
    ruda_checker_facts_fixture_dispose(&limited);
}

static void ruda_test_checker_facts_allocation_failure(void) {
    static const char source_a[] =
        "type Owner {}\n"
        "type First = Owner;\n"
        "fn first(value: First): First { return value; }\n";
    static const char source_b[] =
        "type Second = int;\n"
        "enum State { Ready, }\n"
        "const answer: Second = 42;\n";
    RudaCheckerFactsFixture first;
    RudaCheckerFactsFixture second;
    const RudaAstTree *asts[2];
    size_t failure_index = 0;
    bool reached_success = false;

    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &first,
        "a_alloc_facts.ruda",
        source_a
    ));
    RUDA_EXPECT(ruda_checker_facts_fixture_init(
        &second,
        "b_alloc_facts.ruda",
        source_b
    ));
    asts[0] = &second.ast;
    asts[1] = &first.ast;
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaCheckResult checked;
        RudaCheckStatus status;
        ruda_fail_allocator_init(
            &state,
            ruda_system_allocator(),
            failure_index
        );
        allocator = ruda_fail_allocator(&state);
        memset(&checked, 0, sizeof(checked));
        status = ruda_check_package(
            asts,
            2,
            "facts/allocation",
            allocator,
            ruda_check_default_options(),
            &checked
        );
        if (status == RUDA_CHECK_OK) {
            reached_success = true;
            RUDA_EXPECT(checked.initialized);
            RUDA_EXPECT(checked.diagnostics.length == 0);
            RUDA_EXPECT(checked.symbols.length == 6);
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
    ruda_checker_facts_fixture_dispose(&second);
    ruda_checker_facts_fixture_dispose(&first);
}

void ruda_test_checker_facts(void) {
    ruda_test_checker_facts_inventory();
    ruda_test_checker_facts_exact_path_identity();
    ruda_test_checker_facts_receiver_identity();
    ruda_test_checker_facts_diagnostics_and_limit();
    ruda_test_checker_facts_allocation_failure();
}
