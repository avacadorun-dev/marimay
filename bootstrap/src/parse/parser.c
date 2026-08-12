#include "ruda/parse/parser.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/generated/diagnostic_codes.h"

typedef enum RudaParseEventKind {
    RUDA_PARSE_EVENT_TOMBSTONE,
    RUDA_PARSE_EVENT_START,
    RUDA_PARSE_EVENT_TOKEN,
    RUDA_PARSE_EVENT_SYNTHETIC_TOKEN,
    RUDA_PARSE_EVENT_FINISH
} RudaParseEventKind;

typedef struct RudaParseStartEvent {
    RudaSyntaxKind syntax_kind;
    size_t forward_parent;
} RudaParseStartEvent;

typedef struct RudaParseTokenEvent {
    size_t token_index;
    RudaTokenKind kind;
} RudaParseTokenEvent;

typedef struct RudaParseEvent {
    RudaParseEventKind kind;
    union {
        RudaParseStartEvent start;
        RudaParseTokenEvent token;
        RudaSyntaxToken synthetic_token;
    } as;
} RudaParseEvent;

typedef struct RudaMarker {
    size_t position;
    bool valid;
} RudaMarker;

typedef struct RudaCompletedMarker {
    size_t position;
    bool valid;
} RudaCompletedMarker;

typedef struct RudaParser {
    const RudaLexResult *lexed;
    RudaParseResult *result;
    RudaParseOptions options;
    RudaVector events;
    size_t raw_index;
    size_t nesting;
    size_t pending_type_greater;
    size_t type_angle_depth;
    bool allocation_failed;
    bool limit_reached;
} RudaParser;

typedef struct RudaBuildFrame {
    RudaSyntaxKind kind;
    size_t element_start;
} RudaBuildFrame;

static bool ruda_token_is_trivia(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_BOM || kind == RUDA_TOKEN_WHITESPACE
        || kind == RUDA_TOKEN_LINE_COMMENT || kind == RUDA_TOKEN_DOC_LINE_COMMENT
        || kind == RUDA_TOKEN_BLOCK_COMMENT || kind == RUDA_TOKEN_DOC_BLOCK_COMMENT;
}

static const RudaToken *ruda_parser_raw_token(const RudaParser *parser, size_t index) {
    return ruda_lex_token(parser->lexed, index);
}

static size_t ruda_parser_skip_trivia(const RudaParser *parser, size_t index) {
    while (index < parser->lexed->tokens.length) {
        const RudaToken *token = ruda_parser_raw_token(parser, index);
        if (token == NULL || !ruda_token_is_trivia(token->kind)) {
            break;
        }
        index += 1;
    }
    return index;
}

static size_t ruda_parser_significant_index(const RudaParser *parser) {
    return ruda_parser_skip_trivia(parser, parser->raw_index);
}

static const RudaToken *ruda_parser_current_token(const RudaParser *parser) {
    return ruda_parser_raw_token(parser, ruda_parser_significant_index(parser));
}

static RudaTokenKind ruda_parser_current(const RudaParser *parser) {
    const RudaToken *token = ruda_parser_current_token(parser);
    return token == NULL ? RUDA_TOKEN_EOF : token->kind;
}

static size_t ruda_parser_current_offset(const RudaParser *parser) {
    const RudaToken *token = ruda_parser_current_token(parser);
    if (token != NULL) {
        return token->start;
    }
    return ruda_source_length(parser->lexed->snapshot);
}

static bool ruda_parser_at(const RudaParser *parser, RudaTokenKind kind) {
    return ruda_parser_current(parser) == kind;
}

static bool ruda_token_is_contextual_keyword(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_KW_AS || kind == RUDA_TOKEN_KW_BREAK
        || kind == RUDA_TOKEN_KW_CASE || kind == RUDA_TOKEN_KW_CONST
        || kind == RUDA_TOKEN_KW_CONTINUE || kind == RUDA_TOKEN_KW_ELSE
        || kind == RUDA_TOKEN_KW_ENUM || kind == RUDA_TOKEN_KW_EXTERN
        || kind == RUDA_TOKEN_KW_FN || kind == RUDA_TOKEN_KW_FOR
        || kind == RUDA_TOKEN_KW_FROM || kind == RUDA_TOKEN_KW_IMPORT
        || kind == RUDA_TOKEN_KW_INTERFACE || kind == RUDA_TOKEN_KW_LET
        || kind == RUDA_TOKEN_KW_MUTABLE || kind == RUDA_TOKEN_KW_OF
        || kind == RUDA_TOKEN_KW_PUBLIC || kind == RUDA_TOKEN_KW_SCOPE
        || kind == RUDA_TOKEN_KW_TYPE || kind == RUDA_TOKEN_KW_UNSAFE
        || kind == RUDA_TOKEN_KW_YIELD;
}

static bool ruda_parser_at_identifier(const RudaParser *parser) {
    RudaTokenKind kind = ruda_parser_current(parser);
    return kind == RUDA_TOKEN_IDENTIFIER
        || ruda_token_is_contextual_keyword(kind);
}

static RudaTokenKind ruda_parser_next_significant_kind(
    const RudaParser *parser
) {
    size_t current = ruda_parser_significant_index(parser);
    size_t next = ruda_parser_skip_trivia(parser, current + 1);
    const RudaToken *token = ruda_parser_raw_token(parser, next);
    return token == NULL ? RUDA_TOKEN_EOF : token->kind;
}

static RudaTokenKind ruda_parser_second_next_significant_kind(
    const RudaParser *parser
) {
    size_t current = ruda_parser_significant_index(parser);
    size_t next = ruda_parser_skip_trivia(parser, current + 1);
    size_t second_next = ruda_parser_skip_trivia(parser, next + 1);
    const RudaToken *token = ruda_parser_raw_token(parser, second_next);
    return token == NULL ? RUDA_TOKEN_EOF : token->kind;
}

static bool ruda_parser_add_diagnostic(
    RudaParser *parser,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    RudaParseDiagnostic diagnostic;
    if (parser->result->diagnostics.length >= parser->options.max_diagnostics) {
        parser->result->suppressed_diagnostics += 1;
        parser->limit_reached = true;
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!ruda_vector_push(&parser->result->diagnostics, &diagnostic)) {
        parser->allocation_failed = true;
        return false;
    }
    return true;
}

static bool ruda_parser_push_event(RudaParser *parser, const RudaParseEvent *event) {
    if (parser->events.length >= parser->options.max_events) {
        if (!parser->limit_reached) {
            size_t offset = ruda_parser_current_offset(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_PARSER_WORK_LIMIT,
                "parser event limit reached",
                offset,
                offset
            );
        }
        parser->limit_reached = true;
        return false;
    }
    if (!ruda_vector_push(&parser->events, event)) {
        parser->allocation_failed = true;
        return false;
    }
    return true;
}

static RudaMarker ruda_parser_start(RudaParser *parser) {
    RudaMarker marker;
    RudaParseEvent event;
    marker.position = parser->events.length;
    marker.valid = false;
    if (parser->nesting >= parser->options.max_nesting) {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_PARSER_NESTING_LIMIT,
            "parser nesting exceeds the configured limit",
            offset,
            offset
        );
        parser->limit_reached = true;
        return marker;
    }
    event.kind = RUDA_PARSE_EVENT_START;
    event.as.start.syntax_kind = RUDA_SYNTAX_ERROR;
    event.as.start.forward_parent = 0;
    if (ruda_parser_push_event(parser, &event)) {
        marker.valid = true;
        parser->nesting += 1;
    }
    return marker;
}

static RudaCompletedMarker ruda_parser_complete(
    RudaParser *parser,
    RudaMarker marker,
    RudaSyntaxKind kind
) {
    RudaParseEvent *start = NULL;
    RudaParseEvent finish;
    RudaCompletedMarker completed;
    completed.position = marker.position;
    completed.valid = false;
    if (!marker.valid || parser->allocation_failed || parser->limit_reached) {
        return completed;
    }
    start = ruda_vector_at(&parser->events, marker.position);
    if (start == NULL || start->kind != RUDA_PARSE_EVENT_START) {
        parser->allocation_failed = true;
        return completed;
    }
    start->as.start.syntax_kind = kind;
    finish.kind = RUDA_PARSE_EVENT_FINISH;
    finish.as.token.token_index = 0;
    finish.as.token.kind = RUDA_TOKEN_EOF;
    if (ruda_parser_push_event(parser, &finish) && parser->nesting != 0) {
        parser->nesting -= 1;
        completed.valid = true;
    }
    return completed;
}

static RudaMarker ruda_parser_precede(
    RudaParser *parser,
    RudaCompletedMarker completed
) {
    RudaMarker parent;
    RudaParseEvent *child_start = NULL;
    parent.position = parser->events.length;
    parent.valid = false;
    if (!completed.valid || completed.position >= parser->events.length) {
        parser->allocation_failed = true;
        return parent;
    }
    parent = ruda_parser_start(parser);
    if (!parent.valid) {
        return parent;
    }
    child_start = ruda_vector_at(&parser->events, completed.position);
    if (child_start == NULL || child_start->kind != RUDA_PARSE_EVENT_START
        || child_start->as.start.forward_parent != 0
        || parent.position <= completed.position) {
        parser->allocation_failed = true;
        return parent;
    }
    child_start->as.start.forward_parent = parent.position - completed.position;
    return parent;
}

static bool ruda_parser_completed_contains_kind(
    const RudaParser *parser,
    RudaCompletedMarker completed,
    RudaSyntaxKind kind
) {
    size_t index = completed.position;
    if (!completed.valid) {
        return false;
    }
    while (index < parser->events.length) {
        const RudaParseEvent *event = ruda_vector_at_const(
            &parser->events,
            index
        );
        if (event == NULL) {
            return false;
        }
        if (event->kind == RUDA_PARSE_EVENT_START
            && event->as.start.syntax_kind == kind) {
            return true;
        }
        index += 1;
    }
    return false;
}

static RudaSyntaxKind ruda_parser_completed_kind(
    const RudaParser *parser,
    RudaCompletedMarker completed
) {
    const RudaParseEvent *event = completed.valid
        ? ruda_vector_at_const(&parser->events, completed.position)
        : NULL;
    return event != NULL && event->kind == RUDA_PARSE_EVENT_START
        ? event->as.start.syntax_kind
        : RUDA_SYNTAX_ERROR;
}

static bool ruda_parser_completed_contains_token_kind(
    const RudaParser *parser,
    RudaCompletedMarker completed,
    RudaTokenKind kind
) {
    size_t index = completed.position;
    if (!completed.valid) {
        return false;
    }
    while (index < parser->events.length) {
        const RudaParseEvent *event = ruda_vector_at_const(
            &parser->events,
            index
        );
        if (event == NULL) {
            return false;
        }
        if (event->kind == RUDA_PARSE_EVENT_TOKEN) {
            const RudaToken *token = ruda_parser_raw_token(
                parser,
                event->as.token.token_index
            );
            if (token != NULL && event->as.token.kind == kind) {
                return true;
            }
        }
        index += 1;
    }
    return false;
}

static void ruda_parser_bump_as(
    RudaParser *parser,
    RudaTokenKind significant_kind
) {
    size_t significant = ruda_parser_significant_index(parser);
    size_t index = parser->raw_index;
    if (significant >= parser->lexed->tokens.length) {
        return;
    }
    while (index <= significant && !parser->allocation_failed
        && !parser->limit_reached) {
        RudaParseEvent event;
        event.kind = RUDA_PARSE_EVENT_TOKEN;
        event.as.token.token_index = index;
        event.as.token.kind = index == significant
            ? significant_kind
            : ruda_parser_raw_token(parser, index)->kind;
        if (!ruda_parser_push_event(parser, &event)) {
            return;
        }
        index += 1;
    }
    parser->raw_index = index;
}

static void ruda_parser_bump(RudaParser *parser) {
    ruda_parser_bump_as(parser, ruda_parser_current(parser));
}

static bool ruda_parser_eat_identifier(RudaParser *parser) {
    if (!ruda_parser_at_identifier(parser)) {
        return false;
    }
    ruda_parser_bump_as(parser, RUDA_TOKEN_IDENTIFIER);
    return true;
}

static bool ruda_parser_eat(RudaParser *parser, RudaTokenKind kind) {
    if (!ruda_parser_at(parser, kind)) {
        return false;
    }
    ruda_parser_bump(parser);
    return true;
}

