#include "lexer_internal.h"

#include <stdlib.h>
#include <string.h>

#include "mmay/base/unicode.h"
#include "mmay/base/utf8.h"
#include "mmay/generated/diagnostic_codes.h"


static bool mmay_lexer_add_diagnostic(
    MarimayLexer *lexer,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    MarimayLexDiagnostic diagnostic;
    if (lexer->result->diagnostics.length >= lexer->options.max_diagnostics) {
        lexer->result->suppressed_diagnostics += 1;
        lexer->limit_reached = true;
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.start = start;
    diagnostic.end = end > lexer->length ? lexer->length : end;
    if (!mmay_vector_push(&lexer->result->diagnostics, &diagnostic)) {
        lexer->allocation_failed = true;
        return false;
    }
    return true;
}

static void mmay_lexer_reach_limit(MarimayLexer *lexer, size_t offset) {
    if (lexer->limit_reached) {
        return;
    }
    lexer->limit_reached = true;
    (void)mmay_lexer_add_diagnostic(
        lexer,
        MMAY_DIAGNOSTIC_LEXICAL_WORK_LIMIT,
        "lexical work limit reached; remaining source was not tokenized",
        offset,
        offset
    );
}

static bool mmay_lexer_emit_token(
    MarimayLexer *lexer,
    MarimayTokenKind kind,
    size_t start,
    size_t end
) {
    MarimayToken token;
    if (kind != MMAY_TOKEN_EOF
        && lexer->result->tokens.length >= lexer->options.max_tokens - 1) {
        mmay_lexer_reach_limit(lexer, start);
        return false;
    }
    if (kind == MMAY_TOKEN_EOF
        && lexer->result->tokens.length >= lexer->options.max_tokens) {
        mmay_lexer_reach_limit(lexer, start);
        return false;
    }
    token.kind = kind;
    token.start = start;
    token.end = end;
    if (!mmay_vector_push(&lexer->result->tokens, &token)) {
        lexer->allocation_failed = true;
        return false;
    }
    return true;
}

static MarimayLexContext *mmay_lexer_context(MarimayLexer *lexer) {
    return mmay_vector_at(&lexer->contexts, lexer->contexts.length - 1);
}

static bool mmay_lexer_push_context(
    MarimayLexer *lexer,
    MarimayLexContextKind kind,
    size_t literal_start
) {
    MarimayLexContext context;
    if (lexer->contexts.length >= lexer->options.max_nesting) {
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_LEXICAL_NESTING_LIMIT,
            "interpolation nesting exceeds the configured limit",
            literal_start,
            lexer->offset
        );
        lexer->limit_reached = true;
        return false;
    }
    context.kind = kind;
    context.literal_start = literal_start;
    context.brace_depth = 0;
    if (!mmay_vector_push(&lexer->contexts, &context)) {
        lexer->allocation_failed = true;
        return false;
    }
    return true;
}

static void mmay_lexer_pop_context(MarimayLexer *lexer) {
    if (lexer->contexts.length > 1) {
        lexer->contexts.length -= 1;
    }
}

static bool mmay_ascii_whitespace(unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r'
        || byte == '\f' || byte == '\v';
}

static bool mmay_ascii_digit(unsigned char byte) {
    return byte >= '0' && byte <= '9';
}

static int mmay_digit_value(unsigned char byte) {
    if (byte >= '0' && byte <= '9') {
        return (int)(byte - '0');
    }
    if (byte >= 'a' && byte <= 'f') {
        return (int)(byte - 'a') + 10;
    }
    if (byte >= 'A' && byte <= 'F') {
        return (int)(byte - 'A') + 10;
    }
    return -1;
}

static bool mmay_ascii_alphanumeric(unsigned char byte) {
    return mmay_ascii_digit(byte)
        || (byte >= 'a' && byte <= 'z')
        || (byte >= 'A' && byte <= 'Z');
}

static bool mmay_lexer_next_scalar(
    const MarimayLexer *lexer,
    size_t offset,
    MarimayUtf8Scalar *scalar
) {
    return offset < lexer->length
        && mmay_utf8_decode(lexer->bytes + offset, lexer->length - offset, scalar);
}

static void mmay_lexer_scan_invalid_utf8(MarimayLexer *lexer) {
    size_t offset = 0;
    while (offset < lexer->length && !lexer->allocation_failed
        && !lexer->limit_reached) {
        MarimayUtf8Scalar scalar;
        if (!mmay_utf8_decode(
                lexer->bytes + offset,
                lexer->length - offset,
                &scalar
            )) {
            (void)mmay_lexer_add_diagnostic(
                lexer,
                MMAY_DIAGNOSTIC_INVALID_UTF8,
                "source contains malformed UTF-8",
                offset,
                offset + 1
            );
            offset += 1;
        } else {
            offset += scalar.width;
        }
    }
}

