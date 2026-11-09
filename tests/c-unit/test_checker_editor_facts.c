#include "test_checker_imports_support.h"

#include "mmay/check/service.h"
#include <string.h>

static const char editor_source[] =
    "type User { mutable value: int, }\n"
    "type Other { value: int, }\n"
    "fn demo(seed: int): int {\n"
    " let value = seed;\n"
    " { const value = 2; const copy = value; }\n"
    " value += seed;\n"
    " const user = User { value };\n"
    " user.value = value;\n"
    " return user.value + value;\n"
    "}\n"
    "fn loops(input: Option<int>): int {\n"
    " let total = 0; for item in 0..2 { total += item; }\n"
    " if const item = input { total += item; }\n"
    " return total;\n"
    "}\n"
    "fn pattern(user: User): int { return match user { User { value } => value, }; }\n"
    "fn closure(seed: int): int { const use = (arg: int): int => arg + seed; return use(1); }\n";

static size_t editor_offset(const char *source, const char *needle, size_t skip) {
    const char *found = strstr(source, needle);
    MMAY_EXPECT(found != NULL);
    return found != NULL ? (size_t)(found - source) + skip : 0;
}

static const MarimayCheckedSymbol *editor_declaration(const MarimayCheckResult *result,
    const char *source, const char *needle, size_t skip) {
    size_t offset = editor_offset(source, needle, skip);
    for (size_t i = 0; i < result->symbols.length; i += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(result, i);
        if (symbol->start == offset) return symbol;
    }
    return NULL;
}

static const MarimayCheckedSymbol *editor_reference(const MarimayCheckResult *result,
    const MarimaySourceSnapshot *snapshot, const char *source, const char *needle, size_t skip) {
    return mmay_checked_definition_from_reference(result, snapshot,
        editor_offset(source, needle, skip));
}

static void editor_expect_same(const MarimayCheckedSymbol *definition,
    const MarimayCheckedSymbol *reference) {
    MMAY_EXPECT(definition != NULL && reference != NULL);
    if (definition != NULL && reference != NULL) {
        if (!mmay_symbol_id_equal(definition->symbol, reference->symbol))
            fprintf(stderr, "editor definition mismatch: wanted %.*s@%zu, got %.*s@%zu\n",
                (int)definition->name.length, definition->name.data, definition->start,
                (int)reference->name.length, reference->name.data, reference->start);
        MMAY_EXPECT(mmay_symbol_id_equal(definition->symbol, reference->symbol));
    }
}

static void editor_binding_facts(void) {
    MarimayCheckerFixture fixture = {0};
    MarimayCheckResult checked = {0};
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, editor_source));
    MMAY_EXPECT(mmay_check_ast(&fixture.ast, "test/editor", mmay_system_allocator(),
        mmay_check_default_options(), &checked) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.error_count == 0);
    if (checked.error_count != 0) {
        for (size_t i = 0; i < checked.diagnostics.length; i += 1)
            fprintf(stderr, "editor facts: %s\n", mmay_check_diagnostic(&checked, i)->message);
        goto cleanup;
    }
    const MarimayCheckedSymbol *outer = editor_declaration(&checked, editor_source, "let value", 4);
    const MarimayCheckedSymbol *inner = editor_declaration(&checked, editor_source, "const value", 6);
    const MarimayCheckedSymbol *parameter = editor_declaration(&checked, editor_source, "seed: int", 0);
    const MarimayCheckedSymbol *field = editor_declaration(&checked, editor_source, "mutable value", 8);
    const MarimayCheckedSymbol *other = editor_declaration(&checked, editor_source, "Other { value", 8);
    MMAY_EXPECT(outer != NULL && outer->kind == MMAY_CHECKED_SYMBOL_LOCAL);
    MMAY_EXPECT(parameter != NULL && parameter->kind == MMAY_CHECKED_SYMBOL_PARAMETER);
    MMAY_EXPECT(field != NULL && field->kind == MMAY_CHECKED_SYMBOL_FIELD);
    if (outer != NULL && inner != NULL) {
        MMAY_EXPECT(!mmay_symbol_id_equal(outer->symbol, inner->symbol));
        MMAY_EXPECT(outer->scope_depth < inner->scope_depth);
        MMAY_EXPECT(inner->scope_end < outer->scope_end);
        MMAY_EXPECT(outer->visible_from > editor_offset(editor_source, "value = seed", 8));
    }
    if (field != NULL && other != NULL) {
        MMAY_EXPECT(!mmay_symbol_id_equal(field->symbol, other->symbol));
        MMAY_EXPECT(!mmay_symbol_id_equal(field->owner_symbol, other->owner_symbol));
    }
    editor_expect_same(parameter, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "value = seed", 8));
    editor_expect_same(inner, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "copy = value", 7));
    editor_expect_same(outer, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "value += seed", 0));
    editor_expect_same(outer, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "user.value = value;", 13));
    editor_expect_same(field, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "user.value =", 5));
    editor_expect_same(field, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "return user.value", 12));
    /* Constructor shorthand publishes both independently resolved roles. */
    size_t shorthand = editor_offset(editor_source, "User { value }", 7), roles = 0;
    for (size_t i = 0; i < checked.occurrences.length; i += 1) {
        const MarimayCheckedOccurrence *occurrence = mmay_checked_occurrence(&checked, i);
        if (occurrence->start == shorthand) roles += 1;
    }
    MMAY_EXPECT(roles == 2);
    const MarimayCheckedSymbol *range = editor_declaration(&checked, editor_source, "for item", 4);
    const MarimayCheckedSymbol *option = editor_declaration(&checked, editor_source, "if const item", 9);
    editor_expect_same(range, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "total += item; }", 9));
    if (range != NULL && option != NULL)
        MMAY_EXPECT(range->scope_end <= option->start && option->visible_from > option->end);
    const MarimayCheckedSymbol *arg = editor_declaration(&checked, editor_source, "arg: int", 0);
    editor_expect_same(arg, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "=> arg +", 3));
    const MarimayCheckedSymbol *captured = editor_declaration(&checked, editor_source, "closure(seed", 8);
    editor_expect_same(captured, editor_reference(&checked, fixture.ast.snapshot,
        editor_source, "arg + seed", 6));
    /* Package-symbol search retains its original top-level-only contract. */
    MMAY_EXPECT(mmay_checked_symbol_search_count(&checked, NULL, 0) == 6);
