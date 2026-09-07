#include "parser_internal.h"

#include "mmay/generated/diagnostic_codes.h"

static void mmay_parse_type_parameters(MarimayParser *parser) {
    MarimayMarker list = mmay_parser_start(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LESS, "expected '<' for type parameters");
    mmay_parser_begin_type_angle(parser);
    if (mmay_parser_type_at_greater(parser)) {
        MarimayMarker parameter = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected type parameter name");
        (void)mmay_parser_complete(
            parser,
            parameter,
            MMAY_SYNTAX_TYPE_PARAMETER
        );
    }
    while (!mmay_parser_type_at_greater(parser)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        MarimayMarker parameter = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected type parameter name");
        if (mmay_parser_eat(parser, MMAY_TOKEN_COLON)) {
            do {
                mmay_parse_type_until(
                    parser,
                    MMAY_TOKEN_AMPERSAND,
                    MMAY_TOKEN_COMMA,
                    MMAY_TOKEN_GREATER
                );
            } while (mmay_parser_eat(parser, MMAY_TOKEN_AMPERSAND));
        }
        mmay_parser_complete(parser, parameter, MMAY_SYNTAX_TYPE_PARAMETER);
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
    }
    mmay_parser_expect_type_greater(parser);
    mmay_parser_end_type_angle(parser);
    mmay_parser_complete(parser, list, MMAY_SYNTAX_TYPE_PARAMETERS);
}

void mmay_parse_parameters(MarimayParser *parser) {
    MarimayMarker list = mmay_parser_start(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_PAREN, "expected parameter list");
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_PAREN)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        MarimayMarker parameter = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected parameter name");
        mmay_parser_expect(parser, MMAY_TOKEN_COLON, "expected ':' after parameter name");
        mmay_parse_type_until(
            parser,
            MMAY_TOKEN_COMMA,
            MMAY_TOKEN_RIGHT_PAREN,
            MMAY_TOKEN_EOF
        );
        mmay_parser_complete(parser, parameter, MMAY_SYNTAX_PARAMETER);
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
    }
    mmay_parser_expect(parser, MMAY_TOKEN_RIGHT_PAREN, "expected ')' after parameters");
    mmay_parser_complete(parser, list, MMAY_SYNTAX_PARAMETERS);
}

static void mmay_parse_binding_decl(
    MarimayParser *parser,
    MarimayMarker marker,
    MarimaySyntaxKind kind
) {
    mmay_parser_bump(parser);
    if (mmay_parser_at(parser, MMAY_TOKEN_UNDERSCORE)) {
        mmay_parser_bump(parser);
    } else {
        mmay_parser_expect_identifier(parser, "expected binding name");
    }
    if (mmay_parser_eat(parser, MMAY_TOKEN_COLON)) {
        mmay_parse_type_until(
            parser,
            MMAY_TOKEN_EQUAL,
            MMAY_TOKEN_SEMICOLON,
            MMAY_TOKEN_EOF
        );
    }
    if (mmay_parser_eat(parser, MMAY_TOKEN_EQUAL)) {
        mmay_parse_expression_until(
            parser,
            MMAY_TOKEN_SEMICOLON,
            MMAY_TOKEN_EOF,
            MMAY_TOKEN_EOF
        );
    } else if (kind == MMAY_SYNTAX_CONST_DECL) {
        mmay_parser_expect(
            parser,
            MMAY_TOKEN_EQUAL,
            "expected '=' in const declaration"
        );
        mmay_parse_expression_until(
            parser,
            MMAY_TOKEN_SEMICOLON,
            MMAY_TOKEN_EOF,
            MMAY_TOKEN_EOF
        );
    }
    mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after binding");
    mmay_parser_complete(parser, marker, kind);
}

void mmay_parse_block(MarimayParser *parser);

