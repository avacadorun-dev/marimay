#ifndef MMAY_PARSE_PARSER_INTERNAL_H
#define MMAY_PARSE_PARSER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/parse/parser.h"

typedef enum MarimayParseEventKind {
    MMAY_PARSE_EVENT_TOMBSTONE,
    MMAY_PARSE_EVENT_START,
    MMAY_PARSE_EVENT_TOKEN,
    MMAY_PARSE_EVENT_SYNTHETIC_TOKEN,
    MMAY_PARSE_EVENT_FINISH
} MarimayParseEventKind;

typedef struct MarimayParseStartEvent {
    MarimaySyntaxKind syntax_kind;
    size_t forward_parent;
} MarimayParseStartEvent;

typedef struct MarimayParseTokenEvent {
    size_t token_index;
    MarimayTokenKind kind;
} MarimayParseTokenEvent;

typedef struct MarimayParseEvent {
    MarimayParseEventKind kind;
    union {
        MarimayParseStartEvent start;
        MarimayParseTokenEvent token;
        MarimaySyntaxToken synthetic_token;
    } as;
} MarimayParseEvent;

typedef struct MarimayMarker {
    size_t position;
    bool valid;
} MarimayMarker;

typedef struct MarimayCompletedMarker {
    size_t position;
    bool valid;
} MarimayCompletedMarker;

typedef struct MarimayParser {
    const MarimayLexResult *lexed;
    MarimayParseResult *result;
    MarimayParseOptions options;
    MarimayVector events;
    size_t raw_index;
    size_t nesting;
    size_t pending_type_greater;
    size_t type_angle_depth;
    bool allocation_failed;
    bool limit_reached;
} MarimayParser;

const MarimayToken *mmay_parser_raw_token(const MarimayParser *parser, size_t index);
size_t mmay_parser_skip_trivia(const MarimayParser *parser, size_t index);
size_t mmay_parser_significant_index(const MarimayParser *parser);
const MarimayToken *mmay_parser_current_token(const MarimayParser *parser);
MarimayTokenKind mmay_parser_current(const MarimayParser *parser);
size_t mmay_parser_current_offset(const MarimayParser *parser);
bool mmay_parser_at(const MarimayParser *parser, MarimayTokenKind kind);
bool mmay_token_is_contextual_keyword(MarimayTokenKind kind);
bool mmay_parser_at_identifier(const MarimayParser *parser);
MarimayTokenKind mmay_parser_next_significant_kind(const MarimayParser *parser);
MarimayTokenKind mmay_parser_second_next_significant_kind(const MarimayParser *parser);
bool mmay_parser_add_diagnostic(MarimayParser *parser, const char *code,
    const char *message, size_t start, size_t end);
MarimayMarker mmay_parser_start(MarimayParser *parser);
MarimayCompletedMarker mmay_parser_complete(MarimayParser *parser,
    MarimayMarker marker, MarimaySyntaxKind kind);
MarimayMarker mmay_parser_precede(MarimayParser *parser,
    MarimayCompletedMarker completed);
bool mmay_parser_completed_contains_kind(const MarimayParser *parser,
    MarimayCompletedMarker completed, MarimaySyntaxKind kind);
MarimaySyntaxKind mmay_parser_completed_kind(const MarimayParser *parser,
    MarimayCompletedMarker completed);
bool mmay_parser_completed_contains_token_kind(const MarimayParser *parser,
    MarimayCompletedMarker completed, MarimayTokenKind kind);
void mmay_parser_bump_as(MarimayParser *parser, MarimayTokenKind significant_kind);
void mmay_parser_bump(MarimayParser *parser);
bool mmay_parser_eat_identifier(MarimayParser *parser);
bool mmay_parser_eat(MarimayParser *parser, MarimayTokenKind kind);
void mmay_parser_expect(MarimayParser *parser, MarimayTokenKind kind,
    const char *message);
void mmay_parser_expect_identifier(MarimayParser *parser, const char *message);
bool mmay_parser_is_stop(const MarimayParser *parser, MarimayTokenKind first,
    MarimayTokenKind second, MarimayTokenKind third);
void mmay_parser_expect_closing_delimiter(MarimayParser *parser,
    MarimayTokenKind close, size_t open_offset, const char *message);

void mmay_parse_expression_until(MarimayParser *parser, MarimayTokenKind first,
    MarimayTokenKind second, MarimayTokenKind third);
void mmay_parse_type_until(MarimayParser *parser, MarimayTokenKind first,
    MarimayTokenKind second, MarimayTokenKind third);
bool mmay_parser_type_at_greater(const MarimayParser *parser);
void mmay_parser_expect_type_greater(MarimayParser *parser);
void mmay_parser_begin_type_angle(MarimayParser *parser);
void mmay_parser_end_type_angle(MarimayParser *parser);
void mmay_parse_type_list(MarimayParser *parser, MarimayTokenKind close,
    bool allow_empty);
void mmay_parse_arguments(MarimayParser *parser);
bool mmay_parser_yield_is_name_expression(const MarimayParser *parser);
bool mmay_parser_contextual_statement_starts(const MarimayParser *parser,
    MarimayTokenKind current);
void mmay_parse_block(MarimayParser *parser);
void mmay_parse_parameters(MarimayParser *parser);
void mmay_parse_top_decl(MarimayParser *parser);

#endif
