#include "formatter_internal.h"

#include <stdlib.h>
#include <string.h>

#include "ruda/base/string_literal.h"
#include "ruda/syntax/syntax.h"

typedef struct RudaFormatTopLevel {
    size_t start;
    size_t end;
    size_t source_order;
    RudaSyntaxKind kind;
    RudaOwnedBytes import_path;
    unsigned int import_group;
    bool has_comment;
} RudaFormatTopLevel;

bool ruda_format_token_is_trivia(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_BOM || kind == RUDA_TOKEN_WHITESPACE;
}

bool ruda_format_token_is_comment(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_LINE_COMMENT || kind == RUDA_TOKEN_DOC_LINE_COMMENT
        || kind == RUDA_TOKEN_BLOCK_COMMENT || kind == RUDA_TOKEN_DOC_BLOCK_COMMENT;
}

bool ruda_format_token_is_visible(RudaTokenKind kind) {
    return kind != RUDA_TOKEN_EOF && !ruda_format_token_is_trivia(kind);
}

static bool ruda_format_collect_token(
    RudaFormatCollector *collector,
    const RudaSyntaxToken *token,
    RudaSyntaxKind parent_kind,
    RudaSyntaxKind top_level_kind,
    size_t top_level_id,
    bool in_for_header,
    bool in_attribute,
    bool in_record_expression
) {
    RudaFormatToken formatted;
    if (token->synthetic) {
        collector->status = RUDA_FORMAT_SOURCE_INVALID;
        return false;
    }
    if (collector->tokens.length >= collector->max_tokens) {
        collector->status = RUDA_FORMAT_LIMIT_REACHED;
        return false;
    }
    formatted.token = *token;
    formatted.parent_kind = parent_kind;
    formatted.top_level_kind = top_level_kind;
    formatted.top_level_id = top_level_id;
    formatted.in_for_header = in_for_header;
    formatted.in_attribute = in_attribute;
    formatted.in_record_expression = in_record_expression;
    formatted.import_group = 0;
    if (!ruda_vector_push(&collector->tokens, &formatted)) {
        collector->status = RUDA_FORMAT_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static RudaFormatStatus ruda_format_decode_normal_string(
    const unsigned char *source,
    size_t source_length,
    const RudaSyntaxToken *token,
    RudaAllocator allocator,
    RudaOwnedBytes *decoded
) {
    RudaStringLiteralStatus status;
    if (token == NULL || token->kind != RUDA_TOKEN_STRING
        || token->end > source_length || token->start > token->end) {
        return RUDA_FORMAT_SOURCE_INVALID;
    }
    status = ruda_string_literal_decode(
        source + token->start,
        token->end - token->start,
        false,
        allocator,
        decoded
    );
    return status == RUDA_STRING_LITERAL_OK ? RUDA_FORMAT_OK
        : (status == RUDA_STRING_LITERAL_ALLOCATION_FAILED
            ? RUDA_FORMAT_ALLOCATION_FAILED
            : RUDA_FORMAT_SOURCE_INVALID);
}

RudaFormatStatus ruda_format_decode_ordinary_string(
    const unsigned char *source,
    size_t source_length,
    const RudaSyntaxToken *token,
    RudaAllocator allocator,
    RudaOwnedBytes *decoded
) {
    RudaStringLiteralStatus status;
    bool raw = token != NULL && token->kind == RUDA_TOKEN_RAW_STRING;
    if (token == NULL || token->end > source_length
        || token->start > token->end
        || (token->kind != RUDA_TOKEN_STRING && !raw)) {
        return RUDA_FORMAT_SOURCE_INVALID;
    }
    if (token->kind == RUDA_TOKEN_STRING) {
        return ruda_format_decode_normal_string(
            source,
            source_length,
            token,
            allocator,
            decoded
        );
    }
    status = ruda_string_literal_decode(
        source + token->start,
        token->end - token->start,
        raw,
        allocator,
        decoded
    );
    return status == RUDA_STRING_LITERAL_OK ? RUDA_FORMAT_OK
        : (status == RUDA_STRING_LITERAL_ALLOCATION_FAILED
            ? RUDA_FORMAT_ALLOCATION_FAILED
            : RUDA_FORMAT_SOURCE_INVALID);
}

static int ruda_format_owned_bytes_compare(
    const RudaOwnedBytes *left,
    const RudaOwnedBytes *right
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

static unsigned int ruda_format_import_group(const RudaOwnedBytes *path) {
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

static int ruda_format_top_level_compare(const void *left, const void *right) {
    const RudaFormatTopLevel *first = left;
    const RudaFormatTopLevel *second = right;
    int result = 0;
    if (first->import_group != second->import_group) {
        return first->import_group < second->import_group ? -1 : 1;
    }
    result = ruda_format_owned_bytes_compare(
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

static void ruda_format_top_levels_dispose(RudaVector *top_levels) {
    size_t index = 0;
    for (index = 0; index < top_levels->length; index += 1) {
        RudaFormatTopLevel *top_level = ruda_vector_at(top_levels, index);
        ruda_owned_bytes_dispose(&top_level->import_path);
    }
    ruda_vector_dispose(top_levels);
}

RudaFormatStatus ruda_format_order_imports(
    RudaVector *tokens,
    const unsigned char *source,
    size_t source_length,
    RudaAllocator allocator
) {
    RudaVector top_levels;
    RudaVector ordered;
    RudaFormatStatus status = RUDA_FORMAT_OK;
    size_t cursor = 0;
    size_t import_count = 0;
    size_t index = 0;
    memset(&top_levels, 0, sizeof(top_levels));
    memset(&ordered, 0, sizeof(ordered));
    if (!ruda_vector_init(
            &top_levels,
            allocator,
            sizeof(RudaFormatTopLevel)
        )) {
        return RUDA_FORMAT_INVALID_ARGUMENT;
    }
    while (cursor < tokens->length) {
        const RudaFormatToken *first = ruda_vector_at_const(tokens, cursor);
        RudaFormatTopLevel top_level;
        size_t scan = cursor;
        bool saw_from = false;
        const RudaSyntaxToken *path_token = NULL;
        if (first == NULL) {
            status = RUDA_FORMAT_INVALID_ARGUMENT;
            break;
        }
        memset(&top_level, 0, sizeof(top_level));
        top_level.start = cursor;
        top_level.kind = first->top_level_id == 0
            ? RUDA_SYNTAX_ROOT
            : first->top_level_kind;
        top_level.source_order = top_levels.length;
        while (scan < tokens->length) {
            RudaFormatToken *token = ruda_vector_at(tokens, scan);
            if (token == NULL || token->top_level_id != first->top_level_id) {
                break;
            }
            if (ruda_format_token_is_comment(token->token.kind)) {
                top_level.has_comment = true;
            }
            if (top_level.kind == RUDA_SYNTAX_IMPORT_DECL) {
                if (token->token.kind == RUDA_TOKEN_KW_FROM) {
                    saw_from = true;
                } else if (saw_from && token->token.kind == RUDA_TOKEN_STRING
                    && path_token == NULL) {
                    path_token = &token->token;
                }
            }
            scan += 1;
        }
        top_level.end = scan;
        if (top_level.kind == RUDA_SYNTAX_IMPORT_DECL) {
            if (path_token == NULL) {
                status = RUDA_FORMAT_SOURCE_INVALID;
                break;
            }
            status = ruda_format_decode_normal_string(
                source,
                source_length,
                path_token,
                allocator,
                &top_level.import_path
            );
            if (status != RUDA_FORMAT_OK) {
                break;
            }
            top_level.import_group = ruda_format_import_group(
                &top_level.import_path
            );
            for (index = top_level.start; index < top_level.end; index += 1) {
                RudaFormatToken *token = ruda_vector_at(tokens, index);
                token->import_group = top_level.import_group;
            }
            import_count += 1;
        }
        if (!ruda_vector_push(&top_levels, &top_level)) {
            ruda_owned_bytes_dispose(&top_level.import_path);
            status = RUDA_FORMAT_ALLOCATION_FAILED;
            break;
        }
        cursor = scan;
    }
    if (status != RUDA_FORMAT_OK || import_count == 0) {
        ruda_format_top_levels_dispose(&top_levels);
        return status;
    }
    index = 0;
    while (index < top_levels.length) {
        RudaFormatTopLevel *first = ruda_vector_at(&top_levels, index);
        size_t run_end = index;
        size_t segment_start = index;
        if (first == NULL || first->kind != RUDA_SYNTAX_IMPORT_DECL) {
            index += 1;
            continue;
        }
        while (run_end < top_levels.length) {
            RudaFormatTopLevel *item = ruda_vector_at(&top_levels, run_end);
            if (item == NULL || item->kind != RUDA_SYNTAX_IMPORT_DECL) {
                break;
            }
            run_end += 1;
        }
        while (segment_start < run_end) {
            RudaFormatTopLevel *item = ruda_vector_at(
                &top_levels,
                segment_start
            );
            size_t segment_end = segment_start;
            if (item->has_comment) {
                segment_start += 1;
                continue;
            }
            while (segment_end < run_end) {
                item = ruda_vector_at(&top_levels, segment_end);
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
                    sizeof(RudaFormatTopLevel),
                    ruda_format_top_level_compare
                );
            }
            segment_start = segment_end;
        }
        index = run_end;
    }
    if (!ruda_vector_init(&ordered, allocator, sizeof(RudaFormatToken))
        || !ruda_vector_reserve(&ordered, tokens->length)) {
        status = RUDA_FORMAT_ALLOCATION_FAILED;
    }
    for (index = 0; index < top_levels.length
        && status == RUDA_FORMAT_OK; index += 1) {
        const RudaFormatTopLevel *top_level = ruda_vector_at_const(
            &top_levels,
            index
        );
        const RudaFormatToken *start = top_level == NULL
            ? NULL
            : ruda_vector_at_const(tokens, top_level->start);
        if (top_level == NULL || start == NULL
            || top_level->end < top_level->start
            || !ruda_vector_append(
                &ordered,
                start,
                top_level->end - top_level->start
            )) {
            status = RUDA_FORMAT_ALLOCATION_FAILED;
        }
    }
    if (status == RUDA_FORMAT_OK) {
        RudaVector original = *tokens;
        *tokens = ordered;
        memset(&ordered, 0, sizeof(ordered));
        ruda_vector_dispose(&original);
    }
    ruda_vector_dispose(&ordered);
    ruda_format_top_levels_dispose(&top_levels);
    return status;
}

bool ruda_format_collect_node(
    RudaFormatCollector *collector,
    const RudaGreenNode *node,
    size_t depth,
    RudaSyntaxKind top_level_kind,
    size_t top_level_id,
    bool in_for_header,
    bool in_attribute,
    bool in_record_expression
) {
    size_t index = 0;
    if (node == NULL) {
        collector->status = RUDA_FORMAT_INVALID_ARGUMENT;
        return false;
    }
    if (depth > collector->max_nesting) {
        collector->status = RUDA_FORMAT_LIMIT_REACHED;
        return false;
    }
    in_for_header = in_for_header || node->kind == RUDA_SYNTAX_FOR_HEADER;
    in_attribute = in_attribute || node->kind == RUDA_SYNTAX_ATTRIBUTE;
    in_record_expression = in_record_expression
        || node->kind == RUDA_SYNTAX_RECORD_EXPRESSION;
    for (index = 0; index < node->child_count; index += 1) {
        const RudaSyntaxElement *element = ruda_syntax_child(node, index);
        size_t child_top_level_id = top_level_id;
        if (element == NULL) {
            collector->status = RUDA_FORMAT_INVALID_ARGUMENT;
            return false;
        }
        if (node->kind == RUDA_SYNTAX_ROOT
            && element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            if (!ruda_size_add(
                    collector->next_top_level_id,
                    1,
                    &collector->next_top_level_id
                )) {
                collector->status = RUDA_FORMAT_LIMIT_REACHED;
                return false;
            }
            child_top_level_id = collector->next_top_level_id;
            top_level_kind = element->as.node->kind;
        }
        if (element->kind == RUDA_SYNTAX_ELEMENT_NODE) {
            if (!ruda_format_collect_node(
                    collector,
                    element->as.node,
                    depth + 1,
                    top_level_kind,
                    child_top_level_id,
                    in_for_header,
                    in_attribute,
                    in_record_expression
                )) {
                return false;
            }
        } else if (element->kind == RUDA_SYNTAX_ELEMENT_TOKEN) {
            if (!ruda_format_collect_token(
                    collector,
                    &element->as.token,
                    node->kind,
                    top_level_kind,
                    top_level_id,
                    in_for_header,
                    in_attribute,
                    in_record_expression
                )) {
                return false;
            }
        } else {
            collector->status = RUDA_FORMAT_INVALID_ARGUMENT;
            return false;
        }
    }
    return true;
}
