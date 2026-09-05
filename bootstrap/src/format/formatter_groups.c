#include "formatter_groups_api.h"

/* Owns statement and boolean-expression grouping. Adjacent logical binary
 * nodes share a continuation; parentheses, arguments, arms and fields do not. */
bool mmay_formatter_boolean_node(const MarimayGreenNode *node) {
    size_t index;
    if (node->kind != MMAY_SYNTAX_BINARY_EXPRESSION) return false;
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        if (element != NULL && element->kind == MMAY_SYNTAX_ELEMENT_TOKEN
            && (element->as.token.kind == MMAY_TOKEN_AMPERSAND_AMPERSAND
                || element->as.token.kind == MMAY_TOKEN_PIPE_PIPE)) return true;
    }
    return false;
}

bool mmay_formatter_boolean_group(MarimayFormatCollector *collector, size_t start) {
    size_t index, first = SIZE_MAX, last = SIZE_MAX, operator_index = SIZE_MAX;
    size_t nested = 0;
    for (index = start; index < collector->tokens.length; index += 1) {
        MarimayFormatToken *token = mmay_vector_at(&collector->tokens, index);
        if (!mmay_format_token_is_visible(token->token.kind)
            || mmay_format_token_is_comment(token->token.kind)) continue;
        if (first == SIZE_MAX) first = index;
        last = index;
        if (!mmay_size_add(nested, token->boolean_begins, &nested)
            || nested < token->boolean_ends) {
            collector->status = MMAY_FORMAT_LIMIT_REACHED;
            return false;
        }
        if (nested == 0 && operator_index == SIZE_MAX && !token->in_interpolation
            && (token->token.kind == MMAY_TOKEN_AMPERSAND_AMPERSAND
                || token->token.kind == MMAY_TOKEN_PIPE_PIPE)) operator_index = index;
        nested -= token->boolean_ends;
    }
    if (first == SIZE_MAX || operator_index == SIZE_MAX || nested != 0) {
        collector->status = MMAY_FORMAT_SOURCE_INVALID;
        return false;
    }
    {
        MarimayFormatToken *begin = mmay_vector_at(&collector->tokens, first);
        MarimayFormatToken *end = mmay_vector_at(&collector->tokens, last);
        MarimayFormatToken *operator_token = mmay_vector_at(&collector->tokens, operator_index);
        if (!mmay_size_add(begin->boolean_begins, 1, &begin->boolean_begins)
            || !mmay_size_add(end->boolean_ends, 1, &end->boolean_ends)) {
            collector->status = MMAY_FORMAT_LIMIT_REACHED;
            return false;
        }
        operator_token->boolean_indent = true;
    }
    return true;
}

MarimayDocumentStatus mmay_formatter_boolean_boundary(
    MarimayDocument *document, const MarimayFormatToken *token, bool begin
) {
    size_t index, count = begin ? token->boolean_begins : token->boolean_ends;
    for (index = 0; index < count; index += 1) {
        MarimayDocumentStatus status;
        if (!begin) {
            status = mmay_document_part(document, MMAY_DOCUMENT_INDENT_END);
            if (status != MMAY_DOCUMENT_OK) return status;
        }
        status = mmay_document_part(document,
            begin ? MMAY_DOCUMENT_GROUP_BEGIN : MMAY_DOCUMENT_GROUP_END);
        if (status != MMAY_DOCUMENT_OK) return status;
    }
    return MMAY_DOCUMENT_OK;
}

MarimayDocumentStatus mmay_formatter_begin_statement_group(
    MarimayFormatter *formatter, size_t statement_id
) {
    MarimayDocumentStatus status = mmay_document_part(&formatter->document, MMAY_DOCUMENT_GROUP_BEGIN);
    if (status != MMAY_DOCUMENT_OK) return status;
    formatter->active_statement_id = statement_id;
    formatter->statement_group_depth = formatter->delimiters.length;
    formatter->statement_grouped = true;
    return MMAY_DOCUMENT_OK;
}

MarimayDocumentStatus mmay_formatter_end_statement_group(MarimayFormatter *formatter) {
    MarimayDocumentStatus status = mmay_document_part(&formatter->document, MMAY_DOCUMENT_GROUP_END);
    if (status != MMAY_DOCUMENT_OK) return status;
    formatter->active_statement_id = 0;
    formatter->statement_group_depth = 0;
    formatter->statement_grouped = false;
    return MMAY_DOCUMENT_OK;
}