static void mmay_parse_for_binding(MarimayParser *parser) {
    if (mmay_parser_at_identifier(parser)) {
        mmay_parser_bump_as(parser, MMAY_TOKEN_IDENTIFIER);
        return;
    }
    if (mmay_parser_at(parser, MMAY_TOKEN_UNDERSCORE)) {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            "loop binding must be an identifier",
            offset,
            offset
        );
        mmay_parser_bump(parser);
        return;
    }
    if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_PAREN)
        || mmay_parser_at(parser, MMAY_TOKEN_LEFT_BRACKET)) {
        MarimayMarker invalid = mmay_parser_start(parser);
        mmay_parser_expect_identifier(
            parser, "loop binding must be a single identifier"
        );
        /* D068 rejects compound bindings. Keep their tokens out of the
           iterable expression so recovery preserves the actual loop body. */
        while (!mmay_parser_is_stop(parser, MMAY_TOKEN_KW_IN,
                MMAY_TOKEN_LEFT_BRACE, MMAY_TOKEN_RIGHT_BRACE)
            && !parser->allocation_failed && !parser->limit_reached) {
            mmay_parser_bump(parser);
        }
        (void)mmay_parser_complete(parser, invalid, MMAY_SYNTAX_ERROR);
        return;
    }
    mmay_parser_expect_identifier(parser, "expected loop binding name");
}

static void mmay_parse_for_header(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    mmay_parse_for_binding(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_KW_IN, "expected 'in' after loop binding");
    mmay_parse_expression_until(
        parser,
        MMAY_TOKEN_LEFT_BRACE,
        MMAY_TOKEN_EOF,
        MMAY_TOKEN_EOF
    );
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_FOR_HEADER);
}

void mmay_parse_statement(MarimayParser *parser) {
    MarimayTokenKind current = mmay_parser_current(parser);
    MarimayMarker marker = mmay_parser_start(parser);
    if (current == MMAY_TOKEN_KW_CONST
        && mmay_parser_contextual_statement_starts(parser, current)) {
        mmay_parse_binding_decl(parser, marker, MMAY_SYNTAX_CONST_DECL);
    } else if (current == MMAY_TOKEN_KW_LET
        && mmay_parser_contextual_statement_starts(parser, current)) {
        mmay_parse_binding_decl(parser, marker, MMAY_SYNTAX_LET_DECL);
    } else if (current == MMAY_TOKEN_KW_RETURN) {
        mmay_parser_bump(parser);
        if (!mmay_parser_at(parser, MMAY_TOKEN_SEMICOLON)) {
            mmay_parse_expression_until(
                parser,
                MMAY_TOKEN_SEMICOLON,
                MMAY_TOKEN_EOF,
                MMAY_TOKEN_EOF
            );
        }
        mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after return");
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_RETURN_STATEMENT);
    } else if (current == MMAY_TOKEN_KW_DEFER) {
        mmay_parser_bump(parser);
        mmay_parse_expression_until(
            parser,
            MMAY_TOKEN_SEMICOLON,
            MMAY_TOKEN_EOF,
            MMAY_TOKEN_EOF
        );
        mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after defer");
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_DEFER_STATEMENT);
    } else if (current == MMAY_TOKEN_KW_IF || current == MMAY_TOKEN_KW_WHILE
        || (current == MMAY_TOKEN_KW_FOR
            && mmay_parser_contextual_statement_starts(parser, current))) {
        MarimaySyntaxKind statement_kind = current == MMAY_TOKEN_KW_IF
            ? MMAY_SYNTAX_IF_STATEMENT
            : (current == MMAY_TOKEN_KW_WHILE
                ? MMAY_SYNTAX_WHILE_STATEMENT
                : MMAY_SYNTAX_FOR_STATEMENT);
        mmay_parser_bump(parser);
        if (current == MMAY_TOKEN_KW_FOR) {
            mmay_parse_for_header(parser);
        } else {
            mmay_parse_expression_until(
                parser,
                MMAY_TOKEN_LEFT_BRACE,
                MMAY_TOKEN_EOF,
                MMAY_TOKEN_EOF
            );
        }
        mmay_parse_block(parser);
        if (current == MMAY_TOKEN_KW_IF
            && mmay_parser_at(parser, MMAY_TOKEN_KW_ELSE)
            && (mmay_parser_next_significant_kind(parser) == MMAY_TOKEN_KW_IF
                || mmay_parser_next_significant_kind(parser)
                    == MMAY_TOKEN_LEFT_BRACE)) {
            mmay_parser_bump(parser);
            if (mmay_parser_at(parser, MMAY_TOKEN_KW_IF)) {
                mmay_parse_statement(parser);
            } else {
                mmay_parse_block(parser);
            }
        }
        mmay_parser_complete(parser, marker, statement_kind);
    } else if (current == MMAY_TOKEN_KW_SCOPE
        && mmay_parser_contextual_statement_starts(parser, current)) {
        mmay_parser_bump(parser);
        mmay_parse_block(parser);
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_SCOPE_STATEMENT);
    } else if (current == MMAY_TOKEN_KW_UNSAFE
        && mmay_parser_contextual_statement_starts(parser, current)) {
        mmay_parser_bump(parser);
        mmay_parse_block(parser);
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_UNSAFE_STATEMENT);
    } else if (current == MMAY_TOKEN_LEFT_BRACE) {
        mmay_parse_block(parser);
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_BLOCK_STATEMENT);
    } else if ((current == MMAY_TOKEN_KW_BREAK
            || current == MMAY_TOKEN_KW_CONTINUE)
        && mmay_parser_contextual_statement_starts(parser, current)) {
        mmay_parser_bump(parser);
        mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after loop control");
        mmay_parser_complete(
            parser,
            marker,
            current == MMAY_TOKEN_KW_BREAK
                ? MMAY_SYNTAX_BREAK_STATEMENT
                : MMAY_SYNTAX_CONTINUE_STATEMENT
        );
    } else {
        mmay_parse_expression_until(
            parser,
            MMAY_TOKEN_SEMICOLON,
            MMAY_TOKEN_RIGHT_BRACE,
            MMAY_TOKEN_EOF
        );
        mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after expression");
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_EXPRESSION_STATEMENT);
    }
}