static void mmay_lexer_scan_whitespace(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    while (lexer->offset < lexer->length
        && mmay_ascii_whitespace(lexer->bytes[lexer->offset])) {
        lexer->offset += 1;
    }
    (void)mmay_lexer_emit_token(lexer, MMAY_TOKEN_WHITESPACE, start, lexer->offset);
}

static void mmay_lexer_scan_line_comment(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    MarimayTokenKind kind = start + 2 < lexer->length && lexer->bytes[start + 2] == '/'
        ? MMAY_TOKEN_DOC_LINE_COMMENT
        : MMAY_TOKEN_LINE_COMMENT;
    lexer->offset += 2;
    while (lexer->offset < lexer->length
        && lexer->bytes[lexer->offset] != '\n'
        && lexer->bytes[lexer->offset] != '\r') {
        lexer->offset += 1;
    }
    (void)mmay_lexer_emit_token(lexer, kind, start, lexer->offset);
}

static void mmay_lexer_scan_block_comment(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    size_t depth = 1;
    MarimayTokenKind kind = start + 2 < lexer->length && lexer->bytes[start + 2] == '*'
        ? MMAY_TOKEN_DOC_BLOCK_COMMENT
        : MMAY_TOKEN_BLOCK_COMMENT;
    lexer->offset += 2;
    while (lexer->offset < lexer->length && depth != 0
        && !lexer->allocation_failed && !lexer->limit_reached) {
        if (lexer->offset + 1 < lexer->length
            && lexer->bytes[lexer->offset] == '/'
            && lexer->bytes[lexer->offset + 1] == '*') {
            if (depth >= lexer->options.max_nesting) {
                (void)mmay_lexer_add_diagnostic(
                    lexer,
                    MMAY_DIAGNOSTIC_LEXICAL_NESTING_LIMIT,
                    "block comment nesting exceeds the configured limit",
                    lexer->offset,
                    lexer->offset + 2
                );
                lexer->limit_reached = true;
                break;
            }
            depth += 1;
            lexer->offset += 2;
        } else if (lexer->offset + 1 < lexer->length
            && lexer->bytes[lexer->offset] == '*'
            && lexer->bytes[lexer->offset + 1] == '/') {
            depth -= 1;
            lexer->offset += 2;
        } else {
            lexer->offset += 1;
        }
    }
    if (depth != 0 && !lexer->limit_reached) {
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_UNTERMINATED_BLOCK_COMMENT,
            "block comment reaches end of source before closing",
            start,
            lexer->offset
        );
    }
    (void)mmay_lexer_emit_token(lexer, kind, start, lexer->offset);
}

static void mmay_lexer_scan_identifier(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    MarimayUtf8Scalar scalar;
    if (!mmay_lexer_next_scalar(lexer, lexer->offset, &scalar)) {
        lexer->offset += 1;
        (void)mmay_lexer_emit_token(lexer, MMAY_TOKEN_INVALID, start, lexer->offset);
        return;
    }
    lexer->offset += scalar.width;
    while (mmay_lexer_next_scalar(lexer, lexer->offset, &scalar)
        && mmay_unicode_is_xid_continue(scalar.value)) {
        lexer->offset += scalar.width;
    }
    {
        MarimayTokenKind kind = mmay_token_identifier_kind(
            lexer->bytes + start,
            lexer->offset - start
        );
        (void)mmay_lexer_emit_token(lexer, kind, start, lexer->offset);
        if (kind == MMAY_TOKEN_RESERVED_WORD) {
            (void)mmay_lexer_add_diagnostic(
                lexer,
                MMAY_DIAGNOSTIC_RESERVED_WORD,
                "reserved word cannot be used as an identifier",
                start,
                lexer->offset
            );
        }
    }
}

static void mmay_lexer_scan_underscore(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    MarimayUtf8Scalar scalar;
    lexer->offset += 1;
    if (mmay_lexer_next_scalar(lexer, lexer->offset, &scalar)
        && mmay_unicode_is_xid_continue(scalar.value)) {
        do {
            lexer->offset += scalar.width;
        } while (mmay_lexer_next_scalar(lexer, lexer->offset, &scalar)
            && mmay_unicode_is_xid_continue(scalar.value));
        (void)mmay_lexer_emit_token(lexer, MMAY_TOKEN_INVALID, start, lexer->offset);
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_UNEXPECTED_CHARACTER,
            "identifier cannot start with '_'; '_' alone is a wildcard or discard",
            start,
            lexer->offset
        );
    } else {
        (void)mmay_lexer_emit_token(lexer, MMAY_TOKEN_UNDERSCORE, start, lexer->offset);
    }
}

