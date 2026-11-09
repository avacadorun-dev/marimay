#include "completion_probe.h"

#include <string.h>

static bool lsp_probe_trivia(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_WHITESPACE || kind == MMAY_TOKEN_BOM
        || kind == MMAY_TOKEN_LINE_COMMENT || kind == MMAY_TOKEN_DOC_LINE_COMMENT
        || kind == MMAY_TOKEN_BLOCK_COMMENT || kind == MMAY_TOKEN_DOC_BLOCK_COMMENT;
}

static bool lsp_probe_literal_or_comment(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_LINE_COMMENT || kind == MMAY_TOKEN_DOC_LINE_COMMENT
        || kind == MMAY_TOKEN_BLOCK_COMMENT || kind == MMAY_TOKEN_DOC_BLOCK_COMMENT
        || kind == MMAY_TOKEN_STRING || kind == MMAY_TOKEN_RAW_STRING
        || kind == MMAY_TOKEN_BYTE_STRING || kind == MMAY_TOKEN_RUNE
        || kind == MMAY_TOKEN_INTERPOLATED_TEXT;
}

static size_t lsp_probe_previous(const MarimayLexResult *lexed, size_t index) {
    while (index != 0) {
        const MarimayToken *token = mmay_lex_token(lexed, --index);
        if (token != NULL && !lsp_probe_trivia(token->kind)) return index;
    }
    return SIZE_MAX;
}

static size_t lsp_probe_next(const MarimayLexResult *lexed, size_t offset) {
    size_t index;
    for (index = 0; index < lexed->tokens.length; index += 1) {
        const MarimayToken *token = mmay_lex_token(lexed, index);
        if (token->start >= offset && !lsp_probe_trivia(token->kind)) return index;
    }
    return SIZE_MAX;
}

static bool lsp_probe_assignment(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_EQUAL || kind == MMAY_TOKEN_PLUS_EQUAL
        || kind == MMAY_TOKEN_MINUS_EQUAL || kind == MMAY_TOKEN_STAR_EQUAL
        || kind == MMAY_TOKEN_SLASH_EQUAL || kind == MMAY_TOKEN_PERCENT_EQUAL
        || kind == MMAY_TOKEN_AMPERSAND_EQUAL || kind == MMAY_TOKEN_PIPE_EQUAL
        || kind == MMAY_TOKEN_CARET_EQUAL || kind == MMAY_TOKEN_SHIFT_LEFT_EQUAL
        || kind == MMAY_TOKEN_SHIFT_RIGHT_EQUAL;
}

static size_t lsp_probe_call_end(const MarimayLexResult *lexed,
    size_t index, size_t fallback) {
    size_t depth = 0;
    for (; index < lexed->tokens.length; index += 1) {
        const MarimayToken *token = mmay_lex_token(lexed, index);
        if (token->kind == MMAY_TOKEN_LEFT_PAREN) depth += 1;
        else if (token->kind == MMAY_TOKEN_RIGHT_PAREN) {
            if (depth == 0) return fallback;
            if (--depth == 0) return token->end;
        } else if (token->kind == MMAY_TOKEN_EOF) break;
    }
    return fallback;
}

static size_t lsp_probe_assignment_end(const MarimayLexResult *lexed,
    size_t index, size_t fallback) {
    size_t parentheses = 0, brackets = 0, braces = 0;
    for (; index < lexed->tokens.length; index += 1) {
        const MarimayToken *token = mmay_lex_token(lexed, index);
        if (token->kind == MMAY_TOKEN_LEFT_PAREN) parentheses += 1;
        else if (token->kind == MMAY_TOKEN_LEFT_BRACKET) brackets += 1;
        else if (token->kind == MMAY_TOKEN_LEFT_BRACE) braces += 1;
        else if (token->kind == MMAY_TOKEN_RIGHT_PAREN) {
            if (parentheses == 0) return fallback;
            parentheses -= 1;
        } else if (token->kind == MMAY_TOKEN_RIGHT_BRACKET) {
            if (brackets == 0) return fallback;
            brackets -= 1;
        } else if (token->kind == MMAY_TOKEN_RIGHT_BRACE) {
            if (braces == 0) return token->start;
            braces -= 1;
        } else if ((token->kind == MMAY_TOKEN_SEMICOLON || token->kind == MMAY_TOKEN_EOF)
            && parentheses == 0 && brackets == 0 && braces == 0) return token->start;
    }
    return fallback;
}