void mmay_parse_block(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACE, "expected '{' to start block");
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        mmay_parse_statement(parser);
        if (parser->raw_index == before && !parser->limit_reached
            && !parser->allocation_failed) {
            MarimayMarker error = mmay_parser_start(parser);
            size_t offset = mmay_parser_current_offset(parser);
            (void)mmay_parser_add_diagnostic(
                parser,
                MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "unexpected token in block",
                offset,
                offset
            );
            mmay_parser_bump(parser);
            mmay_parser_complete(parser, error, MMAY_SYNTAX_ERROR);
        }
    }
    mmay_parser_expect(parser, MMAY_TOKEN_RIGHT_BRACE, "expected '}' to close block");
    mmay_parser_complete(parser, marker, MMAY_SYNTAX_BLOCK);
}

static void mmay_parse_attribute(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_AT, "expected '@'");
    mmay_parser_expect_identifier(parser, "expected attribute name");
    if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_PAREN)) {
        mmay_parse_arguments(parser);
    }
    mmay_parser_complete(parser, marker, MMAY_SYNTAX_ATTRIBUTE);
}

static void mmay_parse_import_decl(MarimayParser *parser, MarimayMarker marker) {
    MarimayMarker clause;
    mmay_parser_expect(parser, MMAY_TOKEN_KW_IMPORT, "expected 'import'");
    clause = mmay_parser_start(parser);
    if (mmay_parser_eat(parser, MMAY_TOKEN_STAR)) {
        mmay_parser_expect(parser, MMAY_TOKEN_KW_AS, "expected 'as' after '*'");
        mmay_parser_expect_identifier(parser, "expected import namespace");
    } else if (mmay_parser_eat(parser, MMAY_TOKEN_LEFT_BRACE)) {
        while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
            && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
            && !parser->allocation_failed && !parser->limit_reached) {
            MarimayMarker item = mmay_parser_start(parser);
            mmay_parser_expect_identifier(parser, "expected imported name");
            if (mmay_parser_eat(parser, MMAY_TOKEN_KW_AS)) {
                mmay_parser_expect_identifier(parser, "expected import alias");
            }
            mmay_parser_complete(parser, item, MMAY_SYNTAX_IMPORT_ITEM);
            if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
                break;
            }
        }
        mmay_parser_expect(parser, MMAY_TOKEN_RIGHT_BRACE, "expected '}' after imports");
    } else {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            "expected named or namespace import clause",
            offset,
            offset
        );
    }
    mmay_parser_complete(parser, clause, MMAY_SYNTAX_IMPORT_CLAUSE);
    mmay_parser_expect(parser, MMAY_TOKEN_KW_FROM, "expected 'from' in import");
    mmay_parser_expect(parser, MMAY_TOKEN_STRING, "expected import path string");
    mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after import");
    mmay_parser_complete(parser, marker, MMAY_SYNTAX_IMPORT_DECL);
}