static void mmay_lexer_scan_digit_sequence(
    MarimayLexer *lexer,
    unsigned int base,
    bool consume_letters,
    size_t *digit_count,
    bool *valid
) {
    bool previous_digit = false;
    while (lexer->offset < lexer->length) {
        unsigned char byte = lexer->bytes[lexer->offset];
        int value = mmay_digit_value(byte);
        if (value >= 0 && (unsigned int)value < base) {
            *digit_count += 1;
            previous_digit = true;
            lexer->offset += 1;
        } else if (byte == '_') {
            bool next_is_digit = false;
            if (lexer->offset + 1 < lexer->length) {
                int next = mmay_digit_value(lexer->bytes[lexer->offset + 1]);
                next_is_digit = next >= 0 && (unsigned int)next < base;
            }
            if (!previous_digit || !next_is_digit) {
                *valid = false;
            }
            previous_digit = false;
            lexer->offset += 1;
        } else if (consume_letters && mmay_ascii_alphanumeric(byte)) {
            *valid = false;
            previous_digit = false;
            lexer->offset += 1;
        } else {
            break;
        }
    }
}

static void mmay_lexer_consume_identifier_suffix(MarimayLexer *lexer, bool *valid) {
    MarimayUtf8Scalar scalar;
    if (mmay_lexer_next_scalar(lexer, lexer->offset, &scalar)
        && mmay_unicode_is_xid_continue(scalar.value)) {
        *valid = false;
        do {
            lexer->offset += scalar.width;
        } while (mmay_lexer_next_scalar(lexer, lexer->offset, &scalar)
            && mmay_unicode_is_xid_continue(scalar.value));
    }
}

static void mmay_lexer_scan_number(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    size_t digits = 0;
    bool valid = true;
    bool floating = false;
    unsigned char first = lexer->bytes[lexer->offset];
    if (first == '0' && lexer->offset + 1 < lexer->length) {
        unsigned char prefix = lexer->bytes[lexer->offset + 1];
        unsigned int base = 0;
        if (prefix == 'b' || prefix == 'B') {
            base = 2;
        } else if (prefix == 'o' || prefix == 'O') {
            base = 8;
        } else if (prefix == 'x' || prefix == 'X') {
            base = 16;
        }
        if (base != 0) {
            lexer->offset += 2;
            mmay_lexer_scan_digit_sequence(
                lexer,
                base,
                true,
                &digits,
                &valid
            );
            if (digits == 0) {
                valid = false;
            }
            mmay_lexer_consume_identifier_suffix(lexer, &valid);
            (void)mmay_lexer_emit_token(
                lexer,
                MMAY_TOKEN_INTEGER,
                start,
                lexer->offset
            );
            if (!valid) {
                (void)mmay_lexer_add_diagnostic(
                    lexer,
                    MMAY_DIAGNOSTIC_INVALID_NUMBER,
                    "numeric literal has an invalid digit or separator for its base",
                    start,
                    lexer->offset
                );
            }
            return;
        }
    }

    mmay_lexer_scan_digit_sequence(lexer, 10, false, &digits, &valid);
    if (lexer->offset + 1 < lexer->length
        && lexer->bytes[lexer->offset] == '.'
        && mmay_ascii_digit(lexer->bytes[lexer->offset + 1])) {
        size_t fraction_digits = 0;
        floating = true;
        lexer->offset += 1;
        mmay_lexer_scan_digit_sequence(
            lexer,
            10,
            false,
            &fraction_digits,
            &valid
        );
    }
    if (lexer->offset < lexer->length
        && (lexer->bytes[lexer->offset] == 'e'
            || lexer->bytes[lexer->offset] == 'E')) {
        size_t exponent_digits = 0;
        floating = true;
        lexer->offset += 1;
        if (lexer->offset < lexer->length
            && (lexer->bytes[lexer->offset] == '+'
                || lexer->bytes[lexer->offset] == '-')) {
            lexer->offset += 1;
        }
        mmay_lexer_scan_digit_sequence(
            lexer,
            10,
            false,
            &exponent_digits,
            &valid
        );
        if (exponent_digits == 0) {
            valid = false;
        }
    }
    mmay_lexer_consume_identifier_suffix(lexer, &valid);
    (void)mmay_lexer_emit_token(
        lexer,
        floating ? MMAY_TOKEN_FLOAT : MMAY_TOKEN_INTEGER,
        start,
        lexer->offset
    );
    if (!valid) {
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_INVALID_NUMBER,
            "numeric literal has an invalid separator, exponent, or suffix",
            start,
            lexer->offset
        );
    }
}

static bool mmay_unicode_scalar_value(uint32_t value) {
    return value <= 0x10FFFFU && !(value >= 0xD800U && value <= 0xDFFFU);
}

