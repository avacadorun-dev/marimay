#include "formatter_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/base/string_literal.h"
#include "ruda/base/utf8.h"
#include "ruda/base/writer.h"
#include "ruda/syntax/syntax.h"

static void ruda_formatter_owned_text_dispose(RudaVector *owned_text) {
    size_t index = 0;
    for (index = 0; index < owned_text->length; index += 1) {
        RudaOwnedBytes *text = ruda_vector_at(owned_text, index);
        if (text != NULL) {
            ruda_owned_bytes_dispose(text);
        }
    }
    ruda_vector_dispose(owned_text);
}

static void ruda_formatter_document_status(
    RudaFormatter *formatter,
    RudaDocumentStatus status
) {
    if (status == RUDA_DOCUMENT_OK || formatter->status != RUDA_FORMAT_OK) {
        return;
    }
    if (status == RUDA_DOCUMENT_ALLOCATION_FAILED) {
        formatter->status = RUDA_FORMAT_ALLOCATION_FAILED;
    } else if (status == RUDA_DOCUMENT_LIMIT_REACHED) {
        formatter->status = RUDA_FORMAT_LIMIT_REACHED;
    } else {
        formatter->status = RUDA_FORMAT_INVALID_ARGUMENT;
    }
}

static void ruda_formatter_part(
    RudaFormatter *formatter,
    RudaDocumentPartKind kind
) {
    if (formatter->status == RUDA_FORMAT_OK) {
        ruda_formatter_document_status(
            formatter,
            ruda_document_part(&formatter->document, kind)
        );
    }
}

static void ruda_formatter_text(
    RudaFormatter *formatter,
    const unsigned char *text,
    size_t length
) {
    if (formatter->status == RUDA_FORMAT_OK) {
        ruda_formatter_document_status(
            formatter,
            ruda_document_text(&formatter->document, text, length)
        );
    }
    if (length != 0) {
        formatter->at_line_start = false;
        formatter->after_flexible_line = false;
    }
}

static bool ruda_formatter_adopt_text(
    RudaFormatter *formatter,
    RudaOwnedBytes *text
) {
    if (!ruda_vector_push(&formatter->owned_text, text)) {
        formatter->status = RUDA_FORMAT_ALLOCATION_FAILED;
        ruda_owned_bytes_dispose(text);
        return false;
    }
    text->data = NULL;
    text->length = 0;
    return true;
}

static void ruda_formatter_copy_text(
    RudaFormatter *formatter,
    const unsigned char *text,
    size_t length
) {
    RudaOwnedBytes owned;
    const unsigned char *stable = NULL;
    if (formatter->status != RUDA_FORMAT_OK) {
        return;
    }
    if (!ruda_owned_bytes_copy(&owned, formatter->allocator, text, length)) {
        formatter->status = RUDA_FORMAT_ALLOCATION_FAILED;
        return;
    }
    stable = owned.data;
    if (!ruda_formatter_adopt_text(formatter, &owned)) {
        return;
    }
    ruda_formatter_text(formatter, stable, length);
}

static void ruda_formatter_unicode_escape(
    RudaFormatter *formatter,
    uint32_t value
) {
    static const unsigned char digits[] = "0123456789abcdef";
    unsigned char spelling[10];
    unsigned char reverse[6];
    size_t digit_count = 0;
    size_t index = 0;
    spelling[0] = '\\';
    spelling[1] = 'u';
    spelling[2] = '{';
    do {
        reverse[digit_count] = digits[value & UINT32_C(0xf)];
        digit_count += 1;
        value >>= 4;
    } while (value != 0 && digit_count < sizeof(reverse));
    for (index = 0; index < digit_count; index += 1) {
        spelling[3 + index] = reverse[digit_count - index - 1];
    }
    spelling[3 + digit_count] = '}';
    ruda_formatter_copy_text(formatter, spelling, digit_count + 4);
}

static bool ruda_formatter_raw_string_is_shorter(
    const RudaOwnedBytes *decoded
) {
    size_t cursor = 0;
    size_t backslashes = 0;
    while (cursor < decoded->length) {
        RudaUtf8Scalar scalar;
        if (!ruda_utf8_decode(
                decoded->data + cursor,
                decoded->length - cursor,
                &scalar
            )) {
            return false;
        }
        if (scalar.value == '"' || scalar.value <= UINT32_C(0x1f)
            || (scalar.value >= UINT32_C(0x7f)
                && scalar.value <= UINT32_C(0x9f))) {
            return false;
        }
        if (scalar.value == '\\') {
            backslashes += 1;
        }
        cursor += scalar.width;
    }
    return backslashes >= 2;
}

