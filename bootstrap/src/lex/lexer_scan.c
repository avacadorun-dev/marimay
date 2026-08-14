#include "lexer_internal.h"

#include <stdlib.h>
#include <string.h>

#include "ruda/base/unicode.h"
#include "ruda/base/utf8.h"
#include "ruda/generated/diagnostic_codes.h"


static bool ruda_lexer_add_diagnostic(
    RudaLexer *lexer,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    RudaLexDiagnostic diagnostic;
    if (lexer->result->diagnostics.length >= lexer->options.max_diagnostics) {
        lexer->result->suppressed_diagnostics += 1;
        lexer->limit_reached = true;
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.start = start;
    diagnostic.end = end > lexer->length ? lexer->length : end;
    if (!ruda_vector_push(&lexer->result->diagnostics, &diagnostic)) {
        lexer->allocation_failed = true;
        return false;
    }
    return true;
}

static void ruda_lexer_reach_limit(RudaLexer *lexer, size_t offset) {
    if (lexer->limit_reached) {
        return;
    }
    lexer->limit_reached = true;
    (void)ruda_lexer_add_diagnostic(
        lexer,
        RUDA_DIAGNOSTIC_LEXICAL_WORK_LIMIT,
        "lexical work limit reached; remaining source was not tokenized",
        offset,
        offset
    );
}

static bool ruda_lexer_emit_token(
    RudaLexer *lexer,
    RudaTokenKind kind,
    size_t start,
    size_t end
) {
    RudaToken token;
    if (kind != RUDA_TOKEN_EOF
        && lexer->result->tokens.length >= lexer->options.max_tokens - 1) {
        ruda_lexer_reach_limit(lexer, start);
        return false;
    }
    if (kind == RUDA_TOKEN_EOF
        && lexer->result->tokens.length >= lexer->options.max_tokens) {
        ruda_lexer_reach_limit(lexer, start);
        return false;
    }
    token.kind = kind;
    token.start = start;
    token.end = end;
    if (!ruda_vector_push(&lexer->result->tokens, &token)) {
        lexer->allocation_failed = true;
        return false;
    }
    return true;
}

static RudaLexContext *ruda_lexer_context(RudaLexer *lexer) {
    return ruda_vector_at(&lexer->contexts, lexer->contexts.length - 1);
}

static bool ruda_lexer_push_context(
    RudaLexer *lexer,
    RudaLexContextKind kind,
    size_t literal_start
) {
    RudaLexContext context;
    if (lexer->contexts.length >= lexer->options.max_nesting) {
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_LEXICAL_NESTING_LIMIT,
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
    if (!ruda_vector_push(&lexer->contexts, &context)) {
        lexer->allocation_failed = true;
        return false;
    }
    return true;
}

static void ruda_lexer_pop_context(RudaLexer *lexer) {
    if (lexer->contexts.length > 1) {
        lexer->contexts.length -= 1;
    }
}

static bool ruda_ascii_whitespace(unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r'
        || byte == '\f' || byte == '\v';
}

static bool ruda_ascii_digit(unsigned char byte) {
    return byte >= '0' && byte <= '9';
}

static int ruda_digit_value(unsigned char byte) {
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

static bool ruda_ascii_alphanumeric(unsigned char byte) {
    return ruda_ascii_digit(byte)
        || (byte >= 'a' && byte <= 'z')
        || (byte >= 'A' && byte <= 'Z');
}

static bool ruda_lexer_next_scalar(
    const RudaLexer *lexer,
    size_t offset,
    RudaUtf8Scalar *scalar
) {
    return offset < lexer->length
        && ruda_utf8_decode(lexer->bytes + offset, lexer->length - offset, scalar);
}

static void ruda_lexer_scan_invalid_utf8(RudaLexer *lexer) {
    size_t offset = 0;
    while (offset < lexer->length && !lexer->allocation_failed
        && !lexer->limit_reached) {
        RudaUtf8Scalar scalar;
        if (!ruda_utf8_decode(
                lexer->bytes + offset,
                lexer->length - offset,
                &scalar
            )) {
            (void)ruda_lexer_add_diagnostic(
                lexer,
                RUDA_DIAGNOSTIC_INVALID_UTF8,
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

static void ruda_lexer_scan_whitespace(RudaLexer *lexer) {
    size_t start = lexer->offset;
    while (lexer->offset < lexer->length
        && ruda_ascii_whitespace(lexer->bytes[lexer->offset])) {
        lexer->offset += 1;
    }
    (void)ruda_lexer_emit_token(lexer, RUDA_TOKEN_WHITESPACE, start, lexer->offset);
}

static void ruda_lexer_scan_line_comment(RudaLexer *lexer) {
    size_t start = lexer->offset;
    RudaTokenKind kind = start + 2 < lexer->length && lexer->bytes[start + 2] == '/'
        ? RUDA_TOKEN_DOC_LINE_COMMENT
        : RUDA_TOKEN_LINE_COMMENT;
    lexer->offset += 2;
    while (lexer->offset < lexer->length
        && lexer->bytes[lexer->offset] != '\n'
        && lexer->bytes[lexer->offset] != '\r') {
        lexer->offset += 1;
    }
    (void)ruda_lexer_emit_token(lexer, kind, start, lexer->offset);
}

static void ruda_lexer_scan_block_comment(RudaLexer *lexer) {
    size_t start = lexer->offset;
    size_t depth = 1;
    RudaTokenKind kind = start + 2 < lexer->length && lexer->bytes[start + 2] == '*'
        ? RUDA_TOKEN_DOC_BLOCK_COMMENT
        : RUDA_TOKEN_BLOCK_COMMENT;
    lexer->offset += 2;
    while (lexer->offset < lexer->length && depth != 0
        && !lexer->allocation_failed && !lexer->limit_reached) {
        if (lexer->offset + 1 < lexer->length
            && lexer->bytes[lexer->offset] == '/'
            && lexer->bytes[lexer->offset + 1] == '*') {
            if (depth >= lexer->options.max_nesting) {
                (void)ruda_lexer_add_diagnostic(
                    lexer,
                    RUDA_DIAGNOSTIC_LEXICAL_NESTING_LIMIT,
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
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_UNTERMINATED_BLOCK_COMMENT,
            "block comment reaches end of source before closing",
            start,
            lexer->offset
        );
    }
    (void)ruda_lexer_emit_token(lexer, kind, start, lexer->offset);
}

static void ruda_lexer_scan_identifier(RudaLexer *lexer) {
    size_t start = lexer->offset;
    RudaUtf8Scalar scalar;
    if (!ruda_lexer_next_scalar(lexer, lexer->offset, &scalar)) {
        lexer->offset += 1;
        (void)ruda_lexer_emit_token(lexer, RUDA_TOKEN_INVALID, start, lexer->offset);
        return;
    }
    lexer->offset += scalar.width;
    while (ruda_lexer_next_scalar(lexer, lexer->offset, &scalar)
        && ruda_unicode_is_xid_continue(scalar.value)) {
        lexer->offset += scalar.width;
    }
    {
        RudaTokenKind kind = ruda_token_identifier_kind(
            lexer->bytes + start,
            lexer->offset - start
        );
        (void)ruda_lexer_emit_token(lexer, kind, start, lexer->offset);
        if (kind == RUDA_TOKEN_RESERVED_WORD) {
            (void)ruda_lexer_add_diagnostic(
                lexer,
                RUDA_DIAGNOSTIC_RESERVED_WORD,
                "reserved word cannot be used as an identifier",
                start,
                lexer->offset
            );
        }
    }
}

static void ruda_lexer_scan_underscore(RudaLexer *lexer) {
    size_t start = lexer->offset;
    RudaUtf8Scalar scalar;
    lexer->offset += 1;
    if (ruda_lexer_next_scalar(lexer, lexer->offset, &scalar)
        && ruda_unicode_is_xid_continue(scalar.value)) {
        do {
            lexer->offset += scalar.width;
        } while (ruda_lexer_next_scalar(lexer, lexer->offset, &scalar)
            && ruda_unicode_is_xid_continue(scalar.value));
        (void)ruda_lexer_emit_token(lexer, RUDA_TOKEN_INVALID, start, lexer->offset);
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_UNEXPECTED_CHARACTER,
            "identifier cannot start with '_'; '_' alone is a wildcard or discard",
            start,
            lexer->offset
        );
    } else {
        (void)ruda_lexer_emit_token(lexer, RUDA_TOKEN_UNDERSCORE, start, lexer->offset);
    }
}

static void ruda_lexer_scan_digit_sequence(
    RudaLexer *lexer,
    unsigned int base,
    bool consume_letters,
    size_t *digit_count,
    bool *valid
) {
    bool previous_digit = false;
    while (lexer->offset < lexer->length) {
        unsigned char byte = lexer->bytes[lexer->offset];
        int value = ruda_digit_value(byte);
        if (value >= 0 && (unsigned int)value < base) {
            *digit_count += 1;
            previous_digit = true;
            lexer->offset += 1;
        } else if (byte == '_') {
            bool next_is_digit = false;
            if (lexer->offset + 1 < lexer->length) {
                int next = ruda_digit_value(lexer->bytes[lexer->offset + 1]);
                next_is_digit = next >= 0 && (unsigned int)next < base;
            }
            if (!previous_digit || !next_is_digit) {
                *valid = false;
            }
            previous_digit = false;
            lexer->offset += 1;
        } else if (consume_letters && ruda_ascii_alphanumeric(byte)) {
            *valid = false;
            previous_digit = false;
            lexer->offset += 1;
        } else {
            break;
        }
    }
}

static void ruda_lexer_consume_identifier_suffix(RudaLexer *lexer, bool *valid) {
    RudaUtf8Scalar scalar;
    if (ruda_lexer_next_scalar(lexer, lexer->offset, &scalar)
        && ruda_unicode_is_xid_continue(scalar.value)) {
        *valid = false;
        do {
            lexer->offset += scalar.width;
        } while (ruda_lexer_next_scalar(lexer, lexer->offset, &scalar)
            && ruda_unicode_is_xid_continue(scalar.value));
    }
}

static void ruda_lexer_scan_number(RudaLexer *lexer) {
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
            ruda_lexer_scan_digit_sequence(
                lexer,
                base,
                true,
                &digits,
                &valid
            );
            if (digits == 0) {
                valid = false;
            }
            ruda_lexer_consume_identifier_suffix(lexer, &valid);
            (void)ruda_lexer_emit_token(
                lexer,
                RUDA_TOKEN_INTEGER,
                start,
                lexer->offset
            );
            if (!valid) {
                (void)ruda_lexer_add_diagnostic(
                    lexer,
                    RUDA_DIAGNOSTIC_INVALID_NUMBER,
                    "numeric literal has an invalid digit or separator for its base",
                    start,
                    lexer->offset
                );
            }
            return;
        }
    }

    ruda_lexer_scan_digit_sequence(lexer, 10, false, &digits, &valid);
    if (lexer->offset + 1 < lexer->length
        && lexer->bytes[lexer->offset] == '.'
        && ruda_ascii_digit(lexer->bytes[lexer->offset + 1])) {
        size_t fraction_digits = 0;
        floating = true;
        lexer->offset += 1;
        ruda_lexer_scan_digit_sequence(
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
        ruda_lexer_scan_digit_sequence(
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
    ruda_lexer_consume_identifier_suffix(lexer, &valid);
    (void)ruda_lexer_emit_token(
        lexer,
        floating ? RUDA_TOKEN_FLOAT : RUDA_TOKEN_INTEGER,
        start,
        lexer->offset
    );
    if (!valid) {
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_INVALID_NUMBER,
            "numeric literal has an invalid separator, exponent, or suffix",
            start,
            lexer->offset
        );
    }
}

static bool ruda_unicode_scalar_value(uint32_t value) {
    return value <= 0x10FFFFU && !(value >= 0xD800U && value <= 0xDFFFU);
}

static size_t ruda_lexer_scan_escape(
    RudaLexer *lexer,
    size_t offset,
    RudaLiteralMode mode,
    bool *valid
) {
    size_t cursor = offset + 1;
    size_t end;
    if (cursor >= lexer->length || lexer->bytes[cursor] == '\n'
        || lexer->bytes[cursor] == '\r') {
        *valid = false;
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_INVALID_ESCAPE,
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
    if (mode == RUDA_LITERAL_BYTE && lexer->bytes[cursor] == 'x') {
        end = cursor + 1;
        if (end + 2 <= lexer->length
            && ruda_digit_value(lexer->bytes[end]) >= 0
            && ruda_digit_value(lexer->bytes[end + 1]) >= 0) {
            return end + 2;
        }
        while (end < lexer->length && end < cursor + 3
            && lexer->bytes[end] != '"' && lexer->bytes[end] != '\n'
            && lexer->bytes[end] != '\r') {
            end += 1;
        }
        *valid = false;
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_INVALID_ESCAPE,
            "byte escape requires exactly two hexadecimal digits",
            offset,
            end
        );
        return end;
    }
    if (mode != RUDA_LITERAL_BYTE && lexer->bytes[cursor] == 'u') {
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
                int digit = ruda_digit_value(lexer->bytes[end]);
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
                || !ruda_unicode_scalar_value(value)) {
                unicode_valid = false;
            }
        }
        if (unicode_valid) {
            return end;
        }
        *valid = false;
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_INVALID_ESCAPE,
            "Unicode escape must be \\u{H...} naming one Unicode scalar",
            offset,
            end
        );
        return end;
    }
    *valid = false;
    end = cursor + 1;
    (void)ruda_lexer_add_diagnostic(
        lexer,
        mode == RUDA_LITERAL_BYTE
            ? RUDA_DIAGNOSTIC_INVALID_BYTE_STRING
            : RUDA_DIAGNOSTIC_INVALID_ESCAPE,
        mode == RUDA_LITERAL_BYTE
            ? "byte string escape is outside the closed byte escape set"
            : "escape is outside the closed string/rune escape set",
        offset,
        end
    );
    return end;
}

static void ruda_lexer_scan_quoted(
    RudaLexer *lexer,
    RudaLiteralMode mode,
    RudaTokenKind kind,
    size_t prefix_length
) {
    size_t start = lexer->offset;
    size_t quote = start + prefix_length;
    size_t scalar_count = 0;
    bool valid = true;
    bool terminated = false;
    unsigned char delimiter = mode == RUDA_LITERAL_RUNE ? '\'' : '"';
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
        if (byte == '\\' && mode != RUDA_LITERAL_RAW) {
            lexer->offset = ruda_lexer_scan_escape(
                lexer,
                lexer->offset,
                mode,
                &valid
            );
            scalar_count += 1;
            continue;
        }
        if (byte >= 0x80U) {
            RudaUtf8Scalar scalar;
            if (ruda_lexer_next_scalar(lexer, lexer->offset, &scalar)) {
                if (mode == RUDA_LITERAL_BYTE) {
                    valid = false;
                    (void)ruda_lexer_add_diagnostic(
                        lexer,
                        RUDA_DIAGNOSTIC_INVALID_BYTE_STRING,
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
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_UNTERMINATED_LITERAL,
            "quoted literal reaches a physical newline or end of source",
            start,
            lexer->offset
        );
    }
    if (mode == RUDA_LITERAL_RUNE && scalar_count != 1) {
        valid = false;
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_INVALID_RUNE,
            "rune literal must contain exactly one Unicode scalar or escape",
            start,
            lexer->offset
        );
    }
    (void)valid;
    (void)ruda_lexer_emit_token(lexer, kind, start, lexer->offset);
}

static void ruda_lexer_start_interpolated(RudaLexer *lexer) {
    size_t start = lexer->offset;
    lexer->offset += 2;
    if (ruda_lexer_emit_token(
            lexer,
            RUDA_TOKEN_INTERPOLATED_START,
            start,
            lexer->offset
        )) {
        (void)ruda_lexer_push_context(
            lexer,
            RUDA_LEX_CONTEXT_INTERPOLATED_TEXT,
            start
        );
    }
}

static void ruda_lexer_scan_interpolated_text(RudaLexer *lexer) {
    RudaLexContext *context = ruda_lexer_context(lexer);
    size_t literal_start = context->literal_start;
    size_t start = lexer->offset;
    bool valid = true;
    while (lexer->offset < lexer->length && !lexer->allocation_failed
        && !lexer->limit_reached) {
        unsigned char byte = lexer->bytes[lexer->offset];
        if (byte == '"') {
            if (lexer->offset != start) {
                (void)ruda_lexer_emit_token(
                    lexer,
                    RUDA_TOKEN_INTERPOLATED_TEXT,
                    start,
                    lexer->offset
                );
                return;
            }
            lexer->offset += 1;
            (void)ruda_lexer_emit_token(
                lexer,
                RUDA_TOKEN_INTERPOLATED_END,
                start,
                lexer->offset
            );
            ruda_lexer_pop_context(lexer);
            return;
        }
        if (byte == '{') {
            if (lexer->offset + 1 < lexer->length
                && lexer->bytes[lexer->offset + 1] == '{') {
                lexer->offset += 2;
                continue;
            }
            if (lexer->offset != start) {
                (void)ruda_lexer_emit_token(
                    lexer,
                    RUDA_TOKEN_INTERPOLATED_TEXT,
                    start,
                    lexer->offset
                );
                return;
            }
            lexer->offset += 1;
            if (ruda_lexer_emit_token(
                    lexer,
                    RUDA_TOKEN_INTERPOLATION_START,
                    start,
                    lexer->offset
                )) {
                (void)ruda_lexer_push_context(
                    lexer,
                    RUDA_LEX_CONTEXT_INTERPOLATION_EXPRESSION,
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
                (void)ruda_lexer_emit_token(
                    lexer,
                    RUDA_TOKEN_INTERPOLATED_TEXT,
                    start,
                    lexer->offset
                );
                return;
            }
            lexer->offset += 1;
            (void)ruda_lexer_emit_token(
                lexer,
                RUDA_TOKEN_INVALID,
                start,
                lexer->offset
            );
            (void)ruda_lexer_add_diagnostic(
                lexer,
                RUDA_DIAGNOSTIC_UNMATCHED_INTERPOLATION_BRACE,
                "single '}' in interpolated text must be escaped as '}}'",
                start,
                lexer->offset
            );
            return;
        }
        if (byte == '\n' || byte == '\r') {
            if (lexer->offset != start) {
                (void)ruda_lexer_emit_token(
                    lexer,
                    RUDA_TOKEN_INTERPOLATED_TEXT,
                    start,
                    lexer->offset
                );
                return;
            }
            (void)ruda_lexer_add_diagnostic(
                lexer,
                RUDA_DIAGNOSTIC_UNTERMINATED_LITERAL,
                "interpolated string reaches a physical newline before closing",
                literal_start,
                lexer->offset
            );
            lexer->contexts.length = 1;
            return;
        }
        if (byte == '\\') {
            lexer->offset = ruda_lexer_scan_escape(
                lexer,
                lexer->offset,
                RUDA_LITERAL_NORMAL,
                &valid
            );
        } else if (byte >= 0x80U) {
            RudaUtf8Scalar scalar;
            if (ruda_lexer_next_scalar(lexer, lexer->offset, &scalar)) {
                lexer->offset += scalar.width;
            } else {
                lexer->offset += 1;
            }
        } else {
            lexer->offset += 1;
        }
    }
    if (lexer->offset != start) {
        (void)ruda_lexer_emit_token(
            lexer,
            RUDA_TOKEN_INTERPOLATED_TEXT,
            start,
            lexer->offset
        );
    } else if (!lexer->limit_reached) {
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_UNTERMINATED_LITERAL,
            "interpolated string reaches end of source before closing",
            literal_start,
            lexer->offset
        );
        lexer->contexts.length = 1;
    }
}

static bool ruda_lexer_emit_operator(RudaLexer *lexer) {
    size_t start = lexer->offset;
    unsigned char byte = lexer->bytes[start];
    unsigned char next = start + 1 < lexer->length ? lexer->bytes[start + 1] : 0;
    unsigned char third = start + 2 < lexer->length ? lexer->bytes[start + 2] : 0;
    RudaTokenKind kind = RUDA_TOKEN_INVALID;
    size_t width = 1;

    if (byte == '<' && next == '<' && third == '=') {
        kind = RUDA_TOKEN_SHIFT_LEFT_EQUAL;
        width = 3;
    } else if (byte == '>' && next == '>' && third == '=') {
        kind = RUDA_TOKEN_SHIFT_RIGHT_EQUAL;
        width = 3;
    } else if (byte == '=' && next == '>') {
        kind = RUDA_TOKEN_FAT_ARROW;
        width = 2;
    } else if (byte == '-' && next == '>') {
        kind = RUDA_TOKEN_THIN_ARROW;
        width = 2;
    } else if (byte == '=' && next == '=') {
        kind = RUDA_TOKEN_EQUAL_EQUAL;
        width = 2;
    } else if (byte == '!' && next == '=') {
        kind = RUDA_TOKEN_BANG_EQUAL;
        width = 2;
    } else if (byte == '<' && next == '=') {
        kind = RUDA_TOKEN_LESS_EQUAL;
        width = 2;
    } else if (byte == '>' && next == '=') {
        kind = RUDA_TOKEN_GREATER_EQUAL;
        width = 2;
    } else if (byte == '&' && next == '&') {
        kind = RUDA_TOKEN_AMPERSAND_AMPERSAND;
        width = 2;
    } else if (byte == '|' && next == '|') {
        kind = RUDA_TOKEN_PIPE_PIPE;
        width = 2;
    } else if (byte == '<' && next == '<') {
        kind = RUDA_TOKEN_SHIFT_LEFT;
        width = 2;
    } else if (byte == '>' && next == '>') {
        kind = RUDA_TOKEN_SHIFT_RIGHT;
        width = 2;
    } else if (byte == '+' && next == '=') {
        kind = RUDA_TOKEN_PLUS_EQUAL;
        width = 2;
    } else if (byte == '-' && next == '=') {
        kind = RUDA_TOKEN_MINUS_EQUAL;
        width = 2;
    } else if (byte == '*' && next == '=') {
        kind = RUDA_TOKEN_STAR_EQUAL;
        width = 2;
    } else if (byte == '/' && next == '=') {
        kind = RUDA_TOKEN_SLASH_EQUAL;
        width = 2;
    } else if (byte == '%' && next == '=') {
        kind = RUDA_TOKEN_PERCENT_EQUAL;
        width = 2;
    } else if (byte == '&' && next == '=') {
        kind = RUDA_TOKEN_AMPERSAND_EQUAL;
        width = 2;
    } else if (byte == '|' && next == '=') {
        kind = RUDA_TOKEN_PIPE_EQUAL;
        width = 2;
    } else if (byte == '^' && next == '=') {
        kind = RUDA_TOKEN_CARET_EQUAL;
        width = 2;
    } else {
        switch (byte) {
            case '@': kind = RUDA_TOKEN_AT; break;
            case ',': kind = RUDA_TOKEN_COMMA; break;
            case ':': kind = RUDA_TOKEN_COLON; break;
            case ';': kind = RUDA_TOKEN_SEMICOLON; break;
            case '.': kind = RUDA_TOKEN_DOT; break;
            case '(': kind = RUDA_TOKEN_LEFT_PAREN; break;
            case ')': kind = RUDA_TOKEN_RIGHT_PAREN; break;
            case '[': kind = RUDA_TOKEN_LEFT_BRACKET; break;
            case ']': kind = RUDA_TOKEN_RIGHT_BRACKET; break;
            case '+': kind = RUDA_TOKEN_PLUS; break;
            case '-': kind = RUDA_TOKEN_MINUS; break;
            case '*': kind = RUDA_TOKEN_STAR; break;
            case '/': kind = RUDA_TOKEN_SLASH; break;
            case '%': kind = RUDA_TOKEN_PERCENT; break;
            case '!': kind = RUDA_TOKEN_BANG; break;
            case '~': kind = RUDA_TOKEN_TILDE; break;
            case '&': kind = RUDA_TOKEN_AMPERSAND; break;
            case '|': kind = RUDA_TOKEN_PIPE; break;
            case '^': kind = RUDA_TOKEN_CARET; break;
            case '?': kind = RUDA_TOKEN_QUESTION; break;
            case '=': kind = RUDA_TOKEN_EQUAL; break;
            case '<': kind = RUDA_TOKEN_LESS; break;
            case '>': kind = RUDA_TOKEN_GREATER; break;
            default: return false;
        }
    }
    lexer->offset += width;
    (void)ruda_lexer_emit_token(lexer, kind, start, lexer->offset);
    return true;
}

static void ruda_lexer_scan_code_token(RudaLexer *lexer) {
    size_t start = lexer->offset;
    unsigned char byte = lexer->bytes[start];
    RudaLexContext *context = ruda_lexer_context(lexer);

    if (byte == '\n' || byte == '\r') {
        if (context->kind != RUDA_LEX_CONTEXT_ROOT) {
            (void)ruda_lexer_add_diagnostic(
                lexer,
                RUDA_DIAGNOSTIC_UNTERMINATED_LITERAL,
                "interpolated string reaches a physical newline before closing",
                context->literal_start,
                start
            );
            lexer->contexts.length = 1;
        }
        ruda_lexer_scan_whitespace(lexer);
        return;
    }
    if (ruda_ascii_whitespace(byte)) {
        ruda_lexer_scan_whitespace(lexer);
        return;
    }
    if (byte == '/' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '/') {
        ruda_lexer_scan_line_comment(lexer);
        return;
    }
    if (byte == '/' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '*') {
        ruda_lexer_scan_block_comment(lexer);
        return;
    }
    if (byte == '$' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '"') {
        ruda_lexer_start_interpolated(lexer);
        return;
    }
    if (byte == 'r' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '"') {
        ruda_lexer_scan_quoted(lexer, RUDA_LITERAL_RAW, RUDA_TOKEN_RAW_STRING, 1);
        return;
    }
    if (byte == 'b' && start + 1 < lexer->length
        && lexer->bytes[start + 1] == '"') {
        ruda_lexer_scan_quoted(lexer, RUDA_LITERAL_BYTE, RUDA_TOKEN_BYTE_STRING, 1);
        return;
    }
    if (byte == '"') {
        ruda_lexer_scan_quoted(lexer, RUDA_LITERAL_NORMAL, RUDA_TOKEN_STRING, 0);
        return;
    }
    if (byte == '\'') {
        ruda_lexer_scan_quoted(lexer, RUDA_LITERAL_RUNE, RUDA_TOKEN_RUNE, 0);
        return;
    }
    if (ruda_ascii_digit(byte)) {
        ruda_lexer_scan_number(lexer);
        return;
    }
    if (byte == '_') {
        ruda_lexer_scan_underscore(lexer);
        return;
    }
    {
        RudaUtf8Scalar scalar;
        if (ruda_lexer_next_scalar(lexer, start, &scalar)
            && ruda_unicode_is_xid_start(scalar.value)) {
            ruda_lexer_scan_identifier(lexer);
            return;
        }
        if (!ruda_lexer_next_scalar(lexer, start, &scalar)) {
            lexer->offset += 1;
            (void)ruda_lexer_emit_token(
                lexer,
                RUDA_TOKEN_INVALID,
                start,
                lexer->offset
            );
            return;
        }
    }
    if (byte == '{') {
        lexer->offset += 1;
        if (context->kind == RUDA_LEX_CONTEXT_INTERPOLATION_EXPRESSION) {
            if (context->brace_depth >= lexer->options.max_nesting) {
                (void)ruda_lexer_add_diagnostic(
                    lexer,
                    RUDA_DIAGNOSTIC_LEXICAL_NESTING_LIMIT,
                    "interpolation brace nesting exceeds the configured limit",
                    start,
                    lexer->offset
                );
                lexer->limit_reached = true;
                return;
            }
            context->brace_depth += 1;
        }
        (void)ruda_lexer_emit_token(
            lexer,
            RUDA_TOKEN_LEFT_BRACE,
            start,
            lexer->offset
        );
        return;
    }
    if (byte == '}') {
        lexer->offset += 1;
        if (context->kind == RUDA_LEX_CONTEXT_INTERPOLATION_EXPRESSION) {
            if (context->brace_depth == 0) {
                (void)ruda_lexer_emit_token(
                    lexer,
                    RUDA_TOKEN_INTERPOLATION_END,
                    start,
                    lexer->offset
                );
                ruda_lexer_pop_context(lexer);
            } else {
                context->brace_depth -= 1;
                (void)ruda_lexer_emit_token(
                    lexer,
                    RUDA_TOKEN_RIGHT_BRACE,
                    start,
                    lexer->offset
                );
            }
        } else {
            (void)ruda_lexer_emit_token(
                lexer,
                RUDA_TOKEN_RIGHT_BRACE,
                start,
                lexer->offset
            );
        }
        return;
    }
    if (ruda_lexer_emit_operator(lexer)) {
        return;
    }
    {
        RudaUtf8Scalar scalar;
        size_t width = ruda_lexer_next_scalar(lexer, start, &scalar) ? scalar.width : 1;
        lexer->offset += width;
        (void)ruda_lexer_emit_token(
            lexer,
            RUDA_TOKEN_INVALID,
            start,
            lexer->offset
        );
        (void)ruda_lexer_add_diagnostic(
            lexer,
            RUDA_DIAGNOSTIC_UNEXPECTED_CHARACTER,
            "character cannot begin a Ruda token",
            start,
            lexer->offset
        );
    }
}

static int ruda_lex_diagnostic_compare(const void *left, const void *right) {
    const RudaLexDiagnostic *left_diagnostic = left;
    const RudaLexDiagnostic *right_diagnostic = right;
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

RudaLexStatus ruda_lex_snapshot(
    const RudaSourceSnapshot *snapshot,
    RudaAllocator allocator,
    RudaLexOptions options,
    RudaLexResult *result
) {
    RudaLexer lexer;
    RudaLexContext root;
    if (result == NULL) {
        return RUDA_LEX_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (snapshot == NULL || !ruda_lex_options_valid(options)
        || !ruda_vector_init(&result->tokens, allocator, sizeof(RudaToken))
        || !ruda_vector_init(
            &result->diagnostics,
            allocator,
            sizeof(RudaLexDiagnostic)
        )) {
        return RUDA_LEX_INVALID_ARGUMENT;
    }
    result->allocator = allocator;
    result->snapshot = snapshot;
    result->initialized = true;
    memset(&lexer, 0, sizeof(lexer));
    lexer.bytes = ruda_source_bytes(snapshot);
    lexer.length = ruda_source_length(snapshot);
    lexer.options = options;
    lexer.result = result;
    if (!ruda_vector_init(&lexer.contexts, allocator, sizeof(RudaLexContext))) {
        ruda_lex_result_dispose(result);
        return RUDA_LEX_INVALID_ARGUMENT;
    }
    root.kind = RUDA_LEX_CONTEXT_ROOT;
    root.literal_start = 0;
    root.brace_depth = 0;
    if (!ruda_vector_push(&lexer.contexts, &root)) {
        ruda_vector_dispose(&lexer.contexts);
        ruda_lex_result_dispose(result);
        return RUDA_LEX_ALLOCATION_FAILED;
    }

    ruda_lexer_scan_invalid_utf8(&lexer);
    if (!lexer.limit_reached && lexer.length >= 3
        && lexer.bytes[0] == 0xEFU && lexer.bytes[1] == 0xBBU
        && lexer.bytes[2] == 0xBFU) {
        lexer.offset = 3;
        (void)ruda_lexer_emit_token(&lexer, RUDA_TOKEN_BOM, 0, 3);
    }
    while (lexer.offset < lexer.length && !lexer.allocation_failed
        && !lexer.limit_reached) {
        RudaLexContext *context = ruda_lexer_context(&lexer);
        if (context->kind == RUDA_LEX_CONTEXT_INTERPOLATED_TEXT) {
            ruda_lexer_scan_interpolated_text(&lexer);
        } else {
            ruda_lexer_scan_code_token(&lexer);
        }
    }
    if (!lexer.allocation_failed) {
        (void)ruda_lexer_emit_token(
            &lexer,
            RUDA_TOKEN_EOF,
            lexer.offset,
            lexer.offset
        );
    }
    ruda_vector_dispose(&lexer.contexts);
    if (lexer.allocation_failed) {
        ruda_lex_result_dispose(result);
        return RUDA_LEX_ALLOCATION_FAILED;
    }
    if (result->diagnostics.length > 1) {
        qsort(
            result->diagnostics.data,
            result->diagnostics.length,
            sizeof(RudaLexDiagnostic),
            ruda_lex_diagnostic_compare
        );
    }
    return lexer.limit_reached ? RUDA_LEX_LIMIT_REACHED : RUDA_LEX_OK;
}
