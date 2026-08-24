#include "formatter_layout_api.h"

bool mmay_formatter_source_gap_has_blank_line(
    const unsigned char *source,
    size_t source_length,
    size_t start,
    size_t end
) {
    size_t index = start;
    size_t newlines = 0;
    if (source == NULL || start > end || end > source_length) {
        return false;
    }
    while (index < end) {
        if (source[index] == '\r' || source[index] == '\n') {
            newlines += 1;
            if (source[index] == '\r' && index + 1 < end
                && source[index + 1] == '\n') {
                index += 1;
            }
            if (newlines >= 2) {
                return true;
            }
        }
        index += 1;
    }
    return false;
}

bool mmay_formatter_index_is_compact(
    const MarimayVector *tokens,
    size_t open_index,
    size_t source_length,
    bool postfix_context
) {
    size_t index = open_index + 1;
    size_t width = 0;
    bool saw_token = false;
    if (tokens == NULL || !postfix_context) {
        return false;
    }
    while (index < tokens->length) {
        const MarimayFormatToken *token = mmay_vector_at_const(tokens, index);
        size_t token_width = 0;
        if (token == NULL) {
            return false;
        }
        index += 1;
        if (!mmay_format_token_is_visible(token->token.kind)) {
            continue;
        }
        if (mmay_format_token_is_comment(token->token.kind)
            || token->token.end < token->token.start
            || token->token.end > source_length) {
            return false;
        }
        if (token->token.kind == MMAY_TOKEN_RIGHT_BRACKET) {
            return saw_token && width <= 32;
        }
        if (token->token.kind == MMAY_TOKEN_LEFT_PAREN
            || token->token.kind == MMAY_TOKEN_RIGHT_PAREN
            || token->token.kind == MMAY_TOKEN_LEFT_BRACKET
            || token->token.kind == MMAY_TOKEN_LEFT_BRACE
            || token->token.kind == MMAY_TOKEN_RIGHT_BRACE
            || token->token.kind == MMAY_TOKEN_COMMA) {
            return false;
        }
        token_width = token->token.end - token->token.start;
        if (!mmay_size_add(width, token_width, &width)
            || (saw_token && !mmay_size_add(width, 1, &width))
            || width > 32) {
            return false;
        }
        saw_token = true;
    }
    return false;
}

static bool mmay_formatter_brace_source_is_long(
    const MarimayVector *tokens,
    size_t open_index,
    size_t source_length
) {
    const MarimayFormatToken *open = mmay_vector_at_const(tokens, open_index);
    size_t index = open_index;
    size_t depth = 0;
    if (open == NULL || open->token.kind != MMAY_TOKEN_LEFT_BRACE
        || open->token.start > source_length) {
        return false;
    }
    while (index < tokens->length) {
        const MarimayFormatToken *token = mmay_vector_at_const(tokens, index);
        if (token == NULL || token->token.end > source_length) {
            return false;
        }
        if (token->token.kind == MMAY_TOKEN_LEFT_BRACE) {
            depth += 1;
        } else if (token->token.kind == MMAY_TOKEN_RIGHT_BRACE) {
            if (depth == 0) {
                return false;
            }
            depth -= 1;
            if (depth == 0) {
                return token->token.end - open->token.start > 72;
            }
        }
        index += 1;
    }
    return false;
}

bool mmay_formatter_brace_is_grouped(
    const MarimayFormatToken *token,
    const MarimayVector *tokens,
    size_t open_index,
    size_t source_length
) {
    bool grouped = token != NULL && (token->in_record_expression
        || token->parent_kind == MMAY_SYNTAX_IMPORT_CLAUSE
        || token->parent_kind == MMAY_SYNTAX_ENUM_VARIANT
        || token->parent_kind == MMAY_SYNTAX_NAMED_PAYLOAD
        || token->parent_kind == MMAY_SYNTAX_NAMED_PATTERN
        || token->parent_kind == MMAY_SYNTAX_PATTERN_FIELDS);
    return grouped && !(token->parent_kind == MMAY_SYNTAX_NAMED_PATTERN
        && mmay_formatter_brace_source_is_long(
            tokens, open_index, source_length
        ));
}

bool mmay_formatter_postfix_predecessor(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_IDENTIFIER || kind == MMAY_TOKEN_INTEGER
        || kind == MMAY_TOKEN_FLOAT || kind == MMAY_TOKEN_STRING
        || kind == MMAY_TOKEN_RAW_STRING || kind == MMAY_TOKEN_BYTE_STRING
        || kind == MMAY_TOKEN_RUNE || kind == MMAY_TOKEN_KW_TRUE
        || kind == MMAY_TOKEN_KW_FALSE || kind == MMAY_TOKEN_RIGHT_PAREN
        || kind == MMAY_TOKEN_RIGHT_BRACKET || kind == MMAY_TOKEN_RIGHT_BRACE
        || kind == MMAY_TOKEN_INTERPOLATED_END || kind == MMAY_TOKEN_QUESTION
        || kind == MMAY_TOKEN_GREATER || kind == MMAY_TOKEN_SHIFT_RIGHT;
}

