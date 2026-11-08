#include "test_formatter_support.h"

static void mmay_type_separator_compare(const char *a, const char *b,
    bool right_valid, bool expected) {
    MarimayFormatterFixture left;
    MarimayFormatterFixture right;
    MarimayAstTree left_ast = {0};
    MarimayAstTree right_ast = {0};
    bool left_ready = mmay_formatter_fixture_init(
        &left, "type-left.my", (const unsigned char *)a, strlen(a));
    bool right_ready = mmay_formatter_fixture_init(
        &right, "type-right.my", (const unsigned char *)b, strlen(b));
    MMAY_EXPECT(left_ready && right_ready);
    if (!left_ready || !right_ready) goto cleanup;
    MMAY_EXPECT(left.lexed.diagnostics.length == 0 && left.parsed.diagnostics.length == 0);
    MMAY_EXPECT(right.lexed.diagnostics.length == 0);
    MMAY_EXPECT((right.parsed.diagnostics.length == 0) == right_valid);
    MMAY_EXPECT(mmay_ast_project(&left.parsed.tree, mmay_system_allocator(),
        mmay_ast_default_options(), &left_ast) == MMAY_AST_OK);
    MMAY_EXPECT(mmay_ast_project(&right.parsed.tree, mmay_system_allocator(),
        mmay_ast_default_options(), &right_ast) == MMAY_AST_OK);
    if (!left_ast.initialized || !right_ast.initialized) goto cleanup;
    MMAY_EXPECT(mmay_fuzz_ast_semantically_equal(
        &left_ast, left.snapshot, &right_ast, right.snapshot) == expected);
    MMAY_EXPECT(mmay_fuzz_ast_semantically_equal(
        &right_ast, right.snapshot, &left_ast, left.snapshot) == expected);
    MMAY_EXPECT(mmay_formatter_ast_semantically_equal(
        left_ast.root, left.snapshot, right_ast.root, right.snapshot) == expected);
    MMAY_EXPECT(mmay_formatter_ast_semantically_equal(
        right_ast.root, right.snapshot, left_ast.root, left.snapshot) == expected);
cleanup:
    mmay_ast_tree_dispose(&right_ast);
    mmay_ast_tree_dispose(&left_ast);
    if (right_ready) mmay_formatter_fixture_dispose(&right);
    if (left_ready) mmay_formatter_fixture_dispose(&left);
}

void mmay_test_formatter_type_list_separators(void) {
    /* D071's 2026-11-08 clarification preserves optional type-list separators,
       while the sole comma in a singleton tuple remains part of its identity. */
    static const struct { const char *a, *b; } equal[] = {
        {"type X = (int, string,);", "type X = (int, string);"},
        {"type X = ((int,), string,);", "type X = ((int,), string);"},
        {"type X = (int,) => int;", "type X = (int) => int;"},
        {"type X = (int, string,) => int;", "type X = (int, string) => int;"},
        {"enum E { A(int,), }", "enum E { A(int), }"},
        {"enum E { A(int, string,), }", "enum E { A(int, string), }"},
        {"enum E { A((int,),), }", "enum E { A((int,)), }"}
    };
    static const struct { const char *a, *b; bool valid; } different[] = {
        {"type X = (int,);", "type X = int;", true},
        {"type X = (int,);", "type X = (int) => int;", true},
        {"type X = (int,);", "type X = (int);", false},
        {"type X = (int,);", "type X = ();", false},
        {"type X = (int, string,);", "type X = (string, int);", true},
        {"enum E { A(int,), }", "enum E { A((int,)), }", true},
        {"fn f(): void { const x = (1,); }", "fn f(): void { const x = (1); }", true}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(equal) / sizeof(equal[0]); index += 1)
        mmay_type_separator_compare(equal[index].a, equal[index].b, true, true);
    for (index = 0; index < sizeof(different) / sizeof(different[0]); index += 1)
        mmay_type_separator_compare(different[index].a, different[index].b,
            different[index].valid, false);
}
