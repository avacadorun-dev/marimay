#ifndef RUDA_PARSE_PARSER_INTERNAL_H
#define RUDA_PARSE_PARSER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/parse/parser.h"

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

const RudaToken *ruda_parser_raw_token(const RudaParser *parser, size_t index);
size_t ruda_parser_skip_trivia(const RudaParser *parser, size_t index);
size_t ruda_parser_significant_index(const RudaParser *parser);
const RudaToken *ruda_parser_current_token(const RudaParser *parser);
RudaTokenKind ruda_parser_current(const RudaParser *parser);
size_t ruda_parser_current_offset(const RudaParser *parser);
bool ruda_parser_at(const RudaParser *parser, RudaTokenKind kind);
bool ruda_token_is_contextual_keyword(RudaTokenKind kind);
bool ruda_parser_at_identifier(const RudaParser *parser);
RudaTokenKind ruda_parser_next_significant_kind(const RudaParser *parser);
RudaTokenKind ruda_parser_second_next_significant_kind(const RudaParser *parser);
bool ruda_parser_add_diagnostic(RudaParser *parser, const char *code,
    const char *message, size_t start, size_t end);
RudaMarker ruda_parser_start(RudaParser *parser);
RudaCompletedMarker ruda_parser_complete(RudaParser *parser,
    RudaMarker marker, RudaSyntaxKind kind);
RudaMarker ruda_parser_precede(RudaParser *parser,
    RudaCompletedMarker completed);
bool ruda_parser_completed_contains_kind(const RudaParser *parser,
    RudaCompletedMarker completed, RudaSyntaxKind kind);
RudaSyntaxKind ruda_parser_completed_kind(const RudaParser *parser,
    RudaCompletedMarker completed);
bool ruda_parser_completed_contains_token_kind(const RudaParser *parser,
    RudaCompletedMarker completed, RudaTokenKind kind);
void ruda_parser_bump_as(RudaParser *parser, RudaTokenKind significant_kind);
void ruda_parser_bump(RudaParser *parser);
bool ruda_parser_eat_identifier(RudaParser *parser);
bool ruda_parser_eat(RudaParser *parser, RudaTokenKind kind);
void ruda_parser_expect(RudaParser *parser, RudaTokenKind kind,
    const char *message);
void ruda_parser_expect_identifier(RudaParser *parser, const char *message);
bool ruda_parser_is_stop(const RudaParser *parser, RudaTokenKind first,
    RudaTokenKind second, RudaTokenKind third);
void ruda_parser_expect_closing_delimiter(RudaParser *parser,
    RudaTokenKind close, size_t open_offset, const char *message);

void ruda_parse_expression_until(RudaParser *parser, RudaTokenKind first,
    RudaTokenKind second, RudaTokenKind third);
void ruda_parse_type_until(RudaParser *parser, RudaTokenKind first,
    RudaTokenKind second, RudaTokenKind third);
bool ruda_parser_type_at_greater(const RudaParser *parser);
void ruda_parser_expect_type_greater(RudaParser *parser);
void ruda_parser_begin_type_angle(RudaParser *parser);
void ruda_parser_end_type_angle(RudaParser *parser);
void ruda_parse_type_list(RudaParser *parser, RudaTokenKind close,
    bool allow_empty);
void ruda_parse_arguments(RudaParser *parser);
bool ruda_parser_yield_is_name_expression(const RudaParser *parser);
bool ruda_parser_contextual_statement_starts(const RudaParser *parser,
    RudaTokenKind current);
void ruda_parse_block(RudaParser *parser);
void ruda_parse_parameters(RudaParser *parser);
void ruda_parse_top_decl(RudaParser *parser);

#endif
