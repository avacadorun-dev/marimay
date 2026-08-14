#include "test_parser_support.h"

bool ruda_parse_fixture_init_bytes(
    RudaParseFixture *fixture,
    const unsigned char *bytes,
    size_t length
) {
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(&fixture->sources, ruda_system_allocator())) {
        return false;
    }
    if (ruda_source_manager_add(
            &fixture->sources,
            "parse.ruda",
            strlen("parse.ruda"),
            bytes,
            length,
            &fixture->snapshot
        ) != RUDA_SOURCE_OK) {
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    if (ruda_lex_snapshot(
            fixture->snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &fixture->lexed
        ) != RUDA_LEX_OK) {
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    if (ruda_parse_file(
            &fixture->lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &fixture->parsed
        ) != RUDA_PARSE_OK) {
        ruda_lex_result_dispose(&fixture->lexed);
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    return true;
}

bool ruda_parse_fixture_init(RudaParseFixture *fixture, const char *text) {
    return ruda_parse_fixture_init_bytes(
        fixture,
        (const unsigned char *)text,
        strlen(text)
    );
}

void ruda_parse_fixture_dispose(RudaParseFixture *fixture) {
    ruda_parse_result_dispose(&fixture->parsed);
    ruda_lex_result_dispose(&fixture->lexed);
    ruda_source_manager_dispose(&fixture->sources);
}

bool ruda_parse_has_diagnostic(
    const RudaParseResult *result,
    const char *code
) {
    size_t index = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        const RudaParseDiagnostic *diagnostic = ruda_parse_diagnostic(result, index);
        if (diagnostic != NULL && strcmp(diagnostic->code, code) == 0) {
            return true;
        }
    }
    return false;
}

size_t ruda_syntax_count_kind(
    const RudaGreenNode *node,
    RudaSyntaxKind kind
) {
    size_t count = node != NULL && node->kind == kind ? 1 : 0;
    size_t index = 0;
    if (node == NULL) {
        return 0;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element != NULL && element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            count += ruda_syntax_count_kind(element->as.node, kind);
        }
    }
    return count;
}

size_t ruda_syntax_count_synthetic_tokens(const RudaGreenNode *node) {
    size_t count = 0;
    size_t index = 0;
    if (node == NULL) {
        return 0;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element == NULL) {
            continue;
        }
        if (element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            count += ruda_syntax_count_synthetic_tokens(element->as.node);
        } else if (element->as.token.synthetic) {
            count += 1;
        }
    }
    return count;
}

const RudaGreenNode *ruda_syntax_direct_nth_kind(
    const RudaGreenNode *node,
    RudaSyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element == NULL || element->kind != RUDA_SYNTAX_ELEMENT_NODE
            || element->as.node->kind != kind) {
            continue;
        }
        if (found == ordinal) {
            return element->as.node;
        }
        found += 1;
    }
    return NULL;
}

bool ruda_syntax_has_direct_node(
    const RudaGreenNode *node,
    RudaSyntaxKind kind
) {
    size_t index = 0;
    if (node == NULL) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element != NULL && element->kind == RUDA_SYNTAX_ELEMENT_NODE
            && element->as.node->kind == kind) {
            return true;
        }
    }
    return false;
}

const RudaGreenNode *ruda_syntax_find_kind_with_direct_token(
    const RudaGreenNode *node,
    RudaSyntaxKind kind,
    RudaTokenKind token_kind
) {
    size_t index = 0;
    bool has_token = false;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        if (element != NULL && element->kind == RUDA_SYNTAX_ELEMENT_TOKEN
            && element->as.token.kind == token_kind) {
            has_token = true;
        }
    }
    if (node->kind == kind && has_token) {
        return node;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        const RudaGreenNode *found = NULL;
        if (element == NULL || element->kind != RUDA_SYNTAX_ELEMENT_NODE) {
            continue;
        }
        found = ruda_syntax_find_kind_with_direct_token(
            element->as.node,
            kind,
            token_kind
        );
        if (found != NULL) {
            return found;
        }
    }
    return NULL;
}
