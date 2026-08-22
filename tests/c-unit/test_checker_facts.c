#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/check/checker.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"

typedef struct MarimayCheckerFactsFixture {
    MarimaySourceManager sources;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayAstTree ast;
    bool sources_initialized;
    bool lexed_initialized;
    bool parsed_initialized;
    bool ast_initialized;
} MarimayCheckerFactsFixture;

typedef struct MarimayCheckerFactsExpected {
    const char *name;
    MarimayCheckedSymbolKind kind;
    bool is_public;
    const MarimaySourceSnapshot *snapshot;
    size_t start;
    size_t end;
    MarimaySymbolId symbol;
} MarimayCheckerFactsExpected;

static bool mmay_checker_facts_fixture_init_bytes(
    MarimayCheckerFactsFixture *fixture,
    const char *path,
    size_t path_length,
    const char *source
) {
    const MarimaySourceSnapshot *snapshot = NULL;
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_source_manager_init(
            &fixture->sources,
            mmay_system_allocator()
        )) {
        return false;
    }
    fixture->sources_initialized = true;
    if (mmay_source_manager_add(
            &fixture->sources,
            path,
            path_length,
            source,
            strlen(source),
            &snapshot
        ) != MMAY_SOURCE_OK) {
        return false;
    }
    if (mmay_lex_snapshot(
            snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &fixture->lexed
        ) != MMAY_LEX_OK) {
        return false;
    }
    fixture->lexed_initialized = true;
    if (fixture->lexed.diagnostics.length != 0 || mmay_parse_file(
            &fixture->lexed,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &fixture->parsed
        ) != MMAY_PARSE_OK) {
        return false;
    }
    fixture->parsed_initialized = true;
    if (fixture->parsed.diagnostics.length != 0 || mmay_ast_project(
            &fixture->parsed.tree,
            mmay_system_allocator(),
            mmay_ast_default_options(),
            &fixture->ast
        ) != MMAY_AST_OK) {
        return false;
    }
    fixture->ast_initialized = true;
    return true;
}

static bool mmay_checker_facts_fixture_init(
    MarimayCheckerFactsFixture *fixture,
    const char *path,
    const char *source
) {
    return mmay_checker_facts_fixture_init_bytes(
        fixture,
        path,
        strlen(path),
        source
    );
}

static void mmay_checker_facts_fixture_dispose(
    MarimayCheckerFactsFixture *fixture
) {
    if (fixture->ast_initialized) {
        mmay_ast_tree_dispose(&fixture->ast);
    }
    if (fixture->parsed_initialized) {
        mmay_parse_result_dispose(&fixture->parsed);
    }
    if (fixture->lexed_initialized) {
        mmay_lex_result_dispose(&fixture->lexed);
    }
    if (fixture->sources_initialized) {
        mmay_source_manager_dispose(&fixture->sources);
    }
    memset(fixture, 0, sizeof(*fixture));
}

static bool mmay_checker_facts_bytes_equal(
    const MarimayOwnedBytes *actual,
    const char *expected
) {
    size_t length = expected == NULL ? 0 : strlen(expected);
    return actual != NULL && expected != NULL && actual->length == length
        && memcmp(actual->data, expected, length) == 0;
}

static size_t mmay_checker_facts_name_start(
    const char *source,
    const char *name
) {
    const char *found = strstr(source, name);
    MMAY_EXPECT(found != NULL);
    return found == NULL ? 0 : (size_t)(found - source);
}

