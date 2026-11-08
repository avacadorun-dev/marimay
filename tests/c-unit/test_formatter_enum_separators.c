#include "test_formatter_support.h"

/* bootstrap/tables/grammar-productions.tsv defines enum_variants with an
   optional final comma. The parser retains it inside the last ENUM_VARIANT,
   after its payload rather than before the enum body's closing brace. */
static void mmay_enum_separator_compare(
    const char *left_text, const char *right_text, bool expected
) {
    MarimayFormatterFixture left;
    MarimayFormatterFixture right;
    MarimayAstTree left_ast = {0};
    MarimayAstTree right_ast = {0};
    bool left_ready = mmay_formatter_fixture_init(
        &left, "enum-left.my", (const unsigned char *)left_text, strlen(left_text)
    );
    bool right_ready = mmay_formatter_fixture_init(
        &right, "enum-right.my", (const unsigned char *)right_text, strlen(right_text)
    );
    MMAY_EXPECT(left_ready && right_ready);
    if (!left_ready || !right_ready) {
        goto cleanup;
    }
    MMAY_EXPECT(left.lexed.diagnostics.length == 0);
    MMAY_EXPECT(right.lexed.diagnostics.length == 0);
    MMAY_EXPECT(left.parsed.diagnostics.length == 0);
    MMAY_EXPECT(right.parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_ast_project(
        &left.parsed.tree, mmay_system_allocator(), mmay_ast_default_options(),
        &left_ast
    ) == MMAY_AST_OK);
    MMAY_EXPECT(mmay_ast_project(
        &right.parsed.tree, mmay_system_allocator(), mmay_ast_default_options(),
        &right_ast
    ) == MMAY_AST_OK);
    if (!left_ast.initialized || !right_ast.initialized) {
        goto cleanup;
    }
    MMAY_EXPECT(mmay_fuzz_ast_semantically_equal(
        &left_ast, left.snapshot, &right_ast, right.snapshot
    ) == expected);
    MMAY_EXPECT(mmay_fuzz_ast_semantically_equal(
        &right_ast, right.snapshot, &left_ast, left.snapshot
    ) == expected);
    MMAY_EXPECT(mmay_formatter_ast_semantically_equal(
        left_ast.root, left.snapshot, right_ast.root, right.snapshot
    ) == expected);
    MMAY_EXPECT(mmay_formatter_ast_semantically_equal(
        right_ast.root, right.snapshot, left_ast.root, left.snapshot
    ) == expected);

cleanup:
    mmay_ast_tree_dispose(&right_ast);
    mmay_ast_tree_dispose(&left_ast);
    if (right_ready) mmay_formatter_fixture_dispose(&right);
    if (left_ready) mmay_formatter_fixture_dispose(&left);
}

void mmay_test_formatter_enum_separators(void) {
    static const struct {
        const char *source;
        const char *canonical;
    } equivalent[] = {
        {"enum E { A }", "enum E { A, }"},
        {"enum E { A, B }", "enum E { A, B, }"},
        {"enum E { A, B(int) }", "enum E { A, B(int), }"},
        {"enum E { A, B { x: int, } }", "enum E { A, B { x: int, }, }"},
        {"enum E<T> { A, B(T) }", "enum E<T> { A, B(T), }"}
    };
    static const struct {
        const char *left;
        const char *right;
    } different[] = {
        {"enum E { A }", "enum E { B, }"},
        {"enum E { A(int) }", "enum E { A(string), }"},
        {"enum E { A, B }", "enum E { B, A, }"},
        {"enum E { A { x: int, } }", "enum E { A { x: string, }, }"},
        {"enum E { A(int) }", "enum E { A(int, int), }"}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(equivalent) / sizeof(equivalent[0]); index += 1) {
        mmay_enum_separator_compare(equivalent[index].source,
            equivalent[index].canonical, true);
    }
    for (index = 0; index < sizeof(different) / sizeof(different[0]); index += 1) {
        mmay_enum_separator_compare(different[index].left, different[index].right, false);
    }
}
