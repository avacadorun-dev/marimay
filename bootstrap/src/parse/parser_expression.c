#include "parser_internal.h"

#include "mmay/generated/diagnostic_codes.h"

typedef struct MarimayExpressionStops {
    MarimayTokenKind first;
    MarimayTokenKind second;
    MarimayTokenKind third;
} MarimayExpressionStops;

static MarimayCompletedMarker mmay_parse_expression_bp(
    MarimayParser *parser,
    MarimayExpressionStops stops,
    unsigned int minimum_binding_power
);

static MarimayCompletedMarker mmay_parse_interpolated_expression(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    mmay_parser_expect(
        parser,
        MMAY_TOKEN_INTERPOLATED_START,
        "expected interpolated string start"
    );
    while (!mmay_parser_at(parser, MMAY_TOKEN_INTERPOLATED_END)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        if (mmay_parser_at(parser, MMAY_TOKEN_INTERPOLATED_TEXT)) {
            mmay_parser_bump(parser);
        } else if (mmay_parser_eat(parser, MMAY_TOKEN_INTERPOLATION_START)) {
            mmay_parse_expression_until(
                parser,
                MMAY_TOKEN_INTERPOLATION_END,
                MMAY_TOKEN_EOF,
                MMAY_TOKEN_EOF
            );
            mmay_parser_expect(
                parser,
                MMAY_TOKEN_INTERPOLATION_END,
                "expected end of interpolation expression"
            );
        } else {
            size_t offset = mmay_parser_current_offset(parser);
            (void)mmay_parser_add_diagnostic(
                parser,
                MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "unexpected token in interpolated string",
                offset,
                offset
            );
            mmay_parser_bump(parser);
        }
    }
    mmay_parser_expect(
        parser,
        MMAY_TOKEN_INTERPOLATED_END,
        "expected end of interpolated string"
    );
    return mmay_parser_complete(
        parser,
        marker,
        MMAY_SYNTAX_INTERPOLATED_EXPRESSION
    );
}

static bool mmay_expression_at_stop(
    const MarimayParser *parser,
    MarimayExpressionStops stops
) {
    return mmay_parser_is_stop(parser, stops.first, stops.second, stops.third);
}

static bool mmay_expression_is_literal(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_INTEGER || kind == MMAY_TOKEN_FLOAT
        || kind == MMAY_TOKEN_STRING || kind == MMAY_TOKEN_RAW_STRING
        || kind == MMAY_TOKEN_BYTE_STRING || kind == MMAY_TOKEN_RUNE
        || kind == MMAY_TOKEN_KW_TRUE || kind == MMAY_TOKEN_KW_FALSE;
}

static bool mmay_expression_is_assignment(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_EQUAL || kind == MMAY_TOKEN_PLUS_EQUAL
        || kind == MMAY_TOKEN_MINUS_EQUAL || kind == MMAY_TOKEN_STAR_EQUAL
        || kind == MMAY_TOKEN_SLASH_EQUAL || kind == MMAY_TOKEN_PERCENT_EQUAL
        || kind == MMAY_TOKEN_AMPERSAND_EQUAL || kind == MMAY_TOKEN_PIPE_EQUAL
        || kind == MMAY_TOKEN_CARET_EQUAL || kind == MMAY_TOKEN_SHIFT_LEFT_EQUAL
        || kind == MMAY_TOKEN_SHIFT_RIGHT_EQUAL;
}

bool mmay_parser_yield_is_name_expression(
    const MarimayParser *parser
) {
    MarimayTokenKind next = mmay_parser_next_significant_kind(parser);
    return next == MMAY_TOKEN_DOT || next == MMAY_TOKEN_LEFT_PAREN
        || next == MMAY_TOKEN_LEFT_BRACKET || next == MMAY_TOKEN_LESS
        || next == MMAY_TOKEN_QUESTION || next == MMAY_TOKEN_PLUS
        || next == MMAY_TOKEN_STAR
        || next == MMAY_TOKEN_SLASH || next == MMAY_TOKEN_PERCENT
        || next == MMAY_TOKEN_EQUAL_EQUAL || next == MMAY_TOKEN_BANG_EQUAL
        || next == MMAY_TOKEN_LESS_EQUAL || next == MMAY_TOKEN_GREATER
        || next == MMAY_TOKEN_GREATER_EQUAL || next == MMAY_TOKEN_AMPERSAND
        || next == MMAY_TOKEN_PIPE || next == MMAY_TOKEN_CARET
        || next == MMAY_TOKEN_AMPERSAND_AMPERSAND
        || next == MMAY_TOKEN_PIPE_PIPE || next == MMAY_TOKEN_SHIFT_LEFT
        || next == MMAY_TOKEN_SHIFT_RIGHT
        || mmay_expression_is_assignment(next);
}