static bool mmay_checker_facts_type_for_symbol(
    const MarimayCheckResult *checked,
    const MarimayCheckedSymbol *symbol,
    MarimayTypeId *type
) {
    size_t index = 0;
    if (checked == NULL || symbol == NULL || type == NULL) {
        return false;
    }
    if (symbol->kind == MMAY_CHECKED_SYMBOL_FUNCTION) {
        for (index = 0; index < checked->functions.length; index += 1) {
            const MarimayCheckedFunction *function = mmay_checked_function(
                checked,
                index
            );
            if (function != NULL && mmay_symbol_id_equal(
                    function->mir.symbol,
                    symbol->symbol
                )) {
                *type = function->signature_type;
                return true;
            }
        }
    } else if (symbol->kind == MMAY_CHECKED_SYMBOL_CONSTANT) {
        for (index = 0; index < checked->constants.length; index += 1) {
            const MarimayCheckedConstant *constant = mmay_checked_constant(
                checked,
                index
            );
            if (constant != NULL && mmay_symbol_id_equal(
                    constant->symbol,
                    symbol->symbol
                )) {
                *type = constant->value.type;
                return true;
            }
        }
    } else if (symbol->kind == MMAY_CHECKED_SYMBOL_TYPE_ALIAS) {
        for (index = 0; index < checked->type_aliases.length; index += 1) {
            const MarimayCheckedTypeAlias *alias = mmay_checked_type_alias(
                checked,
                index
            );
            if (alias != NULL && mmay_symbol_id_equal(
                    alias->symbol,
                    symbol->symbol
                )) {
                *type = alias->target_type;
                return true;
            }
        }
    } else if (symbol->kind == MMAY_CHECKED_SYMBOL_RECORD) {
        for (index = 0; index < checked->records.length; index += 1) {
            const MarimayCheckedRecord *record = mmay_checked_record(
                checked,
                index
            );
            if (record != NULL && mmay_symbol_id_equal(
                    record->symbol,
                    symbol->symbol
                )) {
                *type = record->type;
                return true;
            }
        }
    } else if (symbol->kind == MMAY_CHECKED_SYMBOL_ENUM) {
        for (index = 0; index < checked->enums.length; index += 1) {
            const MarimayCheckedEnum *enum_value = mmay_checked_enum(
                checked,
                index
            );
            if (enum_value != NULL && mmay_symbol_id_equal(
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

static void mmay_checker_facts_expect_symbol(
    const MarimayCheckResult *checked,
    size_t index,
    const char *name,
    MarimayCheckedSymbolKind kind,
    bool is_public,
    const MarimaySourceSnapshot *snapshot,
    size_t start,
    MarimayCheckerFactsExpected *expected
) {
    const MarimayCheckedSymbol *symbol = mmay_checked_symbol(checked, index);
    MarimayTypeId artifact_type = MMAY_TYPE_INVALID;
    size_t end = start + strlen(name);
    MMAY_EXPECT(symbol != NULL);
    if (symbol == NULL) {
        return;
    }
    MMAY_EXPECT(mmay_checker_facts_bytes_equal(&symbol->name, name));
    MMAY_EXPECT(symbol->kind == kind);
    MMAY_EXPECT(symbol->is_public == is_public);
    MMAY_EXPECT(symbol->snapshot == snapshot);
    MMAY_EXPECT(symbol->start == start);
    MMAY_EXPECT(symbol->end == end);
    MMAY_EXPECT(symbol->type != MMAY_TYPE_INVALID);
    MMAY_EXPECT(mmay_checker_facts_type_for_symbol(
        checked,
        symbol,
        &artifact_type
    ));
    MMAY_EXPECT(symbol->type == artifact_type);
    MMAY_EXPECT(mmay_checked_symbol_by_id(checked, symbol->symbol) == symbol);
    MMAY_EXPECT(mmay_checked_symbol_defined_at(checked, snapshot, start)
        == symbol);
    MMAY_EXPECT(mmay_checked_symbol_defined_at(checked, snapshot, start + 1)
        == symbol);
    MMAY_EXPECT(mmay_checked_symbol_defined_at(checked, snapshot, end) == NULL);
    if (start != 0) {
        MMAY_EXPECT(mmay_checked_symbol_defined_at(
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

static void mmay_checker_facts_expect_canonical(
    const MarimayCheckResult *checked,
    const MarimayCheckerFactsExpected *expected,
    size_t expected_count
) {
    size_t index = 0;
    MMAY_EXPECT(checked != NULL && checked->symbols.length == expected_count);
    for (index = 0; index < expected_count; index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(checked, index);
        MarimayTypeId artifact_type = MMAY_TYPE_INVALID;
        MMAY_EXPECT(symbol != NULL);
        if (symbol != NULL) {
            MMAY_EXPECT(mmay_checker_facts_bytes_equal(
                &symbol->name,
                expected[index].name
            ));
            MMAY_EXPECT(symbol->kind == expected[index].kind);
            MMAY_EXPECT(symbol->is_public == expected[index].is_public);
            MMAY_EXPECT(symbol->snapshot == expected[index].snapshot);
            MMAY_EXPECT(symbol->start == expected[index].start);
            MMAY_EXPECT(symbol->end == expected[index].end);
            MMAY_EXPECT(mmay_symbol_id_equal(
                symbol->symbol,
                expected[index].symbol
            ));
            MMAY_EXPECT(mmay_checker_facts_type_for_symbol(
                checked,
                symbol,
                &artifact_type
            ));
            MMAY_EXPECT(symbol->type == artifact_type);
            MMAY_EXPECT(mmay_checked_symbol_by_id(checked, symbol->symbol)
                == symbol);
        }
    }
}

static void mmay_test_checker_facts_inventory(void) {
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
    MarimayCheckerFactsFixture first;
    MarimayCheckerFactsFixture second;
    const MarimayAstTree *asts[2];
    MarimayCheckerFactsExpected expected[10];
    MarimayCheckResult checked;
    const MarimaySourceSnapshot *wrong_snapshot = NULL;
    char package_name[] = "facts/package";

    memset(expected, 0, sizeof(expected));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &first,
        "a_facts.my",
        source_a
    ));
    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &second,
        "b_facts.my",
        source_b
    ));
    asts[0] = &second.ast;
    asts[1] = &first.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        package_name,
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        MMAY_EXPECT(mmay_checker_facts_bytes_equal(
            &checked.package_name,
            "facts/package"
        ));
        package_name[0] = 'X';
        MMAY_EXPECT(mmay_checker_facts_bytes_equal(
            &checked.package_name,
            "facts/package"
        ));
        mmay_checker_facts_expect_symbol(
            &checked, 0, "PublicId", MMAY_CHECKED_SYMBOL_TYPE_ALIAS, true,
            first.ast.snapshot,
            mmay_checker_facts_name_start(source_a, "PublicId"), &expected[0]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 1, "Person", MMAY_CHECKED_SYMBOL_RECORD, true,
            first.ast.snapshot,
            mmay_checker_facts_name_start(source_a, "Person"), &expected[1]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 2, "Signal", MMAY_CHECKED_SYMBOL_ENUM, true,
            first.ast.snapshot,
            mmay_checker_facts_name_start(source_a, "Signal"), &expected[2]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 3, "answer", MMAY_CHECKED_SYMBOL_CONSTANT, true,
            first.ast.snapshot,
            mmay_checker_facts_name_start(source_a, "answer"), &expected[3]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 4, "first", MMAY_CHECKED_SYMBOL_FUNCTION, true,
            first.ast.snapshot,
            mmay_checker_facts_name_start(source_a, "first"), &expected[4]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 5, "PrivateId", MMAY_CHECKED_SYMBOL_TYPE_ALIAS, false,
            second.ast.snapshot,
            mmay_checker_facts_name_start(source_b, "PrivateId"), &expected[5]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 6, "Secret", MMAY_CHECKED_SYMBOL_RECORD, false,
            second.ast.snapshot,
            mmay_checker_facts_name_start(source_b, "Secret"), &expected[6]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 7, "Hidden", MMAY_CHECKED_SYMBOL_ENUM, false,
            second.ast.snapshot,
            mmay_checker_facts_name_start(source_b, "Hidden"), &expected[7]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 8, "hidden", MMAY_CHECKED_SYMBOL_CONSTANT, false,
            second.ast.snapshot,
            mmay_checker_facts_name_start(source_b, "hidden"), &expected[8]
        );
        mmay_checker_facts_expect_symbol(
            &checked, 9, "second", MMAY_CHECKED_SYMBOL_FUNCTION, false,
            second.ast.snapshot,
            mmay_checker_facts_name_start(source_b, "second"), &expected[9]
        );
        MMAY_EXPECT(mmay_source_manager_overlay(
            &first.sources,
            mmay_source_file_id(first.ast.snapshot),
            source_a,
            strlen(source_a),
            &wrong_snapshot
        ) == MMAY_SOURCE_OK);
        MMAY_EXPECT(wrong_snapshot != NULL);
        MMAY_EXPECT(mmay_checked_symbol_defined_at(
            &checked,
            wrong_snapshot,
            expected[0].start
        ) == NULL);
        mmay_check_result_dispose(&checked);
    }

    memset(&checked, 0, sizeof(checked));
    asts[0] = &first.ast;
    asts[1] = &second.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        2,
        "facts/package",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length == 0);
        mmay_checker_facts_expect_canonical(&checked, expected, 10);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_facts_fixture_dispose(&second);
    mmay_checker_facts_fixture_dispose(&first);
}

static void mmay_test_checker_facts_exact_path_identity(void) {
    static const char first_path[] = {'a', '\0', 'x'};
    static const char second_path[] = {'a', '\0', 'y'};
    static const char source[] = "fn stable(): void {}\n";
    MarimayCheckerFactsFixture first;
    MarimayCheckerFactsFixture second;
    const MarimayAstTree *asts[1];
    MarimayCheckResult checked_first;
    MarimayCheckResult checked_second;
    const MarimayCheckedSymbol *first_symbol = NULL;
    const MarimayCheckedSymbol *second_symbol = NULL;

    memset(&checked_first, 0, sizeof(checked_first));
    memset(&checked_second, 0, sizeof(checked_second));
    MMAY_EXPECT(mmay_checker_facts_fixture_init_bytes(
        &first,
        first_path,
        sizeof(first_path),
        source
    ));
    MMAY_EXPECT(mmay_checker_facts_fixture_init_bytes(
        &second,
        second_path,
        sizeof(second_path),
        source
    ));
    asts[0] = &first.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        1,
        "facts/path",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked_first
    ) == MMAY_CHECK_OK);
    asts[0] = &second.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        1,
        "facts/path",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked_second
    ) == MMAY_CHECK_OK);
    first_symbol = mmay_checked_symbol(&checked_first, 0);
    second_symbol = mmay_checked_symbol(&checked_second, 0);
    MMAY_EXPECT(first_symbol != NULL && second_symbol != NULL);
    if (first_symbol != NULL && second_symbol != NULL) {
        MMAY_EXPECT(!mmay_symbol_id_equal(
            first_symbol->symbol,
            second_symbol->symbol
        ));
    }
    mmay_check_result_dispose(&checked_second);
    mmay_check_result_dispose(&checked_first);
    mmay_checker_facts_fixture_dispose(&second);
    mmay_checker_facts_fixture_dispose(&first);
}