static void ruda_parser_emit_leading_trivia(RudaParser *parser) {
    size_t significant = ruda_parser_significant_index(parser);
    while (parser->raw_index < significant && !parser->allocation_failed
        && !parser->limit_reached) {
        RudaParseEvent event;
        event.kind = RUDA_PARSE_EVENT_TOKEN;
        event.as.token.token_index = parser->raw_index;
        event.as.token.kind = ruda_parser_raw_token(
            parser,
            parser->raw_index
        )->kind;
        if (!ruda_parser_push_event(parser, &event)) {
            return;
        }
        parser->raw_index += 1;
    }
}

static void ruda_parser_expect(RudaParser *parser, RudaTokenKind kind, const char *message) {
    RudaParseEvent event;
    size_t offset = 0;
    if (ruda_parser_eat(parser, kind)) {
        return;
    }
    ruda_parser_emit_leading_trivia(parser);
    offset = ruda_parser_current_offset(parser);
    if (!ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            message,
            offset,
            offset
        )) {
        return;
    }
    event.kind = RUDA_PARSE_EVENT_SYNTHETIC_TOKEN;
    event.as.synthetic_token.kind = kind;
    event.as.synthetic_token.start = offset;
    event.as.synthetic_token.end = offset;
    event.as.synthetic_token.synthetic = true;
    (void)ruda_parser_push_event(parser, &event);
}

static void ruda_parser_expect_identifier(
    RudaParser *parser,
    const char *message
) {
    RudaParseEvent event;
    size_t offset = 0;
    if (ruda_parser_eat_identifier(parser)) {
        return;
    }
    ruda_parser_emit_leading_trivia(parser);
    offset = ruda_parser_current_offset(parser);
    if (!ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            message,
            offset,
            offset
        )) {
        return;
    }
    event.kind = RUDA_PARSE_EVENT_SYNTHETIC_TOKEN;
    event.as.synthetic_token.kind = RUDA_TOKEN_IDENTIFIER;
    event.as.synthetic_token.start = offset;
    event.as.synthetic_token.end = offset;
    event.as.synthetic_token.synthetic = true;
    (void)ruda_parser_push_event(parser, &event);
}

static bool ruda_parser_is_stop(
    const RudaParser *parser,
    RudaTokenKind first,
    RudaTokenKind second,
    RudaTokenKind third
) {
    RudaTokenKind current = ruda_parser_current(parser);
    return current == first || current == second || current == third
        || current == RUDA_TOKEN_EOF;
}

typedef struct RudaExpressionStops {
    RudaTokenKind first;
    RudaTokenKind second;
    RudaTokenKind third;
} RudaExpressionStops;

static void ruda_parse_expression_until(
    RudaParser *parser,
    RudaTokenKind first,
    RudaTokenKind second,
    RudaTokenKind third
);

static RudaCompletedMarker ruda_parse_expression_bp(
    RudaParser *parser,
    RudaExpressionStops stops,
    unsigned int minimum_binding_power
);

static void ruda_parse_type_until(
    RudaParser *parser,
    RudaTokenKind first,
    RudaTokenKind second,
    RudaTokenKind third
);

static void ruda_parse_block(RudaParser *parser);
static void ruda_parse_parameters(RudaParser *parser);

static void ruda_parser_expect_closing_delimiter(
    RudaParser *parser,
    RudaTokenKind close,
    size_t open_offset,
    const char *message
) {
    size_t offset = 0;
    if (ruda_parser_eat(parser, close)) {
        return;
    }
    offset = ruda_parser_current_offset(parser);
    if (!ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_UNTERMINATED_DELIMITER,
            "delimited syntax is missing its closing token",
            open_offset,
            offset
        )) {
        return;
    }
    ruda_parser_expect(parser, close, message);
}

static RudaCompletedMarker ruda_parse_interpolated_expression(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    ruda_parser_expect(
        parser,
        RUDA_TOKEN_INTERPOLATED_START,
        "expected interpolated string start"
    );
    while (!ruda_parser_at(parser, RUDA_TOKEN_INTERPOLATED_END)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        if (ruda_parser_at(parser, RUDA_TOKEN_INTERPOLATED_TEXT)) {
            ruda_parser_bump(parser);
        } else if (ruda_parser_eat(parser, RUDA_TOKEN_INTERPOLATION_START)) {
            ruda_parse_expression_until(
                parser,
                RUDA_TOKEN_INTERPOLATION_END,
                RUDA_TOKEN_EOF,
                RUDA_TOKEN_EOF
            );
            ruda_parser_expect(
                parser,
                RUDA_TOKEN_INTERPOLATION_END,
                "expected end of interpolation expression"
            );
        } else {
            size_t offset = ruda_parser_current_offset(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "unexpected token in interpolated string",
                offset,
                offset
            );
            ruda_parser_bump(parser);
        }
    }
    ruda_parser_expect(
        parser,
        RUDA_TOKEN_INTERPOLATED_END,
        "expected end of interpolated string"
    );
    return ruda_parser_complete(
        parser,
        marker,
        RUDA_SYNTAX_INTERPOLATED_EXPRESSION
    );
}

static bool ruda_expression_at_stop(
    const RudaParser *parser,
    RudaExpressionStops stops
) {
    return ruda_parser_is_stop(parser, stops.first, stops.second, stops.third);
}

static bool ruda_expression_is_literal(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_INTEGER || kind == RUDA_TOKEN_FLOAT
        || kind == RUDA_TOKEN_STRING || kind == RUDA_TOKEN_RAW_STRING
        || kind == RUDA_TOKEN_BYTE_STRING || kind == RUDA_TOKEN_RUNE
        || kind == RUDA_TOKEN_KW_TRUE || kind == RUDA_TOKEN_KW_FALSE;
}

static bool ruda_expression_is_assignment(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_EQUAL || kind == RUDA_TOKEN_PLUS_EQUAL
        || kind == RUDA_TOKEN_MINUS_EQUAL || kind == RUDA_TOKEN_STAR_EQUAL
        || kind == RUDA_TOKEN_SLASH_EQUAL || kind == RUDA_TOKEN_PERCENT_EQUAL
        || kind == RUDA_TOKEN_AMPERSAND_EQUAL || kind == RUDA_TOKEN_PIPE_EQUAL
        || kind == RUDA_TOKEN_CARET_EQUAL || kind == RUDA_TOKEN_SHIFT_LEFT_EQUAL
        || kind == RUDA_TOKEN_SHIFT_RIGHT_EQUAL;
}

static bool ruda_parser_yield_is_name_expression(
    const RudaParser *parser
) {
    RudaTokenKind next = ruda_parser_next_significant_kind(parser);
    return next == RUDA_TOKEN_DOT || next == RUDA_TOKEN_LEFT_PAREN
        || next == RUDA_TOKEN_LEFT_BRACKET || next == RUDA_TOKEN_LESS
        || next == RUDA_TOKEN_QUESTION || next == RUDA_TOKEN_PLUS
        || next == RUDA_TOKEN_STAR
        || next == RUDA_TOKEN_SLASH || next == RUDA_TOKEN_PERCENT
        || next == RUDA_TOKEN_EQUAL_EQUAL || next == RUDA_TOKEN_BANG_EQUAL
        || next == RUDA_TOKEN_LESS_EQUAL || next == RUDA_TOKEN_GREATER
        || next == RUDA_TOKEN_GREATER_EQUAL || next == RUDA_TOKEN_AMPERSAND
        || next == RUDA_TOKEN_PIPE || next == RUDA_TOKEN_CARET
        || next == RUDA_TOKEN_AMPERSAND_AMPERSAND
        || next == RUDA_TOKEN_PIPE_PIPE || next == RUDA_TOKEN_SHIFT_LEFT
        || next == RUDA_TOKEN_SHIFT_RIGHT
        || ruda_expression_is_assignment(next);
}

static bool ruda_parser_contextual_statement_starts(
    const RudaParser *parser,
    RudaTokenKind current
) {
    RudaTokenKind next = ruda_parser_next_significant_kind(parser);
    if (current == RUDA_TOKEN_KW_CONST || current == RUDA_TOKEN_KW_LET) {
        return next == RUDA_TOKEN_IDENTIFIER
            || ruda_token_is_contextual_keyword(next)
            || next == RUDA_TOKEN_UNDERSCORE
            || next == RUDA_TOKEN_COLON;
    }
    if (current == RUDA_TOKEN_KW_FOR) {
        return next == RUDA_TOKEN_KW_CONST || next == RUDA_TOKEN_KW_LET
            || ((next == RUDA_TOKEN_IDENTIFIER
                    || ruda_token_is_contextual_keyword(next))
                && ruda_parser_second_next_significant_kind(parser)
                    == RUDA_TOKEN_KW_OF);
    }
    if (current == RUDA_TOKEN_KW_SCOPE || current == RUDA_TOKEN_KW_UNSAFE) {
        return next == RUDA_TOKEN_LEFT_BRACE;
    }
    if (current == RUDA_TOKEN_KW_BREAK
        || current == RUDA_TOKEN_KW_CONTINUE) {
        return next == RUDA_TOKEN_SEMICOLON
            || next == RUDA_TOKEN_RIGHT_BRACE || next == RUDA_TOKEN_EOF;
    }
    return true;
}

static bool ruda_expression_infix_binding_power(
    RudaTokenKind kind,
    unsigned int *left,
    unsigned int *right,
    RudaSyntaxKind *syntax_kind
) {
    unsigned int binding_power = 0;
    if (kind == RUDA_TOKEN_STAR || kind == RUDA_TOKEN_SLASH
        || kind == RUDA_TOKEN_PERCENT) {
        binding_power = 80;
    } else if (kind == RUDA_TOKEN_PLUS || kind == RUDA_TOKEN_MINUS) {
        binding_power = 70;
    } else if (kind == RUDA_TOKEN_SHIFT_LEFT || kind == RUDA_TOKEN_SHIFT_RIGHT) {
        binding_power = 60;
    } else if (kind == RUDA_TOKEN_LESS || kind == RUDA_TOKEN_LESS_EQUAL
        || kind == RUDA_TOKEN_GREATER || kind == RUDA_TOKEN_GREATER_EQUAL) {
        binding_power = 50;
    } else if (kind == RUDA_TOKEN_EQUAL_EQUAL || kind == RUDA_TOKEN_BANG_EQUAL) {
        binding_power = 40;
    } else if (kind == RUDA_TOKEN_AMPERSAND || kind == RUDA_TOKEN_CARET
        || kind == RUDA_TOKEN_PIPE) {
        binding_power = 35;
    } else if (kind == RUDA_TOKEN_AMPERSAND_AMPERSAND) {
        binding_power = 30;
    } else if (kind == RUDA_TOKEN_PIPE_PIPE) {
        binding_power = 20;
    } else if (ruda_expression_is_assignment(kind)) {
        *left = 10;
        *right = 11;
        *syntax_kind = RUDA_SYNTAX_ASSIGNMENT_EXPRESSION;
        return true;
    } else {
        return false;
    }
    *left = binding_power;
    *right = binding_power + 1;
    *syntax_kind = RUDA_SYNTAX_BINARY_EXPRESSION;
    return true;
}

static bool ruda_parser_paren_is_closure(const RudaParser *parser) {
    size_t index = ruda_parser_significant_index(parser);
    size_t depth = 0;
    if (ruda_parser_raw_token(parser, index) == NULL
        || ruda_parser_raw_token(parser, index)->kind != RUDA_TOKEN_LEFT_PAREN) {
        return false;
    }
    while (index < parser->lexed->tokens.length) {
        const RudaToken *token = ruda_parser_raw_token(parser, index);
        if (token == NULL) {
            return false;
        }
        if (token->kind == RUDA_TOKEN_LEFT_PAREN) {
            depth += 1;
        } else if (token->kind == RUDA_TOKEN_RIGHT_PAREN) {
            if (depth == 0) {
                return false;
            }
            depth -= 1;
            if (depth == 0) {
                size_t next = ruda_parser_skip_trivia(parser, index + 1);
                const RudaToken *after = ruda_parser_raw_token(parser, next);
                if (after == NULL) {
                    return false;
                }
                if (after->kind == RUDA_TOKEN_FAT_ARROW) {
                    return true;
                }
                if (after->kind != RUDA_TOKEN_COLON) {
                    return false;
                }
                next = ruda_parser_skip_trivia(parser, next + 1);
                while (next < parser->lexed->tokens.length) {
                    after = ruda_parser_raw_token(parser, next);
                    if (after == NULL || after->kind == RUDA_TOKEN_EOF
                        || after->kind == RUDA_TOKEN_SEMICOLON
                        || after->kind == RUDA_TOKEN_RIGHT_BRACE) {
                        return false;
                    }
                    if (after->kind == RUDA_TOKEN_FAT_ARROW) {
                        return true;
                    }
                    next = ruda_parser_skip_trivia(parser, next + 1);
                }
                return false;
            }
        }
        index += 1;
    }
    return false;
}