static size_t mmay_lexer_scan_escape(
    MarimayLexer *lexer,
    size_t offset,
    MarimayLiteralMode mode,
    bool *valid
) {
    size_t cursor = offset + 1;
    size_t end;
    if (cursor >= lexer->length || lexer->bytes[cursor] == '\n'
        || lexer->bytes[cursor] == '\r') {
        *valid = false;
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_INVALID_ESCAPE,
            "escape sequence is incomplete",
            offset,
            cursor
        );
        return cursor;
    }
    if (lexer->bytes[cursor] == '\\' || lexer->bytes[cursor] == '"'
        || lexer->bytes[cursor] == '\'' || lexer->bytes[cursor] == 'n'
        || lexer->bytes[cursor] == 'r' || lexer->bytes[cursor] == 't'
        || lexer->bytes[cursor] == '0') {
        return cursor + 1;
    }
    if (mode == MMAY_LITERAL_BYTE && lexer->bytes[cursor] == 'x') {
        end = cursor + 1;
        if (end + 2 <= lexer->length
            && mmay_digit_value(lexer->bytes[end]) >= 0
            && mmay_digit_value(lexer->bytes[end + 1]) >= 0) {
            return end + 2;
        }
        while (end < lexer->length && end < cursor + 3
            && lexer->bytes[end] != '"' && lexer->bytes[end] != '\n'
            && lexer->bytes[end] != '\r') {
            end += 1;
        }
        *valid = false;
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_INVALID_ESCAPE,
            "byte escape requires exactly two hexadecimal digits",
            offset,
            end
        );
        return end;
    }
    if (mode != MMAY_LITERAL_BYTE && lexer->bytes[cursor] == 'u') {
        size_t digit_count = 0;
        uint32_t value = 0;
        bool unicode_valid = true;
        end = cursor + 1;
        if (end >= lexer->length || lexer->bytes[end] != '{') {
            unicode_valid = false;
        } else {
            end += 1;
            while (end < lexer->length && lexer->bytes[end] != '}'
                && lexer->bytes[end] != '"' && lexer->bytes[end] != '\''
                && lexer->bytes[end] != '\n' && lexer->bytes[end] != '\r') {
                int digit = mmay_digit_value(lexer->bytes[end]);
                if (digit < 0 || digit_count >= 6) {
                    unicode_valid = false;
                } else {
                    value = value * 16U + (uint32_t)digit;
                }
                digit_count += 1;
                end += 1;
            }
            if (end >= lexer->length || lexer->bytes[end] != '}') {
                unicode_valid = false;
            } else {
                end += 1;
            }
            if (digit_count == 0 || digit_count > 6
                || !mmay_unicode_scalar_value(value)) {
                unicode_valid = false;
            }
        }
        if (unicode_valid) {
            return end;
        }
        *valid = false;
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_INVALID_ESCAPE,
            "Unicode escape must be \\u{H...} naming one Unicode scalar",
            offset,
            end
        );
        return end;
    }
    *valid = false;
    end = cursor + 1;
    (void)mmay_lexer_add_diagnostic(
        lexer,
        mode == MMAY_LITERAL_BYTE
            ? MMAY_DIAGNOSTIC_INVALID_BYTE_STRING
            : MMAY_DIAGNOSTIC_INVALID_ESCAPE,
        mode == MMAY_LITERAL_BYTE
            ? "byte string escape is outside the closed byte escape set"
            : "escape is outside the closed string/rune escape set",
        offset,
        end
    );
    return end;
}

static void mmay_lexer_scan_quoted(
    MarimayLexer *lexer,
    MarimayLiteralMode mode,
    MarimayTokenKind kind,
    size_t prefix_length
) {
    size_t start = lexer->offset;
    size_t quote = start + prefix_length;
    size_t scalar_count = 0;
    bool valid = true;
    bool terminated = false;
    unsigned char delimiter = mode == MMAY_LITERAL_RUNE ? '\'' : '"';
    lexer->offset = quote + 1;
    while (lexer->offset < lexer->length && !lexer->allocation_failed
        && !lexer->limit_reached) {
        unsigned char byte = lexer->bytes[lexer->offset];
        if (byte == delimiter) {
            lexer->offset += 1;
            terminated = true;
            break;
        }
        if (byte == '\n' || byte == '\r') {
            break;
        }
        if (byte == '\\' && mode != MMAY_LITERAL_RAW) {
            lexer->offset = mmay_lexer_scan_escape(
                lexer,
                lexer->offset,
                mode,
                &valid
            );
            scalar_count += 1;
            continue;
        }
        if (byte >= 0x80U) {
            MarimayUtf8Scalar scalar;
            if (mmay_lexer_next_scalar(lexer, lexer->offset, &scalar)) {
                if (mode == MMAY_LITERAL_BYTE) {
                    valid = false;
                    (void)mmay_lexer_add_diagnostic(
                        lexer,
                        MMAY_DIAGNOSTIC_INVALID_BYTE_STRING,
                        "unescaped byte string content must be ASCII",
                        lexer->offset,
                        lexer->offset + scalar.width
                    );
                }
                lexer->offset += scalar.width;
            } else {
                lexer->offset += 1;
            }
        } else {
            lexer->offset += 1;
        }
        scalar_count += 1;
    }
    if (!terminated && !lexer->limit_reached) {
        valid = false;
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_UNTERMINATED_LITERAL,
            "quoted literal reaches a physical newline or end of source",
            start,
            lexer->offset
        );
    }
    if (mode == MMAY_LITERAL_RUNE && scalar_count != 1) {
        valid = false;
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_INVALID_RUNE,
            "rune literal must contain exactly one Unicode scalar or escape",
            start,
            lexer->offset
        );
    }
    (void)valid;
    (void)mmay_lexer_emit_token(lexer, kind, start, lexer->offset);
}