static void mmay_parse_field_decl(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    if (mmay_parser_at(parser, MMAY_TOKEN_KW_PUBLIC)
        && mmay_parser_next_significant_kind(parser) != MMAY_TOKEN_COLON) {
        mmay_parser_bump(parser);
    }
    if (mmay_parser_at(parser, MMAY_TOKEN_KW_MUTABLE)
        && mmay_parser_next_significant_kind(parser) != MMAY_TOKEN_COLON) {
        mmay_parser_bump(parser);
    }
    mmay_parser_expect_identifier(parser, "expected field name");
    mmay_parser_expect(parser, MMAY_TOKEN_COLON, "expected ':' after field name");
    mmay_parse_type_until(
        parser,
        MMAY_TOKEN_COMMA,
        MMAY_TOKEN_RIGHT_BRACE,
        MMAY_TOKEN_EOF
    );
    mmay_parser_complete(parser, marker, MMAY_SYNTAX_FIELD_DECL);
}

static void mmay_parse_positional_payload(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(
        parser,
        MMAY_TOKEN_LEFT_PAREN,
        "expected '(' for positional payload"
    );
    mmay_parse_type_list(parser, MMAY_TOKEN_RIGHT_PAREN, false);
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_PAREN,
        open_offset,
        "expected ')' after positional payload"
    );
    (void)mmay_parser_complete(
        parser,
        marker,
        MMAY_SYNTAX_POSITIONAL_PAYLOAD
    );
}

static void mmay_parse_named_payload(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(
        parser,
        MMAY_TOKEN_LEFT_BRACE,
        "expected '{' for named payload"
    );
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        MarimayMarker field = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected payload field name");
        mmay_parser_expect(
            parser,
            MMAY_TOKEN_COLON,
            "expected ':' after payload field name"
        );
        mmay_parse_type_until(
            parser,
            MMAY_TOKEN_COMMA,
            MMAY_TOKEN_RIGHT_BRACE,
            MMAY_TOKEN_EOF
        );
        (void)mmay_parser_complete(
            parser,
            field,
            MMAY_SYNTAX_PAYLOAD_FIELD
        );
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            mmay_parser_bump(parser);
        }
    }
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_BRACE,
        open_offset,
        "expected '}' after named payload"
    );
    (void)mmay_parser_complete(parser, marker, MMAY_SYNTAX_NAMED_PAYLOAD);
}

static void mmay_parse_type_decl(MarimayParser *parser, MarimayMarker marker) {
    mmay_parser_expect(parser, MMAY_TOKEN_KW_TYPE, "expected 'type'");
    mmay_parser_expect_identifier(parser, "expected type name");
    if (mmay_parser_at(parser, MMAY_TOKEN_LESS)) {
        mmay_parse_type_parameters(parser);
    }
    if (mmay_parser_eat(parser, MMAY_TOKEN_EQUAL)) {
        mmay_parse_type_until(
            parser,
            MMAY_TOKEN_SEMICOLON,
            MMAY_TOKEN_EOF,
            MMAY_TOKEN_EOF
        );
        mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after type alias");
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_TYPE_ALIAS_DECL);
        return;
    }
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACE, "expected record body");
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        mmay_parse_field_decl(parser);
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
    }
    mmay_parser_expect(parser, MMAY_TOKEN_RIGHT_BRACE, "expected '}' after record");
    mmay_parser_complete(parser, marker, MMAY_SYNTAX_RECORD_DECL);
}