bool mmay_parser_contextual_statement_starts(
    const MarimayParser *parser,
    MarimayTokenKind current
) {
    MarimayTokenKind next = mmay_parser_next_significant_kind(parser);
    if (current == MMAY_TOKEN_KW_CONST || current == MMAY_TOKEN_KW_LET) {
        return next == MMAY_TOKEN_IDENTIFIER
            || mmay_token_is_contextual_keyword(next)
            || next == MMAY_TOKEN_UNDERSCORE
            || next == MMAY_TOKEN_COLON;
    }
    if (current == MMAY_TOKEN_KW_FOR) {
        return next == MMAY_TOKEN_KW_CONST || next == MMAY_TOKEN_KW_LET
            || ((next == MMAY_TOKEN_IDENTIFIER
                    || mmay_token_is_contextual_keyword(next))
                && mmay_parser_second_next_significant_kind(parser)
                    == MMAY_TOKEN_KW_OF);
    }
    if (current == MMAY_TOKEN_KW_SCOPE || current == MMAY_TOKEN_KW_UNSAFE) {
        return next == MMAY_TOKEN_LEFT_BRACE;
    }
    if (current == MMAY_TOKEN_KW_BREAK
        || current == MMAY_TOKEN_KW_CONTINUE) {
        return next == MMAY_TOKEN_SEMICOLON
            || next == MMAY_TOKEN_RIGHT_BRACE || next == MMAY_TOKEN_EOF;
    }
    return true;
}

static bool mmay_expression_infix_binding_power(
    MarimayTokenKind kind,
    unsigned int *left,
    unsigned int *right,
    MarimaySyntaxKind *syntax_kind
) {
    unsigned int binding_power = 0;
    if (kind == MMAY_TOKEN_STAR || kind == MMAY_TOKEN_SLASH
        || kind == MMAY_TOKEN_PERCENT) {
        binding_power = 80;
    } else if (kind == MMAY_TOKEN_PLUS || kind == MMAY_TOKEN_MINUS) {
        binding_power = 70;
    } else if (kind == MMAY_TOKEN_SHIFT_LEFT || kind == MMAY_TOKEN_SHIFT_RIGHT) {
        binding_power = 60;
    } else if (kind == MMAY_TOKEN_LESS || kind == MMAY_TOKEN_LESS_EQUAL
        || kind == MMAY_TOKEN_GREATER || kind == MMAY_TOKEN_GREATER_EQUAL) {
        binding_power = 50;
    } else if (kind == MMAY_TOKEN_EQUAL_EQUAL || kind == MMAY_TOKEN_BANG_EQUAL) {
        binding_power = 40;
    } else if (kind == MMAY_TOKEN_AMPERSAND || kind == MMAY_TOKEN_CARET
        || kind == MMAY_TOKEN_PIPE) {
        binding_power = 35;
    } else if (kind == MMAY_TOKEN_AMPERSAND_AMPERSAND) {
        binding_power = 30;
    } else if (kind == MMAY_TOKEN_PIPE_PIPE) {
        binding_power = 20;
    } else if (mmay_expression_is_assignment(kind)) {
        *left = 10;
        *right = 11;
        *syntax_kind = MMAY_SYNTAX_ASSIGNMENT_EXPRESSION;
        return true;
    } else {
        return false;
    }
    *left = binding_power;
    *right = binding_power + 1;
    *syntax_kind = MMAY_SYNTAX_BINARY_EXPRESSION;
    return true;
}

static bool mmay_parser_paren_is_closure(const MarimayParser *parser) {
    size_t index = mmay_parser_significant_index(parser);
    size_t depth = 0;
    if (mmay_parser_raw_token(parser, index) == NULL
        || mmay_parser_raw_token(parser, index)->kind != MMAY_TOKEN_LEFT_PAREN) {
        return false;
    }
    while (index < parser->lexed->tokens.length) {
        const MarimayToken *token = mmay_parser_raw_token(parser, index);
        if (token == NULL) {
            return false;
        }
        if (token->kind == MMAY_TOKEN_LEFT_PAREN) {
            depth += 1;
        } else if (token->kind == MMAY_TOKEN_RIGHT_PAREN) {
            if (depth == 0) {
                return false;
            }
            depth -= 1;
            if (depth == 0) {
                size_t next = mmay_parser_skip_trivia(parser, index + 1);
                const MarimayToken *after = mmay_parser_raw_token(parser, next);
                if (after == NULL) {
                    return false;
                }
                if (after->kind == MMAY_TOKEN_FAT_ARROW) {
                    return true;
                }
                if (after->kind != MMAY_TOKEN_COLON) {
                    return false;
                }
                next = mmay_parser_skip_trivia(parser, next + 1);
                while (next < parser->lexed->tokens.length) {
                    after = mmay_parser_raw_token(parser, next);
                    if (after == NULL || after->kind == MMAY_TOKEN_EOF
                        || after->kind == MMAY_TOKEN_SEMICOLON
                        || after->kind == MMAY_TOKEN_RIGHT_BRACE) {
                        return false;
                    }
                    if (after->kind == MMAY_TOKEN_FAT_ARROW) {
                        return true;
                    }
                    next = mmay_parser_skip_trivia(parser, next + 1);
                }
                return false;
            }
        }
        index += 1;
    }
    return false;
}

