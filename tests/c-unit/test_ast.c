#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

static bool ruda_ast_test_has_no_trivia(const RudaAstNode *node) {
    size_t index = 0;
    if (node == NULL || node->end < node->start) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (!ruda_ast_test_has_no_trivia(element->as.node)) {
                return false;
            }
        } else if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
            RudaTokenKind kind = element->as.token.kind;
            if (kind == RUDA_TOKEN_EOF || kind == RUDA_TOKEN_BOM
                || kind == RUDA_TOKEN_WHITESPACE
                || kind == RUDA_TOKEN_LINE_COMMENT
                || kind == RUDA_TOKEN_DOC_LINE_COMMENT
                || kind == RUDA_TOKEN_BLOCK_COMMENT
                || kind == RUDA_TOKEN_DOC_BLOCK_COMMENT) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

static void ruda_ast_test_append(
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

static void ruda_ast_test_dump_node(
    const RudaAstNode *node,
    size_t depth,
    char *output,
    size_t capacity,
    size_t *length
) {
    size_t index = 0;
    char line[160];
    int written = 0;
    for (index = 0; index < depth; index += 1) {
        ruda_ast_test_append(output, capacity, length, "  ");
    }
    written = snprintf(
        line,
        sizeof(line),
        "%s %zu..%zu\n",
        ruda_syntax_kind_name(node->kind),
        node->start,
        node->end
    );
    if (written < 0 || (size_t)written >= sizeof(line)) {
        return;
    }
    ruda_ast_test_append(output, capacity, length, line);
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        size_t indent = 0;
        if (element == NULL) {
            return;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            ruda_ast_test_dump_node(
                element->as.node,
                depth + 1,
                output,
                capacity,
                length
            );
        } else {
            for (indent = 0; indent < depth + 1; indent += 1) {
                ruda_ast_test_append(output, capacity, length, "  ");
            }
            written = snprintf(
                line,
                sizeof(line),
                "%s %zu..%zu%s\n",
                ruda_token_kind_name(element->as.token.kind),
                element->as.token.start,
                element->as.token.end,
                element->as.token.synthetic ? " synthetic" : ""
            );
            if (written < 0 || (size_t)written >= sizeof(line)) {
                return;
            }
            ruda_ast_test_append(output, capacity, length, line);
        }
    }
}

void ruda_test_ast(void) {
    static const char source[] =
        "fn calc(a: int, b: int, c: int): int {\n"
        "  // projection drops this trivia\n"
        "  return a + b * c;\n"
        "}\n";
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot = NULL;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaAstTree ast;
    RudaAstOptions options = ruda_ast_default_options();
    char dump[16384] = {0};
    size_t dump_length = 0;
    size_t failure_index = 0;
    bool reached_success = false;

    RUDA_EXPECT(ruda_source_manager_init(&sources, ruda_system_allocator()));
    RUDA_EXPECT(ruda_source_manager_add(
        &sources,
        "ast.ruda",
        strlen("ast.ruda"),
        source,
        strlen(source),
        &snapshot
    ) == RUDA_SOURCE_OK);
    RUDA_EXPECT(ruda_lex_snapshot(
        snapshot,
        ruda_system_allocator(),
        ruda_lex_default_options(),
        &lexed
    ) == RUDA_LEX_OK);
    RUDA_EXPECT(ruda_parse_file(
        &lexed,
        ruda_system_allocator(),
        ruda_parse_default_options(),
        &parsed
    ) == RUDA_PARSE_OK);
    RUDA_EXPECT(parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_ast_project(
        &parsed.tree,
        ruda_system_allocator(),
        options,
        &ast
    ) == RUDA_AST_OK);
    if (ast.initialized) {
        RUDA_EXPECT(ast.root != NULL);
        RUDA_EXPECT(ast.node_count >= 10);
        RUDA_EXPECT(ruda_ast_test_has_no_trivia(ast.root));
        ruda_ast_test_dump_node(
            ast.root,
            0,
            dump,
            sizeof(dump),
            &dump_length
        );
        RUDA_EXPECT_SNAPSHOT("ast-expression.tree", dump);
        ruda_ast_tree_dispose(&ast);
    }

    options = ruda_ast_default_options();
    options.max_nodes = 2;
    RUDA_EXPECT(ruda_ast_project(
        &parsed.tree,
        ruda_system_allocator(),
        options,
        &ast
    ) == RUDA_AST_LIMIT_REACHED);
    RUDA_EXPECT(!ast.initialized);

    options = ruda_ast_default_options();
    options.max_depth = 2;
    RUDA_EXPECT(ruda_ast_project(
        &parsed.tree,
        ruda_system_allocator(),
        options,
        &ast
    ) == RUDA_AST_LIMIT_REACHED);
    RUDA_EXPECT(!ast.initialized);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaAstStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_ast_project(
            &parsed.tree,
            allocator,
            ruda_ast_default_options(),
            &ast
        );
        if (status == RUDA_AST_OK) {
            reached_success = true;
            RUDA_EXPECT(ruda_ast_test_has_no_trivia(ast.root));
            ruda_ast_tree_dispose(&ast);
        } else {
            RUDA_EXPECT(status == RUDA_AST_ALLOCATION_FAILED);
            RUDA_EXPECT(!ast.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_parse_result_dispose(&parsed);
    ruda_lex_result_dispose(&lexed);
    ruda_source_manager_dispose(&sources);
}