static void mmay_lexer_start_interpolated(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    lexer->offset += 2;
    if (mmay_lexer_emit_token(
            lexer,
            MMAY_TOKEN_INTERPOLATED_START,
            start,
            lexer->offset
        )) {
        (void)mmay_lexer_push_context(
            lexer,
            MMAY_LEX_CONTEXT_INTERPOLATED_TEXT,
            start
        );
    }
}

static void mmay_lexer_scan_interpolated_text(MarimayLexer *lexer) {
    MarimayLexContext *context = mmay_lexer_context(lexer);
    size_t literal_start = context->literal_start;
    size_t start = lexer->offset;
    bool valid = true;
    while (lexer->offset < lexer->length && !lexer->allocation_failed
        && !lexer->limit_reached) {
        unsigned char byte = lexer->bytes[lexer->offset];
        if (byte == '"') {
            if (lexer->offset != start) {
                (void)mmay_lexer_emit_token(
                    lexer,
                    MMAY_TOKEN_INTERPOLATED_TEXT,
                    start,
                    lexer->offset
                );
                return;
            }
            lexer->offset += 1;
            (void)mmay_lexer_emit_token(
                lexer,
                MMAY_TOKEN_INTERPOLATED_END,
                start,
                lexer->offset
            );
            mmay_lexer_pop_context(lexer);
            return;
        }
        if (byte == '{') {
            if (lexer->offset + 1 < lexer->length
                && lexer->bytes[lexer->offset + 1] == '{') {
                lexer->offset += 2;
                continue;
            }
            if (lexer->offset != start) {
                (void)mmay_lexer_emit_token(
                    lexer,
                    MMAY_TOKEN_INTERPOLATED_TEXT,
                    start,
                    lexer->offset
                );
                return;
            }
            lexer->offset += 1;
            if (mmay_lexer_emit_token(
                    lexer,
                    MMAY_TOKEN_INTERPOLATION_START,
                    start,
                    lexer->offset
                )) {
                (void)mmay_lexer_push_context(
                    lexer,
                    MMAY_LEX_CONTEXT_INTERPOLATION_EXPRESSION,
                    literal_start
                );
            }
            return;
        }
        if (byte == '}') {
            if (lexer->offset + 1 < lexer->length
                && lexer->bytes[lexer->offset + 1] == '}') {
                lexer->offset += 2;
                continue;
            }
            if (lexer->offset != start) {
                (void)mmay_lexer_emit_token(
                    lexer,
                    MMAY_TOKEN_INTERPOLATED_TEXT,
                    start,
                    lexer->offset
                );
                return;
            }
            lexer->offset += 1;
            (void)mmay_lexer_emit_token(
                lexer,
                MMAY_TOKEN_INVALID,
                start,
                lexer->offset
            );
            (void)mmay_lexer_add_diagnostic(
                lexer,
                MMAY_DIAGNOSTIC_UNMATCHED_INTERPOLATION_BRACE,
                "single '}' in interpolated text must be escaped as '}}'",
                start,
                lexer->offset
            );
            return;
        }
        if (byte == '\n' || byte == '\r') {
            if (lexer->offset != start) {
                (void)mmay_lexer_emit_token(
                    lexer,
                    MMAY_TOKEN_INTERPOLATED_TEXT,
                    start,
                    lexer->offset
                );
                return;
            }
            (void)mmay_lexer_add_diagnostic(
                lexer,
                MMAY_DIAGNOSTIC_UNTERMINATED_LITERAL,
                "interpolated string reaches a physical newline before closing",
                literal_start,
                lexer->offset
            );
            lexer->contexts.length = 1;
            return;
        }
        if (byte == '\\') {
            lexer->offset = mmay_lexer_scan_escape(
                lexer,
                lexer->offset,
                MMAY_LITERAL_NORMAL,
                &valid
            );
        } else if (byte >= 0x80U) {
            MarimayUtf8Scalar scalar;
            if (mmay_lexer_next_scalar(lexer, lexer->offset, &scalar)) {
                lexer->offset += scalar.width;
            } else {
                lexer->offset += 1;
            }
        } else {
            lexer->offset += 1;
        }
    }
    if (lexer->offset != start) {
        (void)mmay_lexer_emit_token(
            lexer,
            MMAY_TOKEN_INTERPOLATED_TEXT,
            start,
            lexer->offset
        );
    } else if (!lexer->limit_reached) {
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_UNTERMINATED_LITERAL,
            "interpolated string reaches end of source before closing",
            literal_start,
            lexer->offset
        );
        lexer->contexts.length = 1;
    }
}