LspStatus lsp_completion_probe_build(const MarimayCompilerUnit *unit,
    size_t offset, MarimayAllocator allocator, LspCompletionProbe *probe) {
    static const char replacement[] = "panic(\"\")";
    MarimayVector text = {0};
    const unsigned char *source;
    size_t length, index, prefix_index = SIZE_MAX, previous = SIZE_MAX;
    size_t expression_end, next;
    bool semicolon = false;
    LspStatus status = LSP_INVALID;
    if (unit == NULL || probe == NULL || unit->snapshot == NULL) return status;
    memset(probe, 0, sizeof(*probe));
    source = mmay_source_bytes(unit->snapshot);
    length = mmay_source_length(unit->snapshot);
    if (offset > length || unit->lexed.diagnostics.length != 0) return status;
    probe->prefix_start = probe->prefix_end = offset;
    for (index = 0; index < unit->lexed.tokens.length; index += 1) {
        const MarimayToken *token = mmay_lex_token(&unit->lexed, index);
        if (token->start <= offset && offset <= token->end
            && lsp_probe_literal_or_comment(token->kind)) return status;
        if (token->start <= offset && offset <= token->end
            && token->kind == MMAY_TOKEN_IDENTIFIER) {
            prefix_index = index;
            probe->prefix_start = token->start;
            probe->prefix_end = token->end;
            probe->prefix_length = offset - token->start;
            previous = lsp_probe_previous(&unit->lexed, index);
            break;
        }
        if (token->start < offset && offset < token->end
            && token->kind != MMAY_TOKEN_WHITESPACE) return status;
        if (token->end <= offset && !lsp_probe_trivia(token->kind)
            && token->kind != MMAY_TOKEN_EOF) previous = index;
    }
    if (prefix_index == SIZE_MAX && previous != SIZE_MAX) {
        const MarimayToken *token = mmay_lex_token(&unit->lexed, previous);
        if (token->end == offset && (token->kind == MMAY_TOKEN_LINE_COMMENT
            || token->kind == MMAY_TOKEN_DOC_LINE_COMMENT)) return status;
    }
    probe->expression_start = probe->prefix_start;
    expression_end = probe->prefix_end;
    if (previous != SIZE_MAX && mmay_lex_token(&unit->lexed, previous)->kind == MMAY_TOKEN_DOT) {
        size_t cursor = previous;
        probe->member = true;
        for (;;) {
            size_t receiver_index = lsp_probe_previous(&unit->lexed, cursor);
            const MarimayToken *receiver = mmay_lex_token(&unit->lexed, receiver_index);
            size_t before;
            if (receiver == NULL || receiver->kind != MMAY_TOKEN_IDENTIFIER
                || probe->receiver_count == 64) return LSP_INVALID;
            probe->receiver[probe->receiver_count++] = *receiver;
            probe->expression_start = receiver->start;
            before = lsp_probe_previous(&unit->lexed, receiver_index);
            if (before == SIZE_MAX || mmay_lex_token(&unit->lexed, before)->kind != MMAY_TOKEN_DOT)
                break;
            cursor = before;
        }
        for (index = 0; index < probe->receiver_count / 2; index += 1) {
            MarimayToken swap = probe->receiver[index];
            probe->receiver[index] = probe->receiver[probe->receiver_count - index - 1];
            probe->receiver[probe->receiver_count - index - 1] = swap;
        }
    }
    probe->query_offset = probe->expression_start;
    next = lsp_probe_next(&unit->lexed, expression_end);
    if (next != SIZE_MAX) {
        const MarimayToken *token = mmay_lex_token(&unit->lexed, next);
        if (probe->member && token->kind == MMAY_TOKEN_LEFT_PAREN)
            expression_end = lsp_probe_call_end(&unit->lexed, next, expression_end);
        else if (probe->member && lsp_probe_assignment(token->kind))
            expression_end = lsp_probe_assignment_end(&unit->lexed, next, expression_end);
    }
    next = lsp_probe_next(&unit->lexed, expression_end);
    if (next != SIZE_MAX) {
        MarimayTokenKind kind = mmay_lex_token(&unit->lexed, next)->kind;
        semicolon = kind == MMAY_TOKEN_RIGHT_BRACE || kind == MMAY_TOKEN_EOF
            || (probe->prefix_start == probe->prefix_end
                && (kind == MMAY_TOKEN_KW_RETURN || kind == MMAY_TOKEN_KW_CONST
                    || kind == MMAY_TOKEN_KW_LET));
    }
    if (!mmay_vector_init(&text, allocator, 1)) return LSP_ALLOCATION_FAILED;
    status = LSP_ALLOCATION_FAILED;
    if (!mmay_vector_append(&text, source, probe->expression_start)
        || !mmay_vector_append(&text, replacement, sizeof(replacement) - 1)
        || (semicolon && !mmay_vector_append(&text, ";", 1))
        || !mmay_vector_append(&text, source + expression_end, length - expression_end)
        || !mmay_owned_bytes_copy(&probe->text, allocator, text.data, text.length)) goto cleanup;
    status = LSP_OK;
cleanup:
    mmay_vector_dispose(&text);
    if (status != LSP_OK) lsp_completion_probe_dispose(probe);
    return status;
}

typedef struct LspProbeSpan {
    size_t start, end, last_start;
    MarimayTokenKind last_kind;
    bool present;
} LspProbeSpan;

typedef struct LspProbeSyntax {
    size_t offset, statement_start, statement_width;
    size_t block_starts[64], block_ends[64], block_count, work;
    bool valid;
} LspProbeSyntax;

