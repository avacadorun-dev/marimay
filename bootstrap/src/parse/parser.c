#include "parser_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/generated/diagnostic_codes.h"

static bool mmay_token_is_trivia(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_BOM || kind == MMAY_TOKEN_WHITESPACE
        || kind == MMAY_TOKEN_LINE_COMMENT || kind == MMAY_TOKEN_DOC_LINE_COMMENT
        || kind == MMAY_TOKEN_BLOCK_COMMENT || kind == MMAY_TOKEN_DOC_BLOCK_COMMENT;
}

const MarimayToken *mmay_parser_raw_token(const MarimayParser *parser, size_t index) {
    return mmay_lex_token(parser->lexed, index);
}

size_t mmay_parser_skip_trivia(const MarimayParser *parser, size_t index) {
    while (index < parser->lexed->tokens.length) {
        const MarimayToken *token = mmay_parser_raw_token(parser, index);
        if (token == NULL || !mmay_token_is_trivia(token->kind)) {
            break;
        }
        index += 1;
    }
    return index;
}

size_t mmay_parser_significant_index(const MarimayParser *parser) {
    return mmay_parser_skip_trivia(parser, parser->raw_index);
}

const MarimayToken *mmay_parser_current_token(const MarimayParser *parser) {
    return mmay_parser_raw_token(parser, mmay_parser_significant_index(parser));
}

MarimayTokenKind mmay_parser_current(const MarimayParser *parser) {
    const MarimayToken *token = mmay_parser_current_token(parser);
    return token == NULL ? MMAY_TOKEN_EOF : token->kind;
}

size_t mmay_parser_current_offset(const MarimayParser *parser) {
    const MarimayToken *token = mmay_parser_current_token(parser);
    if (token != NULL) {
        return token->start;
    }
    return mmay_source_length(parser->lexed->snapshot);
}

bool mmay_parser_at(const MarimayParser *parser, MarimayTokenKind kind) {
    return mmay_parser_current(parser) == kind;
}

bool mmay_token_is_contextual_keyword(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_KW_AS || kind == MMAY_TOKEN_KW_BREAK
        || kind == MMAY_TOKEN_KW_CASE || kind == MMAY_TOKEN_KW_CONST
        || kind == MMAY_TOKEN_KW_CONTINUE || kind == MMAY_TOKEN_KW_ELSE
        || kind == MMAY_TOKEN_KW_ENUM || kind == MMAY_TOKEN_KW_EXTERN
        || kind == MMAY_TOKEN_KW_FN || kind == MMAY_TOKEN_KW_FOR
        || kind == MMAY_TOKEN_KW_FROM || kind == MMAY_TOKEN_KW_IMPORT
        || kind == MMAY_TOKEN_KW_INTERFACE || kind == MMAY_TOKEN_KW_LET
        || kind == MMAY_TOKEN_KW_MUTABLE || kind == MMAY_TOKEN_KW_OF
        || kind == MMAY_TOKEN_KW_PUBLIC || kind == MMAY_TOKEN_KW_SCOPE
        || kind == MMAY_TOKEN_KW_TYPE || kind == MMAY_TOKEN_KW_UNSAFE
        || kind == MMAY_TOKEN_KW_YIELD;
}

bool mmay_parser_at_identifier(const MarimayParser *parser) {
    MarimayTokenKind kind = mmay_parser_current(parser);
    return kind == MMAY_TOKEN_IDENTIFIER
        || mmay_token_is_contextual_keyword(kind);
}

MarimayTokenKind mmay_parser_next_significant_kind(
    const MarimayParser *parser
) {
    size_t current = mmay_parser_significant_index(parser);
    size_t next = mmay_parser_skip_trivia(parser, current + 1);
    const MarimayToken *token = mmay_parser_raw_token(parser, next);
    return token == NULL ? MMAY_TOKEN_EOF : token->kind;
}