static bool mmay_parser_generic_call_ahead(const MarimayParser *parser) {
    size_t index = mmay_parser_significant_index(parser);
    size_t depth = 0;
    const MarimayToken *token = mmay_parser_raw_token(parser, index);
    if (token == NULL || token->kind != MMAY_TOKEN_LESS) {
        return false;
    }
    while (index < parser->lexed->tokens.length) {
        token = mmay_parser_raw_token(parser, index);
        if (token == NULL || token->kind == MMAY_TOKEN_EOF) {
            return false;
        }
        if (token->kind == MMAY_TOKEN_LESS) {
            depth += 1;
        } else if (token->kind == MMAY_TOKEN_GREATER) {
            if (depth == 0) {
                return false;
            }
            depth -= 1;
        } else if (token->kind == MMAY_TOKEN_SHIFT_RIGHT) {
            if (depth < 2) {
                return false;
            }
            depth -= 2;
        }
        if (depth == 0) {
            size_t next = mmay_parser_skip_trivia(parser, index + 1);
            const MarimayToken *after = mmay_parser_raw_token(parser, next);
            return after != NULL && after->kind == MMAY_TOKEN_LEFT_PAREN;
        }
        index += 1;
    }
    return false;
}

bool mmay_parser_type_at_greater(const MarimayParser *parser) {
    return parser->pending_type_greater != 0
        || mmay_parser_at(parser, MMAY_TOKEN_GREATER)
        || mmay_parser_at(parser, MMAY_TOKEN_SHIFT_RIGHT);
}

static bool mmay_parser_eat_type_greater(MarimayParser *parser) {
    if (parser->pending_type_greater != 0) {
        parser->pending_type_greater -= 1;
        return true;
    }
    if (mmay_parser_eat(parser, MMAY_TOKEN_GREATER)) {
        return true;
    }
    if (mmay_parser_at(parser, MMAY_TOKEN_SHIFT_RIGHT)) {
        mmay_parser_bump(parser);
        parser->pending_type_greater = 1;
        return true;
    }
    return false;
}

void mmay_parser_expect_type_greater(MarimayParser *parser) {
    if (!mmay_parser_eat_type_greater(parser)) {
        mmay_parser_expect(
            parser,
            MMAY_TOKEN_GREATER,
            "expected '>' after type list"
        );
    }
}

void mmay_parser_begin_type_angle(MarimayParser *parser) {
    if (!mmay_size_add(
            parser->type_angle_depth,
            1,
            &parser->type_angle_depth
        )) {
        parser->limit_reached = true;
    }
}

void mmay_parser_end_type_angle(MarimayParser *parser) {
    if (parser->type_angle_depth == 0) {
        parser->limit_reached = true;
        return;
    }
    parser->type_angle_depth -= 1;
    if (parser->type_angle_depth == 0 && parser->pending_type_greater != 0) {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "unmatched '>' after type list",
            offset,
            offset
        );
        parser->pending_type_greater = 0;
    }
}

static void mmay_parse_type_arguments(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LESS, "expected '<' for type arguments");
    mmay_parser_begin_type_angle(parser);
    if (mmay_parser_type_at_greater(parser)) {
        mmay_parse_type_until(
            parser,
            MMAY_TOKEN_COMMA,
            MMAY_TOKEN_GREATER,
            MMAY_TOKEN_SHIFT_RIGHT
        );
    }
    while (!mmay_parser_type_at_greater(parser)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        mmay_parse_type_until(
            parser,
            MMAY_TOKEN_COMMA,
            MMAY_TOKEN_GREATER,
            MMAY_TOKEN_SHIFT_RIGHT
        );
        if (mmay_parser_type_at_greater(parser)
            || !mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
    }
    mmay_parser_expect_type_greater(parser);
    mmay_parser_end_type_angle(parser);
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_TYPE_ARGUMENTS);
}

static void mmay_parse_expression_list(
    MarimayParser *parser,
    MarimayTokenKind close
) {
    while (!mmay_parser_at(parser, close) && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        mmay_parse_expression_until(
            parser,
            MMAY_TOKEN_COMMA,
            close,
            MMAY_TOKEN_EOF
        );
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
        if (parser->raw_index == before) {
            break;
        }
    }
}

static MarimayCompletedMarker mmay_parse_parenthesized_expression(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_PAREN, "expected '('");
    mmay_parse_expression_until(
        parser,
        MMAY_TOKEN_COMMA,
        MMAY_TOKEN_RIGHT_PAREN,
        MMAY_TOKEN_EOF
    );
    if (mmay_parser_at(parser, MMAY_TOKEN_COMMA)) {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "parenthesized expression contains more than one value",
            offset,
            offset
        );
        while (mmay_parser_eat(parser, MMAY_TOKEN_COMMA)
            && !mmay_parser_at(parser, MMAY_TOKEN_RIGHT_PAREN)
            && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
            && !parser->allocation_failed && !parser->limit_reached) {
            mmay_parse_expression_until(
                parser,
                MMAY_TOKEN_COMMA,
                MMAY_TOKEN_RIGHT_PAREN,
                MMAY_TOKEN_EOF
            );
        }
    }
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_PAREN,
        open_offset,
        "expected ')' after expression"
    );
    return mmay_parser_complete(parser, marker, MMAY_SYNTAX_PAREN_GROUP);
}

