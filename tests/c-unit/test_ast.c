#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "mmay/ast/ast.h"
#include "mmay/base/allocator.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"

static bool mmay_ast_test_has_no_trivia(const MarimayAstNode *node) {
    size_t index = 0;
    if (node == NULL || node->end < node->start) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == MMAY_AST_ELEMENT_NODE) {
            if (!mmay_ast_test_has_no_trivia(element->as.node)) {
                return false;
            }
        } else if (element->kind == MMAY_AST_ELEMENT_TOKEN) {
            MarimayTokenKind kind = element->as.token.kind;
            if (kind == MMAY_TOKEN_EOF || kind == MMAY_TOKEN_BOM
                || kind == MMAY_TOKEN_WHITESPACE
                || kind == MMAY_TOKEN_LINE_COMMENT
                || kind == MMAY_TOKEN_DOC_LINE_COMMENT
                || kind == MMAY_TOKEN_BLOCK_COMMENT
                || kind == MMAY_TOKEN_DOC_BLOCK_COMMENT) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

static void mmay_ast_test_append(
    char *output,
    size_t capacity,
    size_t *length,
    const char *text
) {
    size_t text_length = strlen(text);
    if (*length >= capacity || text_length >= capacity - *length) {
        return;
    }
    memcpy(output + *length, text, text_length);
    *length += text_length;
    output[*length] = '\0';
}

static void mmay_ast_test_dump_node(
    const MarimayAstNode *node,
    size_t depth,
    char *output,
    size_t capacity,
    size_t *length
) {
    size_t index = 0;
    char line[160];
    int written = 0;
    for (index = 0; index < depth; index += 1) {
        mmay_ast_test_append(output, capacity, length, "  ");
    }
    written = snprintf(
        line,
        sizeof(line),
        "%s %zu..%zu\n",
        mmay_syntax_kind_name(node->kind),
        node->start,
        node->end
    );
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return;
    }
    mmay_ast_test_append(output, capacity, length, line);
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        size_t indent = 0;
        if (element == NULL) {
            return;
        }
        if (element->kind == MMAY_AST_ELEMENT_NODE) {
            mmay_ast_test_dump_node(
                element->as.node,
                depth + 1,
                output,
                capacity,
                length
            );
        } else {
            for (indent = 0; indent < depth + 1; indent += 1) {
                mmay_ast_test_append(output, capacity, length, "  ");
            }
            written = snprintf(
                line,
                sizeof(line),
                "%s %zu..%zu%s\n",
                mmay_token_kind_name(element->as.token.kind),
                element->as.token.start,
                element->as.token.end,
                element->as.token.synthetic ? " synthetic" : ""
            );
            if (written < 0 || (size_t)written >= sizeof(line)) {
                return;
            }
            mmay_ast_test_append(output, capacity, length, line);
        }
    }
}

void mmay_test_ast(void) {
    static const char source[] =
        "fn calc(a: int, b: int, c: int): int {\n"
        "  // projection drops this trivia\n"
        "  return a + b * c;\n"
        "}\n";
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayAstTree ast;
    MarimayAstOptions options = mmay_ast_default_options();
    char dump[16384] = {0};
    size_t dump_length = 0;
    size_t failure_index = 0;
    bool reached_success = false;

    MMAY_EXPECT(mmay_source_manager_init(&sources, mmay_system_allocator()));
    MMAY_EXPECT(mmay_source_manager_add(
        &sources,
        "ast.my",
        strlen("ast.my"),
        source,
        strlen(source),
        &snapshot
    ) == MMAY_SOURCE_OK);
    MMAY_EXPECT(mmay_lex_snapshot(
        snapshot,
        mmay_system_allocator(),
        mmay_lex_default_options(),
        &lexed
    ) == MMAY_LEX_OK);
    MMAY_EXPECT(mmay_parse_file(
        &lexed,
        mmay_system_allocator(),
        mmay_parse_default_options(),
        &parsed
    ) == MMAY_PARSE_OK);
    MMAY_EXPECT(parsed.diagnostics.length == 0);
    MMAY_EXPECT(mmay_ast_project(
        &parsed.tree,
        mmay_system_allocator(),
        options,
        &ast
    ) == MMAY_AST_OK);
    if (ast.initialized) {
        MMAY_EXPECT(ast.root != NULL);
        MMAY_EXPECT(ast.node_count >= 10);
        MMAY_EXPECT(mmay_ast_test_has_no_trivia(ast.root));
        mmay_ast_test_dump_node(
            ast.root,
            0,
            dump,
            sizeof(dump),
            &dump_length
        );
        MMAY_EXPECT_SNAPSHOT("ast-expression.tree", dump);
        mmay_ast_tree_dispose(&ast);
    }

    options = mmay_ast_default_options();
    options.max_nodes = 2;
    MMAY_EXPECT(mmay_ast_project(
        &parsed.tree,
        mmay_system_allocator(),
        options,
        &ast
    ) == MMAY_AST_LIMIT_REACHED);
    MMAY_EXPECT(!ast.initialized);

    options = mmay_ast_default_options();
    options.max_depth = 2;
    MMAY_EXPECT(mmay_ast_project(
        &parsed.tree,
        mmay_system_allocator(),
        options,
        &ast
    ) == MMAY_AST_LIMIT_REACHED);
    MMAY_EXPECT(!ast.initialized);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayAllocator allocator;
        MarimayAstStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
        allocator = mmay_fail_allocator(&state);
        status = mmay_ast_project(
            &parsed.tree,
            allocator,
            mmay_ast_default_options(),
            &ast
        );
        if (status == MMAY_AST_OK) {
            reached_success = true;
            MMAY_EXPECT(mmay_ast_test_has_no_trivia(ast.root));
            mmay_ast_tree_dispose(&ast);
        } else {
            MMAY_EXPECT(status == MMAY_AST_ALLOCATION_FAILED);
            MMAY_EXPECT(!ast.initialized);
        }
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    MMAY_EXPECT(reached_success);
    mmay_parse_result_dispose(&parsed);
    mmay_lex_result_dispose(&lexed);
    mmay_source_manager_dispose(&sources);
}