static void mmay_parse_enum_decl(MarimayParser *parser, MarimayMarker marker) {
    mmay_parser_expect(parser, MMAY_TOKEN_KW_ENUM, "expected 'enum'");
    mmay_parser_expect_identifier(parser, "expected enum name");
    if (mmay_parser_at(parser, MMAY_TOKEN_LESS)) {
        mmay_parse_type_parameters(parser);
    }
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACE, "expected enum body");
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        MarimayMarker variant = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected enum variant");
        if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_PAREN)) {
            mmay_parse_positional_payload(parser);
        } else if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_BRACE)) {
            mmay_parse_named_payload(parser);
        }
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)
            && !mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)) {
            mmay_parser_expect(
                parser,
                MMAY_TOKEN_COMMA,
                "expected ',' between enum variants"
            );
        }
        mmay_parser_complete(parser, variant, MMAY_SYNTAX_ENUM_VARIANT);
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            size_t offset = mmay_parser_current_offset(parser);
            (void)mmay_parser_add_diagnostic(
                parser,
                MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "unexpected token in enum body",
                offset,
                offset
            );
            mmay_parser_bump(parser);
        }
    }
    mmay_parser_expect(parser, MMAY_TOKEN_RIGHT_BRACE, "expected '}' after enum");
    mmay_parser_complete(parser, marker, MMAY_SYNTAX_ENUM_DECL);
}

static void mmay_parse_interface_decl(MarimayParser *parser, MarimayMarker marker) {
    mmay_parser_expect(parser, MMAY_TOKEN_KW_INTERFACE, "expected 'interface'");
    mmay_parser_expect_identifier(parser, "expected interface name");
    if (mmay_parser_at(parser, MMAY_TOKEN_LESS)) {
        mmay_parse_type_parameters(parser);
    }
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACE, "expected interface body");
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        size_t before = parser->raw_index;
        MarimayMarker item = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected interface item");
        if (mmay_parser_at(parser, MMAY_TOKEN_LESS)) {
            mmay_parse_type_parameters(parser);
        }
        if (mmay_parser_at(parser, MMAY_TOKEN_LEFT_PAREN)) {
            mmay_parse_parameters(parser);
            mmay_parser_expect(parser, MMAY_TOKEN_COLON, "expected interface return type");
            mmay_parse_type_until(
                parser,
                MMAY_TOKEN_SEMICOLON,
                MMAY_TOKEN_RIGHT_BRACE,
                MMAY_TOKEN_EOF
            );
        }
        mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after interface item");
        mmay_parser_complete(parser, item, MMAY_SYNTAX_INTERFACE_ITEM);
        if (parser->raw_index == before && !parser->allocation_failed
            && !parser->limit_reached) {
            size_t offset = mmay_parser_current_offset(parser);
            (void)mmay_parser_add_diagnostic(
                parser,
                MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "unexpected token in interface body",
                offset,
                offset
            );
            mmay_parser_bump(parser);
        }
    }
    mmay_parser_expect(parser, MMAY_TOKEN_RIGHT_BRACE, "expected '}' after interface");
    mmay_parser_complete(parser, marker, MMAY_SYNTAX_INTERFACE_DECL);
}

static void mmay_parse_function_decl(
    MarimayParser *parser,
    MarimayMarker marker,
    bool external
) {
    if (external) {
        mmay_parser_expect(parser, MMAY_TOKEN_KW_EXTERN, "expected 'extern'");
        mmay_parser_expect(parser, MMAY_TOKEN_STRING, "expected extern ABI string");
    }
    (void)mmay_parser_eat(parser, MMAY_TOKEN_KW_UNSAFE);
    mmay_parser_expect(parser, MMAY_TOKEN_KW_FN, "expected 'fn'");
    mmay_parser_expect_identifier(parser, "expected function name");
    if (mmay_parser_at(parser, MMAY_TOKEN_LESS)) {
        mmay_parse_type_parameters(parser);
    }
    mmay_parse_parameters(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_COLON, "expected function return type");
    mmay_parse_type_until(
        parser,
        external ? MMAY_TOKEN_SEMICOLON : MMAY_TOKEN_LEFT_BRACE,
        MMAY_TOKEN_EOF,
        MMAY_TOKEN_EOF
    );
    if (external) {
        mmay_parser_expect(parser, MMAY_TOKEN_SEMICOLON, "expected ';' after extern");
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_EXTERN_DECL);
    } else {
        mmay_parse_block(parser);
        mmay_parser_complete(parser, marker, MMAY_SYNTAX_FUNCTION_DECL);
    }
}