static void ruda_formatter_canonical_string(
    RudaFormatter *formatter,
    const RudaFormatToken *token
) {
    static const unsigned char quote[] = "\"";
    static const unsigned char raw_start[] = "r\"";
    static const unsigned char slash_escape[] = "\\\\";
    static const unsigned char quote_escape[] = "\\\"";
    static const unsigned char newline_escape[] = "\\n";
    static const unsigned char carriage_escape[] = "\\r";
    static const unsigned char tab_escape[] = "\\t";
    static const unsigned char zero_escape[] = "\\0";
    RudaOwnedBytes decoded;
    RudaFormatStatus status = ruda_format_decode_ordinary_string(
        formatter->source,
        formatter->source_length,
        &token->token,
        formatter->allocator,
        &decoded
    );
    size_t cursor = 0;
    size_t literal_start = 0;
    const unsigned char *decoded_data = NULL;
    size_t decoded_length = 0;
    bool allow_raw = token->parent_kind == RUDA_SYNTAX_LITERAL_EXPRESSION;
    if (status != RUDA_FORMAT_OK) {
        formatter->status = status;
        return;
    }
    decoded_data = decoded.data;
    decoded_length = decoded.length;
    if (!ruda_formatter_adopt_text(formatter, &decoded)) {
        return;
    }
    {
        const RudaOwnedBytes *stable = ruda_vector_at_const(
            &formatter->owned_text,
            formatter->owned_text.length - 1
        );
        if (stable == NULL) {
            formatter->status = RUDA_FORMAT_INVALID_ARGUMENT;
            return;
        }
        if (allow_raw && ruda_formatter_raw_string_is_shorter(stable)) {
            ruda_formatter_text(formatter, raw_start, sizeof(raw_start) - 1);
            ruda_formatter_text(formatter, decoded_data, decoded_length);
            ruda_formatter_text(formatter, quote, sizeof(quote) - 1);
            return;
        }
    }

    ruda_formatter_text(formatter, quote, sizeof(quote) - 1);
    while (cursor < decoded_length && formatter->status == RUDA_FORMAT_OK) {
        RudaUtf8Scalar scalar;
        const unsigned char *escape = NULL;
        size_t escape_length = 0;
        bool unicode_escape = false;
        if (!ruda_utf8_decode(
                decoded_data + cursor,
                decoded_length - cursor,
                &scalar
            )) {
            formatter->status = RUDA_FORMAT_SOURCE_INVALID;
            break;
        }
        if (scalar.value == '\\') {
            escape = slash_escape;
            escape_length = sizeof(slash_escape) - 1;
        } else if (scalar.value == '"') {
            escape = quote_escape;
            escape_length = sizeof(quote_escape) - 1;
        } else if (scalar.value == '\n') {
            escape = newline_escape;
            escape_length = sizeof(newline_escape) - 1;
        } else if (scalar.value == '\r') {
            escape = carriage_escape;
            escape_length = sizeof(carriage_escape) - 1;
        } else if (scalar.value == '\t') {
            escape = tab_escape;
            escape_length = sizeof(tab_escape) - 1;
        } else if (scalar.value == 0) {
            escape = zero_escape;
            escape_length = sizeof(zero_escape) - 1;
        } else if (scalar.value <= UINT32_C(0x1f)
            || (scalar.value >= UINT32_C(0x7f)
                && scalar.value <= UINT32_C(0x9f))) {
            unicode_escape = true;
        }
        if (escape != NULL || unicode_escape) {
            ruda_formatter_text(
                formatter,
                decoded_data + literal_start,
                cursor - literal_start
            );
            if (unicode_escape) {
                ruda_formatter_unicode_escape(formatter, scalar.value);
            } else {
                ruda_formatter_text(formatter, escape, escape_length);
            }
            cursor += scalar.width;
            literal_start = cursor;
        } else {
            cursor += scalar.width;
        }
    }
    if (formatter->status == RUDA_FORMAT_OK) {
        ruda_formatter_text(
            formatter,
            decoded_data + literal_start,
            decoded_length - literal_start
        );
        ruda_formatter_text(formatter, quote, sizeof(quote) - 1);
    }
}

static void ruda_formatter_break_text(
    RudaFormatter *formatter,
    const unsigned char *text,
    size_t length
) {
    if (formatter->status == RUDA_FORMAT_OK) {
        ruda_formatter_document_status(
            formatter,
            ruda_document_break_text(&formatter->document, text, length)
        );
    }
}

static bool ruda_formatter_part_is_line(RudaDocumentPartKind kind) {
    return kind == RUDA_DOCUMENT_HARDLINE || kind == RUDA_DOCUMENT_LITERAL_LINE;
}

static size_t ruda_formatter_trailing_lines(const RudaFormatter *formatter) {
    size_t count = 0;
    size_t index = formatter->document.parts.length;
    while (index != 0) {
        const RudaDocumentPart *part = ruda_vector_at_const(
            &formatter->document.parts,
            index - 1
        );
        if (part == NULL || !ruda_formatter_part_is_line(part->kind)) {
            break;
        }
        count += 1;
        index -= 1;
    }
    return count;
}

static void ruda_formatter_ensure_hardlines(
    RudaFormatter *formatter,
    size_t count
) {
    size_t existing = ruda_formatter_trailing_lines(formatter);
    while (existing < count && formatter->status == RUDA_FORMAT_OK) {
        ruda_formatter_part(formatter, RUDA_DOCUMENT_HARDLINE);
        existing += 1;
    }
    formatter->pending_space = false;
    formatter->at_line_start = true;
    formatter->after_flexible_line = false;
}

static void ruda_formatter_softline(
    RudaFormatter *formatter,
    RudaDocumentPartKind kind
) {
    ruda_formatter_part(formatter, kind);
    formatter->pending_space = false;
    formatter->at_line_start = false;
    formatter->after_flexible_line = true;
}