static const MarimayCheckedSymbol *mmay_checker_facts_find_symbol(
    const MarimayCheckResult *checked,
    const char *name,
    MarimayCheckedSymbolKind kind
) {
    size_t index = 0;
    for (index = 0; checked != NULL && index < checked->symbols.length;
         index += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(checked, index);
        if (symbol != NULL && symbol->kind == kind
            && mmay_checker_facts_bytes_equal(&symbol->name, name)) {
            return symbol;
        }
    }
    return NULL;
}

static void mmay_test_checker_facts_receiver_identity(void) {
    static const char nominal_source[] =
        "type Owner {}\n"
        "type Alias = Owner;\n"
        "fn stable(value: Alias): void {}\n";
    static const char primitive_source[] =
        "fn stable(value: int): void {}\n";
    MarimayCheckerFactsFixture nominal;
    MarimayCheckerFactsFixture primitive;
    const MarimayAstTree *asts[1];
    MarimayCheckResult checked_nominal;
    MarimayCheckResult checked_primitive;
    const MarimayCheckedSymbol *owner = NULL;
    const MarimayCheckedSymbol *nominal_function = NULL;
    const MarimayCheckedSymbol *primitive_function = NULL;
    const MarimayCheckedFunction *published_function = NULL;
    MarimaySymbolId expected;
    MarimaySymbolId ordinary;

    memset(&checked_nominal, 0, sizeof(checked_nominal));
    memset(&checked_primitive, 0, sizeof(checked_primitive));
    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &nominal,
        "receiver_facts.my",
        nominal_source
    ));
    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &primitive,
        "receiver_facts.my",
        primitive_source
    ));
    asts[0] = &nominal.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        1,
        "facts/receiver",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked_nominal
    ) == MMAY_CHECK_OK);
    asts[0] = &primitive.ast;
    MMAY_EXPECT(mmay_check_package(
        asts,
        1,
        "facts/receiver",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked_primitive
    ) == MMAY_CHECK_OK);
    nominal_function = mmay_checker_facts_find_symbol(
        &checked_nominal,
        "stable",
        MMAY_CHECKED_SYMBOL_FUNCTION
    );
    primitive_function = mmay_checker_facts_find_symbol(
        &checked_primitive,
        "stable",
        MMAY_CHECKED_SYMBOL_FUNCTION
    );
    owner = mmay_checker_facts_find_symbol(
        &checked_nominal,
        "Owner",
        MMAY_CHECKED_SYMBOL_RECORD
    );
    published_function = mmay_checked_function(&checked_nominal, 0);
    MMAY_EXPECT(owner != NULL && nominal_function != NULL
        && primitive_function != NULL && published_function != NULL);
    if (owner != NULL && nominal_function != NULL
        && primitive_function != NULL && published_function != NULL) {
        MMAY_EXPECT(mmay_symbol_id_from_bytes_with_receiver(
            "facts/receiver",
            strlen("facts/receiver"),
            "receiver_facts.my",
            strlen("receiver_facts.my"),
            "stable",
            strlen("stable"),
            owner->symbol,
            &expected
        ));
        MMAY_EXPECT(mmay_symbol_id_from_bytes(
            "facts/receiver",
            strlen("facts/receiver"),
            "receiver_facts.my",
            strlen("receiver_facts.my"),
            "stable",
            strlen("stable"),
            0,
            &ordinary
        ));
        MMAY_EXPECT(mmay_symbol_id_equal(nominal_function->symbol, expected));
        MMAY_EXPECT(mmay_symbol_id_equal(
            published_function->mir.symbol,
            expected
        ));
        MMAY_EXPECT(!mmay_symbol_id_equal(expected, ordinary));
        MMAY_EXPECT(!mmay_symbol_id_equal(
            nominal_function->symbol,
            primitive_function->symbol
        ));
    }
    mmay_check_result_dispose(&checked_primitive);
    mmay_check_result_dispose(&checked_nominal);
    mmay_checker_facts_fixture_dispose(&primitive);
    mmay_checker_facts_fixture_dispose(&nominal);
}

