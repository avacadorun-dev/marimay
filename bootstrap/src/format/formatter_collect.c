#include "formatter_internal.h"
#include "formatter_groups_api.h"

#include <stdlib.h>
#include <string.h>

#include "mmay/base/string_literal.h"
#include "mmay/syntax/syntax.h"

typedef struct MarimayFormatTopLevel {
    size_t start;
    size_t end;
    size_t source_order;
    MarimaySyntaxKind kind;
    MarimayOwnedBytes import_path;
    unsigned int import_group;
    bool has_comment;
} MarimayFormatTopLevel;

bool mmay_format_token_is_trivia(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_BOM || kind == MMAY_TOKEN_WHITESPACE;
}

bool mmay_format_token_is_comment(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_LINE_COMMENT || kind == MMAY_TOKEN_DOC_LINE_COMMENT
        || kind == MMAY_TOKEN_BLOCK_COMMENT || kind == MMAY_TOKEN_DOC_BLOCK_COMMENT;
}

bool mmay_format_token_is_visible(MarimayTokenKind kind) {
    return kind != MMAY_TOKEN_EOF && !mmay_format_token_is_trivia(kind);
}

static bool mmay_format_collect_token(
    MarimayFormatCollector *collector,
    const MarimaySyntaxToken *token,
    MarimaySyntaxKind parent_kind,
    MarimaySyntaxKind top_level_kind,
    size_t top_level_id,
    size_t statement_id,
    bool in_for_header,
    bool in_attribute,
    bool in_record_expression,
    bool in_interpolation
) {
    MarimayFormatToken formatted;
    if (token->synthetic) {
        collector->status = MMAY_FORMAT_SOURCE_INVALID;
        return false;
    }
    if (collector->tokens.length >= collector->max_tokens) {
        collector->status = MMAY_FORMAT_LIMIT_REACHED;
        return false;
    }
    formatted.token = *token;
    formatted.parent_kind = parent_kind;
    formatted.top_level_kind = top_level_kind;
    formatted.top_level_id = top_level_id;
    formatted.statement_id = statement_id;
    formatted.in_for_header = in_for_header;
    formatted.in_attribute = in_attribute;
    formatted.in_record_expression = in_record_expression;
    formatted.in_interpolation = in_interpolation;
    formatted.boolean_begins = 0;
    formatted.boolean_ends = 0;
    formatted.boolean_indent = false;
    formatted.import_group = 0;
    if (!mmay_vector_push(&collector->tokens, &formatted)) {
        collector->status = MMAY_FORMAT_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static MarimayFormatStatus mmay_format_decode_normal_string(
    const unsigned char *source,
    size_t source_length,
    const MarimaySyntaxToken *token,
    MarimayAllocator allocator,
    MarimayOwnedBytes *decoded
) {
    MarimayStringLiteralStatus status;
    if (token == NULL || token->kind != MMAY_TOKEN_STRING
        || token->end > source_length || token->start > token->end) {
        return MMAY_FORMAT_SOURCE_INVALID;
    }
    status = mmay_string_literal_decode(
        source + token->start,
        token->end - token->start,
        false,
        allocator,
        decoded
    );
    return status == MMAY_STRING_LITERAL_OK ? MMAY_FORMAT_OK
        : (status == MMAY_STRING_LITERAL_ALLOCATION_FAILED
            ? MMAY_FORMAT_ALLOCATION_FAILED
            : MMAY_FORMAT_SOURCE_INVALID);
}

MarimayFormatStatus mmay_format_decode_ordinary_string(
    const unsigned char *source,
    size_t source_length,
    const MarimaySyntaxToken *token,
    MarimayAllocator allocator,
    MarimayOwnedBytes *decoded
) {
    MarimayStringLiteralStatus status;
    bool raw = token != NULL && token->kind == MMAY_TOKEN_RAW_STRING;
    if (token == NULL || token->end > source_length
        || token->start > token->end
        || (token->kind != MMAY_TOKEN_STRING && !raw)) {
        return MMAY_FORMAT_SOURCE_INVALID;
    }
    if (token->kind == MMAY_TOKEN_STRING) {
        return mmay_format_decode_normal_string(
            source,
            source_length,
            token,
            allocator,
            decoded
        );
    }
    status = mmay_string_literal_decode(
        source + token->start,
        token->end - token->start,
        raw,
        allocator,
        decoded
    );
    return status == MMAY_STRING_LITERAL_OK ? MMAY_FORMAT_OK
        : (status == MMAY_STRING_LITERAL_ALLOCATION_FAILED
            ? MMAY_FORMAT_ALLOCATION_FAILED
            : MMAY_FORMAT_SOURCE_INVALID);
}

static int mmay_format_owned_bytes_compare(
    const MarimayOwnedBytes *left,
    const MarimayOwnedBytes *right
) {
    size_t common = left->length < right->length
        ? left->length
        : right->length;
    int result = common == 0 ? 0 : memcmp(left->data, right->data, common);
    if (result != 0) {
        return result;
    }
    return left->length < right->length
        ? -1
        : (left->length > right->length ? 1 : 0);
}

static unsigned int mmay_format_import_group(const MarimayOwnedBytes *path) {
    if ((path->length == 3 && memcmp(path->data, "std", 3) == 0)
        || (path->length > 4 && memcmp(path->data, "std/", 4) == 0)) {
        return 1;
    }
    if ((path->length >= 2 && path->data[0] == '.'
            && path->data[1] == '/')
        || (path->length >= 3 && path->data[0] == '.'
            && path->data[1] == '.' && path->data[2] == '/')) {
        return 2;
    }
    return 3;
}

static int mmay_format_top_level_compare(const void *left, const void *right) {
    const MarimayFormatTopLevel *first = left;
    const MarimayFormatTopLevel *second = right;
    int result = 0;
    if (first->import_group != second->import_group) {
        return first->import_group < second->import_group ? -1 : 1;
    }
    result = mmay_format_owned_bytes_compare(
        &first->import_path,
        &second->import_path
    );
    if (result != 0) {
        return result;
    }
    return first->source_order < second->source_order
        ? -1
        : (first->source_order > second->source_order ? 1 : 0);
}

static void mmay_format_top_levels_dispose(MarimayVector *top_levels) {
    size_t index = 0;
    for (index = 0; index < top_levels->length; index += 1) {
        MarimayFormatTopLevel *top_level = mmay_vector_at(top_levels, index);
        mmay_owned_bytes_dispose(&top_level->import_path);
    }
    mmay_vector_dispose(top_levels);
}

MarimayFormatStatus mmay_format_order_imports(
    MarimayVector *tokens,
    const unsigned char *source,
    size_t source_length,
    MarimayAllocator allocator
) {
    MarimayVector top_levels;
    MarimayVector ordered;
    MarimayFormatStatus status = MMAY_FORMAT_OK;
    size_t cursor = 0;
    size_t import_count = 0;
    size_t index = 0;
    memset(&top_levels, 0, sizeof(top_levels));
    memset(&ordered, 0, sizeof(ordered));
    if (!mmay_vector_init(
            &top_levels,
            allocator,
            sizeof(MarimayFormatTopLevel)
        )) {
        return MMAY_FORMAT_INVALID_ARGUMENT;
    }
    while (cursor < tokens->length) {
        const MarimayFormatToken *first = mmay_vector_at_const(tokens, cursor);
        MarimayFormatTopLevel top_level;
        size_t scan = cursor;
        bool saw_from = false;
        const MarimaySyntaxToken *path_token = NULL;
        if (first == NULL) {
            status = MMAY_FORMAT_INVALID_ARGUMENT;
            break;
        }
        memset(&top_level, 0, sizeof(top_level));
        top_level.start = cursor;
        top_level.kind = first->top_level_id == 0
            ? MMAY_SYNTAX_ROOT
            : first->top_level_kind;
        top_level.source_order = top_levels.length;
        while (scan < tokens->length) {
            MarimayFormatToken *token = mmay_vector_at(tokens, scan);
            if (token == NULL || token->top_level_id != first->top_level_id) {
                break;
            }
            if (mmay_format_token_is_comment(token->token.kind)) {
                top_level.has_comment = true;
            }
            if (top_level.kind == MMAY_SYNTAX_IMPORT_DECL) {
                if (token->token.kind == MMAY_TOKEN_KW_FROM) {
                    saw_from = true;
                } else if (saw_from && token->token.kind == MMAY_TOKEN_STRING
                    && path_token == NULL) {
                    path_token = &token->token;
                }
            }
            scan += 1;
        }
        top_level.end = scan;
        if (top_level.kind == MMAY_SYNTAX_IMPORT_DECL) {
            if (path_token == NULL) {
                status = MMAY_FORMAT_SOURCE_INVALID;
                break;
            }
            status = mmay_format_decode_normal_string(
                source,
                source_length,
                path_token,
                allocator,
                &top_level.import_path
            );
            if (status != MMAY_FORMAT_OK) {
                break;
            }
            top_level.import_group = mmay_format_import_group(
                &top_level.import_path
            );
            for (index = top_level.start; index < top_level.end; index += 1) {
                MarimayFormatToken *token = mmay_vector_at(tokens, index);
                token->import_group = top_level.import_group;
            }
            import_count += 1;
        }
        if (!mmay_vector_push(&top_levels, &top_level)) {
            mmay_owned_bytes_dispose(&top_level.import_path);
            status = MMAY_FORMAT_ALLOCATION_FAILED;
            break;
        }
        cursor = scan;
    }
    if (status != MMAY_FORMAT_OK || import_count == 0) {
        mmay_format_top_levels_dispose(&top_levels);
        return status;
    }
    index = 0;
    while (index < top_levels.length) {
        MarimayFormatTopLevel *first = mmay_vector_at(&top_levels, index);
        size_t run_end = index;
        size_t segment_start = index;
        if (first == NULL || first->kind != MMAY_SYNTAX_IMPORT_DECL) {
            index += 1;
            continue;
        }
        while (run_end < top_levels.length) {
            MarimayFormatTopLevel *item = mmay_vector_at(&top_levels, run_end);
            if (item == NULL || item->kind != MMAY_SYNTAX_IMPORT_DECL) {
                break;
            }
            run_end += 1;
        }
        while (segment_start < run_end) {
            MarimayFormatTopLevel *item = mmay_vector_at(
                &top_levels,
                segment_start
            );
            size_t segment_end = segment_start;
            if (item->has_comment) {
                segment_start += 1;
                continue;
            }
            while (segment_end < run_end) {
                item = mmay_vector_at(&top_levels, segment_end);
                if (item->has_comment) {
                    break;
                }
                segment_end += 1;
            }
            if (segment_end - segment_start > 1) {
                qsort(
                    (unsigned char *)top_levels.data
                        + segment_start * top_levels.element_size,
                    segment_end - segment_start,
                    sizeof(MarimayFormatTopLevel),
                    mmay_format_top_level_compare
                );
            }
            segment_start = segment_end;
        }
        index = run_end;
    }
    if (!mmay_vector_init(&ordered, allocator, sizeof(MarimayFormatToken))
        || !mmay_vector_reserve(&ordered, tokens->length)) {
        status = MMAY_FORMAT_ALLOCATION_FAILED;
    }
    for (index = 0; index < top_levels.length
        && status == MMAY_FORMAT_OK; index += 1) {
        const MarimayFormatTopLevel *top_level = mmay_vector_at_const(
            &top_levels,
            index
        );
        const MarimayFormatToken *start = top_level == NULL
            ? NULL
            : mmay_vector_at_const(tokens, top_level->start);
        if (top_level == NULL || start == NULL
            || top_level->end < top_level->start
            || !mmay_vector_append(
                &ordered,
                start,
                top_level->end - top_level->start
            )) {
            status = MMAY_FORMAT_ALLOCATION_FAILED;
        }
    }
    if (status == MMAY_FORMAT_OK) {
        MarimayVector original = *tokens;
        *tokens = ordered;
        memset(&ordered, 0, sizeof(ordered));
        mmay_vector_dispose(&original);
    }
    mmay_vector_dispose(&ordered);
    mmay_format_top_levels_dispose(&top_levels);
    return status;
}

bool mmay_format_collect_node(
    MarimayFormatCollector *collector,
    const MarimayGreenNode *node,
    size_t depth,
    MarimaySyntaxKind top_level_kind,
    size_t top_level_id,
    size_t statement_id,
    bool in_for_header,
    bool in_attribute,
    bool in_record_expression,
    bool in_interpolation,
    bool in_boolean_chain
) {
    size_t index = 0;
    size_t start = collector->tokens.length;
    bool boolean_chain;
    if (node == NULL) {
        collector->status = MMAY_FORMAT_INVALID_ARGUMENT;
        return false;
    }
    if (depth > collector->max_nesting) {
        collector->status = MMAY_FORMAT_LIMIT_REACHED;
        return false;
    }
    in_for_header = in_for_header || node->kind == MMAY_SYNTAX_FOR_HEADER;
    in_attribute = in_attribute || node->kind == MMAY_SYNTAX_ATTRIBUTE;
    in_record_expression = in_record_expression
        || node->kind == MMAY_SYNTAX_RECORD_EXPRESSION;
    in_interpolation = in_interpolation
        || node->kind == MMAY_SYNTAX_INTERPOLATED_EXPRESSION;
    boolean_chain = !in_interpolation && mmay_formatter_boolean_node(node);
    for (index = 0; index < node->child_count; index += 1) {
        const MarimaySyntaxElement *element = mmay_syntax_child(node, index);
        size_t child_top_level_id = top_level_id;
        size_t child_statement_id = statement_id;
        if (element == NULL) {
            collector->status = MMAY_FORMAT_INVALID_ARGUMENT;
            return false;
        }
        if (node->kind == MMAY_SYNTAX_ROOT
            && element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
            if (!mmay_size_add(
                    collector->next_top_level_id,
                    1,
                    &collector->next_top_level_id
                )) {
                collector->status = MMAY_FORMAT_LIMIT_REACHED;
                return false;
            }
            child_top_level_id = collector->next_top_level_id;
            top_level_kind = element->as.node->kind;
        }
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE
            && (element->as.node->kind == MMAY_SYNTAX_CONST_DECL
                || element->as.node->kind == MMAY_SYNTAX_LET_DECL
                || element->as.node->kind == MMAY_SYNTAX_EXPRESSION_STATEMENT
                || element->as.node->kind == MMAY_SYNTAX_RETURN_STATEMENT
                || element->as.node->kind == MMAY_SYNTAX_DEFER_STATEMENT
                || element->as.node->kind == MMAY_SYNTAX_BREAK_STATEMENT
                || element->as.node->kind == MMAY_SYNTAX_CONTINUE_STATEMENT)) {
            if (!mmay_size_add(
                    collector->next_statement_id,
                    1,
                    &collector->next_statement_id
                )) {
                collector->status = MMAY_FORMAT_LIMIT_REACHED;
                return false;
            }
            child_statement_id = collector->next_statement_id;
        }
        if (element->kind == MMAY_SYNTAX_ELEMENT_NODE) {
            if (!mmay_format_collect_node(
                    collector,
                    element->as.node,
                    depth + 1,
                    top_level_kind,
                    child_top_level_id,
                    child_statement_id,
                    in_for_header,
                    in_attribute,
                    in_record_expression,
                    in_interpolation,
                    boolean_chain
                )) {
                return false;
            }
        } else if (element->kind == MMAY_SYNTAX_ELEMENT_TOKEN) {
            if (!mmay_format_collect_token(
                    collector,
                    &element->as.token,
                    node->kind,
                    top_level_kind,
                    top_level_id,
                    statement_id,
                    in_for_header,
                    in_attribute,
                    in_record_expression,
                    in_interpolation
                )) {
                return false;
            }
        } else {
            collector->status = MMAY_FORMAT_INVALID_ARGUMENT;
            return false;
        }
    }
    return !boolean_chain || in_boolean_chain
        || mmay_formatter_boolean_group(collector, start);
}
