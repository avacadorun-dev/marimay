#include "parser_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/generated/diagnostic_codes.h"

static bool ruda_token_is_trivia(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_BOM || kind == RUDA_TOKEN_WHITESPACE
        || kind == RUDA_TOKEN_LINE_COMMENT || kind == RUDA_TOKEN_DOC_LINE_COMMENT
        || kind == RUDA_TOKEN_BLOCK_COMMENT || kind == RUDA_TOKEN_DOC_BLOCK_COMMENT;
}

const RudaToken *ruda_parser_raw_token(const RudaParser *parser, size_t index) {
    return ruda_lex_token(parser->lexed, index);
}

size_t ruda_parser_skip_trivia(const RudaParser *parser, size_t index) {
    while (index < parser->lexed->tokens.length) {
        const RudaToken *token = ruda_parser_raw_token(parser, index);
        if (token == NULL || !ruda_token_is_trivia(token->kind)) {
            break;
        }
        index += 1;
    }
    return index;
}

size_t ruda_parser_significant_index(const RudaParser *parser) {
    return ruda_parser_skip_trivia(parser, parser->raw_index);
}

const RudaToken *ruda_parser_current_token(const RudaParser *parser) {
    return ruda_parser_raw_token(parser, ruda_parser_significant_index(parser));
}

RudaTokenKind ruda_parser_current(const RudaParser *parser) {
    const RudaToken *token = ruda_parser_current_token(parser);
    return token == NULL ? RUDA_TOKEN_EOF : token->kind;
}

size_t ruda_parser_current_offset(const RudaParser *parser) {
    const RudaToken *token = ruda_parser_current_token(parser);
    if (token != NULL) {
        return token->start;
    }
    return ruda_source_length(parser->lexed->snapshot);
}

bool ruda_parser_at(const RudaParser *parser, RudaTokenKind kind) {
    return ruda_parser_current(parser) == kind;
}

bool ruda_token_is_contextual_keyword(RudaTokenKind kind) {
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

bool ruda_parser_at_identifier(const RudaParser *parser) {
    RudaTokenKind kind = ruda_parser_current(parser);
    return kind == RUDA_TOKEN_IDENTIFIER
        || ruda_token_is_contextual_keyword(kind);
}

RudaTokenKind ruda_parser_next_significant_kind(
    const RudaParser *parser
) {
    size_t current = ruda_parser_significant_index(parser);
    size_t next = ruda_parser_skip_trivia(parser, current + 1);
    const RudaToken *token = ruda_parser_raw_token(parser, next);
    return token == NULL ? RUDA_TOKEN_EOF : token->kind;
}

RudaTokenKind ruda_parser_second_next_significant_kind(
    const RudaParser *parser
) {
    size_t current = ruda_parser_significant_index(parser);
    size_t next = ruda_parser_skip_trivia(parser, current + 1);
    size_t second_next = ruda_parser_skip_trivia(parser, next + 1);
    const RudaToken *token = ruda_parser_raw_token(parser, second_next);
    return token == NULL ? RUDA_TOKEN_EOF : token->kind;
}

bool ruda_parser_add_diagnostic(
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

RudaMarker ruda_parser_start(RudaParser *parser) {
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

RudaCompletedMarker ruda_parser_complete(
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

RudaMarker ruda_parser_precede(
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

bool ruda_parser_completed_contains_kind(
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

RudaSyntaxKind ruda_parser_completed_kind(
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

bool ruda_parser_completed_contains_token_kind(
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

void ruda_parser_bump_as(
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

void ruda_parser_bump(RudaParser *parser) {
    ruda_parser_bump_as(parser, ruda_parser_current(parser));
}

bool ruda_parser_eat_identifier(RudaParser *parser) {
    size_t before = parser->raw_index;
    if (parser->allocation_failed || parser->limit_reached
        || !ruda_parser_at_identifier(parser)) {
        return false;
    }
    ruda_parser_bump_as(parser, RUDA_TOKEN_IDENTIFIER);
    return parser->raw_index > before;
}

bool ruda_parser_eat(RudaParser *parser, RudaTokenKind kind) {
    size_t before = parser->raw_index;
    if (parser->allocation_failed || parser->limit_reached
        || !ruda_parser_at(parser, kind)) {
        return false;
    }
    ruda_parser_bump(parser);
    return parser->raw_index > before;
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

void ruda_parser_expect(RudaParser *parser, RudaTokenKind kind, const char *message) {
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

void ruda_parser_expect_identifier(
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

bool ruda_parser_is_stop(
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

void ruda_parser_expect_closing_delimiter(
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

typedef struct RudaBuildFrame {
    RudaSyntaxKind kind;
    size_t element_start;
} RudaBuildFrame;

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

bool ruda_parse_options_valid(RudaParseOptions options) {
    return options.max_events >= 2 && options.max_diagnostics != 0
        && options.max_nesting >= 2;
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
        || !ruda_parse_options_valid(options)) {
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