static bool ruda_parser_generic_call_ahead(const RudaParser *parser) {
    size_t index = ruda_parser_significant_index(parser);
    size_t depth = 0;
    const RudaToken *token = ruda_parser_raw_token(parser, index);
    if (token == NULL || token->kind != RUDA_TOKEN_LESS) {
        return false;
    }
    while (index < parser->lexed->tokens.length) {
        token = ruda_parser_raw_token(parser, index);
        if (token == NULL || token->kind == RUDA_TOKEN_EOF) {
            return false;
        }
        if (token->kind == RUDA_TOKEN_LESS) {
            depth += 1;
        } else if (token->kind == RUDA_TOKEN_GREATER) {
            if (depth == 0) {
                return false;
            }
            depth -= 1;
        } else if (token->kind == RUDA_TOKEN_SHIFT_RIGHT) {
            if (depth < 2) {
                return false;
            }
            depth -= 2;
        }
        if (depth == 0) {
            size_t next = ruda_parser_skip_trivia(parser, index + 1);
            const RudaToken *after = ruda_parser_raw_token(parser, next);
            return after != NULL && after->kind == RUDA_TOKEN_LEFT_PAREN;
        }
        index += 1;
    }
    return false;
}

static bool ruda_parser_type_at_greater(const RudaParser *parser) {
    return parser->pending_type_greater != 0
        || ruda_parser_at(parser, RUDA_TOKEN_GREATER)
        || ruda_parser_at(parser, RUDA_TOKEN_SHIFT_RIGHT);
}

static bool ruda_parser_eat_type_greater(RudaParser *parser) {
    if (parser->pending_type_greater != 0) {
        parser->pending_type_greater -= 1;
        return true;
    }
    if (ruda_parser_eat(parser, RUDA_TOKEN_GREATER)) {
        return true;
    }
    if (ruda_parser_at(parser, RUDA_TOKEN_SHIFT_RIGHT)) {
        ruda_parser_bump(parser);
        parser->pending_type_greater = 1;
        return true;
    }
    return false;
}

static void ruda_parser_expect_type_greater(RudaParser *parser) {
    if (!ruda_parser_eat_type_greater(parser)) {
        ruda_parser_expect(
            parser,
            RUDA_TOKEN_GREATER,
            "expected '>' after type list"
        );
    }
}

static void ruda_parser_begin_type_angle(RudaParser *parser) {
    if (!ruda_size_add(
            parser->type_angle_depth,
            1,
            &parser->type_angle_depth
        )) {
        parser->limit_reached = true;
    }
}

static void ruda_parser_end_type_angle(RudaParser *parser) {
    if (parser->type_angle_depth == 0) {
        parser->limit_reached = true;
        return;
    }
    parser->type_angle_depth -= 1;
    if (parser->type_angle_depth == 0 && parser->pending_type_greater != 0) {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "unmatched '>' after type list",
            offset,
            offset
        );
        parser->pending_type_greater = 0;
    }
}

static void ruda_parse_type_arguments(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LESS, "expected '<' for type arguments");
    ruda_parser_begin_type_angle(parser);
    if (ruda_parser_type_at_greater(parser)) {
        ruda_parse_type_until(
            parser,
            RUDA_TOKEN_COMMA,
            RUDA_TOKEN_GREATER,
            RUDA_TOKEN_SHIFT_RIGHT
        );
    }
    while (!ruda_parser_type_at_greater(parser)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        ruda_parse_type_until(
            parser,
            RUDA_TOKEN_COMMA,
            RUDA_TOKEN_GREATER,
            RUDA_TOKEN_SHIFT_RIGHT
        );
        if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
            break;
        }
    }
    ruda_parser_expect_type_greater(parser);
    ruda_parser_end_type_angle(parser);
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_TYPE_ARGUMENTS);
}

static void ruda_parse_expression_list(
    RudaParser *parser,
    RudaTokenKind close
) {
    while (!ruda_parser_at(parser, close) && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        ruda_parse_expression_until(
            parser,
            RUDA_TOKEN_COMMA,
            close,
            RUDA_TOKEN_EOF
        );
        if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
            break;
        }
        if (parser->raw_index == before) {
            break;
        }
    }
}

static RudaCompletedMarker ruda_parse_parenthesized_expression(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_PAREN, "expected '('");
    ruda_parse_expression_until(
        parser,
        RUDA_TOKEN_COMMA,
        RUDA_TOKEN_RIGHT_PAREN,
        RUDA_TOKEN_EOF
    );
    if (ruda_parser_at(parser, RUDA_TOKEN_COMMA)) {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "parenthesized expression contains more than one value",
            offset,
            offset
        );
        while (ruda_parser_eat(parser, RUDA_TOKEN_COMMA)
            && !ruda_parser_at(parser, RUDA_TOKEN_RIGHT_PAREN)
            && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
            && !parser->allocation_failed && !parser->limit_reached) {
            ruda_parse_expression_until(
                parser,
                RUDA_TOKEN_COMMA,
                RUDA_TOKEN_RIGHT_PAREN,
                RUDA_TOKEN_EOF
            );
        }
    }
    ruda_parser_expect_closing_delimiter(
        parser,
        RUDA_TOKEN_RIGHT_PAREN,
        open_offset,
        "expected ')' after expression"
    );
    return ruda_parser_complete(parser, marker, RUDA_SYNTAX_PAREN_GROUP);
}

static RudaCompletedMarker ruda_parse_array_expression(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_BRACKET, "expected '['");
    ruda_parse_expression_list(parser, RUDA_TOKEN_RIGHT_BRACKET);
    ruda_parser_expect_closing_delimiter(
        parser,
        RUDA_TOKEN_RIGHT_BRACKET,
        open_offset,
        "expected ']' after array"
    );
    return ruda_parser_complete(parser, marker, RUDA_SYNTAX_ARRAY_EXPRESSION);
}

static void ruda_parse_arguments(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_PAREN, "expected argument list");
    ruda_parse_expression_list(parser, RUDA_TOKEN_RIGHT_PAREN);
    ruda_parser_expect_closing_delimiter(
        parser,
        RUDA_TOKEN_RIGHT_PAREN,
        open_offset,
        "expected ')' after arguments"
    );
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_ARGUMENTS);
}

static void ruda_parse_index_group(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_BRACKET, "expected '[' for index");
    ruda_parse_expression_until(
        parser,
        RUDA_TOKEN_RIGHT_BRACKET,
        RUDA_TOKEN_EOF,
        RUDA_TOKEN_EOF
    );
    ruda_parser_expect_closing_delimiter(
        parser,
        RUDA_TOKEN_RIGHT_BRACKET,
        open_offset,
        "expected ']' after index"
    );
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_BRACKET_GROUP);
}

static void ruda_parse_record_fields(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(
        parser,
        RUDA_TOKEN_LEFT_BRACE,
        "expected '{' for record fields"
    );
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        RudaMarker field = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected record field name");
        if (ruda_parser_eat(parser, RUDA_TOKEN_COLON)) {
            ruda_parse_expression_until(
                parser,
                RUDA_TOKEN_COMMA,
                RUDA_TOKEN_RIGHT_BRACE,
                RUDA_TOKEN_EOF
            );
        }
        (void)ruda_parser_complete(parser, field, RUDA_SYNTAX_FIELD_VALUE);
        if (ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
            continue;
        }
        if (ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)) {
            break;
        }
        ruda_parser_expect(
            parser,
            RUDA_TOKEN_COMMA,
            "expected ',' between record fields"
        );
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            ruda_parser_bump(parser);
        }
    }
    ruda_parser_expect_closing_delimiter(
        parser,
        RUDA_TOKEN_RIGHT_BRACE,
        open_offset,
        "expected '}' after record fields"
    );
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_RECORD_FIELDS);
}

typedef struct RudaPatternStops {
    RudaTokenKind first;
    RudaTokenKind second;
} RudaPatternStops;

static bool ruda_pattern_at_stop(
    const RudaParser *parser,
    RudaPatternStops stops
) {
    RudaTokenKind current = ruda_parser_current(parser);
    return current == stops.first || current == stops.second
        || current == RUDA_TOKEN_SEMICOLON
        || current == RUDA_TOKEN_RIGHT_BRACE
        || current == RUDA_TOKEN_EOF;
}

static RudaCompletedMarker ruda_parse_pattern_inner(
    RudaParser *parser,
    RudaPatternStops stops
);

static void ruda_parse_pattern_list(
    RudaParser *parser,
    RudaTokenKind close
) {
    RudaMarker list = ruda_parser_start(parser);
    RudaPatternStops stops;
    stops.first = RUDA_TOKEN_COMMA;
    stops.second = close;
    while (!ruda_parser_at(parser, close)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        (void)ruda_parse_pattern_inner(parser, stops);
        if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
            break;
        }
        if (parser->raw_index == before) {
            break;
        }
    }
    (void)ruda_parser_complete(parser, list, RUDA_SYNTAX_PATTERN_LIST);
}

static RudaCompletedMarker ruda_parse_pattern_payload(
    RudaParser *parser,
    RudaTokenKind open,
    RudaTokenKind close,
    RudaSyntaxKind kind
) {
    RudaMarker marker = ruda_parser_start(parser);
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(parser, open, "expected pattern payload delimiter");
    ruda_parse_pattern_list(parser, close);
    ruda_parser_expect_closing_delimiter(
        parser,
        close,
        open_offset,
        "expected closing pattern delimiter"
    );
    return ruda_parser_complete(parser, marker, kind);
}

static RudaCompletedMarker ruda_parse_named_pattern(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    RudaMarker fields;
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_BRACE, "expected '{' in pattern");
    fields = ruda_parser_start(parser);
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        RudaMarker field = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected named pattern field");
        if (ruda_parser_eat(parser, RUDA_TOKEN_COLON)) {
            RudaPatternStops stops;
            stops.first = RUDA_TOKEN_COMMA;
            stops.second = RUDA_TOKEN_RIGHT_BRACE;
            (void)ruda_parse_pattern_inner(parser, stops);
        }
        (void)ruda_parser_complete(parser, field, RUDA_SYNTAX_PATTERN_FIELD);
        if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
            break;
        }
        if (parser->raw_index == before) {
            break;
        }
    }
    (void)ruda_parser_complete(parser, fields, RUDA_SYNTAX_PATTERN_FIELDS);
    ruda_parser_expect_closing_delimiter(
        parser,
        RUDA_TOKEN_RIGHT_BRACE,
        open_offset,
        "expected '}' after named pattern"
    );
    return ruda_parser_complete(parser, marker, RUDA_SYNTAX_NAMED_PATTERN);
}

static RudaCompletedMarker ruda_parse_pattern_primary(
    RudaParser *parser,
    RudaPatternStops stops
) {
    RudaTokenKind current = ruda_parser_current(parser);
    RudaMarker marker;
    RudaCompletedMarker completed;
    if (ruda_pattern_at_stop(parser, stops)) {
        marker = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected match pattern");
        return ruda_parser_complete(parser, marker, RUDA_SYNTAX_ERROR);
    }
    if (current == RUDA_TOKEN_UNDERSCORE) {
        marker = ruda_parser_start(parser);
        ruda_parser_bump(parser);
        return ruda_parser_complete(
            parser,
            marker,
            RUDA_SYNTAX_WILDCARD_PATTERN
        );
    }
    if (ruda_expression_is_literal(current)) {
        marker = ruda_parser_start(parser);
        ruda_parser_bump(parser);
        return ruda_parser_complete(
            parser,
            marker,
            RUDA_SYNTAX_LITERAL_PATTERN
        );
    }
    if (ruda_parser_at_identifier(parser)) {
        marker = ruda_parser_start(parser);
        ruda_parser_bump_as(parser, RUDA_TOKEN_IDENTIFIER);
        while (ruda_parser_eat(parser, RUDA_TOKEN_DOT)) {
            ruda_parser_expect_identifier(parser, "expected pattern name after '.'");
        }
        completed = ruda_parser_complete(
            parser,
            marker,
            RUDA_SYNTAX_NAME_PATTERN
        );
        if (ruda_parser_at(parser, RUDA_TOKEN_LEFT_PAREN)) {
            RudaMarker constructor = ruda_parser_precede(parser, completed);
            (void)ruda_parse_pattern_payload(
                parser,
                RUDA_TOKEN_LEFT_PAREN,
                RUDA_TOKEN_RIGHT_PAREN,
                RUDA_SYNTAX_POSITIONAL_PATTERN
            );
            return ruda_parser_complete(
                parser,
                constructor,
                RUDA_SYNTAX_CONSTRUCTOR_PATTERN
            );
        }
        if (ruda_parser_at(parser, RUDA_TOKEN_LEFT_BRACE)) {
            RudaMarker constructor = ruda_parser_precede(parser, completed);
            (void)ruda_parse_named_pattern(parser);
            return ruda_parser_complete(
                parser,
                constructor,
                RUDA_SYNTAX_CONSTRUCTOR_PATTERN
            );
        }
        return completed;
    }
    if (current == RUDA_TOKEN_LEFT_BRACKET) {
        return ruda_parse_pattern_payload(
            parser,
            RUDA_TOKEN_LEFT_BRACKET,
            RUDA_TOKEN_RIGHT_BRACKET,
            RUDA_SYNTAX_TUPLE_PATTERN
        );
    }

    marker = ruda_parser_start(parser);
    {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "expected match pattern",
            offset,
            offset
        );
    }
    if (!ruda_pattern_at_stop(parser, stops)) {
        ruda_parser_bump(parser);
    }
    return ruda_parser_complete(parser, marker, RUDA_SYNTAX_ERROR);
}