cleanup:
    mmay_check_result_dispose(&checked);
    mmay_checker_fixture_dispose(&fixture);
}

static void editor_imported_field(void) {
    static const MarimayTestImportSource sources[] = {
        {"example/model", "model/user.my", "public type User { public value: int, }"},
        {"example/app", "app/main.my", "import { User } from \"example/model\";\n"
            "fn main(): int { const user = User { value: 1 }; return user.value; }"}
    };
    MarimayCheckerImportFixture fixture = {0};
    MarimayCheckResult checked = {0};
    MMAY_EXPECT(mmay_checker_import_fixture_init(&fixture, sources, 2));
    MMAY_EXPECT(mmay_check_package_graph(fixture.packages, fixture.package_count,
        "example/app", strlen("example/app"), mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.error_count == 0);
    const MarimayCheckedSymbol *field = NULL;
    for (size_t i = 0; i < checked.symbols.length; i += 1) {
        const MarimayCheckedSymbol *symbol = mmay_checked_symbol(&checked, i);
        if (symbol->kind == MMAY_CHECKED_SYMBOL_FIELD) field = symbol;
    }
    const MarimaySourceSnapshot *snapshot = NULL;
    for (size_t i = 0; i < checked.occurrences.length; i += 1) {
        const MarimayCheckedOccurrence *occurrence = mmay_checked_occurrence(&checked, i);
        if (strstr(mmay_source_path(occurrence->snapshot), "app/main.my") != NULL)
            snapshot = occurrence->snapshot;
    }
    MMAY_EXPECT(snapshot != NULL);
    if (snapshot != NULL) editor_expect_same(field, editor_reference(&checked, snapshot,
        sources[1].source, "return user.value", 12));
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&fixture);
}

static void editor_allocation_failures(void) {
    MarimayCheckerFixture fixture = {0};
    bool completed = false;
    MMAY_EXPECT(mmay_checker_fixture_init(&fixture, editor_source));
    for (size_t failure = 0; failure < 32768 && !completed; failure += 1) {
        MarimayFailAllocator state;
        MarimayCheckResult checked = {0};
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
        MarimayCheckStatus status = mmay_check_ast(&fixture.ast, "test/editor",
            mmay_fail_allocator(&state), mmay_check_default_options(), &checked);
        if (status == MMAY_CHECK_OK) {
            completed = true;
            MMAY_EXPECT(checked.error_count == 0);
            MMAY_EXPECT(checked.symbols.length > 10);
        } else {
            MMAY_EXPECT(status == MMAY_CHECK_ALLOCATION_FAILED);
            MMAY_EXPECT(!checked.initialized);
        }
        mmay_check_result_dispose(&checked);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(completed);
    mmay_checker_fixture_dispose(&fixture);
}

void mmay_test_checker_editor_facts(void) {
    editor_binding_facts();
    editor_imported_field();
    editor_allocation_failures();
}
