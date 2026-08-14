#include "parser_internal.h"

#include "ruda/generated/diagnostic_codes.h"

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

void ruda_parse_parameters(RudaParser *parser) {
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

void ruda_parse_block(RudaParser *parser);

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

void ruda_parse_block(RudaParser *parser) {
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

void ruda_parse_top_decl(RudaParser *parser) {
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
