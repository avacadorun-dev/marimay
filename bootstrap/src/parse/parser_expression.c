#include "parser_internal.h"

#include "ruda/generated/diagnostic_codes.h"

typedef struct RudaExpressionStops {
    RudaTokenKind first;
    RudaTokenKind second;
    RudaTokenKind third;
} RudaExpressionStops;

static RudaCompletedMarker ruda_parse_expression_bp(
    RudaParser *parser,
    RudaExpressionStops stops,
    unsigned int minimum_binding_power
);

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

bool ruda_parser_yield_is_name_expression(
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

bool ruda_parser_contextual_statement_starts(
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

bool ruda_parser_type_at_greater(const RudaParser *parser) {
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

void ruda_parser_expect_type_greater(RudaParser *parser) {
    if (!ruda_parser_eat_type_greater(parser)) {
        ruda_parser_expect(
            parser,
            RUDA_TOKEN_GREATER,
            "expected '>' after type list"
        );
    }
}

void ruda_parser_begin_type_angle(RudaParser *parser) {
    if (!ruda_size_add(
            parser->type_angle_depth,
            1,
            &parser->type_angle_depth
        )) {
        parser->limit_reached = true;
    }
}

void ruda_parser_end_type_angle(RudaParser *parser) {
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
        if (ruda_parser_type_at_greater(parser)
            || !ruda_parser_eat(parser, RUDA_TOKEN_COMMA)) {
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

void ruda_parse_arguments(RudaParser *parser) {
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
        if (current == RUDA_TOKEN_KW_AS) {
            RudaMarker type;
            if (85 < minimum_binding_power) {
                break;
            }
            parent = ruda_parser_precede(parser, left);
            ruda_parser_bump(parser);
            type = ruda_parser_start(parser);
            ruda_parser_expect_identifier(
                parser, "expected integer target type after 'as'"
            );
            (void)ruda_parser_complete(parser, type, RUDA_SYNTAX_TYPE);
            left = ruda_parser_complete(
                parser, parent, RUDA_SYNTAX_CONVERSION_EXPRESSION
            );
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

void ruda_parse_expression_until(
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

void ruda_parse_type_list(
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

void ruda_parse_type_until(
    RudaParser *parser,
    RudaTokenKind first,
    RudaTokenKind second,
    RudaTokenKind third
) {
    RudaMarker marker = ruda_parser_start(parser);
    RudaTokenKind current;
    if (!marker.valid || parser->allocation_failed || parser->limit_reached) {
        return;
    }
    current = ruda_parser_current(parser);
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
        if (ruda_parser_eat(parser, RUDA_TOKEN_RIGHT_BRACKET)) {
            if (!parser->allocation_failed && !parser->limit_reached) {
                ruda_parse_type_until(parser, first, second, third);
            }
        } else {
            ruda_parse_type_list(parser, RUDA_TOKEN_RIGHT_BRACKET, false);
            ruda_parser_expect_closing_delimiter(
                parser,
                RUDA_TOKEN_RIGHT_BRACKET,
                open_offset,
                "expected ']' after tuple type"
            );
        }
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
        if (!parser->allocation_failed && !parser->limit_reached) {
            ruda_parse_type_until(parser, first, second, third);
        }
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