static RudaCompletedMarker ruda_parse_pattern_inner(
    RudaParser *parser,
    RudaPatternStops stops
) {
    RudaCompletedMarker left = ruda_parse_pattern_primary(parser, stops);
    while (left.valid && ruda_parser_at(parser, RUDA_TOKEN_PIPE)
        && !parser->allocation_failed && !parser->limit_reached) {
        RudaMarker parent = ruda_parser_precede(parser, left);
        ruda_parser_bump(parser);
        (void)ruda_parse_pattern_primary(parser, stops);
        left = ruda_parser_complete(parser, parent, RUDA_SYNTAX_OR_PATTERN);
    }
    return left;
}

static void ruda_parse_pattern_until(
    RudaParser *parser,
    RudaTokenKind first,
    RudaTokenKind second
) {
    RudaPatternStops stops;
    RudaMarker marker = ruda_parser_start(parser);
    stops.first = first;
    stops.second = second;
    (void)ruda_parse_pattern_inner(parser, stops);
    if (!ruda_pattern_at_stop(parser, stops)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "unexpected token after match pattern",
            offset,
            offset
        );
        while (!ruda_pattern_at_stop(parser, stops)
            && !parser->allocation_failed && !parser->limit_reached) {
            ruda_parser_bump(parser);
        }
    }
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_PATTERN);
}

static RudaCompletedMarker ruda_parse_match_expression(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_KW_MATCH, "expected 'match'");
    ruda_parse_expression_until(
        parser,
        RUDA_TOKEN_LEFT_BRACE,
        RUDA_TOKEN_EOF,
        RUDA_TOKEN_EOF
    );
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_BRACE, "expected '{' after match value");
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        RudaMarker arm = ruda_parser_start(parser);
        ruda_parser_expect(parser, RUDA_TOKEN_KW_CASE, "expected 'case'");
        ruda_parse_pattern_until(parser, RUDA_TOKEN_KW_IF, RUDA_TOKEN_FAT_ARROW);
        if (ruda_parser_eat(parser, RUDA_TOKEN_KW_IF)) {
            ruda_parse_expression_until(
                parser,
                RUDA_TOKEN_FAT_ARROW,
                RUDA_TOKEN_EOF,
                RUDA_TOKEN_EOF
            );
        }
        ruda_parser_expect(parser, RUDA_TOKEN_FAT_ARROW, "expected '=>' after match pattern");
        if (ruda_parser_at(parser, RUDA_TOKEN_LEFT_BRACE)) {
            ruda_parse_block(parser);
        } else {
            ruda_parse_expression_until(
                parser,
                RUDA_TOKEN_SEMICOLON,
                RUDA_TOKEN_RIGHT_BRACE,
                RUDA_TOKEN_EOF
            );
            ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after match arm");
        }
        (void)ruda_parser_complete(parser, arm, RUDA_SYNTAX_MATCH_ARM);
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            ruda_parser_bump(parser);
        }
    }
    ruda_parser_expect(parser, RUDA_TOKEN_RIGHT_BRACE, "expected '}' after match");
    return ruda_parser_complete(parser, marker, RUDA_SYNTAX_MATCH_EXPRESSION);
}

static RudaCompletedMarker ruda_parse_closure_expression(
    RudaParser *parser,
    RudaExpressionStops stops
) {
    RudaMarker marker = ruda_parser_start(parser);
    ruda_parse_parameters(parser);
    if (ruda_parser_eat(parser, RUDA_TOKEN_COLON)) {
        ruda_parse_type_until(
            parser,
            RUDA_TOKEN_FAT_ARROW,
            RUDA_TOKEN_EOF,
            RUDA_TOKEN_EOF
        );
    }
    ruda_parser_expect(parser, RUDA_TOKEN_FAT_ARROW, "expected '=>' in closure");
    if (ruda_parser_at(parser, RUDA_TOKEN_LEFT_BRACE)) {
        ruda_parse_block(parser);
    } else {
        (void)ruda_parse_expression_bp(parser, stops, 10);
    }
    return ruda_parser_complete(parser, marker, RUDA_SYNTAX_CLOSURE_EXPRESSION);
}

static RudaCompletedMarker ruda_parse_expression_prefix(
    RudaParser *parser,
    RudaExpressionStops stops
) {
    RudaTokenKind current = ruda_parser_current(parser);
    RudaMarker marker;
    if (current == RUDA_TOKEN_KW_MATCH) {
        return ruda_parse_match_expression(parser);
    }
    if (ruda_parser_at_identifier(parser)) {
        marker = ruda_parser_start(parser);
        ruda_parser_bump_as(parser, RUDA_TOKEN_IDENTIFIER);
        return ruda_parser_complete(parser, marker, RUDA_SYNTAX_NAME_EXPRESSION);
    }
    if (ruda_expression_is_literal(current)) {
        marker = ruda_parser_start(parser);
        ruda_parser_bump(parser);
        return ruda_parser_complete(parser, marker, RUDA_SYNTAX_LITERAL_EXPRESSION);
    }
    if (current == RUDA_TOKEN_INTERPOLATED_START) {
        return ruda_parse_interpolated_expression(parser);
    }
    if (current == RUDA_TOKEN_LEFT_PAREN) {
        if (ruda_parser_paren_is_closure(parser)) {
            return ruda_parse_closure_expression(parser, stops);
        }
        return ruda_parse_parenthesized_expression(parser);
    }
    if (current == RUDA_TOKEN_LEFT_BRACKET) {
        return ruda_parse_array_expression(parser);
    }
    if (current == RUDA_TOKEN_BANG || current == RUDA_TOKEN_MINUS
        || current == RUDA_TOKEN_TILDE) {
        marker = ruda_parser_start(parser);
        ruda_parser_bump(parser);
        (void)ruda_parse_expression_bp(parser, stops, 90);
        return ruda_parser_complete(
            parser,
            marker,
            RUDA_SYNTAX_UNARY_EXPRESSION
        );
    }
    if (current == RUDA_TOKEN_KW_SPAWN) {
        size_t offset = ruda_parser_current_offset(parser);
        RudaCompletedMarker operand;
        marker = ruda_parser_start(parser);
        ruda_parser_bump(parser);
        operand = ruda_parse_expression_bp(parser, stops, 90);
        if (!ruda_parser_completed_contains_kind(
                parser,
                operand,
                RUDA_SYNTAX_CALL_EXPRESSION
            )) {
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "spawn requires a call expression",
                offset,
                offset
            );
        }
        return ruda_parser_complete(
            parser,
            marker,
            RUDA_SYNTAX_SPAWN_EXPRESSION
        );
    }
    marker = ruda_parser_start(parser);
    {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "expected expression",
            offset,
            offset
        );
    }
    if (!ruda_expression_at_stop(parser, stops)) {
        ruda_parser_bump(parser);
    }
    return ruda_parser_complete(parser, marker, RUDA_SYNTAX_ERROR);
}