static bool mmay_lexer_emit_operator(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    unsigned char byte = lexer->bytes[start];
    unsigned char next = start + 1 < lexer->length ? lexer->bytes[start + 1] : 0;
    unsigned char third = start + 2 < lexer->length ? lexer->bytes[start + 2] : 0;
    MarimayTokenKind kind = MMAY_TOKEN_INVALID;
    size_t width = 1;

    if (byte == '.' && next == '.' && third == '=') {
        kind = MMAY_TOKEN_DOT_DOT_EQUAL;
        width = 3;
    } else if (byte == '<' && next == '<' && third == '=') {
        kind = MMAY_TOKEN_SHIFT_LEFT_EQUAL;
        width = 3;
    } else if (byte == '>' && next == '>' && third == '=') {
        kind = MMAY_TOKEN_SHIFT_RIGHT_EQUAL;
        width = 3;
    } else if (byte == '.' && next == '.') {
        kind = MMAY_TOKEN_DOT_DOT;
        width = 2;
    } else if (byte == '=' && next == '>') {
        kind = MMAY_TOKEN_FAT_ARROW;
        width = 2;
    } else if (byte == '-' && next == '>') {
        kind = MMAY_TOKEN_THIN_ARROW;
        width = 2;
    } else if (byte == '=' && next == '=') {
        kind = MMAY_TOKEN_EQUAL_EQUAL;
        width = 2;
    } else if (byte == '!' && next == '=') {
        kind = MMAY_TOKEN_BANG_EQUAL;
        width = 2;
    } else if (byte == '<' && next == '=') {
        kind = MMAY_TOKEN_LESS_EQUAL;
        width = 2;
    } else if (byte == '>' && next == '=') {
        kind = MMAY_TOKEN_GREATER_EQUAL;
        width = 2;
    } else if (byte == '&' && next == '&') {
        kind = MMAY_TOKEN_AMPERSAND_AMPERSAND;
        width = 2;
    } else if (byte == '|' && next == '|') {
        kind = MMAY_TOKEN_PIPE_PIPE;
        width = 2;
    } else if (byte == '<' && next == '<') {
        kind = MMAY_TOKEN_SHIFT_LEFT;
        width = 2;
    } else if (byte == '>' && next == '>') {
        kind = MMAY_TOKEN_SHIFT_RIGHT;
        width = 2;
    } else if (byte == '+' && next == '=') {
        kind = MMAY_TOKEN_PLUS_EQUAL;
        width = 2;
    } else if (byte == '-' && next == '=') {
        kind = MMAY_TOKEN_MINUS_EQUAL;
        width = 2;
    } else if (byte == '*' && next == '=') {
        kind = MMAY_TOKEN_STAR_EQUAL;
        width = 2;
    } else if (byte == '/' && next == '=') {
        kind = MMAY_TOKEN_SLASH_EQUAL;
        width = 2;
    } else if (byte == '%' && next == '=') {
        kind = MMAY_TOKEN_PERCENT_EQUAL;
        width = 2;
    } else if (byte == '&' && next == '=') {
        kind = MMAY_TOKEN_AMPERSAND_EQUAL;
        width = 2;
    } else if (byte == '|' && next == '=') {
        kind = MMAY_TOKEN_PIPE_EQUAL;
        width = 2;
    } else if (byte == '^' && next == '=') {
        kind = MMAY_TOKEN_CARET_EQUAL;
        width = 2;
    } else {
        switch (byte) {
            case '@': kind = MMAY_TOKEN_AT; break;
            case ',': kind = MMAY_TOKEN_COMMA; break;
            case ':': kind = MMAY_TOKEN_COLON; break;
            case ';': kind = MMAY_TOKEN_SEMICOLON; break;
            case '.': kind = MMAY_TOKEN_DOT; break;
            case '(': kind = MMAY_TOKEN_LEFT_PAREN; break;
            case ')': kind = MMAY_TOKEN_RIGHT_PAREN; break;
            case '[': kind = MMAY_TOKEN_LEFT_BRACKET; break;
            case ']': kind = MMAY_TOKEN_RIGHT_BRACKET; break;
            case '+': kind = MMAY_TOKEN_PLUS; break;
            case '-': kind = MMAY_TOKEN_MINUS; break;
            case '*': kind = MMAY_TOKEN_STAR; break;
            case '/': kind = MMAY_TOKEN_SLASH; break;
            case '%': kind = MMAY_TOKEN_PERCENT; break;
            case '!': kind = MMAY_TOKEN_BANG; break;
            case '~': kind = MMAY_TOKEN_TILDE; break;
            case '&': kind = MMAY_TOKEN_AMPERSAND; break;
            case '|': kind = MMAY_TOKEN_PIPE; break;
            case '^': kind = MMAY_TOKEN_CARET; break;
            case '?': kind = MMAY_TOKEN_QUESTION; break;
            case '=': kind = MMAY_TOKEN_EQUAL; break;
            case '<': kind = MMAY_TOKEN_LESS; break;
            case '>': kind = MMAY_TOKEN_GREATER; break;
            default: return false;
        }
    }
    lexer->offset += width;
    (void)mmay_lexer_emit_token(lexer, kind, start, lexer->offset);
    return true;
}

