#include "test_parser_support.h"

bool mmay_parse_fixture_init_bytes(
    MarimayParseFixture *fixture,
    const unsigned char *bytes,
    size_t length
) {
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_source_manager_init(&fixture->sources, mmay_system_allocator())) {
        return false;
    }
    if (mmay_source_manager_add(
            &fixture->sources,
            "parse.my",
            strlen("parse.my"),
            bytes,
            length,
            &fixture->snapshot
        ) != MMAY_SOURCE_OK) {
        mmay_source_manager_dispose(&fixture->sources);
        return false;
    }
    if (mmay_lex_snapshot(
            fixture->snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &fixture->lexed
        ) != MMAY_LEX_OK) {
        mmay_source_manager_dispose(&fixture->sources);
        return false;
    }
    if (mmay_parse_file(
            &fixture->lexed,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &fixture->parsed
        ) != MMAY_PARSE_OK) {
        mmay_lex_result_dispose(&fixture->lexed);
        mmay_source_manager_dispose(&fixture->sources);
        return false;
    }
    return true;
}

bool mmay_parse_fixture_init(MarimayParseFixture *fixture, const char *text) {
    return mmay_parse_fixture_init_bytes(
        fixture,
        (const unsigned char *)text,
        strlen(text)
    );
}

void mmay_parse_fixture_dispose(MarimayParseFixture *fixture) {
    mmay_parse_result_dispose(&fixture->parsed);
    mmay_lex_result_dispose(&fixture->lexed);
    mmay_source_manager_dispose(&fixture->sources);
}

bool mmay_parse_has_diagnostic(
    const MarimayParseResult *result,
    const char *code
) {
    size_t index = 0;
    for (index = 0; index < result->diagnostics.length; index += 1) {
        const MarimayParseDiagnostic *diagnostic = mmay_parse_diagnostic(result, index);
        if (diagnostic != NULL && strcmp(diagnostic->code, code) == 0) {
            return true;
        }
    }
    return false;
}

size_t mmay_syntax_count_kind(
    const MarimayGreenNode *node,
    MarimaySyntaxKind kind
) {
    size_t count = node != NULL && node->kind == kind ? 1 : 0;
    size_t index = 0;
    if (node == NULL) {
        return 0;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        if (element != NULL && element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
            count += mmay_syntax_count_kind(element->as.node, kind);
        }
    }
    return count;
}

size_t mmay_syntax_count_synthetic_tokens(const MarimayGreenNode *node) {
    size_t count = 0;
    size_t index = 0;
    if (node == NULL) {
        return 0;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        if (element == NULL) {
            continue;
        }
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
            count += mmay_syntax_count_synthetic_tokens(element->as.node);
        } else if (element->as.token.synthetic) {
            count += 1;
        }
    }
    return count;
}

const MarimayGreenNode *mmay_syntax_direct_nth_kind(
    const MarimayGreenNode *node,
    MarimaySyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        if (element == NULL || element->kind != MMAY_SYNTAX_ELEMENT_NODE
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

bool mmay_syntax_has_direct_node(
    const MarimayGreenNode *node,
    MarimaySyntaxKind kind
) {
    size_t index = 0;
    if (node == NULL) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        if (element != NULL && element->kind == MMAY_SYNTAX_ELEMENT_NODE
            && element->as.node->kind == kind) {
            return true;
        }
    }
    return false;
}

const MarimayGreenNode *mmay_syntax_find_kind_with_direct_token(
    const MarimayGreenNode *node,
    MarimaySyntaxKind kind,
    MarimayTokenKind token_kind
) {
    size_t index = 0;
    bool has_token = false;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        if (element != NULL && element->kind == MMAY_SYNTAX_ELEMENT_TOKEN
            && element->as.token.kind == token_kind) {
            has_token = true;
        }
    }
    if (node->kind == kind && has_token) {
        return node;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        const MarimayGreenNode *found = NULL;
        if (element == NULL || element->kind != MMAY_SYNTAX_ELEMENT_NODE) {
            continue;
        }
        found = mmay_syntax_find_kind_with_direct_token(
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