static MarimayCompletedMarker mmay_parse_array_expression(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACKET, "expected '['");
    mmay_parse_expression_list(parser, MMAY_TOKEN_RIGHT_BRACKET);
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_BRACKET,
        open_offset,
        "expected ']' after array"
    );
    return mmay_parser_complete(parser, marker, MMAY_SYNTAX_ARRAY_EXPRESSION);
}

void mmay_parse_arguments(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_PAREN, "expected argument list");
    mmay_parse_expression_list(parser, MMAY_TOKEN_RIGHT_PAREN);
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_PAREN,
        open_offset,
        "expected ')' after arguments"
    );
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_ARGUMENTS);
}

static void mmay_parse_index_group(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACKET, "expected '[' for index");
    mmay_parse_expression_until(
        parser,
        MMAY_TOKEN_RIGHT_BRACKET,
        MMAY_TOKEN_EOF,
        MMAY_TOKEN_EOF
    );
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_BRACKET,
        open_offset,
        "expected ']' after index"
    );
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_BRACKET_GROUP);
}

static void mmay_parse_record_fields(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(
        parser,
        MMAY_TOKEN_LEFT_BRACE,
        "expected '{' for record fields"
    );
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        MarimayMarker field = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected record field name");
        if (mmay_parser_eat(parser, MMAY_TOKEN_COLON)) {
            mmay_parse_expression_until(
                parser,
                MMAY_TOKEN_COMMA,
                MMAY_TOKEN_RIGHT_BRACE,
                MMAY_TOKEN_EOF
            );
        }
        (void)mmay_parser_complete(parser, field, MMAY_SYNTAX_FIELD_VALUE);
        if (mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            continue;
        }
        if (mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)) {
            break;
        }
        mmay_parser_expect(
            parser,
            MMAY_TOKEN_COMMA,
            "expected ',' between record fields"
        );
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            mmay_parser_bump(parser);
        }
    }
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_BRACE,
        open_offset,
        "expected '}' after record fields"
    );
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_RECORD_FIELDS);
}

typedef struct MarimayPatternStops {
    MarimayTokenKind first;
    MarimayTokenKind second;
} MarimayPatternStops;

static bool mmay_pattern_at_stop(
    const MarimayParser *parser,
    MarimayPatternStops stops
) {
    MarimayTokenKind current = mmay_parser_current(parser);
    return current == stops.first || current == stops.second
        || current == MMAY_TOKEN_SEMICOLON
        || current == MMAY_TOKEN_RIGHT_BRACE
        || current == MMAY_TOKEN_EOF;
}

static MarimayCompletedMarker mmay_parse_pattern_inner(
    MarimayParser *parser,
    MarimayPatternStops stops
);

static void mmay_parse_pattern_list(
    MarimayParser *parser,
    MarimayTokenKind close
) {
    MarimayMarker list = mmay_parser_start(parser);
    MarimayPatternStops stops;
    stops.first = MMAY_TOKEN_COMMA;
    stops.second = close;
    while (!mmay_parser_at(parser, close)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        (void)mmay_parse_pattern_inner(parser, stops);
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
        if (parser->raw_index == before) {
            break;
        }
    }
    (void)mmay_parser_complete(parser, list, MMAY_SYNTAX_PATTERN_LIST);
}

static MarimayCompletedMarker mmay_parse_pattern_payload(
    MarimayParser *parser,
    MarimayTokenKind open,
    MarimayTokenKind close,
    MarimaySyntaxKind kind
) {
    MarimayMarker marker = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(parser, open, "expected pattern payload delimiter");
    mmay_parse_pattern_list(parser, close);
    mmay_parser_expect_closing_delimiter(
        parser,
        close,
        open_offset,
        "expected closing pattern delimiter"
    );
    return mmay_parser_complete(parser, marker, kind);
}

static MarimayCompletedMarker mmay_parse_named_pattern(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    MarimayMarker fields;
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACE, "expected '{' in pattern");
    fields = mmay_parser_start(parser);
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        MarimayMarker field = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected named pattern field");
        if (mmay_parser_eat(parser, MMAY_TOKEN_COLON)) {
            MarimayPatternStops stops;
            stops.first = MMAY_TOKEN_COMMA;
            stops.second = MMAY_TOKEN_RIGHT_BRACE;
            (void)mmay_parse_pattern_inner(parser, stops);
        }
        (void)mmay_parser_complete(parser, field, MMAY_SYNTAX_PATTERN_FIELD);
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
        if (parser->raw_index == before) {
            break;
        }
    }
    (void)mmay_parser_complete(parser, fields, MMAY_SYNTAX_PATTERN_FIELDS);
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_BRACE,
        open_offset,
        "expected '}' after named pattern"
    );
    return mmay_parser_complete(parser, marker, MMAY_SYNTAX_NAMED_PATTERN);
}