MarimayTokenKind mmay_parser_second_next_significant_kind(
    const MarimayParser *parser
) {
    size_t current = mmay_parser_significant_index(parser);
    size_t next = mmay_parser_skip_trivia(parser, current + 1);
    size_t second_next = mmay_parser_skip_trivia(parser, next + 1);
    const MarimayToken *token = mmay_parser_raw_token(parser, second_next);
    return token == NULL ? MMAY_TOKEN_EOF : token->kind;
}

bool mmay_parser_add_diagnostic(
    MarimayParser *parser,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    MarimayParseDiagnostic diagnostic;
    if (parser->result->diagnostics.length >= parser->options.max_diagnostics) {
        parser->result->suppressed_diagnostics += 1;
        parser->limit_reached = true;
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!mmay_vector_push(&parser->result->diagnostics, &diagnostic)) {
        parser->allocation_failed = true;
        return false;
    }
    return true;
}

static bool mmay_parser_push_event(MarimayParser *parser, const MarimayParseEvent *event) {
    if (parser->events.length >= parser->options.max_events) {
        if (!parser->limit_reached) {
            size_t offset = mmay_parser_current_offset(parser);
            (void)mmay_parser_add_diagnostic(
                parser,
                MMAY_DIAGNOSTIC_PARSER_WORK_LIMIT,
                "parser event limit reached",
                offset,
                offset
            );
        }
        parser->limit_reached = true;
        return false;
    }
    if (!mmay_vector_push(&parser->events, event)) {
        parser->allocation_failed = true;
        return false;
    }
    return true;
}

MarimayMarker mmay_parser_start(MarimayParser *parser) {
    MarimayMarker marker;
    MarimayParseEvent event;
    marker.position = parser->events.length;
    marker.valid = false;
    if (parser->nesting >= parser->options.max_nesting) {
        size_t offset = mmay_parser_current_offset(parser);
        (void)mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_PARSER_NESTING_LIMIT,
            "parser nesting exceeds the configured limit",
            offset,
            offset
        );
        parser->limit_reached = true;
        return marker;
    }
    event.kind = MMAY_PARSE_EVENT_START;
    event.as.start.syntax_kind = MMAY_SYNTAX_ERROR;
    event.as.start.forward_parent = 0;
    if (mmay_parser_push_event(parser, &event)) {
        marker.valid = true;
        parser->nesting += 1;
    }
    return marker;
}

MarimayCompletedMarker mmay_parser_complete(
    MarimayParser *parser,
    MarimayMarker marker,
    MarimaySyntaxKind kind
) {
    MarimayParseEvent *start = NULL;
    MarimayParseEvent finish;
    MarimayCompletedMarker completed;
    completed.position = marker.position;
    completed.valid = false;
    if (!marker.valid || parser->allocation_failed || parser->limit_reached) {
        return completed;
    }
    start = mmay_vector_at(&parser->events, marker.position);
    if (start == NULL || start->kind != MMAY_PARSE_EVENT_START) {
        parser->allocation_failed = true;
        return completed;
    }
    start->as.start.syntax_kind = kind;
    finish.kind = MMAY_PARSE_EVENT_FINISH;
    finish.as.token.token_index = 0;
    finish.as.token.kind = MMAY_TOKEN_EOF;
    if (mmay_parser_push_event(parser, &finish) && parser->nesting != 0) {
        parser->nesting -= 1;
        completed.valid = true;
    }
    return completed;
}

MarimayMarker mmay_parser_precede(
    MarimayParser *parser,
    MarimayCompletedMarker completed
) {
    MarimayMarker parent;
    MarimayParseEvent *child_start = NULL;
    parent.position = parser->events.length;
    parent.valid = false;
    if (!completed.valid || completed.position >= parser->events.length) {
        parser->allocation_failed = true;
        return parent;
    }
    parent = mmay_parser_start(parser);
    if (!parent.valid) {
        return parent;
    }
    child_start = mmay_vector_at(&parser->events, completed.position);
    if (child_start == NULL || child_start->kind != MMAY_PARSE_EVENT_START
        || child_start->as.start.forward_parent != 0
        || parent.position <= completed.position) {
        parser->allocation_failed = true;
        return parent;
    }
    child_start->as.start.forward_parent = parent.position - completed.position;
    return parent;
}