static void ruda_formatter_space(RudaFormatter *formatter) {
    static const unsigned char space[] = " ";
    if (!formatter->at_line_start && !formatter->after_flexible_line) {
        ruda_formatter_text(formatter, space, 1);
    }
    formatter->pending_space = false;
}

static void ruda_formatter_pending_space(RudaFormatter *formatter) {
    if (formatter->pending_space) {
        ruda_formatter_space(formatter);
    }
}

static const RudaFormatToken *ruda_formatter_token(
    const RudaFormatter *formatter,
    size_t index
) {
    return ruda_vector_at_const(formatter->tokens, index);
}

static const RudaFormatToken *ruda_formatter_next_visible(
    const RudaFormatter *formatter,
    size_t index
) {
    while (index < formatter->tokens->length) {
        const RudaFormatToken *token = ruda_formatter_token(formatter, index);
        if (token == NULL) {
            return NULL;
        }
        if (ruda_format_token_is_visible(token->token.kind)) {
            return token;
        }
        index += 1;
    }
    return NULL;
}

static bool ruda_formatter_gap_has_newline(
    const RudaFormatter *formatter,
    size_t start,
    size_t end
) {
    size_t index = start;
    if (end > formatter->source_length || start > end) {
        return false;
    }
    while (index < end) {
        if (formatter->source[index] == '\n'
            || formatter->source[index] == '\r') {
            return true;
        }
        index += 1;
    }
    return false;
}

static bool ruda_formatter_next_comment_same_line(
    const RudaFormatter *formatter,
    const RudaFormatToken *token,
    const RudaFormatToken *next
) {
    return next != NULL && ruda_format_token_is_comment(next->token.kind)
        && !ruda_formatter_gap_has_newline(
            formatter,
            token->token.end,
            next->token.start
        );
}

static void ruda_formatter_emit_token_text(
    RudaFormatter *formatter,
    const RudaFormatToken *token
) {
    if (token->token.end < token->token.start
        || token->token.end > formatter->source_length) {
        formatter->status = RUDA_FORMAT_INVALID_ARGUMENT;
        return;
    }
    ruda_formatter_text(
        formatter,
        formatter->source + token->token.start,
        token->token.end - token->token.start
    );
}

static void ruda_formatter_emit_comment(
    RudaFormatter *formatter,
    const RudaFormatToken *token,
    const RudaFormatToken *next
) {
    const unsigned char *bytes = formatter->source + token->token.start;
    size_t length = token->token.end - token->token.start;
    size_t segment_start = 0;
    size_t index = 0;
    if (!formatter->at_line_start && !formatter->after_flexible_line) {
        ruda_formatter_space(formatter);
    }
    while (index < length && formatter->status == RUDA_FORMAT_OK) {
        if (bytes[index] == '\n' || bytes[index] == '\r') {
            ruda_formatter_text(
                formatter,
                bytes + segment_start,
                index - segment_start
            );
            ruda_formatter_part(formatter, RUDA_DOCUMENT_LITERAL_LINE);
            formatter->at_line_start = true;
            formatter->after_flexible_line = false;
            formatter->pending_space = false;
            if (bytes[index] == '\r' && index + 1 < length
                && bytes[index + 1] == '\n') {
                index += 1;
            }
            segment_start = index + 1;
        }
        index += 1;
    }
    if (segment_start <= length) {
        ruda_formatter_text(
            formatter,
            bytes + segment_start,
            length - segment_start
        );
    }
    if (token->token.kind == RUDA_TOKEN_LINE_COMMENT
        || token->token.kind == RUDA_TOKEN_DOC_LINE_COMMENT
        || next == NULL
        || ruda_formatter_gap_has_newline(
            formatter,
            token->token.end,
            next->token.start
        )) {
        ruda_formatter_ensure_hardlines(formatter, 1);
    } else {
        formatter->pending_space = true;
    }
    formatter->last_was_comment = true;
}

static bool ruda_formatter_postfix_predecessor(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_IDENTIFIER || kind == RUDA_TOKEN_INTEGER
        || kind == RUDA_TOKEN_FLOAT || kind == RUDA_TOKEN_STRING
        || kind == RUDA_TOKEN_RAW_STRING || kind == RUDA_TOKEN_BYTE_STRING
        || kind == RUDA_TOKEN_RUNE || kind == RUDA_TOKEN_KW_TRUE
        || kind == RUDA_TOKEN_KW_FALSE
        || kind == RUDA_TOKEN_RIGHT_PAREN
        || kind == RUDA_TOKEN_RIGHT_BRACKET || kind == RUDA_TOKEN_RIGHT_BRACE
        || kind == RUDA_TOKEN_INTERPOLATED_END || kind == RUDA_TOKEN_QUESTION
        || kind == RUDA_TOKEN_GREATER || kind == RUDA_TOKEN_SHIFT_RIGHT;
}

