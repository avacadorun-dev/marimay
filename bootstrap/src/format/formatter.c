#include "formatter_internal.h"
#include "formatter_layout_api.h"
#include "formatter_groups_api.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/base/string_literal.h"
#include "mmay/base/utf8.h"
#include "mmay/base/writer.h"
#include "mmay/syntax/syntax.h"
static void mmay_formatter_owned_text_dispose(MarimayVector *owned_text) {
    size_t index = 0;
    for (index = 0; index < owned_text->length; index += 1) {
        MarimayOwnedBytes *text = mmay_vector_at(owned_text, index);
        if (text != NULL) {
            mmay_owned_bytes_dispose(text);
        }
    }
    mmay_vector_dispose(owned_text);
}
static void mmay_formatter_document_status(
    MarimayFormatter *formatter,
    MarimayDocumentStatus status
) {
    if (status == MMAY_DOCUMENT_OK || formatter->status != MMAY_FORMAT_OK) {
        return;
    }
    if (status == MMAY_DOCUMENT_ALLOCATION_FAILED) {
        formatter->status = MMAY_FORMAT_ALLOCATION_FAILED;
    } else if (status == MMAY_DOCUMENT_LIMIT_REACHED) {
        formatter->status = MMAY_FORMAT_LIMIT_REACHED;
    } else {
        formatter->status = MMAY_FORMAT_INVALID_ARGUMENT;
    }
}
static void mmay_formatter_part(
    MarimayFormatter *formatter,
    MarimayDocumentPartKind kind
) {
    if (formatter->status == MMAY_FORMAT_OK) {
        mmay_formatter_document_status(
            formatter,
            mmay_document_part(&formatter->document, kind)
        );
    }
}
static void mmay_formatter_text(
    MarimayFormatter *formatter,
    const unsigned char *text,
    size_t length
) {
    if (formatter->status == MMAY_FORMAT_OK) {
        mmay_formatter_document_status(
            formatter,
            mmay_document_text(&formatter->document, text, length)
        );
    }
    if (length != 0) {
        formatter->at_line_start = false;
        formatter->after_flexible_line = false;
    }
}
static bool mmay_formatter_adopt_text(
    MarimayFormatter *formatter,
    MarimayOwnedBytes *text
) {
    if (!mmay_vector_push(&formatter->owned_text, text)) {
        formatter->status = MMAY_FORMAT_ALLOCATION_FAILED;
        mmay_owned_bytes_dispose(text);
        return false;
    }
    text->data = NULL;
    text->length = 0;
    return true;
}

static void mmay_formatter_copy_text(
    MarimayFormatter *formatter,
    const unsigned char *text,
    size_t length
) {
    MarimayOwnedBytes owned;
    const unsigned char *stable = NULL;
    if (formatter->status != MMAY_FORMAT_OK) {
        return;
    }
    if (!mmay_owned_bytes_copy(&owned, formatter->allocator, text, length)) {
        formatter->status = MMAY_FORMAT_ALLOCATION_FAILED;
        return;
    }
    stable = owned.data;
    if (!mmay_formatter_adopt_text(formatter, &owned)) {
        return;
    }
    mmay_formatter_text(formatter, stable, length);
}

static void mmay_formatter_unicode_escape(
    MarimayFormatter *formatter,
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
    mmay_formatter_copy_text(formatter, spelling, digit_count + 4);
}