bool mmay_parser_completed_contains_kind(
    const MarimayParser *parser,
    MarimayCompletedMarker completed,
    MarimaySyntaxKind kind
) {
    size_t index = completed.position;
    if (!completed.valid) {
        return false;
    }
    while (index < parser->events.length) {
        const MarimayParseEvent *event = mmay_vector_at_const(
            &parser->events,
            index
        );
        if (event == NULL) {
            return false;
        }
        if (event->kind == MMAY_PARSE_EVENT_START
            && event->as.start.syntax_kind == kind) {
            return true;
        }
        index += 1;
    }
    return false;
}

MarimaySyntaxKind mmay_parser_completed_kind(
    const MarimayParser *parser,
    MarimayCompletedMarker completed
) {
    const MarimayParseEvent *event = completed.valid
        ? mmay_vector_at_const(&parser->events, completed.position)
        : NULL;
    return event != NULL && event->kind == MMAY_PARSE_EVENT_START
        ? event->as.start.syntax_kind
        : MMAY_SYNTAX_ERROR;
}

bool mmay_parser_completed_contains_token_kind(
    const MarimayParser *parser,
    MarimayCompletedMarker completed,
    MarimayTokenKind kind
) {
    size_t index = completed.position;
    if (!completed.valid) {
        return false;
    }
    while (index < parser->events.length) {
        const MarimayParseEvent *event = mmay_vector_at_const(
            &parser->events,
            index
        );
        if (event == NULL) {
            return false;
        }
        if (event->kind == MMAY_PARSE_EVENT_TOKEN) {
            const MarimayToken *token = mmay_parser_raw_token(
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

void mmay_parser_bump_as(
    MarimayParser *parser,
    MarimayTokenKind significant_kind
) {
    size_t significant = mmay_parser_significant_index(parser);
    size_t index = parser->raw_index;
    if (significant >= parser->lexed->tokens.length) {
        return;
    }
    while (index <= significant && !parser->allocation_failed
        && !parser->limit_reached) {
        MarimayParseEvent event;
        event.kind = MMAY_PARSE_EVENT_TOKEN;
        event.as.token.token_index = index;
        event.as.token.kind = index == significant
            ? significant_kind
            : mmay_parser_raw_token(parser, index)->kind;
        if (!mmay_parser_push_event(parser, &event)) {
            return;
        }
        index += 1;
    }
    parser->raw_index = index;
}

void mmay_parser_bump(MarimayParser *parser) {
    mmay_parser_bump_as(parser, mmay_parser_current(parser));
}

bool mmay_parser_eat_identifier(MarimayParser *parser) {
    size_t before = parser->raw_index;
    if (parser->allocation_failed || parser->limit_reached
        || !mmay_parser_at_identifier(parser)) {
        return false;
    }
    mmay_parser_bump_as(parser, MMAY_TOKEN_IDENTIFIER);
    return parser->raw_index > before;
}

bool mmay_parser_eat(MarimayParser *parser, MarimayTokenKind kind) {
    size_t before = parser->raw_index;
    if (parser->allocation_failed || parser->limit_reached
        || !mmay_parser_at(parser, kind)) {
        return false;
    }
    mmay_parser_bump(parser);
    return parser->raw_index > before;
}

static void mmay_parser_emit_leading_trivia(MarimayParser *parser) {
    size_t significant = mmay_parser_significant_index(parser);
    while (parser->raw_index < significant && !parser->allocation_failed
        && !parser->limit_reached) {
        MarimayParseEvent event;
        event.kind = MMAY_PARSE_EVENT_TOKEN;
        event.as.token.token_index = parser->raw_index;
        event.as.token.kind = mmay_parser_raw_token(
            parser,
            parser->raw_index
        )->kind;
        if (!mmay_parser_push_event(parser, &event)) {
            return;
        }
        parser->raw_index += 1;
    }
}

void mmay_parser_expect(MarimayParser *parser, MarimayTokenKind kind, const char *message) {
    MarimayParseEvent event;
    size_t offset = 0;
    if (mmay_parser_eat(parser, kind)) {
        return;
    }
    mmay_parser_emit_leading_trivia(parser);
    offset = mmay_parser_current_offset(parser);
    if (!mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            message,
            offset,
            offset
        )) {
        return;
    }
    event.kind = MMAY_PARSE_EVENT_SYNTHETIC_TOKEN;
    event.as.synthetic_token.kind = kind;
    event.as.synthetic_token.start = offset;
    event.as.synthetic_token.end = offset;
    event.as.synthetic_token.synthetic = true;
    (void)mmay_parser_push_event(parser, &event);
}

void mmay_parser_expect_identifier(
    MarimayParser *parser,
    const char *message
) {
    MarimayParseEvent event;
    size_t offset = 0;
    if (mmay_parser_eat_identifier(parser)) {
        return;
    }
    mmay_parser_emit_leading_trivia(parser);
    offset = mmay_parser_current_offset(parser);
    if (!mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_EXPECTED_TOKEN,
            message,
            offset,
            offset
        )) {
        return;
    }
    event.kind = MMAY_PARSE_EVENT_SYNTHETIC_TOKEN;
    event.as.synthetic_token.kind = MMAY_TOKEN_IDENTIFIER;
    event.as.synthetic_token.start = offset;
    event.as.synthetic_token.end = offset;
    event.as.synthetic_token.synthetic = true;
    (void)mmay_parser_push_event(parser, &event);
}

bool mmay_parser_is_stop(
    const MarimayParser *parser,
    MarimayTokenKind first,
    MarimayTokenKind second,
    MarimayTokenKind third
) {
    MarimayTokenKind current = mmay_parser_current(parser);
    return current == first || current == second || current == third
        || current == MMAY_TOKEN_EOF;
}

typedef struct MarimayExpressionStops {
    MarimayTokenKind first;
    MarimayTokenKind second;
    MarimayTokenKind third;
} MarimayExpressionStops;

void mmay_parser_expect_closing_delimiter(
    MarimayParser *parser,
    MarimayTokenKind close,
    size_t open_offset,
    const char *message
) {
    size_t offset = 0;
    if (mmay_parser_eat(parser, close)) {
        return;
    }
    offset = mmay_parser_current_offset(parser);
    if (!mmay_parser_add_diagnostic(
            parser,
            MMAY_DIAGNOSTIC_UNTERMINATED_DELIMITER,
            "delimited syntax is missing its closing token",
            open_offset,
            offset
        )) {
        return;
    }
    mmay_parser_expect(parser, close, message);
}

typedef struct MarimayBuildFrame {
    MarimaySyntaxKind kind;
    size_t element_start;
} MarimayBuildFrame;

static bool mmay_builder_push_token(
    MarimayVector *elements,
    const MarimaySyntaxToken *token
) {
    MarimaySyntaxElement element;
    element.kind = MMAY_SYNTAX_ELEMENT_TOKEN;
    element.as.token = *token;
    return mmay_vector_push(elements, &element);
}

static bool mmay_builder_finish_node(
    MarimaySyntaxTree *tree,
    MarimayVector *frames,
    MarimayVector *elements
) {
    MarimayBuildFrame *frame = mmay_vector_at(frames, frames->length - 1);
    size_t child_count = 0;
    size_t children_size = 0;
    size_t allocation_size = 0;
    size_t text_length = 0;
    size_t index = 0;
    MarimayGreenNode *node = NULL;
    MarimaySyntaxElement node_element;
    if (frame == NULL || frame->element_start > elements->length) {
        return false;
    }
    child_count = elements->length - frame->element_start;
    if (!mmay_size_multiply(child_count, sizeof(MarimaySyntaxElement), &children_size)
        || !mmay_size_add(offsetof(MarimayGreenNode, children), children_size, &allocation_size)) {
        return false;
    }
    node = mmay_syntax_tree_allocate(
        tree,
        allocation_size,
        _Alignof(MarimayGreenNode)
    );
    if (node == NULL) {
        return false;
    }
    node->kind = frame->kind;
    node->child_count = child_count;
    for (index = 0; index < child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_vector_at_const(
            elements,
            frame->element_start + index
        );
        if (element == NULL) {
            return false;
        }
        node->children[index] = *element;
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
            if (!mmay_size_add(
                    text_length,
                    element->as.node->text_length,
                    &text_length
                )) {
                return false;
            }
        } else if (!element->as.token.synthetic
            && element->as.token.kind != MMAY_TOKEN_EOF) {
            if (!mmay_size_add(
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
    node_element.kind = MMAY_SYNTAX_ELEMENT_NODE;
    node_element.as.node = node;
    return mmay_vector_push(elements, &node_element);
}

static bool mmay_build_tree(
    const MarimayLexResult *lexed,
    MarimayAllocator allocator,
    MarimayVector *events,
    MarimaySyntaxTree *tree
) {
    MarimayVector frames;
    MarimayVector elements;
    MarimayVector start_kinds;
    size_t index = 0;
    bool ok = false;
    memset(&frames, 0, sizeof(frames));
    memset(&elements, 0, sizeof(elements));
    memset(&start_kinds, 0, sizeof(start_kinds));
    if (!mmay_syntax_tree_storage_init(tree, allocator)) {
        return false;
    }
    tree->snapshot = lexed->snapshot;
    tree->root = NULL;
    tree->initialized = true;
    if (!mmay_vector_init(&frames, allocator, sizeof(MarimayBuildFrame))
        || !mmay_vector_init(&elements, allocator, sizeof(MarimaySyntaxElement))
        || !mmay_vector_init(&start_kinds, allocator, sizeof(MarimaySyntaxKind))) {
        mmay_vector_dispose(&start_kinds);
        mmay_vector_dispose(&elements);
        mmay_vector_dispose(&frames);
        mmay_syntax_tree_dispose(tree);
        return false;
    }
    for (index = 0; index < events->length; index += 1) {
        MarimayParseEvent *event = mmay_vector_at(events, index);
        if (event == NULL) {
            goto cleanup;
        }
        if (event->kind == MMAY_PARSE_EVENT_START) {
            size_t start_index = index;
            size_t kind_index = 0;
            start_kinds.length = 0;
            for (;;) {
                MarimayParseEvent *start = mmay_vector_at(events, start_index);
                MarimaySyntaxKind syntax_kind;
                size_t forward_parent = 0;
                size_t next_index = 0;
                if (start == NULL || start->kind != MMAY_PARSE_EVENT_START) {
                    goto cleanup;
                }
                syntax_kind = start->as.start.syntax_kind;
                forward_parent = start->as.start.forward_parent;
                if (start_index != index) {
                    start->kind = MMAY_PARSE_EVENT_TOMBSTONE;
                }
                if (!mmay_vector_push(&start_kinds, &syntax_kind)) {
                    goto cleanup;
                }
                if (forward_parent == 0) {
                    break;
                }
                if (!mmay_size_add(start_index, forward_parent, &next_index)
                    || next_index <= start_index || next_index >= events->length) {
                    goto cleanup;
                }
                start_index = next_index;
            }
            for (kind_index = start_kinds.length; kind_index != 0; kind_index -= 1) {
                const MarimaySyntaxKind *syntax_kind = mmay_vector_at_const(
                    &start_kinds,
                    kind_index - 1
                );
                MarimayBuildFrame frame;
                if (syntax_kind == NULL) {
                    goto cleanup;
                }
                frame.kind = *syntax_kind;
                frame.element_start = elements.length;
                if (!mmay_vector_push(&frames, &frame)) {
                    goto cleanup;
                }
            }
        } else if (event->kind == MMAY_PARSE_EVENT_TOMBSTONE) {
            continue;
        } else if (event->kind == MMAY_PARSE_EVENT_TOKEN) {
            const MarimayToken *source = mmay_lex_token(
                lexed,
                event->as.token.token_index
            );
            MarimaySyntaxToken token;
            if (source == NULL) {
                goto cleanup;
            }
            token.kind = event->as.token.kind;
            token.start = source->start;
            token.end = source->end;
            token.synthetic = false;
            if (!mmay_builder_push_token(&elements, &token)) {
                goto cleanup;
            }
        } else if (event->kind == MMAY_PARSE_EVENT_SYNTHETIC_TOKEN) {
            if (!mmay_builder_push_token(&elements, &event->as.synthetic_token)) {
                goto cleanup;
            }
        } else if (event->kind == MMAY_PARSE_EVENT_FINISH) {
            if (frames.length == 0
                || !mmay_builder_finish_node(tree, &frames, &elements)) {
                goto cleanup;
            }
        }
    }
    if (frames.length != 0 || elements.length != 1) {
        goto cleanup;
    }
    {
        const MarimaySyntaxElement *root = mmay_vector_at_const(&elements, 0);
        if (root == NULL || root->kind != MMAY_SYNTAX_ELEMENT_NODE
            || root->as.node->kind != MMAY_SYNTAX_ROOT) {
            goto cleanup;
        }
        tree->root = root->as.node;
    }
    ok = true;

cleanup:
    mmay_vector_dispose(&start_kinds);
    mmay_vector_dispose(&elements);
    mmay_vector_dispose(&frames);
    if (!ok) {
        mmay_syntax_tree_dispose(tree);
    }
    return ok;
}

MarimayParseOptions mmay_parse_default_options(void) {
    MarimayParseOptions options;
    options.max_events = 2000000;
    options.max_diagnostics = 100;
    options.max_nesting = 512;
    return options;
}

bool mmay_parse_options_valid(MarimayParseOptions options) {
    return options.max_events >= 2 && options.max_diagnostics != 0
        && options.max_nesting >= 2;
}

MarimayParseStatus mmay_parse_file(
    const MarimayLexResult *lexed,
    MarimayAllocator allocator,
    MarimayParseOptions options,
    MarimayParseResult *result
) {
    MarimayParser parser;
    MarimayMarker root;
    const MarimayToken *last = NULL;
    if (result == NULL) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (lexed == NULL || !lexed->initialized || lexed->tokens.length == 0
        || !mmay_parse_options_valid(options)) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    last = mmay_lex_token(lexed, lexed->tokens.length - 1);
    if (last == NULL || last->kind != MMAY_TOKEN_EOF) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    if (!mmay_vector_init(
            &result->diagnostics,
            allocator,
            sizeof(MarimayParseDiagnostic)
        )) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    result->allocator = allocator;
    result->initialized = true;
    memset(&parser, 0, sizeof(parser));
    parser.lexed = lexed;
    parser.result = result;
    parser.options = options;
    if (!mmay_vector_init(&parser.events, allocator, sizeof(MarimayParseEvent))) {
        mmay_parse_result_dispose(result);
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    root = mmay_parser_start(&parser);
    while (!mmay_parser_at(&parser, MMAY_TOKEN_EOF)
        && !parser.allocation_failed && !parser.limit_reached) {
        size_t before = parser.raw_index;
        mmay_parse_top_decl(&parser);
        if (parser.raw_index == before && !parser.limit_reached
            && !parser.allocation_failed) {
            mmay_parser_bump(&parser);
        }
    }
    if (!parser.allocation_failed && !parser.limit_reached) {
        mmay_parser_bump(&parser);
        mmay_parser_complete(&parser, root, MMAY_SYNTAX_ROOT);
    }
    if (parser.allocation_failed || parser.limit_reached) {
        MarimayParseStatus status = parser.allocation_failed
            ? MMAY_PARSE_ALLOCATION_FAILED
            : MMAY_PARSE_LIMIT_REACHED;
        mmay_vector_dispose(&parser.events);
        mmay_parse_result_dispose(result);
        return status;
    }
    if (!mmay_build_tree(lexed, allocator, &parser.events, &result->tree)) {
        mmay_vector_dispose(&parser.events);
        mmay_parse_result_dispose(result);
        return MMAY_PARSE_ALLOCATION_FAILED;
    }
    mmay_vector_dispose(&parser.events);
    return MMAY_PARSE_OK;
}

static size_t mmay_incremental_element_length(
    const MarimaySyntaxElement *element
) {
    if (element == NULL) {
        return 0;
    }
    if (element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
        return element->as.node == NULL ? 0 : element->as.node->text_length;
    }
    if (element->kind == MMAY_SYNTAX_ELEMENT_TOKEN
        && !element->as.token.synthetic
        && element->as.token.kind != MMAY_TOKEN_EOF
        && element->as.token.end >= element->as.token.start) {
        return element->as.token.end - element->as.token.start;
    }
    return 0;
}

static bool mmay_incremental_nodes_match(
    const MarimaySyntaxElement *previous,
    const MarimaySyntaxElement *current
) {
    return previous != NULL && current != NULL
        && previous->kind == MMAY_SYNTAX_ELEMENT_NODE
        && current->kind == MMAY_SYNTAX_ELEMENT_NODE
        && previous->as.node != NULL && current->as.node != NULL
        && previous->as.node->kind == current->as.node->kind
        && previous->as.node->text_length == current->as.node->text_length;
}

MarimayParseStatus mmay_parse_file_incremental(
    const MarimayParseResult *previous,
    const MarimayLexResult *lexed,
    size_t edit_start,
    size_t previous_edit_end,
    size_t new_edit_end,
    MarimayAllocator allocator,
    MarimayParseOptions options,
    MarimayParseResult *result,
    MarimayIncrementalParseStats *stats
) {
    const MarimayGreenNode *previous_root = NULL;
    const MarimayGreenNode *current_root = NULL;
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
    MarimayGreenNode *replacement = NULL;
    MarimayParseStatus status;
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
    if (result == NULL) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (previous == NULL || previous == result || !previous->initialized
        || previous->tree.snapshot == NULL || previous->tree.root == NULL
        || lexed == NULL || !lexed->initialized || lexed->snapshot == NULL
        || mmay_source_file_id(previous->tree.snapshot)
            != mmay_source_file_id(lexed->snapshot)) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    previous_length = mmay_source_length(previous->tree.snapshot);
    current_length = mmay_source_length(lexed->snapshot);
    if (edit_start > previous_edit_end
        || previous_edit_end > previous_length
        || edit_start > new_edit_end || new_edit_end > current_length) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    previous_suffix_length = previous_length - previous_edit_end;
    current_suffix_length = current_length - new_edit_end;
    if (previous_suffix_length != current_suffix_length) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }
    previous_bytes = mmay_source_bytes(previous->tree.snapshot);
    current_bytes = mmay_source_bytes(lexed->snapshot);
    if ((edit_start != 0
            && memcmp(previous_bytes, current_bytes, edit_start) != 0)
        || (previous_suffix_length != 0
            && memcmp(
                previous_bytes + previous_edit_end,
                current_bytes + new_edit_end,
                previous_suffix_length
            ) != 0)) {
        return MMAY_PARSE_INVALID_ARGUMENT;
    }

    status = mmay_parse_file(lexed, allocator, options, result);
    if (status != MMAY_PARSE_OK) {
        return status;
    }
    previous_root = previous->tree.root;
    current_root = result->tree.root;
    if (previous_root->kind != MMAY_SYNTAX_ROOT
        || current_root->kind != MMAY_SYNTAX_ROOT) {
        mmay_parse_result_dispose(result);
        return MMAY_PARSE_INVALID_ARGUMENT;
    }

    while (old_index < previous_root->child_count
        && new_index < current_root->child_count) {
        const MarimaySyntaxElement *old_element = mmay_syntax_child(
            previous_root,
            old_index
        );
        const MarimaySyntaxElement *new_element = mmay_syntax_child(
            current_root,
            new_index
        );
        size_t old_length = mmay_incremental_element_length(old_element);
        size_t new_length = mmay_incremental_element_length(new_element);
        size_t old_end = old_cursor + old_length;
        size_t new_end = new_cursor + new_length;
        if (old_end > edit_start || new_end > edit_start) {
            break;
        }
        if (old_cursor == new_cursor
            && mmay_incremental_nodes_match(old_element, new_element)) {
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
            const MarimaySyntaxElement *old_element = NULL;
            const MarimaySyntaxElement *new_element = NULL;
            size_t old_length = 0;
            size_t new_length = 0;
            size_t old_start = 0;
            size_t new_start = 0;
            old_index -= 1;
            new_index -= 1;
            old_element = mmay_syntax_child(previous_root, old_index);
            new_element = mmay_syntax_child(current_root, new_index);
            old_length = mmay_incremental_element_length(old_element);
            new_length = mmay_incremental_element_length(new_element);
            if (old_length > old_cursor || new_length > new_cursor) {
                break;
            }
            old_start = old_cursor - old_length;
            new_start = new_cursor - new_length;
            if (old_start < previous_edit_end || new_start < new_edit_end) {
                break;
            }
            if (old_start == new_start
                && mmay_incremental_nodes_match(old_element, new_element)) {
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
        return MMAY_PARSE_OK;
    }

    if (!mmay_size_multiply(
            current_root->child_count,
            sizeof(MarimaySyntaxElement),
            &children_size
        ) || !mmay_size_add(
            offsetof(MarimayGreenNode, children),
            children_size,
            &allocation_size
        )) {
        mmay_parse_result_dispose(result);
        return MMAY_PARSE_ALLOCATION_FAILED;
    }
    replacement = mmay_syntax_tree_allocate(
        &result->tree,
        allocation_size,
        _Alignof(MarimayGreenNode)
    );
    if (replacement == NULL) {
        mmay_parse_result_dispose(result);
        return MMAY_PARSE_ALLOCATION_FAILED;
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
        const MarimaySyntaxElement *old_element = mmay_syntax_child(
            previous_root,
            old_index
        );
        const MarimaySyntaxElement *new_element = mmay_syntax_child(
            current_root,
            new_index
        );
        size_t old_length = mmay_incremental_element_length(old_element);
        size_t new_length = mmay_incremental_element_length(new_element);
        size_t old_end = old_cursor + old_length;
        size_t new_end = new_cursor + new_length;
        if (old_end > edit_start || new_end > edit_start) {
            break;
        }
        if (old_cursor == new_cursor
            && mmay_incremental_nodes_match(old_element, new_element)) {
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
            const MarimaySyntaxElement *old_element = NULL;
            const MarimaySyntaxElement *new_element = NULL;
            size_t old_length = 0;
            size_t new_length = 0;
            size_t old_start = 0;
            size_t new_start = 0;
            old_index -= 1;
            new_index -= 1;
            old_element = mmay_syntax_child(previous_root, old_index);
            new_element = mmay_syntax_child(current_root, new_index);
            old_length = mmay_incremental_element_length(old_element);
            new_length = mmay_incremental_element_length(new_element);
            if (old_length > old_cursor || new_length > new_cursor) {
                break;
            }
            old_start = old_cursor - old_length;
            new_start = new_cursor - new_length;
            if (old_start < previous_edit_end || new_start < new_edit_end) {
                break;
            }
            if (old_start == new_start
                && mmay_incremental_nodes_match(old_element, new_element)) {
                replacement->children[new_index].as.node = old_element->as.node;
            }
            old_cursor = old_start;
            new_cursor = new_start;
        }
    }
    if (!mmay_syntax_tree_retain_storage(&result->tree, &previous->tree)) {
        if (stats != NULL) {
            stats->reparsed_bytes = current_length;
        }
        return MMAY_PARSE_OK;
    }
    result->tree.root = replacement;
    if (stats != NULL) {
        stats->reused_top_level_nodes = reused_nodes;
        stats->reparsed_bytes = current_length - reused_bytes;
    }
    return MMAY_PARSE_OK;
}

void mmay_parse_result_dispose(MarimayParseResult *result) {
    if (result == NULL || !result->initialized) {
        return;
    }
    mmay_syntax_tree_dispose(&result->tree);
    mmay_vector_dispose(&result->diagnostics);
    result->suppressed_diagnostics = 0;
    result->initialized = false;
}

const MarimayParseDiagnostic *mmay_parse_diagnostic(
    const MarimayParseResult *result,
    size_t index
) {
    if (result == NULL || !result->initialized) {
        return NULL;
    }
    return mmay_vector_at_const(&result->diagnostics, index);
}