static void mmay_test_checker_facts_diagnostics_and_limit(void) {
    static const char invalid_source[] =
        "fn invalid(): int { return missing; }\n";
    static const char limited_source[] =
        "type First = int;\n"
        "type Second = int;\n";
    static const char preflight_source[] =
        "fn first(): int { return missing; }\n"
        "fn second(): int { return missing; }\n";
    MarimayCheckerFactsFixture invalid;
    MarimayCheckerFactsFixture limited;
    MarimayCheckerFactsFixture preflight;
    MarimayCheckResult checked;
    MarimayCheckOptions options = mmay_check_default_options();

    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &invalid,
        "invalid_facts.my",
        invalid_source
    ));
    MMAY_EXPECT(mmay_check_ast(
        &invalid.ast,
        "facts/invalid",
        mmay_system_allocator(),
        mmay_check_default_options(),
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized);
    if (checked.initialized) {
        MMAY_EXPECT(checked.diagnostics.length != 0);
        MMAY_EXPECT(checked.symbols.length == 0);
        MMAY_EXPECT(mmay_checked_symbol(&checked, 0) == NULL);
        mmay_check_result_dispose(&checked);
    }
    mmay_checker_facts_fixture_dispose(&invalid);

    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &limited,
        "limited_facts.my",
        limited_source
    ));
    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &preflight,
        "preflight_facts.my",
        preflight_source
    ));
    options.max_symbols = 1;
    MMAY_EXPECT(mmay_check_ast(
        &limited.ast,
        "facts/limited",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    options = mmay_check_default_options();
    options.max_symbols = 1;
    MMAY_EXPECT(mmay_check_ast(
        &preflight.ast,
        "facts/preflight",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);

    options.max_symbols = 2;
    MMAY_EXPECT(mmay_check_ast(
        &limited.ast,
        "facts/limited",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.initialized && checked.symbols.length == 2);
    mmay_check_result_dispose(&checked);

    options = mmay_check_default_options();
    options.max_package_name_bytes = 4;
    MMAY_EXPECT(mmay_check_ast(
        &limited.ast,
        "facts",
        mmay_system_allocator(),
        options,
        &checked
    ) == MMAY_CHECK_LIMIT_REACHED);
    MMAY_EXPECT(!checked.initialized);
    mmay_checker_facts_fixture_dispose(&preflight);
    mmay_checker_facts_fixture_dispose(&limited);
}

static void mmay_test_checker_facts_allocation_failure(void) {
    static const char source_a[] =
        "type Owner {}\n"
        "type First = Owner;\n"
        "fn first(value: First): First { return value; }\n";
    static const char source_b[] =
        "type Second = int;\n"
        "enum State { Ready, }\n"
        "const answer: Second = 42;\n";
    MarimayCheckerFactsFixture first;
    MarimayCheckerFactsFixture second;
    const MarimayAstTree *asts[2];
    size_t failure_index = 0;
    bool reached_success = false;

    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &first,
        "a_alloc_facts.my",
        source_a
    ));
    MMAY_EXPECT(mmay_checker_facts_fixture_init(
        &second,
        "b_alloc_facts.my",
        source_b
    ));
    asts[0] = &second.ast;
    asts[1] = &first.ast;
    for (failure_index = 0; failure_index < 1024; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayCheckResult checked;
        MarimayCheckStatus status;
        mmay_fail_allocator_init(
            &state,
            mmay_system_allocator(),
            failure_index
        );
        allocator = mmay_fail_allocator(&state);
        memset(&checked, 0, sizeof(checked));
        status = mmay_check_package(
            asts,
            2,
            "facts/allocation",
            allocator,
            mmay_check_default_options(),
            &checked
        );
        if (status == MMAY_CHECK_OK) {
            reached_success = true;
            MMAY_EXPECT(checked.initialized);
            MMAY_EXPECT(checked.diagnostics.length == 0);
            MMAY_EXPECT(checked.symbols.length == 6);
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
    mmay_checker_facts_fixture_dispose(&second);
    mmay_checker_facts_fixture_dispose(&first);
}

void mmay_test_checker_facts(void) {
    mmay_test_checker_facts_inventory();
    mmay_test_checker_facts_exact_path_identity();
    mmay_test_checker_facts_receiver_identity();
    mmay_test_checker_facts_diagnostics_and_limit();
    mmay_test_checker_facts_allocation_failure();
}