static RudaCompletedMarker ruda_parse_expression_bp(
    RudaParser *parser,
    RudaExpressionStops stops,
    unsigned int minimum_binding_power
) {
    RudaCompletedMarker left;
    bool left_is_assignment = false;
    bool left_is_comparison = false;
    bool record_eligible = false;
    left.position = 0;
    left.valid = false;
    if (parser->allocation_failed || parser->limit_reached) {
        return left;
    }
    left = ruda_parse_expression_prefix(parser, stops);
    record_eligible = ruda_parser_completed_kind(parser, left)
        == RUDA_SYNTAX_NAME_EXPRESSION
        && ruda_parser_completed_contains_token_kind(
            parser,
            left,
            RUDA_TOKEN_IDENTIFIER
        );
    while (left.valid && !ruda_expression_at_stop(parser, stops)
        && !parser->allocation_failed && !parser->limit_reached) {
        RudaTokenKind current = ruda_parser_current(parser);
        RudaMarker parent;
        unsigned int left_power = 0;
        unsigned int right_power = 0;
        RudaSyntaxKind syntax_kind = RUDA_SYNTAX_ERROR;
        if (current == RUDA_TOKEN_DOT) {
            parent = ruda_parser_precede(parser, left);
            ruda_parser_bump(parser);
            ruda_parser_expect_identifier(parser, "expected member name after '.'");
            left = ruda_parser_complete(parser, parent, RUDA_SYNTAX_MEMBER_EXPRESSION);
            continue;
        }
        if (current == RUDA_TOKEN_LEFT_PAREN) {
            parent = ruda_parser_precede(parser, left);
            ruda_parse_arguments(parser);
            left = ruda_parser_complete(parser, parent, RUDA_SYNTAX_CALL_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == RUDA_TOKEN_LESS && ruda_parser_generic_call_ahead(parser)) {
            parent = ruda_parser_precede(parser, left);
            ruda_parse_type_arguments(parser);
            ruda_parse_arguments(parser);
            left = ruda_parser_complete(parser, parent, RUDA_SYNTAX_CALL_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == RUDA_TOKEN_LEFT_BRACKET) {
            parent = ruda_parser_precede(parser, left);
            ruda_parse_index_group(parser);
            left = ruda_parser_complete(parser, parent, RUDA_SYNTAX_INDEX_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == RUDA_TOKEN_QUESTION) {
            parent = ruda_parser_precede(parser, left);
            ruda_parser_bump(parser);
            left = ruda_parser_complete(parser, parent, RUDA_SYNTAX_TRY_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == RUDA_TOKEN_LEFT_BRACE) {
            if (!record_eligible) {
                break;
            }
            parent = ruda_parser_precede(parser, left);
            ruda_parse_record_fields(parser);
            left = ruda_parser_complete(parser, parent, RUDA_SYNTAX_RECORD_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (!ruda_expression_infix_binding_power(
                current,
                &left_power,
                &right_power,
                &syntax_kind
            ) || left_power < minimum_binding_power) {
            break;
        }
        if ((syntax_kind == RUDA_SYNTAX_ASSIGNMENT_EXPRESSION
                && left_is_assignment)
            || (syntax_kind == RUDA_SYNTAX_BINARY_EXPRESSION
                && left_power == 50 && left_is_comparison)) {
            size_t offset = ruda_parser_current_offset(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                syntax_kind == RUDA_SYNTAX_ASSIGNMENT_EXPRESSION
                    ? "chained assignment is not supported"
                    : "comparison chaining is not supported",
                offset,
                offset
            );
        }
        parent = ruda_parser_precede(parser, left);
        ruda_parser_bump(parser);
        (void)ruda_parse_expression_bp(parser, stops, right_power);
        left = ruda_parser_complete(parser, parent, syntax_kind);
        left_is_assignment = syntax_kind == RUDA_SYNTAX_ASSIGNMENT_EXPRESSION;
        left_is_comparison = syntax_kind == RUDA_SYNTAX_BINARY_EXPRESSION
            && left_power == 50;
        record_eligible = false;
    }
    return left;
}

static void ruda_parse_expression_until(
    RudaParser *parser,
    RudaTokenKind first,
    RudaTokenKind second,
    RudaTokenKind third
) {
    RudaExpressionStops stops;
    RudaMarker marker = ruda_parser_start(parser);
    stops.first = first;
    stops.second = second;
    stops.third = third;
    (void)ruda_parse_expression_bp(parser, stops, 0);
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_EXPRESSION);
}

static bool ruda_parser_type_at_stop(
    const RudaParser *parser,
    RudaTokenKind first,
    RudaTokenKind second,
    RudaTokenKind third
) {
    RudaTokenKind current = ruda_parser_current(parser);
    bool closes_type_arguments = first == RUDA_TOKEN_GREATER
        || first == RUDA_TOKEN_SHIFT_RIGHT
        || second == RUDA_TOKEN_GREATER
        || second == RUDA_TOKEN_SHIFT_RIGHT
        || third == RUDA_TOKEN_GREATER
        || third == RUDA_TOKEN_SHIFT_RIGHT;
    bool hard_boundary = current == RUDA_TOKEN_COMMA
        || current == RUDA_TOKEN_SEMICOLON
        || current == RUDA_TOKEN_EQUAL
        || current == RUDA_TOKEN_LEFT_BRACE
        || current == RUDA_TOKEN_RIGHT_PAREN
        || current == RUDA_TOKEN_RIGHT_BRACKET
        || current == RUDA_TOKEN_RIGHT_BRACE
        || current == RUDA_TOKEN_FAT_ARROW
        || current == RUDA_TOKEN_AMPERSAND;
    return (parser->pending_type_greater != 0 && closes_type_arguments)
        || hard_boundary
        || ruda_parser_is_stop(parser, first, second, third);
}

static void ruda_parse_type_list(
    RudaParser *parser,
    RudaTokenKind close,
    bool allow_empty
) {
    if (ruda_parser_at(parser, close)) {
        if (!allow_empty) {
            ruda_parse_type_until(
                parser,
                RUDA_TOKEN_COMMA,
                close,
                RUDA_TOKEN_EOF
            );
        }
        return;
    }
    while (!ruda_parser_at(parser, close)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        ruda_parse_type_until(
            parser,
            RUDA_TOKEN_COMMA,
            close,
            RUDA_TOKEN_EOF
        );
        if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
            break;
        }
    }
}

static void ruda_parse_type_until(
    RudaParser *parser,
    RudaTokenKind first,
    RudaTokenKind second,
    RudaTokenKind third
) {
    RudaMarker marker = ruda_parser_start(parser);
    RudaTokenKind current = ruda_parser_current(parser);
    if (ruda_parser_type_at_stop(parser, first, second, third)) {
        ruda_parser_expect_identifier(parser, "expected type");
    } else if (ruda_parser_at_identifier(parser)) {
        ruda_parser_bump_as(parser, RUDA_TOKEN_IDENTIFIER);
        while (ruda_parser_eat(parser, RUDA_TOKEN_DOT)) {
            ruda_parser_expect_identifier(parser, "expected type name after '.'");
        }
        if (ruda_parser_at(parser, RUDA_TOKEN_LESS)) {
            ruda_parse_type_arguments(parser);
        }
    } else if (current == RUDA_TOKEN_LEFT_BRACKET) {
        size_t open_offset = ruda_parser_current_offset(parser);
        ruda_parser_bump(parser);
        ruda_parse_type_list(parser, RUDA_TOKEN_RIGHT_BRACKET, false);
        ruda_parser_expect_closing_delimiter(
            parser,
            RUDA_TOKEN_RIGHT_BRACKET,
            open_offset,
            "expected ']' after tuple type"
        );
    } else if (current == RUDA_TOKEN_LEFT_PAREN) {
        size_t open_offset = ruda_parser_current_offset(parser);
        ruda_parser_bump(parser);
        ruda_parse_type_list(parser, RUDA_TOKEN_RIGHT_PAREN, true);
        ruda_parser_expect_closing_delimiter(
            parser,
            RUDA_TOKEN_RIGHT_PAREN,
            open_offset,
            "expected ')' after function parameter types"
        );
        ruda_parser_expect(
            parser,
            RUDA_TOKEN_FAT_ARROW,
            "expected '=>' in function type"
        );
        ruda_parse_type_until(parser, first, second, third);
    } else {
        ruda_parser_expect_identifier(parser, "expected type");
        if (!ruda_parser_type_at_stop(parser, first, second, third)
            && !ruda_parser_at(parser, RUDA_TOKEN_EOF)) {
            ruda_parser_bump(parser);
        }
    }
    if (!ruda_parser_type_at_stop(parser, first, second, third)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "unexpected token after type",
            offset,
            offset
        );
        while (!ruda_parser_type_at_stop(parser, first, second, third)
            && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
            && !parser->allocation_failed && !parser->limit_reached) {
            ruda_parser_bump(parser);
        }
    }
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_TYPE);
}

static void ruda_parse_type_parameters(RudaParser *parser) {
    RudaMarker list = ruda_parser_start(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LESS, "expected '<' for type parameters");
    ruda_parser_begin_type_angle(parser);
    if (ruda_parser_type_at_greater(parser)) {
        RudaMarker parameter = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected type parameter name");
        (void)ruda_parser_complete(
            parser,
            parameter,
            RUDA_SYNTAX_TYPE_PARAMETER
        );
    }
    while (!ruda_parser_type_at_greater(parser)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        RudaMarker parameter = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected type parameter name");
        if (ruda_parser_eat(parser, RUDA_TOKEN_COLON)) {
            do {
                ruda_parse_type_until(
                    parser,
                    RUDA_TOKEN_AMPERSAND,
                    RUDA_TOKEN_COMMA,
                    RUDA_TOKEN_GREATER
                );
            } while (ruda_parser_eat(parser, RUDA_TOKEN_AMPERSAND));
        }
        ruda_parser_complete(parser, parameter, RUDA_SYNTAX_TYPE_PARAMETER);
        if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
            break;
        }
    }
    ruda_parser_expect_type_greater(parser);
    ruda_parser_end_type_angle(parser);
    ruda_parser_complete(parser, list, RUDA_SYNTAX_TYPE_PARAMETERS);
}

static void ruda_parse_parameters(RudaParser *parser) {
    RudaMarker list = ruda_parser_start(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_PAREN, "expected parameter list");
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_PAREN)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        RudaMarker parameter = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected parameter name");
        ruda_parser_expect(parser, RUDA_TOKEN_COLON, "expected ':' after parameter name");
        ruda_parse_type_until(
            parser,
            RUDA_TOKEN_COMMA,
            RUDA_TOKEN_RIGHT_PAREN,
            RUDA_TOKEN_EOF
        );
        ruda_parser_complete(parser, parameter, RUDA_SYNTAX_PARAMETER);
        if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
            break;
        }
    }
    ruda_parser_expect(parser, RUDA_TOKEN_RIGHT_PAREN, "expected ')' after parameters");
    ruda_parser_complete(parser, list, RUDA_SYNTAX_PARAMETERS);
}

static void ruda_parse_binding_decl(
    RudaParser *parser,
    RudaMarker marker,
    RudaSyntaxKind kind
) {
    ruda_parser_bump(parser);
    if (ruda_parser_at(parser, RUDA_TOKEN_UNDERSCORE)) {
        ruda_parser_bump(parser);
    } else {
        ruda_parser_expect_identifier(parser, "expected binding name");
    }
    if (ruda_parser_eat(parser, RUDA_TOKEN_COLON)) {
        ruda_parse_type_until(
            parser,
            RUDA_TOKEN_EQUAL,
            RUDA_TOKEN_SEMICOLON,
            RUDA_TOKEN_EOF
        );
    }
    if (ruda_parser_eat(parser, RUDA_TOKEN_EQUAL)) {
        ruda_parse_expression_until(
            parser,
            RUDA_TOKEN_SEMICOLON,
            RUDA_TOKEN_EOF,
            RUDA_TOKEN_EOF
        );
    } else if (kind == RUDA_SYNTAX_CONST_DECL) {
        ruda_parser_expect(
            parser,
            RUDA_TOKEN_EQUAL,
            "expected '=' in const declaration"
        );
        ruda_parse_expression_until(
            parser,
            RUDA_TOKEN_SEMICOLON,
            RUDA_TOKEN_EOF,
            RUDA_TOKEN_EOF
        );
    }
    ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after binding");
    ruda_parser_complete(parser, marker, kind);
}

static void ruda_parse_block(RudaParser *parser);

static void ruda_parse_for_binding(RudaParser *parser) {
    if (ruda_parser_at_identifier(parser)) {
        ruda_parser_bump_as(parser, RUDA_TOKEN_IDENTIFIER);
        return;
    }
    if (ruda_parser_at(parser, RUDA_TOKEN_UNDERSCORE)) {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            "loop binding must be an identifier",
            offset,
            offset
        );
        ruda_parser_bump(parser);
        return;
    }
    ruda_parser_expect_identifier(parser, "expected loop binding name");
}

static void ruda_parse_for_of_tail(RudaParser *parser) {
    ruda_parse_expression_until(
        parser,
        RUDA_TOKEN_LEFT_BRACE,
        RUDA_TOKEN_EOF,
        RUDA_TOKEN_EOF
    );
}

static void ruda_parse_c_for_tail(RudaParser *parser) {
    ruda_parse_expression_until(
        parser,
        RUDA_TOKEN_SEMICOLON,
        RUDA_TOKEN_EOF,
        RUDA_TOKEN_EOF
    );
    ruda_parser_expect(
        parser,
        RUDA_TOKEN_SEMICOLON,
        "expected ';' after for initializer"
    );
    ruda_parse_expression_until(
        parser,
        RUDA_TOKEN_SEMICOLON,
        RUDA_TOKEN_EOF,
        RUDA_TOKEN_EOF
    );
    ruda_parser_expect(
        parser,
        RUDA_TOKEN_SEMICOLON,
        "expected ';' after for condition"
    );
    ruda_parse_expression_until(
        parser,
        RUDA_TOKEN_LEFT_BRACE,
        RUDA_TOKEN_EOF,
        RUDA_TOKEN_EOF
    );
}

static void ruda_parse_for_header(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    if (ruda_parser_at(parser, RUDA_TOKEN_KW_CONST)) {
        ruda_parser_bump(parser);
        ruda_parse_for_binding(parser);
        if (ruda_parser_eat(parser, RUDA_TOKEN_KW_OF)) {
            ruda_parse_for_of_tail(parser);
        } else if (ruda_parser_at(parser, RUDA_TOKEN_EQUAL)) {
            size_t offset = ruda_parser_current_offset(parser);
            ruda_parser_bump(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "C-style for initializer requires 'let', not 'const'",
                offset,
                offset
            );
            ruda_parse_c_for_tail(parser);
        } else {
            ruda_parser_expect(
                parser,
                RUDA_TOKEN_KW_OF,
                "expected 'of' in for-of loop"
            );
            ruda_parse_for_of_tail(parser);
        }
    } else if (ruda_parser_at(parser, RUDA_TOKEN_KW_LET)) {
        ruda_parser_bump(parser);
        ruda_parse_for_binding(parser);
        if (ruda_parser_at(parser, RUDA_TOKEN_KW_OF)) {
            size_t offset = ruda_parser_current_offset(parser);
            ruda_parser_bump(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "for-of binding requires 'const', not 'let'",
                offset,
                offset
            );
            ruda_parse_for_of_tail(parser);
        } else {
            ruda_parser_expect(parser, RUDA_TOKEN_EQUAL, "expected '=' in for initializer");
            ruda_parse_c_for_tail(parser);
        }
    } else {
        size_t offset = ruda_parser_current_offset(parser);
        RudaMarker error = ruda_parser_start(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "expected 'const' for for-of or 'let' for C-style for",
            offset,
            offset
        );
        while (!ruda_parser_at(parser, RUDA_TOKEN_LEFT_BRACE)
            && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
            && !parser->allocation_failed && !parser->limit_reached) {
            ruda_parser_bump(parser);
        }
        (void)ruda_parser_complete(parser, error, RUDA_SYNTAX_ERROR);
    }
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_FOR_HEADER);
}