static bool ruda_formatter_operator_spaced(const RudaFormatToken *token) {
    RudaTokenKind kind = token->token.kind;
    if (kind == RUDA_TOKEN_BANG || kind == RUDA_TOKEN_TILDE
        || kind == RUDA_TOKEN_QUESTION) {
        return false;
    }
    if (kind == RUDA_TOKEN_MINUS
        && token->parent_kind == RUDA_SYNTAX_UNARY_EXPRESSION) {
        return false;
    }
    if (kind == RUDA_TOKEN_LESS || kind == RUDA_TOKEN_GREATER
        || kind == RUDA_TOKEN_SHIFT_RIGHT) {
        return token->parent_kind == RUDA_SYNTAX_BINARY_EXPRESSION;
    }
    return kind == RUDA_TOKEN_PLUS || kind == RUDA_TOKEN_MINUS
        || kind == RUDA_TOKEN_STAR || kind == RUDA_TOKEN_SLASH
        || kind == RUDA_TOKEN_PERCENT || kind == RUDA_TOKEN_AMPERSAND
        || kind == RUDA_TOKEN_PIPE || kind == RUDA_TOKEN_CARET
        || kind == RUDA_TOKEN_EQUAL || kind == RUDA_TOKEN_FAT_ARROW
        || kind == RUDA_TOKEN_THIN_ARROW || kind == RUDA_TOKEN_EQUAL_EQUAL
        || kind == RUDA_TOKEN_BANG_EQUAL || kind == RUDA_TOKEN_LESS_EQUAL
        || kind == RUDA_TOKEN_GREATER_EQUAL
        || kind == RUDA_TOKEN_AMPERSAND_AMPERSAND
        || kind == RUDA_TOKEN_PIPE_PIPE || kind == RUDA_TOKEN_SHIFT_LEFT
        || kind == RUDA_TOKEN_PLUS_EQUAL || kind == RUDA_TOKEN_MINUS_EQUAL
        || kind == RUDA_TOKEN_STAR_EQUAL || kind == RUDA_TOKEN_SLASH_EQUAL
        || kind == RUDA_TOKEN_PERCENT_EQUAL
        || kind == RUDA_TOKEN_AMPERSAND_EQUAL
        || kind == RUDA_TOKEN_PIPE_EQUAL || kind == RUDA_TOKEN_CARET_EQUAL
        || kind == RUDA_TOKEN_SHIFT_LEFT_EQUAL
        || kind == RUDA_TOKEN_SHIFT_RIGHT_EQUAL;
}

static bool ruda_formatter_open_group_needs_space(
    const RudaFormatter *formatter,
    const RudaFormatToken *token
) {
    return formatter->pending_space
        && (token->parent_kind == RUDA_SYNTAX_PAREN_GROUP
            || !formatter->has_previous_code
            || !ruda_formatter_postfix_predecessor(
                formatter->previous_code_kind
            ));
}

static bool ruda_formatter_trailing_comma_kind(RudaSyntaxKind parent_kind) {
    return parent_kind == RUDA_SYNTAX_ARGUMENTS
        || parent_kind == RUDA_SYNTAX_PARAMETERS
        || parent_kind == RUDA_SYNTAX_ARRAY_EXPRESSION
        || parent_kind == RUDA_SYNTAX_IMPORT_CLAUSE
        || parent_kind == RUDA_SYNTAX_POSITIONAL_PATTERN
        || parent_kind == RUDA_SYNTAX_TUPLE_PATTERN
        || parent_kind == RUDA_SYNTAX_NAMED_PATTERN;
}

static void ruda_formatter_push_delimiter(
    RudaFormatter *formatter,
    RudaTokenKind open,
    RudaSyntaxKind parent_kind,
    bool empty,
    bool grouped,
    bool trailing_comma
) {
    RudaFormatDelimiter delimiter;
    if (formatter->delimiters.length >= formatter->options.max_nesting) {
        formatter->status = RUDA_FORMAT_LIMIT_REACHED;
        return;
    }
    delimiter.open = open;
    delimiter.parent_kind = parent_kind;
    delimiter.empty = empty;
    delimiter.grouped = grouped;
    delimiter.trailing_comma = trailing_comma;
    if (!ruda_vector_push(&formatter->delimiters, &delimiter)) {
        formatter->status = RUDA_FORMAT_ALLOCATION_FAILED;
    }
}

static RudaFormatDelimiter *ruda_formatter_top_delimiter(RudaFormatter *formatter) {
    if (formatter->delimiters.length == 0) {
        return NULL;
    }
    return ruda_vector_at(
        &formatter->delimiters,
        formatter->delimiters.length - 1
    );
}

static bool ruda_formatter_delimiter_matches(
    RudaTokenKind open,
    RudaTokenKind close
) {
    return (open == RUDA_TOKEN_LEFT_PAREN && close == RUDA_TOKEN_RIGHT_PAREN)
        || (open == RUDA_TOKEN_LEFT_BRACKET && close == RUDA_TOKEN_RIGHT_BRACKET)
        || (open == RUDA_TOKEN_LEFT_BRACE && close == RUDA_TOKEN_RIGHT_BRACE);
}

static void ruda_formatter_open_group(
    RudaFormatter *formatter,
    const RudaFormatToken *token,
    const RudaFormatToken *next
) {
    RudaTokenKind expected_close = token->token.kind == RUDA_TOKEN_LEFT_PAREN
        ? RUDA_TOKEN_RIGHT_PAREN
        : RUDA_TOKEN_RIGHT_BRACKET;
    bool empty = next != NULL && next->token.kind == expected_close;
    if (ruda_formatter_open_group_needs_space(formatter, token)) {
        ruda_formatter_space(formatter);
    } else {
        formatter->pending_space = false;
    }
    ruda_formatter_part(formatter, RUDA_DOCUMENT_GROUP_BEGIN);
    ruda_formatter_emit_token_text(formatter, token);
    if (!empty) {
        ruda_formatter_part(formatter, RUDA_DOCUMENT_INDENT_BEGIN);
        ruda_formatter_softline(formatter, RUDA_DOCUMENT_SOFTBREAK);
    }
    ruda_formatter_push_delimiter(
        formatter,
        token->token.kind,
        token->parent_kind,
        empty,
        true,
        ruda_formatter_trailing_comma_kind(token->parent_kind)
    );
    formatter->pending_space = false;
}