static MarimayCompletedMarker mmay_parse_pattern_primary(
    MarimayParser *parser,
    MarimayPatternStops stops
) {
    MarimayTokenKind current = mmay_parser_current(parser);
    MarimayMarker marker;
    MarimayCompletedMarker completed;
    if (mmay_pattern_at_stop(parser, stops)) {
        marker = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected match pattern");
        return mmay_parser_complete(parser, marker, MMAY_SYNTAX_ERROR);
    }
    if (current == MMAY_TOKEN_UNDERSCORE) {
        marker = mmay_parser_start(parser);
        mmay_parser_bump(parser);
        return mmay_parser_complete(
            parser,
            marker,
            MMAY_SYNTAX_WILDCARD_PATTERN
        );
    }
    if (mmay_expression_is_literal(current)) {
        marker = mmay_parser_start(parser);
        mmay_parser_bump(parser);
        return mmay_parser_complete(
            parser,
            marker,
            MMAY_SYNTAX_LITERAL_PATTERN
        );
    }
    if (mmay_parser_at_identifier(parser)) {
        marker = mmay_parser_start(parser);
        mmay_parser_bump_as(parser, MMAY_TOKEN_IDENTIFIER);
        while (mmay_parser_eat(parser, MMAY_TOKEN_DOT)) {
            mmay_parser_expect_identifier(parser, "expected pattern name after '.'");
        }
        completed = mmay_parser_complete(
            parser,
            marker,
            MMAY_SYNTAX_NAME_PATTERN
        );
        if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_PAREN)) {
            MarimayMarker constructor = mmay_parser_precede(parser, completed);
            (void)mmay_parse_pattern_payload(
                parser,
                MMAY_TOKEN_LEFT_PAREN,
                MMAY_TOKEN_RIGHT_PAREN,
                MMAY_SYNTAX_POSITIONAL_PATTERN
            );
            return mmay_parser_complete(
                parser,
                constructor,
                MMAY_SYNTAX_CONSTRUCTOR_PATTERN
            );
        }
        if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_BRACE)) {
            MarimayMarker constructor = mmay_parser_precede(parser, completed);
            (void)mmay_parse_named_pattern(parser);
            return mmay_parser_complete(
                parser,
                constructor,
                MMAY_SYNTAX_CONSTRUCTOR_PATTERN
            );
        }
        return completed;
    }
    if (current == MMAY_TOKEN_LEFT_BRACKET) {
        return mmay_parse_pattern_payload(
            parser,
            MMAY_TOKEN_LEFT_BRACKET,
            MMAY_TOKEN_RIGHT_BRACKET,
            MMAY_SYNTAX_TUPLE_PATTERN
        );
    }

    marker = mmay_parser_start(parser);
    {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "expected match pattern",
            offset,
            offset
        );
    }
    if (!mmay_pattern_at_stop(parser, stops)) {
        mmay_parser_bump(parser);
    }
    return mmay_parser_complete(parser, marker, MMAY_SYNTAX_ERROR);
}

static MarimayCompletedMarker mmay_parse_pattern_inner(
    MarimayParser *parser,
    MarimayPatternStops stops
) {
    MarimayCompletedMarker left = mmay_parse_pattern_primary(parser, stops);
    while (left.valid && mmay_parser_at(parser, MMAY_TOKEN_PIPE)
        && !parser->allocation_failed && !parser->limit_reached) {
        MarimayMarker parent = mmay_parser_precede(parser, left);
        mmay_parser_bump(parser);
        (void)mmay_parse_pattern_primary(parser, stops);
        left = mmay_parser_complete(parser, parent, MMAY_SYNTAX_OR_PATTERN);
    }
    return left;
}

static void mmay_parse_pattern_until(
    MarimayParser *parser,
    MarimayTokenKind first,
    MarimayTokenKind second
) {
    MarimayPatternStops stops;
    MarimayMarker marker = mmay_parser_start(parser);
    stops.first = first;
    stops.second = second;
    (void)mmay_parse_pattern_inner(parser, stops);
    if (!mmay_pattern_at_stop(parser, stops)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "unexpected token after match pattern",
            offset,
            offset
        );
        while (!mmay_pattern_at_stop(parser, stops)
            && !parser->allocation_failed && !parser->limit_reached) {
            mmay_parser_bump(parser);
        }
    }
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_PATTERN);
}