static void ruda_parse_statement(RudaParser *parser) {
    RudaTokenKind current = ruda_parser_current(parser);
    RudaMarker marker = ruda_parser_start(parser);
    if (current == RUDA_TOKEN_KW_CONST
        && ruda_parser_contextual_statement_starts(parser, current)) {
        ruda_parse_binding_decl(parser, marker, RUDA_SYNTAX_CONST_DECL);
    } else if (current == RUDA_TOKEN_KW_LET
        && ruda_parser_contextual_statement_starts(parser, current)) {
        ruda_parse_binding_decl(parser, marker, RUDA_SYNTAX_LET_DECL);
    } else if (current == RUDA_TOKEN_KW_RETURN) {
        ruda_parser_bump(parser);
        if (!ruda_parser_at(parser, RUDA_TOKEN_SEMICOLON)) {
            ruda_parse_expression_until(
                parser,
                RUDA_TOKEN_SEMICOLON,
                RUDA_TOKEN_EOF,
                RUDA_TOKEN_EOF
            );
        }
        ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after return");
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_RETURN_STATEMENT);
    } else if (current == RUDA_TOKEN_KW_DEFER) {
        ruda_parser_bump(parser);
        ruda_parse_expression_until(
            parser,
            RUDA_TOKEN_SEMICOLON,
            RUDA_TOKEN_EOF,
            RUDA_TOKEN_EOF
        );
        ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after defer");
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_DEFER_STATEMENT);
    } else if (current == RUDA_TOKEN_KW_IF || current == RUDA_TOKEN_KW_WHILE
        || (current == RUDA_TOKEN_KW_FOR
            && ruda_parser_contextual_statement_starts(parser, current))) {
        RudaSyntaxKind statement_kind = current == RUDA_TOKEN_KW_IF
            ? RUDA_SYNTAX_IF_STATEMENT
            : (current == RUDA_TOKEN_KW_WHILE
                ? RUDA_SYNTAX_WHILE_STATEMENT
                : RUDA_SYNTAX_FOR_STATEMENT);
        ruda_parser_bump(parser);
        if (current == RUDA_TOKEN_KW_FOR) {
            ruda_parse_for_header(parser);
        } else {
            ruda_parse_expression_until(
                parser,
                RUDA_TOKEN_LEFT_BRACE,
                RUDA_TOKEN_EOF,
                RUDA_TOKEN_EOF
            );
        }
        ruda_parse_block(parser);
        if (current == RUDA_TOKEN_KW_IF
            && ruda_parser_at(parser, RUDA_TOKEN_KW_ELSE)
            && (ruda_parser_next_significant_kind(parser) == RUDA_TOKEN_KW_IF
                || ruda_parser_next_significant_kind(parser)
                    == RUDA_TOKEN_LEFT_BRACE)) {
            ruda_parser_bump(parser);
            if (ruda_parser_at(parser, RUDA_TOKEN_KW_IF)) {
                ruda_parse_statement(parser);
            } else {
                ruda_parse_block(parser);
            }
        }
        ruda_parser_complete(parser, marker, statement_kind);
    } else if (current == RUDA_TOKEN_KW_SCOPE
        && ruda_parser_contextual_statement_starts(parser, current)) {
        ruda_parser_bump(parser);
        ruda_parse_block(parser);
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_SCOPE_STATEMENT);
    } else if (current == RUDA_TOKEN_KW_UNSAFE
        && ruda_parser_contextual_statement_starts(parser, current)) {
        ruda_parser_bump(parser);
        ruda_parse_block(parser);
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_UNSAFE_STATEMENT);
    } else if (current == RUDA_TOKEN_LEFT_BRACE) {
        ruda_parse_block(parser);
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_BLOCK_STATEMENT);
    } else if ((current == RUDA_TOKEN_KW_BREAK
            || current == RUDA_TOKEN_KW_CONTINUE)
        && ruda_parser_contextual_statement_starts(parser, current)) {
        ruda_parser_bump(parser);
        ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after loop control");
        ruda_parser_complete(
            parser,
            marker,
            current == RUDA_TOKEN_KW_BREAK
                ? RUDA_SYNTAX_BREAK_STATEMENT
                : RUDA_SYNTAX_CONTINUE_STATEMENT
        );
    } else if (current == RUDA_TOKEN_KW_YIELD
        && !ruda_parser_yield_is_name_expression(parser)) {
        ruda_parser_bump(parser);
        ruda_parse_expression_until(
            parser,
            RUDA_TOKEN_SEMICOLON,
            RUDA_TOKEN_EOF,
            RUDA_TOKEN_EOF
        );
        ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after yield");
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_YIELD_STATEMENT);
    } else {
        ruda_parse_expression_until(
            parser,
            RUDA_TOKEN_SEMICOLON,
            RUDA_TOKEN_RIGHT_BRACE,
            RUDA_TOKEN_EOF
        );
        ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after expression");
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_EXPRESSION_STATEMENT);
    }
}

static void ruda_parse_block(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_BRACE, "expected '{' to start block");
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        ruda_parse_statement(parser);
        if (parser->raw_index == before && !parser->limit_reached
            && !parser->allocation_failed) {
            RudaMarker error = ruda_parser_start(parser);
            size_t offset = ruda_parser_current_offset(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "unexpected token in block",
                offset,
                offset
            );
            ruda_parser_bump(parser);
            ruda_parser_complete(parser, error, RUDA_SYNTAX_ERROR);
        }
    }
    ruda_parser_expect(parser, RUDA_TOKEN_RIGHT_BRACE, "expected '}' to close block");
    ruda_parser_complete(parser, marker, RUDA_SYNTAX_BLOCK);
}

static void ruda_parse_attribute(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_AT, "expected '@'");
    ruda_parser_expect_identifier(parser, "expected attribute name");
    if (ruda_parser_at(parser, RUDA_TOKEN_LEFT_PAREN)) {
        ruda_parse_arguments(parser);
    }
    ruda_parser_complete(parser, marker, RUDA_SYNTAX_ATTRIBUTE);
}

static void ruda_parse_import_decl(RudaParser *parser, RudaMarker marker) {
    RudaMarker clause;
    ruda_parser_expect(parser, RUDA_TOKEN_KW_IMPORT, "expected 'import'");
    clause = ruda_parser_start(parser);
    if (ruda_parser_eat(parser, RUDA_TOKEN_STAR)) {
        ruda_parser_expect(parser, RUDA_TOKEN_KW_AS, "expected 'as' after '*'");
        ruda_parser_expect_identifier(parser, "expected import namespace");
    } else if (ruda_parser_eat(parser, RUDA_TOKEN_LEFT_BRACE)) {
        while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
            && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
            && !parser->allocation_failed && !parser->limit_reached) {
            RudaMarker item = ruda_parser_start(parser);
            ruda_parser_expect_identifier(parser, "expected imported name");
            if (ruda_parser_eat(parser, RUDA_TOKEN_KW_AS)) {
                ruda_parser_expect_identifier(parser, "expected import alias");
            }
            ruda_parser_complete(parser, item, RUDA_SYNTAX_IMPORT_ITEM);
            if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
                break;
            }
        }
        ruda_parser_expect(parser, RUDA_TOKEN_RIGHT_BRACE, "expected '}' after imports");
    } else {
        size_t offset = ruda_parser_current_offset(parser);
        (void)ruda_parser_add_diagnostic(
            parser,
            RUDA_DIAGNOSTIC_EXPECTED_TOKEN,
            "expected named or namespace import clause",
            offset,
            offset
        );
    }
    ruda_parser_complete(parser, clause, RUDA_SYNTAX_IMPORT_CLAUSE);
    ruda_parser_expect(parser, RUDA_TOKEN_KW_FROM, "expected 'from' in import");
    ruda_parser_expect(parser, RUDA_TOKEN_STRING, "expected import path string");
    ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after import");
    ruda_parser_complete(parser, marker, RUDA_SYNTAX_IMPORT_DECL);
}

static void ruda_parse_field_decl(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    if (ruda_parser_at(parser, RUDA_TOKEN_KW_PUBLIC)
        && ruda_parser_next_significant_kind(parser) != RUDA_TOKEN_COLON) {
        ruda_parser_bump(parser);
    }
    if (ruda_parser_at(parser, RUDA_TOKEN_KW_MUTABLE)
        && ruda_parser_next_significant_kind(parser) != RUDA_TOKEN_COLON) {
        ruda_parser_bump(parser);
    }
    ruda_parser_expect_identifier(parser, "expected field name");
    ruda_parser_expect(parser, RUDA_TOKEN_COLON, "expected ':' after field name");
    ruda_parse_type_until(
        parser,
        RUDA_TOKEN_SEMICOLON,
        RUDA_TOKEN_RIGHT_BRACE,
        RUDA_TOKEN_EOF
    );
    ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after field");
    ruda_parser_complete(parser, marker, RUDA_SYNTAX_FIELD_DECL);
}

static void ruda_parse_positional_payload(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(
        parser,
        RUDA_TOKEN_LEFT_PAREN,
        "expected '(' for positional payload"
    );
    ruda_parse_type_list(parser, RUDA_TOKEN_RIGHT_PAREN, false);
    ruda_parser_expect_closing_delimiter(
        parser,
        RUDA_TOKEN_RIGHT_PAREN,
        open_offset,
        "expected ')' after positional payload"
    );
    (void)ruda_parser_complete(
        parser,
        marker,
        RUDA_SYNTAX_POSITIONAL_PAYLOAD
    );
}

static void ruda_parse_named_payload(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    size_t open_offset = ruda_parser_current_offset(parser);
    ruda_parser_expect(
        parser,
        RUDA_TOKEN_LEFT_BRACE,
        "expected '{' for named payload"
    );
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        RudaMarker field = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected payload field name");
        ruda_parser_expect(
            parser,
            RUDA_TOKEN_COLON,
            "expected ':' after payload field name"
        );
        ruda_parse_type_until(
            parser,
            RUDA_TOKEN_SEMICOLON,
            RUDA_TOKEN_RIGHT_BRACE,
            RUDA_TOKEN_EOF
        );
        ruda_parser_expect(
            parser,
            RUDA_TOKEN_SEMICOLON,
            "expected ';' after payload field"
        );
        (void)ruda_parser_complete(
            parser,
            field,
            RUDA_SYNTAX_PAYLOAD_FIELD
        );
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            ruda_parser_bump(parser);
        }
    }
    ruda_parser_expect_closing_delimiter(
        parser,
        RUDA_TOKEN_RIGHT_BRACE,
        open_offset,
        "expected '}' after named payload"
    );
    (void)ruda_parser_complete(parser, marker, RUDA_SYNTAX_NAMED_PAYLOAD);
}

static void ruda_parse_type_decl(RudaParser *parser, RudaMarker marker) {
    ruda_parser_expect(parser, RUDA_TOKEN_KW_TYPE, "expected 'type'");
    ruda_parser_expect_identifier(parser, "expected type name");
    if (ruda_parser_at(parser, RUDA_TOKEN_LESS)) {
        ruda_parse_type_parameters(parser);
    }
    if (ruda_parser_eat(parser, RUDA_TOKEN_EQUAL)) {
        ruda_parse_type_until(
            parser,
            RUDA_TOKEN_SEMICOLON,
            RUDA_TOKEN_EOF,
            RUDA_TOKEN_EOF
        );
        ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after type alias");
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_TYPE_ALIAS_DECL);
        return;
    }
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_BRACE, "expected record body");
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        ruda_parse_field_decl(parser);
    }
    ruda_parser_expect(parser, RUDA_TOKEN_RIGHT_BRACE, "expected '}' after record");
    ruda_parser_complete(parser, marker, RUDA_SYNTAX_RECORD_DECL);
}

static void ruda_parse_enum_decl(RudaParser *parser, RudaMarker marker) {
    ruda_parser_expect(parser, RUDA_TOKEN_KW_ENUM, "expected 'enum'");
    ruda_parser_expect_identifier(parser, "expected enum name");
    if (ruda_parser_at(parser, RUDA_TOKEN_LESS)) {
        ruda_parse_type_parameters(parser);
    }
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_BRACE, "expected enum body");
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        RudaMarker variant = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected enum variant");
        if (ruda_parser_at(parser, RUDA_TOKEN_LEFT_PAREN)) {
            ruda_parse_positional_payload(parser);
        } else if (ruda_parser_at(parser, RUDA_TOKEN_LEFT_BRACE)) {
            ruda_parse_named_payload(parser);
        }
        if (!ruda_parser_eat(parser, RUDA_TOKEN_COMMA)
            && !ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)) {
            ruda_parser_expect(
                parser,
                RUDA_TOKEN_COMMA,
                "expected ',' between enum variants"
            );
        }
        ruda_parser_complete(parser, variant, RUDA_SYNTAX_ENUM_VARIANT);
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            size_t offset = ruda_parser_current_offset(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "unexpected token in enum body",
                offset,
                offset
            );
            ruda_parser_bump(parser);
        }
    }
    ruda_parser_expect(parser, RUDA_TOKEN_RIGHT_BRACE, "expected '}' after enum");
    ruda_parser_complete(parser, marker, RUDA_SYNTAX_ENUM_DECL);
}