static void mmay_lexer_scan_code_token(MarimayLexer *lexer) {
    size_t start = lexer->offset;
    unsigned char byte = lexer->bytes[start];
    MarimayLexContext *context = mmay_lexer_context(lexer);

    if (byte == '\n' || byte == '\r') {
        if (context->kind != MMAY_LEX_CONTEXT_ROOT) {
            (void)mmay_lexer_add_diagnostic(
                lexer,
                MMAY_DIAGNOSTIC_UNTERMINATED_LITERAL,
                "interpolated string reaches a physical newline before closing",
                context->literal_start,
                start
            );
            lexer->contexts.length = 1;
        }
        mmay_lexer_scan_whitespace(lexer);
        return;
    }
    if (mmay_ascii_whitespace(byte)) {
        mmay_lexer_scan_whitespace(lexer);
        return;
    }
    if (byte == '/' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '/') {
        mmay_lexer_scan_line_comment(lexer);
        return;
    }
    if (byte == '/' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '*') {
        mmay_lexer_scan_block_comment(lexer);
        return;
    }
    if (byte == '$' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '"') {
        mmay_lexer_start_interpolated(lexer);
        return;
    }
    if (byte == 'r' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '"') {
        mmay_lexer_scan_quoted(lexer, MMAY_LITERAL_RAW, MMAY_TOKEN_RAW_STRING, 1);
        return;
    }
    if (byte == 'b' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '"') {
        mmay_lexer_scan_quoted(lexer, MMAY_LITERAL_BYTE, MMAY_TOKEN_BYTE_STRING, 1);
        return;
    }
    if (byte == '"') {
        mmay_lexer_scan_quoted(lexer, MMAY_LITERAL_NORMAL, MMAY_TOKEN_STRING, 0);
        return;
    }
    if (byte == '\'') {
        mmay_lexer_scan_quoted(lexer, MMAY_LITERAL_RUNE, MMAY_TOKEN_RUNE, 0);
        return;
    }
    if (mmay_ascii_digit(byte)) {
        mmay_lexer_scan_number(lexer);
        return;
    }
    if (byte == '_') {
        mmay_lexer_scan_underscore(lexer);
        return;
    }
    {
        MarimayUtf8Scalar scalar;
        if (mmay_lexer_next_scalar(lexer, start, &scalar)
            && mmay_unicode_is_xid_start(scalar.value)) {
            mmay_lexer_scan_identifier(lexer);
            return;
        }
        if (!mmay_lexer_next_scalar(lexer, start, &scalar)) {
            lexer->offset += 1;
            (void)mmay_lexer_emit_token(
                lexer,
                MMAY_TOKEN_INVALID,
                start,
                lexer->offset
            );
            return;
        }
    }
    if (byte == '{') {
        lexer->offset += 1;
        if (context->kind == MMAY_LEX_CONTEXT_INTERPOLATION_EXPRESSION) {
            if (context->brace_depth >= lexer->options.max_nesting) {
                (void)mmay_lexer_add_diagnostic(
                    lexer,
                    MMAY_DIAGNOSTIC_LEXICAL_NESTING_LIMIT,
                    "interpolation brace nesting exceeds the configured limit",
                    start,
                    lexer->offset
                );
                lexer->limit_reached = true;
                return;
            }
            context->brace_depth += 1;
        }
        (void)mmay_lexer_emit_token(
            lexer,
            MMAY_TOKEN_LEFT_BRACE,
            start,
            lexer->offset
        );
        return;
    }
    if (byte == '}') {
        lexer->offset += 1;
        if (context->kind == MMAY_LEX_CONTEXT_INTERPOLATION_EXPRESSION) {
            if (context->brace_depth == 0) {
                (void)mmay_lexer_emit_token(
                    lexer,
                    MMAY_TOKEN_INTERPOLATION_END,
                    start,
                    lexer->offset
                );
                mmay_lexer_pop_context(lexer);
            } else {
                context->brace_depth -= 1;
                (void)mmay_lexer_emit_token(
                    lexer,
                    MMAY_TOKEN_RIGHT_BRACE,
                    start,
                    lexer->offset
                );
            }
        } else {
            (void)mmay_lexer_emit_token(
                lexer,
                MMAY_TOKEN_RIGHT_BRACE,
                start,
                lexer->offset
            );
        }
        return;
    }
    if (mmay_lexer_emit_operator(lexer)) {
        return;
    }
    {
        MarimayUtf8Scalar scalar;
        size_t width = mmay_lexer_next_scalar(lexer, start, &scalar) ? scalar.width : 1;
        lexer->offset += width;
        (void)mmay_lexer_emit_token(
            lexer,
            MMAY_TOKEN_INVALID,
            start,
            lexer->offset
        );
        (void)mmay_lexer_add_diagnostic(
            lexer,
            MMAY_DIAGNOSTIC_UNEXPECTED_CHARACTER,
            "character cannot begin a Marimay token",
            start,
            lexer->offset
        );
    }
}

