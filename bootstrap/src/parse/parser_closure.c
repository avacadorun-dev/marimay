#include "parser_internal.h"

bool mmay_parser_paren_is_closure(const MarimayParser *parser) {
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

static void mmay_parse_closure_parameters(MarimayParser *parser) {
    MarimayMarker list = mmay_parser_start(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_PAREN, "expected parameter list");
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_PAREN)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        MarimayMarker parameter = mmay_parser_start(parser);
        mmay_parser_expect_identifier(parser, "expected parameter name");
        if (mmay_parser_eat(parser, MMAY_TOKEN_COLON)) {
            mmay_parse_type_until(
                parser,
                MMAY_TOKEN_COMMA,
                MMAY_TOKEN_RIGHT_PAREN,
                MMAY_TOKEN_EOF
            );
        }
        (void)mmay_parser_complete(
            parser, parameter, MMAY_SYNTAX_PARAMETER
        );
        if (!mmay_parser_eat(parser, MMAY_TOKEN_COMMA)) {
            break;
        }
    }
    mmay_parser_expect(parser, MMAY_TOKEN_RIGHT_PAREN, "expected ')' after parameters");
    (void)mmay_parser_complete(parser, list, MMAY_SYNTAX_PARAMETERS);
}

MarimayCompletedMarker mmay_parse_closure_expression(
    MarimayParser *parser,
    MarimayTokenKind first,
    MarimayTokenKind second,
    MarimayTokenKind third
) {
    MarimayMarker marker = mmay_parser_start(parser);
    mmay_parse_closure_parameters(parser);
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
        mmay_parse_expression_until(parser, first, second, third);
    }
    return mmay_parser_complete(parser, marker, MMAY_SYNTAX_CLOSURE_EXPRESSION);
}