static void ruda_parse_interface_decl(RudaParser *parser, RudaMarker marker) {
    ruda_parser_expect(parser, RUDA_TOKEN_KW_INTERFACE, "expected 'interface'");
    ruda_parser_expect_identifier(parser, "expected interface name");
    if (ruda_parser_at(parser, RUDA_TOKEN_LESS)) {
        ruda_parse_type_parameters(parser);
    }
    ruda_parser_expect(parser, RUDA_TOKEN_LEFT_BRACE, "expected interface body");
    while (!ruda_parser_at(parser, RUDA_TOKEN_RIGHT_BRACE)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        RudaMarker item = ruda_parser_start(parser);
        ruda_parser_expect_identifier(parser, "expected interface item");
        if (ruda_parser_at(parser, RUDA_TOKEN_LESS)) {
            ruda_parse_type_parameters(parser);
        }
        if (ruda_parser_at(parser, RUDA_TOKEN_LEFT_PAREN)) {
            ruda_parse_parameters(parser);
            ruda_parser_expect(parser, RUDA_TOKEN_COLON, "expected interface return type");
            ruda_parse_type_until(
                parser,
                RUDA_TOKEN_SEMICOLON,
                RUDA_TOKEN_RIGHT_BRACE,
                RUDA_TOKEN_EOF
            );
        }
        ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after interface item");
        ruda_parser_complete(parser, item, RUDA_SYNTAX_INTERFACE_ITEM);
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            size_t offset = ruda_parser_current_offset(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "unexpected token in interface body",
                offset,
                offset
            );
            ruda_parser_bump(parser);
        }
    }
    ruda_parser_expect(parser, RUDA_TOKEN_RIGHT_BRACE, "expected '}' after interface");
    ruda_parser_complete(parser, marker, RUDA_SYNTAX_INTERFACE_DECL);
}

static void ruda_parse_function_decl(
    RudaParser *parser,
    RudaMarker marker,
    bool external
) {
    if (external) {
        ruda_parser_expect(parser, RUDA_TOKEN_KW_EXTERN, "expected 'extern'");
        ruda_parser_expect(parser, RUDA_TOKEN_STRING, "expected extern ABI string");
    }
    (void)ruda_parser_eat(parser, RUDA_TOKEN_KW_UNSAFE);
    ruda_parser_expect(parser, RUDA_TOKEN_KW_FN, "expected 'fn'");
    ruda_parser_expect_identifier(parser, "expected function name");
    if (ruda_parser_at(parser, RUDA_TOKEN_LESS)) {
        ruda_parse_type_parameters(parser);
    }
    ruda_parse_parameters(parser);
    ruda_parser_expect(parser, RUDA_TOKEN_COLON, "expected function return type");
    ruda_parse_type_until(
        parser,
        external ? RUDA_TOKEN_SEMICOLON : RUDA_TOKEN_LEFT_BRACE,
        RUDA_TOKEN_EOF,
        RUDA_TOKEN_EOF
    );
    if (external) {
        ruda_parser_expect(parser, RUDA_TOKEN_SEMICOLON, "expected ';' after extern");
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_EXTERN_DECL);
    } else {
        ruda_parse_block(parser);
        ruda_parser_complete(parser, marker, RUDA_SYNTAX_FUNCTION_DECL);
    }
}

static bool ruda_parser_at_top_decl_start(const RudaParser *parser) {
    RudaTokenKind current = ruda_parser_current(parser);
    return current == RUDA_TOKEN_AT
        || current == RUDA_TOKEN_KW_PUBLIC
        || current == RUDA_TOKEN_KW_IMPORT
        || current == RUDA_TOKEN_KW_TYPE
        || current == RUDA_TOKEN_KW_ENUM
        || current == RUDA_TOKEN_KW_INTERFACE
        || current == RUDA_TOKEN_KW_FN
        || current == RUDA_TOKEN_KW_UNSAFE
        || current == RUDA_TOKEN_KW_EXTERN
        || current == RUDA_TOKEN_KW_CONST;
}

static void ruda_parse_invalid_declaration(RudaParser *parser, RudaMarker marker) {
    size_t start = ruda_parser_current_offset(parser);
    (void)ruda_parser_add_diagnostic(
        parser,
        RUDA_DIAGNOSTIC_INVALID_DECLARATION,
        "expected import, type, enum, interface, function, const, or extern declaration",
        start,
        start
    );
    while (!ruda_parser_at(parser, RUDA_TOKEN_SEMICOLON)
        && !ruda_parser_at(parser, RUDA_TOKEN_EOF)
        && !ruda_parser_at_top_decl_start(parser)
        && !parser->allocation_failed && !parser->limit_reached) {
        ruda_parser_bump(parser);
    }
    (void)ruda_parser_eat(parser, RUDA_TOKEN_SEMICOLON);
    ruda_parser_complete(parser, marker, RUDA_SYNTAX_ERROR);
}

static void ruda_parse_top_decl(RudaParser *parser) {
    RudaMarker marker = ruda_parser_start(parser);
    RudaTokenKind current;
    bool has_modifier = false;
    while (ruda_parser_at(parser, RUDA_TOKEN_AT)
        && !parser->allocation_failed && !parser->limit_reached) {
        has_modifier = true;
        ruda_parse_attribute(parser);
    }
    if (ruda_parser_eat(parser, RUDA_TOKEN_KW_PUBLIC)) {
        has_modifier = true;
    }
    current = ruda_parser_current(parser);
    if (current == RUDA_TOKEN_KW_IMPORT) {
        if (has_modifier) {
            size_t offset = ruda_parser_current_offset(parser);
            (void)ruda_parser_add_diagnostic(
                parser,
                RUDA_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "imports cannot have declaration attributes or visibility",
                offset,
                offset
            );
        }
        ruda_parse_import_decl(parser, marker);
    } else if (current == RUDA_TOKEN_KW_TYPE) {
        ruda_parse_type_decl(parser, marker);
    } else if (current == RUDA_TOKEN_KW_ENUM) {
        ruda_parse_enum_decl(parser, marker);
    } else if (current == RUDA_TOKEN_KW_INTERFACE) {
        ruda_parse_interface_decl(parser, marker);
    } else if (current == RUDA_TOKEN_KW_FN || current == RUDA_TOKEN_KW_UNSAFE) {
        ruda_parse_function_decl(parser, marker, false);
    } else if (current == RUDA_TOKEN_KW_EXTERN) {
        ruda_parse_function_decl(parser, marker, true);
    } else if (current == RUDA_TOKEN_KW_CONST) {
        ruda_parse_binding_decl(parser, marker, RUDA_SYNTAX_CONST_DECL);
    } else {
        ruda_parse_invalid_declaration(parser, marker);
    }
}

static bool ruda_builder_push_token(
    RudaVector *elements,
    const RudaSyntaxToken *token
) {
    RudaSyntaxElement element;
    element.kind = RUDA_SYNTAX_ELEMENT_TOKEN;
    element.as.token = *token;
    return ruda_vector_push(elements, &element);
}

static bool ruda_builder_finish_node(
    RudaSyntaxTree *tree,
    RudaVector *frames,
    RudaVector *elements
) {
    RudaBuildFrame *frame = ruda_vector_at(frames, frames->length - 1);
    size_t child_count = 0;
    size_t children_size = 0;
    size_t allocation_size = 0;
    size_t text_length = 0;
    size_t index = 0;
    RudaGreenNode *node = NULL;
    RudaSyntaxElement node_element;
    if (frame == NULL || frame->element_start > elements->length) {
        return false;
    }
    child_count = elements->length - frame->element_start;
    if (!ruda_size_multiply(child_count, sizeof(RudaSyntaxElement), &children_size)
        || !ruda_size_add(offsetof(RudaGreenNode, children), children_size, &allocation_size)) {
        return false;
    }
    node = ruda_syntax_tree_allocate(
        tree,
        allocation_size,
        _Alignof(RudaGreenNode)
    );
    if (node == NULL) {
        return false;
    }
    node->kind = frame->kind;
    node->child_count = child_count;
    for (index = 0; index < child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_vector_at_const(
            elements,
            frame->element_start + index
        );
        if (element == NULL) {
            return false;
        }
        node->children[index] = *element;
        if (element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            if (!ruda_size_add(
                    text_length,
                    element->as.node->text_length,
                    &text_length
                )) {
                return false;
            }
        } else if (!element->as.token.synthetic
            && element->as.token.kind != RUDA_TOKEN_EOF) {
            if (!ruda_size_add(
                    text_length,
                    element->as.token.end - element->as.token.start,
                    &text_length
                )) {
                return false;
            }
        }
    }
    node->text_length = text_length;
    elements->length = frame->element_start;
    frames->length -= 1;
    node_element.kind = RUDA_SYNTAX_ELEMENT_NODE;
    node_element.as.node = node;
    return ruda_vector_push(elements, &node_element);
}

static bool ruda_build_tree(
    const RudaLexResult *lexed,
    RudaAllocator allocator,
    RudaVector *events,
    RudaSyntaxTree *tree
) {
    RudaVector frames;
    RudaVector elements;
    RudaVector start_kinds;
    size_t index = 0;
    bool ok = false;
    memset(&frames, 0, sizeof(frames));
    memset(&elements, 0, sizeof(elements));
    memset(&start_kinds, 0, sizeof(start_kinds));
    if (!ruda_syntax_tree_storage_init(tree, allocator)) {
        return false;
    }
    tree->snapshot = lexed->snapshot;
    tree->root = NULL;
    tree->initialized = true;
    if (!ruda_vector_init(&frames, allocator, sizeof(RudaBuildFrame))
        || !ruda_vector_init(&elements, allocator, sizeof(RudaSyntaxElement))
        || !ruda_vector_init(&start_kinds, allocator, sizeof(RudaSyntaxKind))) {
        ruda_vector_dispose(&start_kinds);
        ruda_vector_dispose(&elements);
        ruda_vector_dispose(&frames);
        ruda_syntax_tree_dispose(tree);
        return false;
    }
    for (index = 0; index < events->length; index += 1) {
        RudaParseEvent *event = ruda_vector_at(events, index);
        if (event == NULL) {
            goto cleanup;
        }
        if (event->kind == RUDA_PARSE_EVENT_START) {
            size_t start_index = index;
            size_t kind_index = 0;
            start_kinds.length = 0;
            for (;;) {
                RudaParseEvent *start = ruda_vector_at(events, start_index);
                RudaSyntaxKind syntax_kind;
                size_t forward_parent = 0;
                size_t next_index = 0;
                if (start == NULL || start->kind != RUDA_PARSE_EVENT_START) {
                    goto cleanup;
                }
                syntax_kind = start->as.start.syntax_kind;
                forward_parent = start->as.start.forward_parent;
                if (start_index != index) {
                    start->kind = RUDA_PARSE_EVENT_TOMBSTONE;
                }
                if (!ruda_vector_push(&start_kinds, &syntax_kind)) {
                    goto cleanup;
                }
                if (forward_parent == 0) {
                    break;
                }
                if (!ruda_size_add(start_index, forward_parent, &next_index)
                    || next_index <= start_index || next_index >= events->length) {
                    goto cleanup;
                }
                start_index = next_index;
            }
            for (kind_index = start_kinds.length; kind_index != 0; kind_index -= 1) {
                const RudaSyntaxKind *syntax_kind = ruda_vector_at_const(
                    &start_kinds,
                    kind_index - 1
                );
                RudaBuildFrame frame;
                if (syntax_kind == NULL) {
                    goto cleanup;
                }
                frame.kind = *syntax_kind;
                frame.element_start = elements.length;
                if (!ruda_vector_push(&frames, &frame)) {
                    goto cleanup;
                }
            }
        } else if (event->kind == RUDA_PARSE_EVENT_TOMBSTONE) {
            continue;
        } else if (event->kind == RUDA_PARSE_EVENT_TOKEN) {
            const RudaToken *source = ruda_lex_token(
                lexed,
                event->as.token.token_index
            );
            RudaSyntaxToken token;
            if (source == NULL) {
                goto cleanup;
            }
            token.kind = event->as.token.kind;
            token.start = source->start;
            token.end = source->end;
            token.synthetic = false;
            if (!ruda_builder_push_token(&elements, &token)) {
                goto cleanup;
            }
        } else if (event->kind == RUDA_PARSE_EVENT_SYNTHETIC_TOKEN) {
            if (!ruda_builder_push_token(&elements, &event->as.synthetic_token)) {
                goto cleanup;
            }
        } else if (event->kind == RUDA_PARSE_EVENT_FINISH) {
            if (frames.length == 0
                || !ruda_builder_finish_node(tree, &frames, &elements)) {
                goto cleanup;
            }
        }
    }
    if (frames.length != 0 || elements.length != 1) {
        goto cleanup;
    }
    {
        const RudaSyntaxElement *root = ruda_vector_at_const(&elements, 0);
        if (root == NULL || root->kind != RUDA_SYNTAX_ELEMENT_NODE
            || root->as.node->kind != RUDA_SYNTAX_ROOT) {
            goto cleanup;
        }
        tree->root = root->as.node;
    }
    ok = true;

cleanup:
    ruda_vector_dispose(&start_kinds);
    ruda_vector_dispose(&elements);
    ruda_vector_dispose(&frames);
    if (!ok) {
        ruda_syntax_tree_dispose(tree);
    }
    return ok;
}