static bool mmay_parser_at_top_decl_start(const MarimayParser *parser) {
    MarimayTokenKind current = mmay_parser_current(parser);
    return current == MMAY_TOKEN_AT
        || current == MMAY_TOKEN_KW_PUBLIC
        || current == MMAY_TOKEN_KW_IMPORT
        || current == MMAY_TOKEN_KW_TYPE
        || current == MMAY_TOKEN_KW_ENUM
        || current == MMAY_TOKEN_KW_INTERFACE
        || current == MMAY_TOKEN_KW_FN
        || current == MMAY_TOKEN_KW_UNSAFE
        || current == MMAY_TOKEN_KW_EXTERN
        || current == MMAY_TOKEN_KW_CONST;
}

static void mmay_parse_invalid_declaration(MarimayParser *parser, MarimayMarker marker) {
    size_t start = mmay_parser_current_offset(parser);
    (void)mmay_parser_add_diagnostic(
        parser,
        MMAY_DIAGNOSTIC_INVALID_DECLARATION,
        "expected import, type, enum, interface, function, const, or extern declaration",
        start,
        start
    );
    while (!mmay_parser_at(parser, MMAY_TOKEN_SEMICOLON)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !mmay_parser_at_top_decl_start(parser)
        && !parser->allocation_failed && !parser->limit_reached) {
        mmay_parser_bump(parser);
    }
    (void)mmay_parser_eat(parser, MMAY_TOKEN_SEMICOLON);
    mmay_parser_complete(parser, marker, MMAY_SYNTAX_ERROR);
}

void mmay_parse_top_decl(MarimayParser *parser) {
    MarimayMarker marker = mmay_parser_start(parser);
    MarimayTokenKind current;
    bool has_modifier = false;
    while (mmay_parser_at(parser, MMAY_TOKEN_AT)
        && !parser->allocation_failed && !parser->limit_reached) {
        has_modifier = true;
        mmay_parse_attribute(parser);
    }
    if (mmay_parser_eat(parser, MMAY_TOKEN_KW_PUBLIC)) {
        has_modifier = true;
    }
    current = mmay_parser_current(parser);
    if (current == MMAY_TOKEN_KW_IMPORT) {
        if (has_modifier) {
            size_t offset = mmay_parser_current_offset(parser);
            (void)mmay_parser_add_diagnostic(
                parser,
                MMAY_DIAGNOSTIC_UNEXPECTED_TOKEN,
                "imports cannot have declaration attributes or visibility",
                offset,
                offset
            );
        }
        mmay_parse_import_decl(parser, marker);
    } else if (current == MMAY_TOKEN_KW_TYPE) {
        mmay_parse_type_decl(parser, marker);
    } else if (current == MMAY_TOKEN_KW_ENUM) {
        mmay_parse_enum_decl(parser, marker);
    } else if (current == MMAY_TOKEN_KW_INTERFACE) {
        mmay_parse_interface_decl(parser, marker);
    } else if (current == MMAY_TOKEN_KW_FN || current == MMAY_TOKEN_KW_UNSAFE) {
        mmay_parse_function_decl(parser, marker, false);
    } else if (current == MMAY_TOKEN_KW_EXTERN) {
        mmay_parse_function_decl(parser, marker, true);
    } else if (current == MMAY_TOKEN_KW_CONST) {
        mmay_parse_binding_decl(parser, marker, MMAY_SYNTAX_CONST_DECL);
    } else {
        mmay_parse_invalid_declaration(parser, marker);
    }
}