static void ruda_formatter_close_group(
    RudaFormatter *formatter,
    const RudaFormatToken *token,
    const RudaFormatToken *next
) {
    static const unsigned char comma[] = ",";
    RudaFormatDelimiter *delimiter = ruda_formatter_top_delimiter(formatter);
    bool type_prefix_adjacent = false;
    if (delimiter == NULL
        || !ruda_formatter_delimiter_matches(
            delimiter->open,
            token->token.kind
        )) {
        formatter->status = RUDA_FORMAT_SOURCE_INVALID;
        return;
    }
    type_prefix_adjacent = ruda_formatter_type_prefix_is_adjacent(
        delimiter,
        token,
        next
    );
    if (!delimiter->empty) {
        if (delimiter->trailing_comma
            && formatter->has_previous_code
            && formatter->previous_code_kind != RUDA_TOKEN_COMMA
            && !formatter->last_was_comment) {
            ruda_formatter_break_text(formatter, comma, 1);
        }
        ruda_formatter_part(formatter, RUDA_DOCUMENT_INDENT_END);
        ruda_formatter_softline(formatter, RUDA_DOCUMENT_SOFTBREAK);
    }
    ruda_formatter_emit_token_text(formatter, token);
    ruda_formatter_part(formatter, RUDA_DOCUMENT_GROUP_END);
    formatter->delimiters.length -= 1;
    if (token->in_attribute
        && !ruda_formatter_next_comment_same_line(formatter, token, next)) {
        ruda_formatter_ensure_hardlines(formatter, 1);
    } else {
        formatter->pending_space = next != NULL && !type_prefix_adjacent;
    }
}

static void ruda_formatter_open_brace(
    RudaFormatter *formatter,
    const RudaFormatToken *token,
    const RudaFormatToken *next
) {
    bool empty = next != NULL && next->token.kind == RUDA_TOKEN_RIGHT_BRACE;
    bool grouped = token->in_record_expression
        || token->parent_kind == RUDA_SYNTAX_IMPORT_CLAUSE
        || token->parent_kind == RUDA_SYNTAX_ENUM_VARIANT
        || token->parent_kind == RUDA_SYNTAX_NAMED_PAYLOAD
        || token->parent_kind == RUDA_SYNTAX_NAMED_PATTERN;
    if (!formatter->at_line_start) {
        ruda_formatter_space(formatter);
    }
    if (grouped) {
        ruda_formatter_part(formatter, RUDA_DOCUMENT_GROUP_BEGIN);
    }
    ruda_formatter_emit_token_text(formatter, token);
    if (!empty) {
        ruda_formatter_part(formatter, RUDA_DOCUMENT_INDENT_BEGIN);
        if (grouped) {
            ruda_formatter_softline(formatter, RUDA_DOCUMENT_SOFTLINE);
        } else {
            ruda_formatter_ensure_hardlines(formatter, 1);
        }
    }
    ruda_formatter_push_delimiter(
        formatter,
        RUDA_TOKEN_LEFT_BRACE,
        token->parent_kind,
        empty,
        grouped,
        token->in_record_expression
            || ruda_formatter_trailing_comma_kind(token->parent_kind)
    );
    formatter->pending_space = false;
}

static void ruda_formatter_close_brace(
    RudaFormatter *formatter,
    const RudaFormatToken *token,
    const RudaFormatToken *next
) {
    RudaFormatDelimiter *delimiter = ruda_formatter_top_delimiter(formatter);
    if (delimiter == NULL || delimiter->open != RUDA_TOKEN_LEFT_BRACE) {
        formatter->status = RUDA_FORMAT_SOURCE_INVALID;
        return;
    }
    if (!delimiter->empty) {
        static const unsigned char comma[] = ",";
        if ((token->in_record_expression || delimiter->trailing_comma)
            && formatter->has_previous_code
            && formatter->previous_code_kind != RUDA_TOKEN_COMMA
            && !formatter->last_was_comment) {
            if (delimiter->grouped) {
                ruda_formatter_break_text(formatter, comma, 1);
            } else {
                ruda_formatter_text(formatter, comma, 1);
            }
        }
        if (delimiter->grouped) {
            ruda_formatter_part(formatter, RUDA_DOCUMENT_INDENT_END);
            if (!formatter->after_flexible_line) {
                ruda_formatter_softline(formatter, RUDA_DOCUMENT_SOFTLINE);
            }
        } else {
            ruda_formatter_ensure_hardlines(formatter, 1);
            ruda_formatter_part(formatter, RUDA_DOCUMENT_INDENT_END);
        }
    }
    ruda_formatter_emit_token_text(formatter, token);
    if (delimiter->grouped) {
        ruda_formatter_part(formatter, RUDA_DOCUMENT_GROUP_END);
    }
    formatter->delimiters.length -= 1;
    if (token->parent_kind == RUDA_SYNTAX_BLOCK
        && next != NULL && next->token.kind != RUDA_TOKEN_KW_ELSE
        && !ruda_formatter_next_comment_same_line(formatter, token, next)
        && next->token.kind != RUDA_TOKEN_COMMA
        && next->token.kind != RUDA_TOKEN_SEMICOLON
        && next->token.kind != RUDA_TOKEN_RIGHT_PAREN
        && next->token.kind != RUDA_TOKEN_RIGHT_BRACKET
        && next->token.kind != RUDA_TOKEN_RIGHT_BRACE) {
        ruda_formatter_ensure_hardlines(formatter, 1);
    } else {
        formatter->pending_space = next != NULL;
    }
}