RudaParseOptions ruda_parse_default_options(void) {
    RudaParseOptions options;
    options.max_events = 2000000;
    options.max_diagnostics = 100;
    options.max_nesting = 512;
    return options;
}

RudaParseStatus ruda_parse_file(
    const RudaLexResult *lexed,
    RudaAllocator allocator,
    RudaParseOptions options,
    RudaParseResult *result
) {
    RudaParser parser;
    RudaMarker root;
    const RudaToken *last = NULL;
    if (result == NULL) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (lexed == NULL || !lexed->initialized || lexed->tokens.length == 0
        || options.max_events < 2 || options.max_diagnostics == 0
        || options.max_nesting < 2) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    last = ruda_lex_token(lexed, lexed->tokens.length - 1);
    if (last == NULL || last->kind != RUDA_TOKEN_EOF) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    if (!ruda_vector_init(
            &result->diagnostics,
            allocator,
            sizeof(RudaParseDiagnostic)
        )) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    result->allocator = allocator;
    result->initialized = true;
    memset(&parser, 0, sizeof(parser));
    parser.lexed = lexed;
    parser.result = result;
    parser.options = options;
    if (!ruda_vector_init(&parser.events, allocator, sizeof(RudaParseEvent))) {
        ruda_parse_result_dispose(result);
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    root = ruda_parser_start(&parser);
    while (!ruda_parser_at(&parser, RUDA_TOKEN_EOF)
        && !parser.allocation_failed && !parser.limit_reached) {
        size_t before = parser.raw_index;
        ruda_parse_top_decl(&parser);
        if (parser.raw_index == before && !parser.limit_reached
            && !parser.allocation_failed) {
            ruda_parser_bump(&parser);
        }
    }
    if (!parser.allocation_failed && !parser.limit_reached) {
        ruda_parser_bump(&parser);
        ruda_parser_complete(&parser, root, RUDA_SYNTAX_ROOT);
    }
    if (parser.allocation_failed || parser.limit_reached) {
        RudaParseStatus status = parser.allocation_failed
            ? RUDA_PARSE_ALLOCATION_FAILED
            : RUDA_PARSE_LIMIT_REACHED;
        ruda_vector_dispose(&parser.events);
        ruda_parse_result_dispose(result);
        return status;
    }
    if (!ruda_build_tree(lexed, allocator, &parser.events, &result->tree)) {
        ruda_vector_dispose(&parser.events);
        ruda_parse_result_dispose(result);
        return RUDA_PARSE_ALLOCATION_FAILED;
    }
    ruda_vector_dispose(&parser.events);
    return RUDA_PARSE_OK;
}

static size_t ruda_incremental_element_length(
    const RudaSyntaxElement *element
) {
    if (element == NULL) {
        return 0;
    }
    if (element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
        return element->as.node == NULL ? 0 : element->as.node->text_length;
    }
    if (element->kind == RUDA_SYNTAX_ELEMENT_TOKEN
        && !element->as.token.synthetic
        && element->as.token.kind != RUDA_TOKEN_EOF
        && element->as.token.end >= element->as.token.start) {
        return element->as.token.end - element->as.token.start;
    }
    return 0;
}

static bool ruda_incremental_nodes_match(
    const RudaSyntaxElement *previous,
    const RudaSyntaxElement *current
) {
    return previous != NULL && current != NULL
        && previous->kind == RUDA_SYNTAX_ELEMENT_NODE
        && current->kind == RUDA_SYNTAX_ELEMENT_NODE
        && previous->as.node != NULL && current->as.node != NULL
        && previous->as.node->kind == current->as.node->kind
        && previous->as.node->text_length == current->as.node->text_length;
}

RudaParseStatus ruda_parse_file_incremental(
    const RudaParseResult *previous,
    const RudaLexResult *lexed,
    size_t edit_start,
    size_t previous_edit_end,
    size_t new_edit_end,
    RudaAllocator allocator,
    RudaParseOptions options,
    RudaParseResult *result,
    RudaIncrementalParseStats *stats
) {
    const RudaGreenNode *previous_root = NULL;
    const RudaGreenNode *current_root = NULL;
    const unsigned char *previous_bytes = NULL;
    const unsigned char *current_bytes = NULL;
    size_t previous_length = 0;
    size_t current_length = 0;
    size_t previous_suffix_length = 0;
    size_t current_suffix_length = 0;
    size_t prefix_elements = 0;
    size_t reused_nodes = 0;
    size_t reused_bytes = 0;
    size_t old_cursor = 0;
    size_t new_cursor = 0;
    size_t old_index = 0;
    size_t new_index = 0;
    size_t children_size = 0;
    size_t allocation_size = 0;
    RudaGreenNode *replacement = NULL;
    RudaParseStatus status;
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
    if (result == NULL) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (previous == NULL || previous == result || !previous->initialized
        || previous->tree.snapshot == NULL || previous->tree.root == NULL
        || lexed == NULL || !lexed->initialized || lexed->snapshot == NULL
        || ruda_source_file_id(previous->tree.snapshot)
            != ruda_source_file_id(lexed->snapshot)) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    previous_length = ruda_source_length(previous->tree.snapshot);
    current_length = ruda_source_length(lexed->snapshot);
    if (edit_start > previous_edit_end
        || previous_edit_end > previous_length
        || edit_start > new_edit_end || new_edit_end > current_length) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    previous_suffix_length = previous_length - previous_edit_end;
    current_suffix_length = current_length - new_edit_end;
    if (previous_suffix_length != current_suffix_length) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }
    previous_bytes = ruda_source_bytes(previous->tree.snapshot);
    current_bytes = ruda_source_bytes(lexed->snapshot);
    if ((edit_start != 0
            && memcmp(previous_bytes, current_bytes, edit_start) != 0)
        || (previous_suffix_length != 0
            && memcmp(
                previous_bytes + previous_edit_end,
                current_bytes + new_edit_end,
                previous_suffix_length
            ) != 0)) {
        return RUDA_PARSE_INVALID_ARGUMENT;
    }

    status = ruda_parse_file(lexed, allocator, options, result);
    if (status != RUDA_PARSE_OK) {
        return status;
    }
    previous_root = previous->tree.root;
    current_root = result->tree.root;
    if (previous_root->kind != RUDA_SYNTAX_ROOT
        || current_root->kind != RUDA_SYNTAX_ROOT) {
        ruda_parse_result_dispose(result);
        return RUDA_PARSE_INVALID_ARGUMENT;
    }

    while (old_index < previous_root->child_count
        && new_index < current_root->child_count) {
        const RudaSyntaxElement *old_element = ruda_syntax_child(
            previous_root,
            old_index
        );
        const RudaSyntaxElement *new_element = ruda_syntax_child(
            current_root,
            new_index
        );
        size_t old_length = ruda_incremental_element_length(old_element);
        size_t new_length = ruda_incremental_element_length(new_element);
        size_t old_end = old_cursor + old_length;
        size_t new_end = new_cursor + new_length;
        if (old_end > edit_start || new_end > edit_start) {
            break;
        }
        if (old_cursor == new_cursor
            && ruda_incremental_nodes_match(old_element, new_element)) {
            reused_nodes += 1;
            reused_bytes += new_length;
        }
        old_cursor = old_end;
        new_cursor = new_end;
        old_index += 1;
        new_index += 1;
        prefix_elements = new_index;
    }

    old_index = previous_root->child_count;
    new_index = current_root->child_count;
    old_cursor = previous_length;
    new_cursor = current_length;
    if (previous_length == current_length) {
        while (old_index != 0 && new_index > prefix_elements) {
            const RudaSyntaxElement *old_element = NULL;
            const RudaSyntaxElement *new_element = NULL;
            size_t old_length = 0;
            size_t new_length = 0;
            size_t old_start = 0;
            size_t new_start = 0;
            old_index -= 1;
            new_index -= 1;
            old_element = ruda_syntax_child(previous_root, old_index);
            new_element = ruda_syntax_child(current_root, new_index);
            old_length = ruda_incremental_element_length(old_element);
            new_length = ruda_incremental_element_length(new_element);
            if (old_length > old_cursor || new_length > new_cursor) {
                break;
            }
            old_start = old_cursor - old_length;
            new_start = new_cursor - new_length;
            if (old_start < previous_edit_end || new_start < new_edit_end) {
                break;
            }
            if (old_start == new_start
                && ruda_incremental_nodes_match(old_element, new_element)) {
                reused_nodes += 1;
                reused_bytes += new_length;
            }
            old_cursor = old_start;
            new_cursor = new_start;
        }
    }
    if (reused_nodes == 0) {
        if (stats != NULL) {
            stats->reparsed_bytes = current_length;
        }
        return RUDA_PARSE_OK;
    }

    if (!ruda_size_multiply(
            current_root->child_count,
            sizeof(RudaSyntaxElement),
            &children_size
        ) || !ruda_size_add(
            offsetof(RudaGreenNode, children),
            children_size,
            &allocation_size
        )) {
        ruda_parse_result_dispose(result);
        return RUDA_PARSE_ALLOCATION_FAILED;
    }
    replacement = ruda_syntax_tree_allocate(
        &result->tree,
        allocation_size,
        _Alignof(RudaGreenNode)
    );
    if (replacement == NULL) {
        ruda_parse_result_dispose(result);
        return RUDA_PARSE_ALLOCATION_FAILED;
    }
    replacement->kind = current_root->kind;
    replacement->child_count = current_root->child_count;
    replacement->text_length = current_root->text_length;
    for (new_index = 0; new_index < current_root->child_count; new_index += 1) {
        replacement->children[new_index] = current_root->children[new_index];
    }

    old_cursor = 0;
    new_cursor = 0;
    old_index = 0;
    new_index = 0;
    while (old_index < previous_root->child_count
        && new_index < current_root->child_count) {
        const RudaSyntaxElement *old_element = ruda_syntax_child(
            previous_root,
            old_index
        );
        const RudaSyntaxElement *new_element = ruda_syntax_child(
            current_root,
            new_index
        );
        size_t old_length = ruda_incremental_element_length(old_element);
        size_t new_length = ruda_incremental_element_length(new_element);
        size_t old_end = old_cursor + old_length;
        size_t new_end = new_cursor + new_length;
        if (old_end > edit_start || new_end > edit_start) {
            break;
        }
        if (old_cursor == new_cursor
            && ruda_incremental_nodes_match(old_element, new_element)) {
            replacement->children[new_index].as.node = old_element->as.node;
        }
        old_cursor = old_end;
        new_cursor = new_end;
        old_index += 1;
        new_index += 1;
    }

    old_index = previous_root->child_count;
    new_index = current_root->child_count;
    old_cursor = previous_length;
    new_cursor = current_length;
    if (previous_length == current_length) {
        while (old_index != 0 && new_index > prefix_elements) {
            const RudaSyntaxElement *old_element = NULL;
            const RudaSyntaxElement *new_element = NULL;
            size_t old_length = 0;
            size_t new_length = 0;
            size_t old_start = 0;
            size_t new_start = 0;
            old_index -= 1;
            new_index -= 1;
            old_element = ruda_syntax_child(previous_root, old_index);
            new_element = ruda_syntax_child(current_root, new_index);
            old_length = ruda_incremental_element_length(old_element);
            new_length = ruda_incremental_element_length(new_element);
            if (old_length > old_cursor || new_length > new_cursor) {
                break;
            }
            old_start = old_cursor - old_length;
            new_start = new_cursor - new_length;
            if (old_start < previous_edit_end || new_start < new_edit_end) {
                break;
            }
            if (old_start == new_start
                && ruda_incremental_nodes_match(old_element, new_element)) {
                replacement->children[new_index].as.node = old_element->as.node;
            }
            old_cursor = old_start;
            new_cursor = new_start;
        }
    }
    if (!ruda_syntax_tree_retain_storage(&result->tree, &previous->tree)) {
        if (stats != NULL) {
            stats->reparsed_bytes = current_length;
        }
        return RUDA_PARSE_OK;
    }
    result->tree.root = replacement;
    if (stats != NULL) {
        stats->reused_top_level_nodes = reused_nodes;
        stats->reparsed_bytes = current_length - reused_bytes;
    }
    return RUDA_PARSE_OK;
}

void ruda_parse_result_dispose(RudaParseResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    ruda_syntax_tree_dispose(&result->tree);
    ruda_vector_dispose(&result->diagnostics);
    result->suppressed_diagnostics = 0;
    result->initialized = false;
}

const RudaParseDiagnostic *ruda_parse_diagnostic(
    const RudaParseResult *result,
    size_t index
) {
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    return ruda_vector_at_const(&result->diagnostics, index);
}