static MarimayCompletedMarker mmay_parse_match_expression(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_KW_MATCH, "expected 'match'");
    mmay_parse_expression_until(
        parser,
        MMAY_TOKEN_LEFT_BRACE,
        MMAY_TOKEN_EOF,
        MMAY_TOKEN_EOF
    );
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACE, "expected '{' after match value");
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        MarimayMarker arm = mmay_parser_start(parser);
        mmay_parser_expect(parser, MMAY_TOKEN_KW_CASE, "expected 'case'");
        mmay_parse_pattern_until(parser, MMAY_TOKEN_KW_IF, MMAY_TOKEN_FAT_ARROW);
        if (mmay_parser_eat(parser, MMAY_TOKEN_KW_IF)) {
            mmay_parse_expression_until(
                parser,
                MMAY_TOKEN_FAT_ARROW,
                MMAY_TOKEN_EOF,
                MMAY_TOKEN_EOF
            );
        }
        mmay_parser_expect(parser, MMAY_TOKEN_FAT_ARROW, "expected '=>' after match pattern");
        if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_BRACE)) {
            mmay_parse_block(parser);
        } else {
            mmay_parse_expression_until(
                parser,
                MMAY_TOKEN_SEMICOLON,
                MMAY_TOKEN_RIGHT_BRACE,
                MMAY_TOKEN_EOF
            );
            mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after match arm");
        }
        (void)mmay_parser_complete(parser, arm, MMAY_SYNTAX_MATCH_ARM);
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            mmay_parser_bump(parser);
        }
    }
    mmay_parser_expect(parser, MMAY_TOKEN_RIGHT_BRACE, "expected '}' after match");
    return mmay_parser_complete(parser, marker, MMAY_SYNTAX_MATCH_EXPRESSION);
}

static MarimayCompletedMarker mmay_parse_closure_expression(
    MarimayParser *parser,
    MarimayExpressionStops stops
) {
    MarimayMarker marker = mmay_parser_start(parser);
    mmay_parse_parameters(parser);
    if (mmay_parser_eat(parser, MMAY_TOKEN_COLON)) {
        mmay_parse_type_until(
            parser,
            MMAY_TOKEN_FAT_ARROW,
            MMAY_TOKEN_EOF,
            MMAY_TOKEN_EOF
        );
    }
    mmay_parser_expect(parser, MMAY_TOKEN_FAT_ARROW, "expected '=>' in closure");
    if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_BRACE)) {
        mmay_parse_block(parser);
    } else {
        (void)mmay_parse_expression_bp(parser, stops, 10);
    }
    return mmay_parser_complete(parser, marker, MMAY_SYNTAX_CLOSURE_EXPRESSION);
}

static MarimayCompletedMarker mmay_parse_expression_prefix(
    MarimayParser *parser,
    MarimayExpressionStops stops
) {
    MarimayTokenKind current = mmay_parser_current(parser);
    MarimayMarker marker;
    if (current == MMAY_TOKEN_KW_MATCH) {
        return mmay_parse_match_expression(parser);
    }
    if (mmay_parser_at_identifier(parser)) {
        marker = mmay_parser_start(parser);
        mmay_parser_bump_as(parser, MMAY_TOKEN_IDENTIFIER);
        return mmay_parser_complete(parser, marker, MMAY_SYNTAX_NAME_EXPRESSION);
    }
    if (mmay_expression_is_literal(current)) {
        marker = mmay_parser_start(parser);
        mmay_parser_bump(parser);
        return mmay_parser_complete(parser, marker, MMAY_SYNTAX_LITERAL_EXPRESSION);
    }
    if (current == MMAY_TOKEN_INTERPOLATED_START) {
        return mmay_parse_interpolated_expression(parser);
    }
    if (current == MMAY_TOKEN_LEFT_PAREN) {
        if (mmay_parser_paren_is_closure(parser)) {
            return mmay_parse_closure_expression(parser, stops);
        }
        return mmay_parse_parenthesized_expression(parser);
    }
    if (current == MMAY_TOKEN_LEFT_BRACKET) {
        return mmay_parse_array_expression(parser);
    }
    if (current == MMAY_TOKEN_BANG || current == MMAY_TOKEN_MINUS
        || current == MMAY_TOKEN_TILDE) {
        marker = mmay_parser_start(parser);
        mmay_parser_bump(parser);
        (void)mmay_parse_expression_bp(parser, stops, 90);
        return mmay_parser_complete(
            parser,
            marker,
            MMAY_SYNTAX_UNARY_EXPRESSION
        );
    }
    if (current == MMAY_TOKEN_KW_SPAWN) {
        size_t offset = mmay_parser_current_offset(parser);
        MarimayCompletedMarker operand;
        marker = mmay_parser_start(parser);
        mmay_parser_bump(parser);
        operand = mmay_parse_expression_bp(parser, stops, 90);
        if (!mmay_parser_completed_contains_kind(
                parser,
                operand,
                MMAY_SYNTAX_CALL_EXPRESSION
            )) {
            (void)mmay_parser_add_diagnostic(
                parser,
                MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "spawn requires a call expression",
                offset,
                offset
            );
        }
        return mmay_parser_complete(
            parser,
            marker,
            MMAY_SYNTAX_SPAWN_EXPRESSION
        );
    }
    marker = mmay_parser_start(parser);
    {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "expected expression",
            offset,
            offset
        );
    }
    if (!mmay_expression_at_stop(parser, stops)) {
        mmay_parser_bump(parser);
    }
    return mmay_parser_complete(parser, marker, MMAY_SYNTAX_ERROR);
}