bool mmay_formatter_delimiter_matches(
    MarimayTokenKind open,
    MarimayTokenKind close
) {
    return (open == MMAY_TOKEN_LEFT_PAREN && close == MMAY_TOKEN_RIGHT_PAREN)
        || (open == MMAY_TOKEN_LEFT_BRACKET && close == MMAY_TOKEN_RIGHT_BRACKET)
        || (open == MMAY_TOKEN_LEFT_BRACE && close == MMAY_TOKEN_RIGHT_BRACE);
}

bool mmay_formatter_required_tuple_comma(
    const MarimayFormatDelimiter *delimiter
) {
    return delimiter != NULL && delimiter->comma_count == 0
        && (delimiter->parent_kind == MMAY_SYNTAX_TYPE
            || delimiter->parent_kind == MMAY_SYNTAX_TUPLE_EXPRESSION
            || delimiter->parent_kind == MMAY_SYNTAX_TUPLE_PATTERN);
}

bool mmay_formatter_operator_is_spaced(const MarimayFormatToken *token) {
    MarimayTokenKind kind = token->token.kind;
    if (kind == MMAY_TOKEN_BANG || kind == MMAY_TOKEN_TILDE
        || kind == MMAY_TOKEN_QUESTION) {
        return false;
    }
    if (kind == MMAY_TOKEN_MINUS
        && token->parent_kind == MMAY_SYNTAX_UNARY_EXPRESSION) {
        return false;
    }
    if (kind == MMAY_TOKEN_LESS || kind == MMAY_TOKEN_GREATER
        || kind == MMAY_TOKEN_SHIFT_RIGHT) {
        return token->parent_kind == MMAY_SYNTAX_BINARY_EXPRESSION;
    }
    return kind == MMAY_TOKEN_PLUS || kind == MMAY_TOKEN_MINUS
        || kind == MMAY_TOKEN_STAR || kind == MMAY_TOKEN_SLASH
        || kind == MMAY_TOKEN_PERCENT || kind == MMAY_TOKEN_AMPERSAND
        || kind == MMAY_TOKEN_PIPE || kind == MMAY_TOKEN_CARET
        || kind == MMAY_TOKEN_EQUAL || kind == MMAY_TOKEN_FAT_ARROW
        || kind == MMAY_TOKEN_THIN_ARROW || kind == MMAY_TOKEN_EQUAL_EQUAL
        || kind == MMAY_TOKEN_BANG_EQUAL || kind == MMAY_TOKEN_LESS_EQUAL
        || kind == MMAY_TOKEN_GREATER_EQUAL
        || kind == MMAY_TOKEN_AMPERSAND_AMPERSAND
        || kind == MMAY_TOKEN_PIPE_PIPE || kind == MMAY_TOKEN_SHIFT_LEFT
        || kind == MMAY_TOKEN_PLUS_EQUAL || kind == MMAY_TOKEN_MINUS_EQUAL
        || kind == MMAY_TOKEN_STAR_EQUAL || kind == MMAY_TOKEN_SLASH_EQUAL
        || kind == MMAY_TOKEN_PERCENT_EQUAL || kind == MMAY_TOKEN_AMPERSAND_EQUAL
        || kind == MMAY_TOKEN_PIPE_EQUAL || kind == MMAY_TOKEN_CARET_EQUAL
        || kind == MMAY_TOKEN_SHIFT_LEFT_EQUAL
        || kind == MMAY_TOKEN_SHIFT_RIGHT_EQUAL;
}

bool mmay_formatter_token_is_operator(MarimayTokenKind kind) {
    return kind >= MMAY_TOKEN_PLUS && kind <= MMAY_TOKEN_SHIFT_RIGHT_EQUAL;
}

bool mmay_formatter_open_group_needs_space(
    const MarimayFormatter *formatter,
    const MarimayFormatToken *token
) {
    return formatter->pending_space
        && (token->parent_kind == MMAY_SYNTAX_PAREN_GROUP
            || !formatter->has_previous_code
            || !mmay_formatter_postfix_predecessor(
                formatter->previous_code_kind
            ));
}

bool mmay_formatter_trailing_comma_kind(MarimaySyntaxKind parent_kind) {
    return parent_kind == MMAY_SYNTAX_ARGUMENTS
        || parent_kind == MMAY_SYNTAX_PARAMETERS
        || parent_kind == MMAY_SYNTAX_ARRAY_EXPRESSION
        || parent_kind == MMAY_SYNTAX_IMPORT_CLAUSE
        || parent_kind == MMAY_SYNTAX_POSITIONAL_PATTERN
        || parent_kind == MMAY_SYNTAX_TUPLE_PATTERN
        || parent_kind == MMAY_SYNTAX_NAMED_PATTERN
        || parent_kind == MMAY_SYNTAX_NAMED_PAYLOAD
        || parent_kind == MMAY_SYNTAX_ENUM_VARIANT
        || parent_kind == MMAY_SYNTAX_RECORD_DECL
        || parent_kind == MMAY_SYNTAX_RECORD_FIELDS
        || parent_kind == MMAY_SYNTAX_ENUM_DECL
        || parent_kind == MMAY_SYNTAX_MATCH_EXPRESSION;
}