static int mmay_lex_diagnostic_compare(const void *left, const void *right) {
    const MarimayLexDiagnostic *left_diagnostic = left;
    const MarimayLexDiagnostic *right_diagnostic = right;
    if (left_diagnostic->start < right_diagnostic->start) {
        return -1;
    }
    if (left_diagnostic->start > right_diagnostic->start) {
        return 1;
    }
    if (left_diagnostic->end < right_diagnostic->end) {
        return -1;
    }
    if (left_diagnostic->end > right_diagnostic->end) {
        return 1;
    }
    return strcmp(left_diagnostic->code, right_diagnostic->code);
}

MarimayLexStatus mmay_lex_snapshot(
    const MarimaySourceSnapshot *snapshot,
    MarimayAllocator allocator,
    MarimayLexOptions options,
    MarimayLexResult *result
) {
    MarimayLexer lexer;
    MarimayLexContext root;
    if (result == NULL) {
        return MMAY_LEX_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (snapshot == NULL || !mmay_lex_options_valid(options)
        || !mmay_vector_init(&result->tokens, allocator, sizeof(MarimayToken))
        || !mmay_vector_init(
            &result->diagnostics,
            allocator,
            sizeof(MarimayLexDiagnostic)
        )) {
        return MMAY_LEX_INVALID_ARGUMENT;
    }
    result->allocator = allocator;
    result->snapshot = snapshot;
    result->initialized = true;
    memset(&lexer, 0, sizeof(lexer));
    lexer.bytes = mmay_source_bytes(snapshot);
    lexer.length = mmay_source_length(snapshot);
    lexer.options = options;
    lexer.result = result;
    if (!mmay_vector_init(&lexer.contexts, allocator, sizeof(MarimayLexContext))) {
        mmay_lex_result_dispose(result);
        return MMAY_LEX_INVALID_ARGUMENT;
    }
    root.kind = MMAY_LEX_CONTEXT_ROOT;
    root.literal_start = 0;
    root.brace_depth = 0;
    if (!mmay_vector_push(&lexer.contexts, &root)) {
        mmay_vector_dispose(&lexer.contexts);
        mmay_lex_result_dispose(result);
        return MMAY_LEX_ALLOCATION_FAILED;
    }

    mmay_lexer_scan_invalid_utf8(&lexer);
    if (!lexer.limit_reached && lexer.length >= 3
        && lexer.bytes[0] == 0xEFU && lexer.bytes[1] == 0xBBU
        && lexer.bytes[2] == 0xBFU) {
        lexer.offset = 3;
        (void)mmay_lexer_emit_token(&lexer, MMAY_TOKEN_BOM, 0, 3);
    }
    while (lexer.offset < lexer.length && !lexer.allocation_failed
        && !lexer.limit_reached) {
        MarimayLexContext *context = mmay_lexer_context(&lexer);
        if (context->kind == MMAY_LEX_CONTEXT_INTERPOLATED_TEXT) {
            mmay_lexer_scan_interpolated_text(&lexer);
        } else {
            mmay_lexer_scan_code_token(&lexer);
        }
    }
    if (!lexer.allocation_failed) {
        (void)mmay_lexer_emit_token(
            &lexer,
            MMAY_TOKEN_EOF,
            lexer.offset,
            lexer.offset
        );
    }
    mmay_vector_dispose(&lexer.contexts);
    if (lexer.allocation_failed) {
        mmay_lex_result_dispose(result);
        return MMAY_LEX_ALLOCATION_FAILED;
    }
    if (result->diagnostics.length > 1) {
        qsort(
            result->diagnostics.data,
            result->diagnostics.length,
            sizeof(MarimayLexDiagnostic),
            mmay_lex_diagnostic_compare
        );
    }
    return lexer.limit_reached ? MMAY_LEX_LIMIT_REACHED : MMAY_LEX_OK;
}