static void ruda_formatter_comma(
    RudaFormatter *formatter,
    const RudaFormatToken *token,
    const RudaFormatToken *next
) {
    RudaFormatDelimiter *delimiter = ruda_formatter_top_delimiter(formatter);
    formatter->pending_space = false;
    if (delimiter != NULL && delimiter->grouped
        && next != NULL
        && ruda_formatter_delimiter_matches(
            delimiter->open,
            next->token.kind
        )) {
        static const unsigned char comma[] = ",";
        ruda_formatter_break_text(formatter, comma, 1);
        return;
    }
    ruda_formatter_emit_token_text(formatter, token);
    if (token->parent_kind == RUDA_SYNTAX_TYPE
        || token->parent_kind == RUDA_SYNTAX_TYPE_PARAMETERS
        || token->parent_kind == RUDA_SYNTAX_TYPE_ARGUMENTS) {
        formatter->pending_space = true;
    } else if (delimiter != NULL && delimiter->grouped) {
        ruda_formatter_softline(formatter, RUDA_DOCUMENT_SOFTLINE);
    } else if (delimiter != NULL && delimiter->open == RUDA_TOKEN_LEFT_BRACE) {
        ruda_formatter_ensure_hardlines(formatter, 1);
    } else {
        formatter->pending_space = true;
    }
}

static void ruda_formatter_operator(
    RudaFormatter *formatter,
    const RudaFormatToken *token
) {
    if (ruda_formatter_operator_spaced(token)) {
        if (!formatter->at_line_start && !formatter->after_flexible_line) {
            ruda_formatter_space(formatter);
        }
        ruda_formatter_emit_token_text(formatter, token);
        formatter->pending_space = true;
    } else if ((token->token.kind == RUDA_TOKEN_LESS
            || token->token.kind == RUDA_TOKEN_GREATER
            || token->token.kind == RUDA_TOKEN_SHIFT_RIGHT)
        && token->parent_kind != RUDA_SYNTAX_BINARY_EXPRESSION) {
        formatter->pending_space = false;
        ruda_formatter_emit_token_text(formatter, token);
    } else {
        ruda_formatter_pending_space(formatter);
        ruda_formatter_emit_token_text(formatter, token);
        formatter->pending_space = false;
    }
}

static bool ruda_formatter_is_operator_token(RudaTokenKind kind) {
    return kind >= RUDA_TOKEN_PLUS && kind <= RUDA_TOKEN_SHIFT_RIGHT_EQUAL;
}

static void ruda_formatter_interpolation_token(
    RudaFormatter *formatter,
    const RudaFormatToken *token
) {
    RudaTokenKind kind = token->token.kind;
    if (kind == RUDA_TOKEN_INTERPOLATED_START) {
        ruda_formatter_pending_space(formatter);
    }
    ruda_formatter_emit_token_text(formatter, token);
    formatter->pending_space = kind == RUDA_TOKEN_INTERPOLATED_END;
}

static void ruda_formatter_trim_final_lines(RudaFormatter *formatter) {
    while (formatter->document.parts.length != 0) {
        const RudaDocumentPart *part = ruda_vector_at_const(
            &formatter->document.parts,
            formatter->document.parts.length - 1
        );
        if (part == NULL || !ruda_formatter_part_is_line(part->kind)) {
            break;
        }
        formatter->document.parts.length -= 1;
    }
    formatter->at_line_start = false;
    ruda_formatter_ensure_hardlines(formatter, 1);
}