static bool lsp_probe_statement(MarimaySyntaxKind kind) {
    return kind == MMAY_SYNTAX_CONST_DECL || kind == MMAY_SYNTAX_LET_DECL
        || kind == MMAY_SYNTAX_EXPRESSION_STATEMENT || kind == MMAY_SYNTAX_RETURN_STATEMENT
        || kind == MMAY_SYNTAX_DEFER_STATEMENT || kind == MMAY_SYNTAX_IF_STATEMENT
        || kind == MMAY_SYNTAX_WHILE_STATEMENT || kind == MMAY_SYNTAX_FOR_STATEMENT
        || kind == MMAY_SYNTAX_SCOPE_STATEMENT || kind == MMAY_SYNTAX_UNSAFE_STATEMENT;
}

static LspProbeSpan lsp_probe_syntax(const MarimayGreenNode *node,
    size_t depth, LspProbeSyntax *syntax) {
    LspProbeSpan span = {0};
    size_t index;
    if (node == NULL || depth > 256 || ++syntax->work > 1000000) {
        syntax->valid = false;
        return span;
    }
    for (index = 0; index < node->child_count && syntax->valid; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        LspProbeSpan child = {0};
        if (element == NULL) { syntax->valid = false; break; }
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE)
            child = lsp_probe_syntax(element->as.node, depth + 1, syntax);
        else if (!element->as.token.synthetic && !lsp_probe_trivia(element->as.token.kind)
            && element->as.token.kind != MMAY_TOKEN_EOF) {
            child = (LspProbeSpan){element->as.token.start, element->as.token.end,
                element->as.token.start, element->as.token.kind, true};
        }
        if (!child.present) continue;
        if (!span.present) span.start = child.start;
        span.end = child.end;
        span.last_start = child.last_start;
        span.last_kind = child.last_kind;
        span.present = true;
    }
    if (!span.present || span.start > syntax->offset || span.end < syntax->offset) return span;
    if (lsp_probe_statement(node->kind) && span.end - span.start < syntax->statement_width) {
        syntax->statement_start = span.start;
        syntax->statement_width = span.end - span.start;
    }
    if (node->kind == MMAY_SYNTAX_BLOCK && span.last_kind == MMAY_TOKEN_RIGHT_BRACE
        && span.last_start >= syntax->offset) {
        if (syntax->block_count == 64) syntax->valid = false;
        else {
            syntax->block_starts[syntax->block_count] = span.start;
            syntax->block_ends[syntax->block_count++] = span.last_start;
        }
    }
    return span;
}

LspStatus lsp_completion_probe_trim(const MarimayCompilerUnit *unit,
    size_t block_index, MarimayAllocator allocator, LspCompletionProbe *probe) {
    static const char terminal[] = "return panic(\"\");";
    LspProbeSyntax syntax = {0};
    MarimayVector text = {0};
    MarimayOwnedBytes replacement = {0};
    const unsigned char *source;
    size_t length, index;
    LspStatus status = LSP_INVALID;
    if (unit == NULL || probe == NULL || unit->parsed.tree.root == NULL) return status;
    syntax.offset = probe->prefix_start;
    syntax.statement_start = syntax.statement_width = SIZE_MAX;
    syntax.valid = true;
    (void)lsp_probe_syntax(unit->parsed.tree.root, 0, &syntax);
    if (!syntax.valid || syntax.statement_start == SIZE_MAX || block_index >= syntax.block_count)
        return status;
    source = mmay_source_bytes(unit->snapshot);
    length = mmay_source_length(unit->snapshot);
    if (syntax.statement_start < syntax.block_starts[block_index]
        || syntax.block_ends[block_index] > length) return status;
    if (!mmay_vector_init(&text, allocator, 1)) return LSP_ALLOCATION_FAILED;
    status = LSP_ALLOCATION_FAILED;
    if (!mmay_vector_append(&text, source, syntax.statement_start)
        || !mmay_vector_append(&text, terminal, sizeof(terminal) - 1)) goto cleanup;
    for (index = 0; index < block_index; index += 1)
        if (!mmay_vector_append(&text, "}", 1)) goto cleanup;
    if (!mmay_vector_append(&text, source + syntax.block_ends[block_index],
            length - syntax.block_ends[block_index])
        || !mmay_owned_bytes_copy(&replacement, allocator, text.data, text.length)) goto cleanup;
    mmay_owned_bytes_dispose(&probe->text);
    probe->text = replacement;
    memset(&replacement, 0, sizeof(replacement));
    probe->query_offset = syntax.statement_start;
    status = LSP_OK;
cleanup:
    mmay_owned_bytes_dispose(&replacement);
    mmay_vector_dispose(&text);
    return status;
}

void lsp_completion_probe_dispose(LspCompletionProbe *probe) {
    if (probe == NULL) return;
    mmay_owned_bytes_dispose(&probe->text);
    memset(probe, 0, sizeof(*probe));
}