static MarimayCompletedMarker mmay_parse_expression_bp(
    MarimayParser *parser,
    MarimayExpressionStops stops,
    unsigned int minimum_binding_power
) {
    MarimayCompletedMarker left;
    bool left_is_assignment = false;
    bool left_is_comparison = false;
    bool record_eligible = false;
    left.position = 0;
    left.valid = false;
    if (parser->allocation_failed || parser->limit_reached) {
        return left;
    }
    left = mmay_parse_expression_prefix(parser, stops);
    record_eligible = mmay_parser_completed_kind(parser, left)
        == MMAY_SYNTAX_NAME_EXPRESSION
        && mmay_parser_completed_contains_token_kind(
            parser,
            left,
            MMAY_TOKEN_IDENTIFIER
        );
    while (left.valid && !mmay_expression_at_stop(parser, stops)
        && !parser->allocation_failed && !parser->limit_reached) {
        MarimayTokenKind current = mmay_parser_current(parser);
        MarimayMarker parent;
        unsigned int left_power = 0;
        unsigned int right_power = 0;
        MarimaySyntaxKind syntax_kind = MMAY_SYNTAX_ERROR;
        if (current == MMAY_TOKEN_DOT) {
            parent = mmay_parser_precede(parser, left);
            mmay_parser_bump(parser);
            mmay_parser_expect_identifier(parser, "expected member name after '.'");
            left = mmay_parser_complete(parser, parent, MMAY_SYNTAX_MEMBER_EXPRESSION);
            continue;
        }
        if (current == MMAY_TOKEN_LEFT_PAREN) {
            parent = mmay_parser_precede(parser, left);
            mmay_parse_arguments(parser);
            left = mmay_parser_complete(parser, parent, MMAY_SYNTAX_CALL_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == MMAY_TOKEN_LESS && mmay_parser_generic_call_ahead(parser)) {
            parent = mmay_parser_precede(parser, left);
            mmay_parse_type_arguments(parser);
            mmay_parse_arguments(parser);
            left = mmay_parser_complete(parser, parent, MMAY_SYNTAX_CALL_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == MMAY_TOKEN_LEFT_BRACKET) {
            parent = mmay_parser_precede(parser, left);
            mmay_parse_index_group(parser);
            left = mmay_parser_complete(parser, parent, MMAY_SYNTAX_INDEX_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == MMAY_TOKEN_QUESTION) {
            parent = mmay_parser_precede(parser, left);
            mmay_parser_bump(parser);
            left = mmay_parser_complete(parser, parent, MMAY_SYNTAX_TRY_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == MMAY_TOKEN_LEFT_BRACE) {
            if (!record_eligible) {
                break;
            }
            parent = mmay_parser_precede(parser, left);
            mmay_parse_record_fields(parser);
            left = mmay_parser_complete(parser, parent, MMAY_SYNTAX_RECORD_EXPRESSION);
            record_eligible = false;
            continue;
        }
        if (current == MMAY_TOKEN_KW_AS) {
            MarimayMarker type;
            if (85 < minimum_binding_power) {
                break;
            }
            parent = mmay_parser_precede(parser, left);
            mmay_parser_bump(parser);
            type = mmay_parser_start(parser);
            mmay_parser_expect_identifier(
                parser, "expected integer target type after 'as'"
            );
            (void)mmay_parser_complete(parser, type, MMAY_SYNTAX_TYPE);
            left = mmay_parser_complete(
                parser, parent, MMAY_SYNTAX_CONVERSION_EXPRESSION
            );
            record_eligible = false;
            continue;
        }
        if (!mmay_expression_infix_binding_power(
                current,
                &left_power,
                &right_power,
                &syntax_kind
            ) || left_power < minimum_binding_power) {
            break;
        }
        if ((syntax_kind == MMAY_SYNTAX_ASSIGNMENT_EXPRESSION
                && left_is_assignment)
            || (syntax_kind == MMAY_SYNTAX_BINARY_EXPRESSION
                && left_power == 50 && left_is_comparison)) {
            size_t offset = mmay_parser_current_offset(parser);
            (void)mmay_parser_add_diagnostic(
                parser,
                MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
                syntax_kind == MMAY_SYNTAX_ASSIGNMENT_EXPRESSION
                    ? "chained assignment is not supported"
                    : "comparison chaining is not supported",
                offset,
                offset
            );
        }
        parent = mmay_parser_precede(parser, left);
        mmay_parser_bump(parser);
        (void)mmay_parse_expression_bp(parser, stops, right_power);
        left = mmay_parser_complete(parser, parent, syntax_kind);
        left_is_assignment = syntax_kind == MMAY_SYNTAX_ASSIGNMENT_EXPRESSION;
        left_is_comparison = syntax_kind == MMAY_SYNTAX_BINARY_EXPRESSION
            && left_power == 50;
        record_eligible = false;
    }
    return left;
}

void mmay_parse_expression_until(
    MarimayParser *parser,
    MarimayTokenKind first,
    MarimayTokenKind second,
    MarimayTokenKind third
) {
    MarimayExpressionStops stops;
    MarimayMarker marker = mmay_parser_start(parser);
    stops.first = first;
    stops.second = second;
    stops.third = third;
    (void)mmay_parse_expression_bp(parser, stops, 0);
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_EXPRESSION);
}

static bool mmay_parser_type_at_stop(
    const MarimayParser *parser,
    MarimayTokenKind first,
    MarimayTokenKind second,
    MarimayTokenKind third
) {
    MarimayTokenKind current = mmay_parser_current(parser);
    bool closes_type_arguments = first == MMAY_TOKEN_GREATER
        || first == MMAY_TOKEN_SHIFT_RIGHT
        || second == MMAY_TOKEN_GREATER
        || second == MMAY_TOKEN_SHIFT_RIGHT
        || third == MMAY_TOKEN_GREATER
        || third == MMAY_TOKEN_SHIFT_RIGHT;
    bool hard_boundary = current == MMAY_TOKEN_COMMA
        || current == MMAY_TOKEN_SEMICOLON
        || current == MMAY_TOKEN_EQUAL
        || current == MMAY_TOKEN_LEFT_BRACE
        || current == MMAY_TOKEN_RIGHT_PAREN
        || current == MMAY_TOKEN_RIGHT_BRACKET
        || current == MMAY_TOKEN_RIGHT_BRACE
        || current == MMAY_TOKEN_FAT_ARROW
        || current == MMAY_TOKEN_AMPERSAND;
    return (parser->pending_type_greater != 0 && closes_type_arguments)
        || hard_boundary
        || mmay_parser_is_stop(parser, first, second, third);
}

void mmay_parse_type_list(
    MarimayParser *parser,
    MarimayTokenKind close,
    bool allow_empty
) {
    if (mmay_parser_at(parser, close)) {
        if (!allow_empty) {
            mmay_parse_type_until(
                parser,
                MMAY_TOKEN_COMMA,
                close,
                MMAY_TOKEN_EOF
            );
        }
        return;
    }
    while (!mmay_parser_at(parser, close)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        mmay_parse_type_until(
            parser,
            MMAY_TOKEN_COMMA,
            close,
            MMAY_TOKEN_EOF
        );
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
    }
}

void mmay_parse_type_until(
    MarimayParser *parser,
    MarimayTokenKind first,
    MarimayTokenKind second,
    MarimayTokenKind third
) {
    MarimayMarker marker = mmay_parser_start(parser);
    MarimayTokenKind current;
    if (!marker.valid || parser->allocation_failed || parser->limit_reached) {
        return;
    }
    current = mmay_parser_current(parser);
    if (mmay_parser_type_at_stop(parser, first, second, third)) {
        mmay_parser_expect_identifier(parser, "expected type");
    } else if (mmay_parser_at_identifier(parser)) {
        mmay_parser_bump_as(parser, MMAY_TOKEN_IDENTIFIER);
        while (mmay_parser_eat(parser, MMAY_TOKEN_DOT)) {
            mmay_parser_expect_identifier(parser, "expected type name after '.'");
        }
        if (mmay_parser_at(parser, MMAY_TOKEN_LESS)) {
            mmay_parse_type_arguments(parser);
        }
    } else if (current == MMAY_TOKEN_LEFT_BRACKET) {
        size_t open_offset = mmay_parser_current_offset(parser);
        mmay_parser_bump(parser);
        if (mmay_parser_eat(parser, MMAY_TOKEN_RIGHT_BRACKET)) {
            if (!parser->allocation_failed && !parser->limit_reached) {
                mmay_parse_type_until(parser, first, second, third);
            }
        } else {
            mmay_parse_type_list(parser, MMAY_TOKEN_RIGHT_BRACKET, false);
            mmay_parser_expect_closing_delimiter(
                parser,
                MMAY_TOKEN_RIGHT_BRACKET,
                open_offset,
                "expected ']' after tuple type"
            );
        }
    } else if (current == MMAY_TOKEN_LEFT_PAREN) {
        size_t open_offset = mmay_parser_current_offset(parser);
        mmay_parser_bump(parser);
        mmay_parse_type_list(parser, MMAY_TOKEN_RIGHT_PAREN, true);
        mmay_parser_expect_closing_delimiter(
            parser,
            MMAY_TOKEN_RIGHT_PAREN,
            open_offset,
            "expected ')' after function parameter types"
        );
        mmay_parser_expect(
            parser,
            MMAY_TOKEN_FAT_ARROW,
            "expected '=>' in function type"
        );
        if (!parser->allocation_failed && !parser->limit_reached) {
            mmay_parse_type_until(parser, first, second, third);
        }
    } else {
        mmay_parser_expect_identifier(parser, "expected type");
        if (!mmay_parser_type_at_stop(parser, first, second, third)
            && !mmay_parser_at(parser, MMAY_TOKEN_EOF)) {
            mmay_parser_bump(parser);
        }
    }
    if (!mmay_parser_type_at_stop(parser, first, second, third)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
            "unexpected token after type",
            offset,
            offset
        );
        while (!mmay_parser_type_at_stop(parser, first, second, third)
            && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
            && !parser->allocation_failed && !parser->limit_reached) {
            mmay_parser_bump(parser);
        }
    }
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_TYPE);
}
