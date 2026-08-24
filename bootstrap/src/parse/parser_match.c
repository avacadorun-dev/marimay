#include "parser_internal.h"

static bool mmay_match_tail_can_start(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_IDENTIFIER
        || kind == MMAY_TOKEN_INTEGER
        || kind == MMAY_TOKEN_FLOAT
        || kind == MMAY_TOKEN_STRING
        || kind == MMAY_TOKEN_RAW_STRING
        || kind == MMAY_TOKEN_BYTE_STRING
        || kind == MMAY_TOKEN_RUNE
        || kind == MMAY_TOKEN_KW_TRUE
        || kind == MMAY_TOKEN_KW_FALSE
        || kind == MMAY_TOKEN_INTERPOLATED_START
        || kind == MMAY_TOKEN_LEFT_PAREN
        || kind == MMAY_TOKEN_LEFT_BRACKET
        || kind == MMAY_TOKEN_BANG
        || kind == MMAY_TOKEN_MINUS
        || kind == MMAY_TOKEN_TILDE
        || kind == MMAY_TOKEN_KW_SPAWN
        || kind == MMAY_TOKEN_KW_MATCH;
}

static bool mmay_match_tail_ahead(const MarimayParser *parser) {
    size_t index = mmay_parser_significant_index(parser);
    size_t parens = 0;
    size_t brackets = 0;
    size_t braces = 0;
    const MarimayToken *first = mmay_parser_raw_token(parser, index);
    if (first == NULL || !mmay_match_tail_can_start(first->kind)) {
        return false;
    }
    while (index < parser->lexed->tokens.length) {
        const MarimayToken *token = mmay_parser_raw_token(parser, index);
        if (token == NULL) {
            return false;
        }
        if (token->kind == MMAY_TOKEN_SEMICOLON
            && parens == 0 && brackets == 0 && braces == 0) {
            return false;
        }
        if (token->kind == MMAY_TOKEN_RIGHT_BRACE && braces == 0
            && parens == 0 && brackets == 0) {
            return true;
        }
        if (token->kind == MMAY_TOKEN_LEFT_PAREN) {
            parens += 1;
        } else if (token->kind == MMAY_TOKEN_RIGHT_PAREN && parens != 0) {
            parens -= 1;
        } else if (token->kind == MMAY_TOKEN_LEFT_BRACKET) {
            brackets += 1;
        } else if (token->kind == MMAY_TOKEN_RIGHT_BRACKET && brackets != 0) {
            brackets -= 1;
        } else if (token->kind == MMAY_TOKEN_LEFT_BRACE) {
            braces += 1;
        } else if (token->kind == MMAY_TOKEN_RIGHT_BRACE && braces != 0) {
            braces -= 1;
        }
        index += 1;
    }
    return false;
}

void mmay_parse_match_arm_block(MarimayParser *parser) {
    MarimayMarker block = mmay_parser_start(parser);
    size_t open_offset = mmay_parser_current_offset(parser);
    mmay_parser_expect(parser, MMAY_TOKEN_LEFT_BRACE, "expected '{' for match arm");
    while (!mmay_parser_at(parser, MMAY_TOKEN_RIGHT_BRACE)
        && !mmay_parser_at(parser, MMAY_TOKEN_EOF)
        && !parser->allocation_failed && !parser->limit_reached) {
        if (mmay_match_tail_ahead(parser)) {
            MarimayMarker tail = mmay_parser_start(parser);
            mmay_parse_expression_until(
                parser,
                MMAY_TOKEN_RIGHT_BRACE,
                MMAY_TOKEN_EOF,
                MMAY_TOKEN_EOF
            );
            (void)mmay_parser_complete(
                parser,
                tail,
                MMAY_SYNTAX_MATCH_ARM_TAIL
            );
            break;
        }
        mmay_parse_statement(parser);
    }
    mmay_parser_expect_closing_delimiter(
        parser,
        MMAY_TOKEN_RIGHT_BRACE,
        open_offset,
        "expected '}' after match arm"
    );
    (void)mmay_parser_complete(parser, block, MMAY_SYNTAX_BLOCK);
}