static void ruda_formatter_build(RudaFormatter *formatter) {
    size_t index = 0;
    const RudaFormatToken *previous_visible = NULL;
    for (index = 0; index < formatter->tokens->length
        && formatter->status == RUDA_FORMAT_OK; index += 1) {
        const RudaFormatToken *token = ruda_formatter_token(formatter, index);
        const RudaFormatToken *next = ruda_formatter_next_visible(
            formatter,
            index + 1
        );
        RudaTokenKind kind;
        if (token == NULL) {
            formatter->status = RUDA_FORMAT_INVALID_ARGUMENT;
            break;
        }
        kind = token->token.kind;
        if (ruda_format_token_is_trivia(kind) || kind == RUDA_TOKEN_EOF) {
            continue;
        }
        if (token->top_level_id != 0 && formatter->last_top_level_id != 0
            && token->top_level_id != formatter->last_top_level_id) {
            ruda_formatter_ensure_hardlines(
                formatter,
                previous_visible != NULL
                    && previous_visible->top_level_kind == RUDA_SYNTAX_IMPORT_DECL
                    && token->top_level_kind == RUDA_SYNTAX_IMPORT_DECL
                    ? (previous_visible->import_group == token->import_group
                        ? 1
                        : 2)
                    : 2
            );
        }
        if (previous_visible != NULL
            && !ruda_format_token_is_comment(kind)
            && !ruda_format_token_is_comment(previous_visible->token.kind)
            && previous_visible->in_attribute
            && !token->in_attribute) {
            ruda_formatter_ensure_hardlines(formatter, 1);
        }
        if (token->top_level_id != 0) {
            formatter->last_top_level_id = token->top_level_id;
        }
        if (ruda_format_token_is_comment(kind)) {
            ruda_formatter_emit_comment(formatter, token, next);
            previous_visible = token;
            continue;
        }
        formatter->last_was_comment = false;
        if (kind >= RUDA_TOKEN_INTERPOLATED_START
            && kind <= RUDA_TOKEN_INTERPOLATED_END) {
            ruda_formatter_interpolation_token(formatter, token);
        } else if (kind == RUDA_TOKEN_LEFT_PAREN
            || kind == RUDA_TOKEN_LEFT_BRACKET) {
            ruda_formatter_open_group(formatter, token, next);
        } else if (kind == RUDA_TOKEN_RIGHT_PAREN
            || kind == RUDA_TOKEN_RIGHT_BRACKET) {
            ruda_formatter_close_group(formatter, token, next);
        } else if (kind == RUDA_TOKEN_LEFT_BRACE) {
            ruda_formatter_open_brace(formatter, token, next);
        } else if (kind == RUDA_TOKEN_RIGHT_BRACE) {
            ruda_formatter_close_brace(formatter, token, next);
        } else if (kind == RUDA_TOKEN_COMMA) {
            ruda_formatter_comma(formatter, token, next);
        } else if (kind == RUDA_TOKEN_SEMICOLON) {
            RudaFormatDelimiter *delimiter = ruda_formatter_top_delimiter(
                formatter
            );
            formatter->pending_space = false;
            ruda_formatter_emit_token_text(formatter, token);
            if (token->in_for_header) {
                formatter->pending_space = true;
            } else if (delimiter != NULL && delimiter->grouped
                && (delimiter->parent_kind == RUDA_SYNTAX_ENUM_VARIANT
                    || delimiter->parent_kind == RUDA_SYNTAX_NAMED_PAYLOAD)) {
                ruda_formatter_softline(formatter, RUDA_DOCUMENT_SOFTLINE);
            } else if (ruda_formatter_next_comment_same_line(
                    formatter,
                    token,
                    next
                )) {
                formatter->pending_space = true;
            } else {
                ruda_formatter_ensure_hardlines(formatter, 1);
            }
        } else if (kind == RUDA_TOKEN_COLON) {
            formatter->pending_space = false;
            ruda_formatter_emit_token_text(formatter, token);
            formatter->pending_space = true;
        } else if (kind == RUDA_TOKEN_DOT || kind == RUDA_TOKEN_AT) {
            formatter->pending_space = false;
            ruda_formatter_emit_token_text(formatter, token);
        } else if (kind == RUDA_TOKEN_QUESTION) {
            formatter->pending_space = false;
            ruda_formatter_emit_token_text(formatter, token);
            formatter->pending_space = true;
        } else if (kind == RUDA_TOKEN_STRING
            || kind == RUDA_TOKEN_RAW_STRING) {
            ruda_formatter_pending_space(formatter);
            ruda_formatter_canonical_string(formatter, token);
            formatter->pending_space = true;
        } else if (ruda_formatter_is_operator_token(kind)) {
            ruda_formatter_operator(formatter, token);
        } else {
            ruda_formatter_pending_space(formatter);
            ruda_formatter_emit_token_text(formatter, token);
            formatter->pending_space = true;
        }
        formatter->previous_code_kind = kind;
        formatter->has_previous_code = true;
        previous_visible = token;
    }
    if (formatter->status == RUDA_FORMAT_OK
        && formatter->delimiters.length != 0) {
        formatter->status = RUDA_FORMAT_SOURCE_INVALID;
    }
    if (formatter->status == RUDA_FORMAT_OK) {
        ruda_formatter_trim_final_lines(formatter);
    }
}

static bool ruda_format_output_write(
    void *context,
    const char *bytes,
    size_t length
) {
    RudaFormatOutput *output = context;
    size_t new_length = 0;
    if (!ruda_size_add(output->bytes.length, length, &new_length)
        || new_length > output->max_bytes) {
        output->limit_reached = true;
        return false;
    }
    if (!ruda_vector_append(&output->bytes, bytes, length)) {
        output->allocation_failed = true;
        return false;
    }
    return true;
}

RudaFormatOptions ruda_format_default_options(void) {
    RudaFormatOptions options;
    options.max_tokens = 2000000;
    options.max_document_parts = 8000000;
    options.max_output_bytes = 64U * 1024U * 1024U;
    options.max_nesting = 512;
    options.max_render_steps = 8000000;
    return options;
}