static bool mmay_formatter_raw_string_is_shorter(
    const MarimayOwnedBytes *decoded
) {
    size_t cursor = 0;
    size_t backslashes = 0;
    while (cursor < decoded->length) {
        MarimayUtf8Scalar scalar;
        if (!mmay_utf8_decode(
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

static void mmay_formatter_canonical_string(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token
) {
    static const unsigned char quote[] = "\"";
    static const unsigned char raw_start[] = "r\"";
    static const unsigned char slash_escape[] = "\\\\";
    static const unsigned char quote_escape[] = "\\\"";
    static const unsigned char newline_escape[] = "\\n";
    static const unsigned char carriage_escape[] = "\\r";
    static const unsigned char tab_escape[] = "\\t";
    static const unsigned char zero_escape[] = "\\0";
    MarimayOwnedBytes decoded;
    MarimayFormatStatus status = mmay_format_decode_ordinary_string(
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
    bool allow_raw = token->parent_kind == MMAY_SYNTAX_LITERAL_EXPRESSION;
    if (status != MMAY_FORMAT_OK) {
        formatter->status = status;
        return;
    }
    decoded_data = decoded.data;
    decoded_length = decoded.length;
    if (!mmay_formatter_adopt_text(formatter, &decoded)) {
        return;
    }
    {
        const MarimayOwnedBytes *stable = mmay_vector_at_const(
            &formatter->owned_text,
            formatter->owned_text.length - 1
        );
        if (stable == NULL) {
            formatter->status = MMAY_FORMAT_INVALID_ARGUMENT;
            return;
        }
        if (allow_raw && mmay_formatter_raw_string_is_shorter(stable)) {
            mmay_formatter_text(formatter, raw_start, sizeof(raw_start) - 1);
            mmay_formatter_text(formatter, decoded_data, decoded_length);
            mmay_formatter_text(formatter, quote, sizeof(quote) - 1);
            return;
        }
    }

    mmay_formatter_text(formatter, quote, sizeof(quote) - 1);
    while (cursor < decoded_length && formatter->status == MMAY_FORMAT_OK) {
        MarimayUtf8Scalar scalar;
        const unsigned char *escape = NULL;
        size_t escape_length = 0;
        bool unicode_escape = false;
        if (!mmay_utf8_decode(
                decoded_data + cursor,
                decoded_length - cursor,
                &scalar
            )) {
            formatter->status = MMAY_FORMAT_SOURCE_INVALID;
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
            mmay_formatter_text(
                formatter,
                decoded_data + literal_start,
                cursor - literal_start
            );
            if (unicode_escape) {
                mmay_formatter_unicode_escape(formatter, scalar.value);
            } else {
                mmay_formatter_text(formatter, escape, escape_length);
            }
            cursor += scalar.width;
            literal_start = cursor;
        } else {
            cursor += scalar.width;
        }
    }
    if (formatter->status == MMAY_FORMAT_OK) {
        mmay_formatter_text(
            formatter,
            decoded_data + literal_start,
            decoded_length - literal_start
        );
        mmay_formatter_text(formatter, quote, sizeof(quote) - 1);
    }
}

static void mmay_formatter_break_text(
    MarimayFormatter *formatter,
    const unsigned char *text,
    size_t length
) {
    if (formatter->status == MMAY_FORMAT_OK) {
        mmay_formatter_document_status(
            formatter,
            mmay_document_break_text(&formatter->document, text, length)
        );
    }
}

static bool mmay_formatter_part_is_line(MarimayDocumentPartKind kind) {
    return kind == MMAY_DOCUMENT_HARDLINE || kind == MMAY_DOCUMENT_LITERAL_LINE;
}

static size_t mmay_formatter_trailing_lines(const MarimayFormatter *formatter) {
    size_t count = 0;
    size_t index = formatter->document.parts.length;
    while (index != 0) {
        const MarimayDocumentPart *part = mmay_vector_at_const(
            &formatter->document.parts,
            index - 1
        );
        if (part == NULL || !mmay_formatter_part_is_line(part->kind)) {
            break;
        }
        count += 1;
        index -= 1;
    }
    return count;
}

static void mmay_formatter_ensure_hardlines(
    MarimayFormatter *formatter,
    size_t count
) {
    size_t existing = mmay_formatter_trailing_lines(formatter);
    while (existing < count && formatter->status == MMAY_FORMAT_OK) {
        mmay_formatter_part(formatter, MMAY_DOCUMENT_HARDLINE);
        existing += 1;
    }
    formatter->pending_space = false;
    formatter->at_line_start = true;
    formatter->after_flexible_line = false;
}

static void mmay_formatter_softline(
    MarimayFormatter *formatter,
    MarimayDocumentPartKind kind
) {
    mmay_formatter_part(formatter, kind);
    formatter->pending_space = false;
    formatter->at_line_start = false;
    formatter->after_flexible_line = true;
}

static void mmay_formatter_space(MarimayFormatter *formatter) {
    static const unsigned char space[] = " ";
    if (!formatter->at_line_start && !formatter->after_flexible_line) {
        mmay_formatter_text(formatter, space, 1);
    }
    formatter->pending_space = false;
}

static void mmay_formatter_pending_space(MarimayFormatter *formatter) {
    if (formatter->pending_space) {
        mmay_formatter_space(formatter);
    }
}

static const MarimayFormatToken *mmay_formatter_token(
    const MarimayFormatter *formatter,
    size_t index
) {
    return mmay_vector_at_const(formatter->tokens, index);
}

static const MarimayFormatToken *mmay_formatter_next_visible(
    const MarimayFormatter *formatter,
    size_t index
) {
    while (index < formatter->tokens->length) {
        const MarimayFormatToken *token = mmay_formatter_token(formatter, index);
        if (token == NULL) {
            return NULL;
        }
        if (mmay_format_token_is_visible(token->token.kind)) {
            return token;
        }
        index += 1;
    }
    return NULL;
}

static bool mmay_formatter_gap_has_newline(
    const MarimayFormatter *formatter,
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

static bool mmay_formatter_next_comment_same_line(
    const MarimayFormatter *formatter,
    const MarimayFormatToken *token,
    const MarimayFormatToken *next
) {
    return next != NULL && mmay_format_token_is_comment(next->token.kind)
        && !mmay_formatter_gap_has_newline(
            formatter,
            token->token.end,
            next->token.start
        );
}

static void mmay_formatter_emit_token_text(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token
) {
    if (token->token.end < token->token.start
        || token->token.end > formatter->source_length) {
        formatter->status = MMAY_FORMAT_INVALID_ARGUMENT;
        return;
    }
    mmay_formatter_text(
        formatter,
        formatter->source + token->token.start,
        token->token.end - token->token.start
    );
}

static void mmay_formatter_emit_comment(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token,
    const MarimayFormatToken *next
) {
    const unsigned char *bytes = formatter->source + token->token.start;
    size_t length = token->token.end - token->token.start;
    size_t segment_start = 0;
    size_t index = 0;
    if (!formatter->at_line_start && !formatter->after_flexible_line) {
        mmay_formatter_space(formatter);
    }
    while (index < length && formatter->status == MMAY_FORMAT_OK) {
        if (bytes[index] == '\n' || bytes[index] == '\r') {
            mmay_formatter_text(
                formatter,
                bytes + segment_start,
                index - segment_start
            );
            mmay_formatter_part(formatter, MMAY_DOCUMENT_LITERAL_LINE);
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
        mmay_formatter_text(
            formatter,
            bytes + segment_start,
            length - segment_start
        );
    }
    if (token->token.kind == MMAY_TOKEN_LINE_COMMENT
        || token->token.kind == MMAY_TOKEN_DOC_LINE_COMMENT
        || next == NULL
        || mmay_formatter_gap_has_newline(
            formatter,
            token->token.end,
            next->token.start
        )) {
        mmay_formatter_ensure_hardlines(formatter, 1);
    } else {
        formatter->pending_space = true;
    }
    formatter->last_was_comment = true;
}

static void mmay_formatter_push_delimiter(
    MarimayFormatter *formatter,
    MarimayTokenKind open,
    MarimaySyntaxKind parent_kind,
    bool empty,
    bool grouped,
    bool compact,
    bool trailing_comma
) {
    MarimayFormatDelimiter delimiter;
    if (formatter->delimiters.length >= formatter->options.max_nesting) {
        formatter->status = MMAY_FORMAT_LIMIT_REACHED;
        return;
    }
    delimiter.open = open;
    delimiter.parent_kind = parent_kind;
    delimiter.comma_count = 0;
    delimiter.empty = empty;
    delimiter.grouped = grouped;
    delimiter.compact = compact;
    delimiter.trailing_comma = trailing_comma;
    if (!mmay_vector_push(&formatter->delimiters, &delimiter)) {
        formatter->status = MMAY_FORMAT_ALLOCATION_FAILED;
    }
}

static MarimayFormatDelimiter *mmay_formatter_top_delimiter(MarimayFormatter *formatter) {
    if (formatter->delimiters.length == 0) {
        return NULL;
    }
    return mmay_vector_at(
        &formatter->delimiters,
        formatter->delimiters.length - 1
    );
}

static void mmay_formatter_open_group(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token,
    const MarimayFormatToken *next,
    bool compact
) {
    MarimayTokenKind expected_close = token->token.kind == MMAY_TOKEN_LEFT_PAREN
        ? MMAY_TOKEN_RIGHT_PAREN
        : MMAY_TOKEN_RIGHT_BRACKET;
    bool empty = next != NULL && next->token.kind == expected_close;
    if (mmay_formatter_open_group_needs_space(formatter, token)) {
        mmay_formatter_space(formatter);
    } else {
        formatter->pending_space = false;
    }
    if (token->in_interpolation) {
        mmay_formatter_emit_token_text(formatter, token);
        mmay_formatter_push_delimiter(
            formatter, token->token.kind, token->parent_kind,
            empty, false, false, false
        );
        formatter->pending_space = false;
        return;
    }
    if (compact) {
        mmay_formatter_emit_token_text(formatter, token);
        mmay_formatter_push_delimiter(
            formatter, token->token.kind, token->parent_kind,
            empty, false, true, false
        );
        formatter->pending_space = false;
        return;
    }
    mmay_formatter_part(formatter, MMAY_DOCUMENT_GROUP_BEGIN);
    mmay_formatter_emit_token_text(formatter, token);
    if (!empty) {
        mmay_formatter_part(formatter, MMAY_DOCUMENT_INDENT_BEGIN);
        mmay_formatter_softline(formatter, MMAY_DOCUMENT_SOFTBREAK);
    }
    mmay_formatter_push_delimiter(
        formatter,
        token->token.kind,
        token->parent_kind,
        empty,
        true,
        false,
        mmay_formatter_trailing_comma_kind(token->parent_kind)
    );
    formatter->pending_space = false;
}

static void mmay_formatter_close_group(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token,
    const MarimayFormatToken *next
) {
    static const unsigned char comma[] = ",";
    MarimayFormatDelimiter *delimiter = mmay_formatter_top_delimiter(formatter);
    bool type_prefix_adjacent = false;
    if (delimiter == NULL
        || !mmay_formatter_delimiter_matches(
            delimiter->open,
            token->token.kind
        )) {
        formatter->status = MMAY_FORMAT_SOURCE_INVALID;
        return;
    }
    type_prefix_adjacent = mmay_formatter_type_prefix_is_adjacent(
        delimiter,
        token,
        next
    );
    if (token->in_interpolation) {
        formatter->pending_space = false;
        mmay_formatter_emit_token_text(formatter, token);
        formatter->delimiters.length -= 1;
        return;
    }
    if (delimiter->compact) {
        formatter->pending_space = false;
        mmay_formatter_emit_token_text(formatter, token);
        formatter->delimiters.length -= 1;
        formatter->pending_space = next != NULL;
        return;
    }
    if (!delimiter->empty) {
        if (delimiter->trailing_comma
            && formatter->has_previous_code
            && formatter->previous_code_kind != MMAY_TOKEN_COMMA
            && !formatter->last_was_comment) {
            mmay_formatter_break_text(formatter, comma, 1);
        }
        mmay_formatter_part(formatter, MMAY_DOCUMENT_INDENT_END);
        mmay_formatter_softline(formatter, MMAY_DOCUMENT_SOFTBREAK);
    }
    mmay_formatter_emit_token_text(formatter, token);
    mmay_formatter_part(formatter, MMAY_DOCUMENT_GROUP_END);
    formatter->delimiters.length -= 1;
    if (token->in_attribute
        && !mmay_formatter_next_comment_same_line(formatter, token, next)) {
        mmay_formatter_ensure_hardlines(formatter, 1);
    } else {
        formatter->pending_space = next != NULL && !type_prefix_adjacent;
    }
}

static void mmay_formatter_open_brace(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token,
    const MarimayFormatToken *next,
    bool grouped
) {
    bool empty = next != NULL && next->token.kind == MMAY_TOKEN_RIGHT_BRACE;
    if (token->in_interpolation) {
        if (!formatter->at_line_start) mmay_formatter_space(formatter);
        mmay_formatter_emit_token_text(formatter, token);
        mmay_formatter_push_delimiter(
            formatter, MMAY_TOKEN_LEFT_BRACE, token->parent_kind,
            empty, false, false, false
        );
        formatter->pending_space = !empty;
        return;
    }
    if (!formatter->at_line_start) {
        mmay_formatter_space(formatter);
    }
    if (grouped) {
        mmay_formatter_part(formatter, MMAY_DOCUMENT_GROUP_BEGIN);
    }
    mmay_formatter_emit_token_text(formatter, token);
    if (!empty) {
        mmay_formatter_part(formatter, MMAY_DOCUMENT_INDENT_BEGIN);
        if (grouped) {
            mmay_formatter_softline(formatter, MMAY_DOCUMENT_SOFTLINE);
        } else {
            mmay_formatter_ensure_hardlines(formatter, 1);
        }
    }
    mmay_formatter_push_delimiter(
        formatter,
        MMAY_TOKEN_LEFT_BRACE,
        token->parent_kind,
        empty,
        grouped,
        false,
        token->in_record_expression
            || mmay_formatter_trailing_comma_kind(token->parent_kind)
    );
    formatter->pending_space = false;
}

static void mmay_formatter_close_brace(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token,
    const MarimayFormatToken *next
) {
    MarimayFormatDelimiter *delimiter = mmay_formatter_top_delimiter(formatter);
    if (delimiter == NULL || delimiter->open != MMAY_TOKEN_LEFT_BRACE) {
        formatter->status = MMAY_FORMAT_SOURCE_INVALID;
        return;
    }
    if (token->in_interpolation) {
        if (!delimiter->empty && formatter->pending_space) {
            mmay_formatter_space(formatter);
        }
        mmay_formatter_emit_token_text(formatter, token);
        formatter->delimiters.length -= 1;
        formatter->pending_space = false;
        return;
    }
    if (!delimiter->empty) {
        static const unsigned char comma[] = ",";
        if ((token->in_record_expression || delimiter->trailing_comma)
            && formatter->has_previous_code
            && formatter->previous_code_kind != MMAY_TOKEN_COMMA
            && !formatter->last_was_comment) {
            if (delimiter->grouped
                && delimiter->parent_kind != MMAY_SYNTAX_NAMED_PAYLOAD) {
                mmay_formatter_break_text(formatter, comma, 1);
            } else {
                mmay_formatter_text(formatter, comma, 1);
            }
        }
        if (delimiter->grouped) {
            mmay_formatter_part(formatter, MMAY_DOCUMENT_INDENT_END);
            if (!formatter->after_flexible_line) {
                mmay_formatter_softline(formatter, MMAY_DOCUMENT_SOFTLINE);
            }
        } else {
            mmay_formatter_ensure_hardlines(formatter, 1);
            mmay_formatter_part(formatter, MMAY_DOCUMENT_INDENT_END);
        }
    }
    mmay_formatter_emit_token_text(formatter, token);
    if (delimiter->grouped) {
        mmay_formatter_part(formatter, MMAY_DOCUMENT_GROUP_END);
    }
    formatter->delimiters.length -= 1;
    if (token->parent_kind == MMAY_SYNTAX_BLOCK
        && next != NULL && next->token.kind != MMAY_TOKEN_KW_ELSE
        && !mmay_formatter_next_comment_same_line(formatter, token, next)
        && next->token.kind != MMAY_TOKEN_COMMA
        && next->token.kind != MMAY_TOKEN_SEMICOLON
        && next->token.kind != MMAY_TOKEN_RIGHT_PAREN
        && next->token.kind != MMAY_TOKEN_RIGHT_BRACKET
        && next->token.kind != MMAY_TOKEN_RIGHT_BRACE) {
        mmay_formatter_ensure_hardlines(formatter, 1);
    } else {
        formatter->pending_space = next != NULL;
    }
}

static void mmay_formatter_comma(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token,
    const MarimayFormatToken *next
) {
    MarimayFormatDelimiter *delimiter = mmay_formatter_top_delimiter(formatter);
    bool required_tuple_comma = mmay_formatter_required_tuple_comma(delimiter);
    formatter->pending_space = false;
    if (token->in_interpolation) {
        mmay_formatter_emit_token_text(formatter, token);
        formatter->pending_space = true;
        return;
    }
    if (delimiter != NULL && delimiter->grouped
        && next != NULL
        && mmay_formatter_delimiter_matches(
            delimiter->open,
            next->token.kind
        )) {
        static const unsigned char comma[] = ",";
        delimiter->comma_count += 1;
        if (delimiter->parent_kind == MMAY_SYNTAX_NAMED_PAYLOAD
            || required_tuple_comma) {
            mmay_formatter_text(formatter, comma, 1);
        } else {
            mmay_formatter_break_text(formatter, comma, 1);
        }
        return;
    }
    if (delimiter != NULL) {
        delimiter->comma_count += 1;
    }
    mmay_formatter_emit_token_text(formatter, token);
    if (token->parent_kind == MMAY_SYNTAX_TYPE
        || token->parent_kind == MMAY_SYNTAX_TYPE_PARAMETERS
        || token->parent_kind == MMAY_SYNTAX_TYPE_ARGUMENTS) {
        formatter->pending_space = true;
    } else if (delimiter != NULL && delimiter->grouped) {
        mmay_formatter_softline(formatter, MMAY_DOCUMENT_SOFTLINE);
    } else if (mmay_formatter_next_comment_same_line(formatter, token, next)) {
        formatter->pending_space = true;
    } else if (delimiter != NULL && delimiter->open == MMAY_TOKEN_LEFT_BRACE) {
        mmay_formatter_ensure_hardlines(formatter, 1);
    } else {
        formatter->pending_space = true;
    }
}
static void mmay_formatter_operator(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token
) {
    if (!token->in_interpolation
        && (token->token.kind == MMAY_TOKEN_AMPERSAND_AMPERSAND
            || token->token.kind == MMAY_TOKEN_PIPE_PIPE)) {
        formatter->pending_space = false;
        if (token->boolean_indent) {
            mmay_formatter_part(formatter, MMAY_DOCUMENT_INDENT_BEGIN);
        }
        if (!formatter->at_line_start) {
            mmay_formatter_softline(formatter, MMAY_DOCUMENT_SOFTLINE);
        }
        mmay_formatter_emit_token_text(formatter, token);
        formatter->pending_space = true;
    } else if (mmay_formatter_operator_is_spaced(token)) {
        if (!formatter->at_line_start && !formatter->after_flexible_line) {
            mmay_formatter_space(formatter);
        }
        mmay_formatter_emit_token_text(formatter, token);
        formatter->pending_space = true;
    } else if ((token->token.kind == MMAY_TOKEN_LESS
            || token->token.kind == MMAY_TOKEN_GREATER
            || token->token.kind == MMAY_TOKEN_SHIFT_RIGHT)
        && token->parent_kind != MMAY_SYNTAX_BINARY_EXPRESSION) {
        formatter->pending_space = false;
        mmay_formatter_emit_token_text(formatter, token);
    } else {
        mmay_formatter_pending_space(formatter);
        mmay_formatter_emit_token_text(formatter, token);
        formatter->pending_space = false;
    }
}
static void mmay_formatter_interpolation_token(
    MarimayFormatter *formatter,
    const MarimayFormatToken *token
) {
    MarimayTokenKind kind = token->token.kind;
    if (kind == MMAY_TOKEN_INTERPOLATED_START) {
        mmay_formatter_pending_space(formatter);
    }
    mmay_formatter_emit_token_text(formatter, token);
    formatter->pending_space = kind == MMAY_TOKEN_INTERPOLATED_END;
}
static void mmay_formatter_trim_final_lines(MarimayFormatter *formatter) {
    while (formatter->document.parts.length != 0) {
        const MarimayDocumentPart *part = mmay_vector_at_const(
            &formatter->document.parts,
            formatter->document.parts.length - 1
        );
        if (part == NULL || !mmay_formatter_part_is_line(part->kind)) {
            break;
        }
        formatter->document.parts.length -= 1;
    }
    formatter->at_line_start = false;
    mmay_formatter_ensure_hardlines(formatter, 1);
}

static void mmay_formatter_build(MarimayFormatter *formatter) {
    size_t index = 0;
    const MarimayFormatToken *previous_visible = NULL;
    for (index = 0; index < formatter->tokens->length
        && formatter->status == MMAY_FORMAT_OK; index += 1) {
        const MarimayFormatToken *token = mmay_formatter_token(formatter, index);
        const MarimayFormatToken *next = mmay_formatter_next_visible(
            formatter,
            index + 1
        );
        MarimayTokenKind kind;
        if (token == NULL) {
            formatter->status = MMAY_FORMAT_INVALID_ARGUMENT;
            break;
        }
        kind = token->token.kind;
        if (mmay_format_token_is_trivia(kind) || kind == MMAY_TOKEN_EOF) {
            continue;
        }
        if (token->top_level_id != 0 && formatter->last_top_level_id != 0
            && token->top_level_id != formatter->last_top_level_id) {
            mmay_formatter_ensure_hardlines(
                formatter,
                previous_visible != NULL
                    && previous_visible->top_level_kind == MMAY_SYNTAX_IMPORT_DECL
                    && token->top_level_kind == MMAY_SYNTAX_IMPORT_DECL
                    ? (previous_visible->import_group == token->import_group
                        ? 1
                        : 2)
                    : 2
            );
        }
        if (previous_visible != NULL
            && !mmay_format_token_is_comment(kind)
            && !mmay_format_token_is_comment(previous_visible->token.kind)
            && previous_visible->in_attribute
            && !token->in_attribute) {
            mmay_formatter_ensure_hardlines(formatter, 1);
        }
        if (previous_visible != NULL
            && previous_visible->top_level_id == token->top_level_id
            && token->token.start >= previous_visible->token.end
            && token->token.kind != MMAY_TOKEN_RIGHT_BRACE
            && token->token.kind != MMAY_TOKEN_KW_ELSE
            && token->token.kind != MMAY_TOKEN_COMMA
            && ((previous_visible->statement_id != 0
                    && token->statement_id != 0
                    && previous_visible->statement_id != token->statement_id)
                || (previous_visible->token.kind == MMAY_TOKEN_RIGHT_BRACE
                    && token->statement_id != 0))
            && mmay_formatter_source_gap_has_blank_line(
                formatter->source,
                formatter->source_length,
                previous_visible->token.end,
                token->token.start
            )) {
            mmay_formatter_ensure_hardlines(formatter, 2);
        }
        if (token->top_level_id != 0) {
            formatter->last_top_level_id = token->top_level_id;
        }
        if (mmay_format_token_is_comment(kind)) {
            mmay_formatter_emit_comment(formatter, token, next);
            previous_visible = token;
            continue;
        }
        formatter->last_was_comment = false;
        if (!formatter->statement_grouped && token->statement_id != 0
            && token->statement_id != formatter->last_statement_id) {
            mmay_formatter_document_status(formatter, mmay_formatter_begin_statement_group(
                formatter, token->statement_id));
        }
        mmay_formatter_document_status(formatter, mmay_formatter_boolean_boundary(
            &formatter->document, token, true));
        if (kind >= MMAY_TOKEN_INTERPOLATED_START
            && kind <= MMAY_TOKEN_INTERPOLATED_END) {
            mmay_formatter_interpolation_token(formatter, token);
        } else if (kind == MMAY_TOKEN_LEFT_PAREN
            || kind == MMAY_TOKEN_LEFT_BRACKET) {
            bool compact_index = kind == MMAY_TOKEN_LEFT_BRACKET
                && mmay_formatter_index_is_compact(
                    formatter->tokens,
                    index,
                    formatter->source_length,
                    formatter->has_previous_code
                        && mmay_formatter_postfix_predecessor(
                            formatter->previous_code_kind
                        )
                );
            mmay_formatter_open_group(formatter, token, next, compact_index);
        } else if (kind == MMAY_TOKEN_RIGHT_PAREN
            || kind == MMAY_TOKEN_RIGHT_BRACKET) {
            mmay_formatter_close_group(formatter, token, next);
        } else if (kind == MMAY_TOKEN_LEFT_BRACE) {
            mmay_formatter_open_brace(
                formatter,
                token,
                next,
                mmay_formatter_brace_is_grouped(token)
            );
        } else if (kind == MMAY_TOKEN_RIGHT_BRACE) {
            mmay_formatter_close_brace(formatter, token, next);
        } else if (kind == MMAY_TOKEN_COMMA) {
            mmay_formatter_comma(formatter, token, next);
        } else if (kind == MMAY_TOKEN_SEMICOLON) {
            MarimayFormatDelimiter *delimiter = mmay_formatter_top_delimiter(
                formatter
            );
            formatter->pending_space = false;
            mmay_formatter_emit_token_text(formatter, token);
            if (formatter->statement_grouped
                && token->statement_id == formatter->active_statement_id
                && formatter->delimiters.length
                    == formatter->statement_group_depth) {
                mmay_formatter_document_status(formatter, mmay_formatter_end_statement_group(formatter));
            }
            if (token->in_for_header) {
                formatter->pending_space = true;
            } else if (delimiter != NULL && delimiter->grouped
                && (delimiter->parent_kind == MMAY_SYNTAX_ENUM_VARIANT
                    || delimiter->parent_kind == MMAY_SYNTAX_NAMED_PAYLOAD)) {
                mmay_formatter_softline(formatter, MMAY_DOCUMENT_SOFTLINE);
            } else if (mmay_formatter_next_comment_same_line(
                    formatter,
                    token,
                    next
                )) {
                formatter->pending_space = true;
            } else {
                mmay_formatter_ensure_hardlines(formatter, 1);
            }
        } else if (kind == MMAY_TOKEN_COLON) {
            formatter->pending_space = false;
            mmay_formatter_emit_token_text(formatter, token);
            formatter->pending_space = true;
        } else if (kind == MMAY_TOKEN_DOT || kind == MMAY_TOKEN_AT) {
            formatter->pending_space = false;
            mmay_formatter_emit_token_text(formatter, token);
        } else if (kind == MMAY_TOKEN_DOT_DOT
            || kind == MMAY_TOKEN_DOT_DOT_EQUAL) {
            formatter->pending_space = false;
            mmay_formatter_emit_token_text(formatter, token);
        } else if (kind == MMAY_TOKEN_QUESTION) {
            formatter->pending_space = false;
            mmay_formatter_emit_token_text(formatter, token);
            formatter->pending_space = true;
        } else if (kind == MMAY_TOKEN_STRING
            || kind == MMAY_TOKEN_RAW_STRING) {
            mmay_formatter_pending_space(formatter);
            mmay_formatter_canonical_string(formatter, token);
            formatter->pending_space = true;
        } else if (mmay_formatter_token_is_operator(kind)) {
            mmay_formatter_operator(formatter, token);
        } else {
            mmay_formatter_pending_space(formatter);
            mmay_formatter_emit_token_text(formatter, token);
            formatter->pending_space = true;
        }
        mmay_formatter_document_status(formatter, mmay_formatter_boolean_boundary(
            &formatter->document, token, false));
        formatter->previous_code_kind = kind;
        formatter->has_previous_code = true;
        if (token->statement_id != 0) {
            formatter->last_statement_id = token->statement_id;
        }
        previous_visible = token;
    }
    if (formatter->status == MMAY_FORMAT_OK
        && (formatter->delimiters.length != 0
            || formatter->statement_grouped)) {
        formatter->status = MMAY_FORMAT_SOURCE_INVALID;
    }
    if (formatter->status == MMAY_FORMAT_OK) {
        mmay_formatter_trim_final_lines(formatter);
    }
}

static bool mmay_format_output_write(
    void *context,
    const char *bytes,
    size_t length
) {
    MarimayFormatOutput *output = context;
    size_t new_length = 0;
    if (!mmay_size_add(output->bytes.length, length, &new_length)
        || new_length > output->max_bytes) {
        output->limit_reached = true;
        return false;
    }
    if (!mmay_vector_append(&output->bytes, bytes, length)) {
        output->allocation_failed = true;
        return false;
    }
    return true;
}

MarimayFormatOptions mmay_format_default_options(void) {
    MarimayFormatOptions options;
    options.max_tokens = 2000000;
    options.max_document_parts = 8000000;
    options.max_output_bytes = 64U * 1024U * 1024U;
    options.max_nesting = 512;
    options.max_render_steps = 8000000;
    return options;
}

MarimayFormatStatus mmay_format_source(
    const MarimayLexResult *lexed,
    const MarimayParseResult *parsed,
    MarimayAllocator allocator,
    MarimayFormatOptions options,
    MarimayFormatResult *result
) {
    MarimayFormatCollector collector;
    MarimayFormatter formatter;
    MarimayFormatOutput output;
    MarimayWriter writer;
    MarimayDocumentRenderOptions render_options;
    MarimayDocumentStatus render_status;
    MarimayFormatStatus status = MMAY_FORMAT_OK;
    if (result == NULL) {
        return MMAY_FORMAT_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (lexed == NULL || !lexed->initialized || parsed == NULL
        || !parsed->initialized || lexed->snapshot == NULL
        || parsed->tree.snapshot != lexed->snapshot
        || parsed->tree.root == NULL || options.max_tokens == 0
        || options.max_document_parts == 0 || options.max_output_bytes == 0
        || options.max_nesting == 0 || options.max_render_steps == 0
        || !mmay_syntax_tree_is_lossless(&parsed->tree)) {
        return MMAY_FORMAT_INVALID_ARGUMENT;
    }
    if (lexed->diagnostics.length != 0 || parsed->diagnostics.length != 0
        || lexed->suppressed_diagnostics != 0
        || parsed->suppressed_diagnostics != 0) {
        return MMAY_FORMAT_SOURCE_INVALID;
    }
    memset(&collector, 0, sizeof(collector));
    collector.max_tokens = options.max_tokens;
    collector.max_nesting = options.max_nesting;
    collector.status = MMAY_FORMAT_OK;
    if (!mmay_vector_init(&collector.tokens, allocator, sizeof(MarimayFormatToken))) {
        return MMAY_FORMAT_INVALID_ARGUMENT;
    }
    if (!mmay_format_collect_node(
            &collector,
            parsed->tree.root,
            1,
            MMAY_SYNTAX_ROOT,
            0,
            0,
            false,
            false,
            false,
            false,
            false
        )) {
        status = collector.status;
        goto cleanup_collector;
    }
    status = mmay_format_order_imports(
        &collector.tokens,
        mmay_source_bytes(lexed->snapshot),
        mmay_source_length(lexed->snapshot),
        allocator
    );
    if (status != MMAY_FORMAT_OK) {
        goto cleanup_collector;
    }
    memset(&formatter, 0, sizeof(formatter));
    formatter.allocator = allocator;
    formatter.snapshot = lexed->snapshot;
    formatter.source = mmay_source_bytes(lexed->snapshot);
    formatter.source_length = mmay_source_length(lexed->snapshot);
    formatter.tokens = &collector.tokens;
    formatter.options = options;
    formatter.status = MMAY_FORMAT_OK;
    formatter.at_line_start = true;
    if (!mmay_document_init(
            &formatter.document,
            allocator,
            options.max_document_parts
        ) || !mmay_vector_init(
            &formatter.delimiters,
            allocator,
            sizeof(MarimayFormatDelimiter)
        ) || !mmay_vector_init(
            &formatter.owned_text,
            allocator,
            sizeof(MarimayOwnedBytes)
        )) {
        status = MMAY_FORMAT_INVALID_ARGUMENT;
        goto cleanup_formatter;
    }
    mmay_formatter_build(&formatter);
    if (formatter.status != MMAY_FORMAT_OK) {
        status = formatter.status;
        goto cleanup_formatter;
    }
    memset(&output, 0, sizeof(output));
    output.max_bytes = options.max_output_bytes;
    if (!mmay_vector_init(&output.bytes, allocator, sizeof(unsigned char))) {
        status = MMAY_FORMAT_INVALID_ARGUMENT;
        goto cleanup_formatter;
    }
    writer.context = &output;
    writer.write = mmay_format_output_write;
    writer.flush = NULL;
    render_options = mmay_document_default_render_options();
    render_options.max_nesting = options.max_nesting;
    render_options.max_scan_steps = options.max_render_steps;
    render_status = mmay_document_render(
        &formatter.document,
        writer,
        render_options
    );
    if (render_status != MMAY_DOCUMENT_OK) {
        status = output.limit_reached || render_status == MMAY_DOCUMENT_LIMIT_REACHED
            ? MMAY_FORMAT_LIMIT_REACHED
            : (output.allocation_failed
                || render_status == MMAY_DOCUMENT_ALLOCATION_FAILED
                ? MMAY_FORMAT_ALLOCATION_FAILED
                : MMAY_FORMAT_INVALID_ARGUMENT);
        mmay_vector_dispose(&output.bytes);
        goto cleanup_formatter;
    }
    if (!mmay_owned_bytes_copy(
            &result->output,
            allocator,
            output.bytes.data,
            output.bytes.length
        )) {
        status = MMAY_FORMAT_ALLOCATION_FAILED;
        mmay_vector_dispose(&output.bytes);
        goto cleanup_formatter;
    }
    result->changed = output.bytes.length != formatter.source_length
        || memcmp(
            output.bytes.data,
            formatter.source,
            output.bytes.length
        ) != 0;
    result->initialized = true;
    mmay_vector_dispose(&output.bytes);

cleanup_formatter:
    mmay_formatter_owned_text_dispose(&formatter.owned_text);
    mmay_vector_dispose(&formatter.delimiters);
    mmay_document_dispose(&formatter.document);
cleanup_collector:
    mmay_vector_dispose(&collector.tokens);
    return status;
}

void mmay_format_result_dispose(MarimayFormatResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    mmay_owned_bytes_dispose(&result->output);
    result->changed = false;
    result->initialized = false;
}