RudaFormatStatus ruda_format_source(
    const RudaLexResult *lexed,
    const RudaParseResult *parsed,
    RudaAllocator allocator,
    RudaFormatOptions options,
    RudaFormatResult *result
) {
    RudaFormatCollector collector;
    RudaFormatter formatter;
    RudaFormatOutput output;
    RudaWriter writer;
    RudaDocumentRenderOptions render_options;
    RudaDocumentStatus render_status;
    RudaFormatStatus status = RUDA_FORMAT_OK;
    if (result == NULL) {
        return RUDA_FORMAT_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (lexed == NULL || !lexed->initialized || parsed == NULL
        || !parsed->initialized || lexed->snapshot == NULL
        || parsed->tree.snapshot != lexed->snapshot
        || parsed->tree.root == NULL || options.max_tokens == 0
        || options.max_document_parts == 0 || options.max_output_bytes == 0
        || options.max_nesting == 0 || options.max_render_steps == 0
        || !ruda_syntax_tree_is_lossless(&parsed->tree)) {
        return RUDA_FORMAT_INVALID_ARGUMENT;
    }
    if (lexed->diagnostics.length != 0 || parsed->diagnostics.length != 0
        || lexed->suppressed_diagnostics != 0
        || parsed->suppressed_diagnostics != 0) {
        return RUDA_FORMAT_SOURCE_INVALID;
    }
    memset(&collector, 0, sizeof(collector));
    collector.max_tokens = options.max_tokens;
    collector.max_nesting = options.max_nesting;
    collector.status = RUDA_FORMAT_OK;
    if (!ruda_vector_init(&collector.tokens, allocator, sizeof(RudaFormatToken))) {
        return RUDA_FORMAT_INVALID_ARGUMENT;
    }
    if (!ruda_format_collect_node(
            &collector,
            parsed->tree.root,
            1,
            RUDA_SYNTAX_ROOT,
            0,
            false,
            false,
            false
        )) {
        status = collector.status;
        goto cleanup_collector;
    }
    status = ruda_format_order_imports(
        &collector.tokens,
        ruda_source_bytes(lexed->snapshot),
        ruda_source_length(lexed->snapshot),
        allocator
    );
    if (status != RUDA_FORMAT_OK) {
        goto cleanup_collector;
    }
    memset(&formatter, 0, sizeof(formatter));
    formatter.allocator = allocator;
    formatter.snapshot = lexed->snapshot;
    formatter.source = ruda_source_bytes(lexed->snapshot);
    formatter.source_length = ruda_source_length(lexed->snapshot);
    formatter.tokens = &collector.tokens;
    formatter.options = options;
    formatter.status = RUDA_FORMAT_OK;
    formatter.at_line_start = true;
    if (!ruda_document_init(
            &formatter.document,
            allocator,
            options.max_document_parts
        ) || !ruda_vector_init(
            &formatter.delimiters,
            allocator,
            sizeof(RudaFormatDelimiter)
        ) || !ruda_vector_init(
            &formatter.owned_text,
            allocator,
            sizeof(RudaOwnedBytes)
        )) {
        status = RUDA_FORMAT_INVALID_ARGUMENT;
        goto cleanup_formatter;
    }
    ruda_formatter_build(&formatter);
    if (formatter.status != RUDA_FORMAT_OK) {
        status = formatter.status;
        goto cleanup_formatter;
    }
    memset(&output, 0, sizeof(output));
    output.max_bytes = options.max_output_bytes;
    if (!ruda_vector_init(&output.bytes, allocator, sizeof(unsigned char))) {
        status = RUDA_FORMAT_INVALID_ARGUMENT;
        goto cleanup_formatter;
    }
    writer.context = &output;
    writer.write = ruda_format_output_write;
    render_options = ruda_document_default_render_options();
    render_options.max_nesting = options.max_nesting;
    render_options.max_scan_steps = options.max_render_steps;
    render_status = ruda_document_render(
        &formatter.document,
        writer,
        render_options
    );
    if (render_status != RUDA_DOCUMENT_OK) {
        status = output.limit_reached || render_status == RUDA_DOCUMENT_LIMIT_REACHED
            ? RUDA_FORMAT_LIMIT_REACHED
            : (output.allocation_failed
                || render_status == RUDA_DOCUMENT_ALLOCATION_FAILED
                ? RUDA_FORMAT_ALLOCATION_FAILED
                : RUDA_FORMAT_INVALID_ARGUMENT);
        ruda_vector_dispose(&output.bytes);
        goto cleanup_formatter;
    }
    if (!ruda_owned_bytes_copy(
            &result->output,
            allocator,
            output.bytes.data,
            output.bytes.length
        )) {
        status = RUDA_FORMAT_ALLOCATION_FAILED;
        ruda_vector_dispose(&output.bytes);
        goto cleanup_formatter;
    }
    result->changed = output.bytes.length != formatter.source_length
        || memcmp(
            output.bytes.data,
            formatter.source,
            output.bytes.length
        ) != 0;
    result->initialized = true;
    ruda_vector_dispose(&output.bytes);

cleanup_formatter:
    ruda_formatter_owned_text_dispose(&formatter.owned_text);
    ruda_vector_dispose(&formatter.delimiters);
    ruda_document_dispose(&formatter.document);
cleanup_collector:
    ruda_vector_dispose(&collector.tokens);
    return status;
}

void ruda_format_result_dispose(RudaFormatResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    ruda_owned_bytes_dispose(&result->output);
    result->changed = false;
    result->initialized = false;
}
