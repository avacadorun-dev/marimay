#include "test_formatter_support.h"

typedef struct MarimayFormatterStringIterator {
    const unsigned char *source;
    size_t cursor;
    size_t end;
    unsigned char pending[4];
    size_t pending_cursor;
    size_t pending_length;
    bool raw;
} MarimayFormatterStringIterator;

static int mmay_formatter_test_hex_digit(unsigned char byte) {
    if (byte >= '0' && byte <= '9') {
        return (int)(byte - '0');
    }
    if (byte >= 'a' && byte <= 'f') {
        return (int)(byte - 'a') + 10;
    }
    if (byte >= 'A' && byte <= 'F') {
        return (int)(byte - 'A') + 10;
    }
    return -1;
}

static bool mmay_formatter_string_iterator_init(
    MarimayFormatterStringIterator *iterator,
    const MarimaySyntaxToken *token,
    const MarimaySourceSnapshot *source
) {
    const unsigned char *bytes = mmay_source_bytes(source);
    size_t length = mmay_source_length(source);
    memset(iterator, 0, sizeof(*iterator));
    if (token->synthetic || token->end > length) {
        return false;
    }
    iterator->source = bytes;
    if (token->kind == MMAY_TOKEN_STRING && token->end >= token->start + 2
        && bytes[token->start] == '"' && bytes[token->end - 1] == '"') {
        iterator->cursor = token->start + 1;
        iterator->end = token->end - 1;
        return true;
    }
    if (token->kind == MMAY_TOKEN_RAW_STRING
        && token->end >= token->start + 3
        && bytes[token->start] == 'r' && bytes[token->start + 1] == '"'
        && bytes[token->end - 1] == '"') {
        iterator->cursor = token->start + 2;
        iterator->end = token->end - 1;
        iterator->raw = true;
        return true;
    }
    return false;
}

static bool mmay_formatter_string_iterator_next(
    MarimayFormatterStringIterator *iterator,
    unsigned char *value,
    bool *present
) {
    unsigned char byte = 0;
    *present = false;
    if (iterator->pending_cursor < iterator->pending_length) {
        *value = iterator->pending[iterator->pending_cursor];
        iterator->pending_cursor += 1;
        *present = true;
        return true;
    }
    if (iterator->cursor >= iterator->end) {
        return true;
    }
    byte = iterator->source[iterator->cursor];
    iterator->cursor += 1;
    if (iterator->raw || byte != '\\') {
        *value = byte;
        *present = true;
        return true;
    }
    if (iterator->cursor >= iterator->end) {
        return false;
    }
    byte = iterator->source[iterator->cursor];
    iterator->cursor += 1;
    if (byte == '\\' || byte == '"' || byte == '\'') {
        *value = byte;
    } else if (byte == 'n') {
        *value = '\n';
    } else if (byte == 'r') {
        *value = '\r';
    } else if (byte == 't') {
        *value = '\t';
    } else if (byte == '0') {
        *value = 0;
    } else if (byte == 'u') {
        uint32_t scalar = 0;
        size_t digits = 0;
        if (iterator->cursor >= iterator->end
            || iterator->source[iterator->cursor] != '{') {
            return false;
        }
        iterator->cursor += 1;
        while (iterator->cursor < iterator->end
            && iterator->source[iterator->cursor] != '}') {
            int digit = mmay_formatter_test_hex_digit(
                iterator->source[iterator->cursor]
            );
            if (digit < 0 || digits >= 6) {
                return false;
            }
            scalar = (scalar << 4) | (uint32_t)digit;
            digits += 1;
            iterator->cursor += 1;
        }
        if (digits == 0 || iterator->cursor >= iterator->end
            || iterator->source[iterator->cursor] != '}'
            || !mmay_utf8_encode(
                scalar,
                iterator->pending,
                &iterator->pending_length
            )) {
            return false;
        }
        iterator->cursor += 1;
        iterator->pending_cursor = 1;
        *value = iterator->pending[0];
    } else {
        return false;
    }
    *present = true;
    return true;
}

static bool mmay_formatter_string_tokens_equal(
    const MarimaySyntaxToken *left,
    const MarimaySourceSnapshot *left_source,
    const MarimaySyntaxToken *right,
    const MarimaySourceSnapshot *right_source
) {
    MarimayFormatterStringIterator left_iterator;
    MarimayFormatterStringIterator right_iterator;
    if (!mmay_formatter_string_iterator_init(
            &left_iterator,
            left,
            left_source
        ) || !mmay_formatter_string_iterator_init(
            &right_iterator,
            right,
            right_source
        )) {
        return false;
    }
    for (;;) {
        unsigned char left_byte = 0;
        unsigned char right_byte = 0;
        bool left_present = false;
        bool right_present = false;
        if (!mmay_formatter_string_iterator_next(
                &left_iterator,
                &left_byte,
                &left_present
            ) || !mmay_formatter_string_iterator_next(
                &right_iterator,
                &right_byte,
                &right_present
            )) {
            return false;
        }
        if (left_present != right_present) {
            return false;
        }
        if (!left_present) {
            return true;
        }
        if (left_byte != right_byte) {
            return false;
        }
    }
}

static bool mmay_formatter_ast_tokens_equal(
    const MarimaySyntaxToken *left,
    const MarimaySourceSnapshot *left_source,
    const MarimaySyntaxToken *right,
    const MarimaySourceSnapshot *right_source
) {
    size_t left_length = left->end - left->start;
    size_t right_length = right->end - right->start;
    bool left_string = left->kind == MMAY_TOKEN_STRING
        || left->kind == MMAY_TOKEN_RAW_STRING;
    bool right_string = right->kind == MMAY_TOKEN_STRING
        || right->kind == MMAY_TOKEN_RAW_STRING;
    if (left->synthetic != right->synthetic) {
        return false;
    }
    if (left_string || right_string) {
        return left_string && right_string
            && mmay_formatter_string_tokens_equal(
                left,
                left_source,
                right,
                right_source
            );
    }
    return left->kind == right->kind && left_length == right_length
        && memcmp(
            mmay_source_bytes(left_source) + left->start,
            mmay_source_bytes(right_source) + right->start,
            left_length
        ) == 0;
}

bool mmay_formatter_fixture_init(
    MarimayFormatterFixture *fixture,
    const char *path,
    const unsigned char *bytes,
    size_t length
) {
    memset(fixture, 0, sizeof(*fixture));
    if (!mmay_source_manager_init(&fixture->sources, mmay_system_allocator())
        || mmay_source_manager_add(
            &fixture->sources,
            path,
            strlen(path),
            bytes,
            length,
            &fixture->snapshot
        ) != MMAY_SOURCE_OK
        || mmay_lex_snapshot(
            fixture->snapshot,
            mmay_system_allocator(),
            mmay_lex_default_options(),
            &fixture->lexed
        ) != MMAY_LEX_OK
        || mmay_parse_file(
            &fixture->lexed,
            mmay_system_allocator(),
            mmay_parse_default_options(),
            &fixture->parsed
        ) != MMAY_PARSE_OK) {
        if (fixture->lexed.initialized) {
            mmay_lex_result_dispose(&fixture->lexed);
        }
        mmay_source_manager_dispose(&fixture->sources);
        return false;
    }
    return true;
}

void mmay_formatter_fixture_dispose(MarimayFormatterFixture *fixture) {
    mmay_parse_result_dispose(&fixture->parsed);
    mmay_lex_result_dispose(&fixture->lexed);
    mmay_source_manager_dispose(&fixture->sources);
}

static bool mmay_formatter_ast_equal_internal(
    const MarimayAstNode *left,
    const MarimaySourceSnapshot *left_source,
    const MarimayAstNode *right,
    const MarimaySourceSnapshot *right_source,
    bool report
) {
    size_t left_index = 0;
    size_t right_index = 0;
    if (left == NULL || right == NULL || left->kind != right->kind) {
        if (report) {
            fprintf(
                stderr,
                "AST node mismatch: left=%d right=%d\n",
                left == NULL ? -1 : (int)left->kind,
                right == NULL ? -1 : (int)right->kind
            );
        }
        return false;
    }
    while (left_index < left->child_count
        || right_index < right->child_count) {
        const MarimayAstElement *left_element = mmay_ast_child(left, left_index);
        const MarimayAstElement *right_element = mmay_ast_child(right, right_index);
        bool list_kind = left->kind == MMAY_SYNTAX_ARGUMENTS
            || left->kind == MMAY_SYNTAX_PARAMETERS
            || left->kind == MMAY_SYNTAX_IMPORT_CLAUSE
            || left->kind == MMAY_SYNTAX_ARRAY_EXPRESSION
            || left->kind == MMAY_SYNTAX_ENUM_DECL
            || left->kind == MMAY_SYNTAX_ENUM_VARIANT
            || left->kind == MMAY_SYNTAX_MATCH_EXPRESSION
            || left->kind == MMAY_SYNTAX_NAMED_PAYLOAD
            || left->kind == MMAY_SYNTAX_RECORD_DECL
            || left->kind == MMAY_SYNTAX_RECORD_EXPRESSION
            || left->kind == MMAY_SYNTAX_RECORD_FIELDS
            || left->kind == MMAY_SYNTAX_PAREN_GROUP
            || left->kind == MMAY_SYNTAX_BRACKET_GROUP
            || left->kind == MMAY_SYNTAX_POSITIONAL_PATTERN
            || left->kind == MMAY_SYNTAX_TUPLE_PATTERN
            || left->kind == MMAY_SYNTAX_NAMED_PATTERN
            || left->kind == MMAY_SYNTAX_PATTERN_LIST
            || left->kind == MMAY_SYNTAX_PATTERN_FIELDS;
        bool left_trailing_comma = false;
        bool right_trailing_comma = false;
        if (list_kind && left_element != NULL
            && left_element->kind == MMAY_AST_ELEMENT_TOKEN
            && left_element->as.token.kind == MMAY_TOKEN_COMMA
            && left_index + 1 < left->child_count) {
            const MarimayAstElement *after = mmay_ast_child(left, left_index + 1);
            left_trailing_comma = after != NULL
                && after->kind == MMAY_AST_ELEMENT_TOKEN
                && (after->as.token.kind == MMAY_TOKEN_RIGHT_PAREN
                    || after->as.token.kind == MMAY_TOKEN_RIGHT_BRACKET
                    || after->as.token.kind == MMAY_TOKEN_RIGHT_BRACE);
        } else if ((left->kind == MMAY_SYNTAX_PATTERN_LIST
                || left->kind == MMAY_SYNTAX_PATTERN_FIELDS)
            && left_element != NULL
            && left_element->kind == MMAY_AST_ELEMENT_TOKEN
            && left_element->as.token.kind == MMAY_TOKEN_COMMA
            && left_index + 1 == left->child_count) {
            left_trailing_comma = true;
        }
        if (list_kind && right_element != NULL
            && right_element->kind == MMAY_AST_ELEMENT_TOKEN
            && right_element->as.token.kind == MMAY_TOKEN_COMMA
            && right_index + 1 < right->child_count) {
            const MarimayAstElement *after = mmay_ast_child(right, right_index + 1);
            right_trailing_comma = after != NULL
                && after->kind == MMAY_AST_ELEMENT_TOKEN
                && (after->as.token.kind == MMAY_TOKEN_RIGHT_PAREN
                    || after->as.token.kind == MMAY_TOKEN_RIGHT_BRACKET
                    || after->as.token.kind == MMAY_TOKEN_RIGHT_BRACE);
        } else if ((right->kind == MMAY_SYNTAX_PATTERN_LIST
                || right->kind == MMAY_SYNTAX_PATTERN_FIELDS)
            && right_element != NULL
            && right_element->kind == MMAY_AST_ELEMENT_TOKEN
            && right_element->as.token.kind == MMAY_TOKEN_COMMA
            && right_index + 1 == right->child_count) {
            right_trailing_comma = true;
        }
        if (left_trailing_comma) {
            left_index += 1;
            continue;
        }
        if (right_trailing_comma) {
            right_index += 1;
            continue;
        }
        if (left_element == NULL || right_element == NULL
            || left_element->kind != right_element->kind) {
            if (report) {
                fprintf(
                    stderr,
                    "AST child mismatch in node %d at %zu/%zu\n",
                    (int)left->kind,
                    left_index,
                    right_index
                );
            }
            return false;
        }
        if (left_element->kind == MMAY_AST_ELEMENT_NODE) {
            if (!mmay_formatter_ast_equal_internal(
                    left_element->as.node,
                    left_source,
                    right_element->as.node,
                    right_source,
                    report
                )) {
                return false;
            }
        } else {
            const MarimaySyntaxToken *left_token = &left_element->as.token;
            const MarimaySyntaxToken *right_token = &right_element->as.token;
            if (!mmay_formatter_ast_tokens_equal(
                    left_token,
                    left_source,
                    right_token,
                    right_source
                )) {
                if (report) {
                    fprintf(
                        stderr,
                        "AST token mismatch in node %d at %zu/%zu: %d/%d\n",
                        (int)left->kind,
                        left_index,
                        right_index,
                        (int)left_token->kind,
                        (int)right_token->kind
                    );
                }
                return false;
            }
        }
        left_index += 1;
        right_index += 1;
    }
    return true;
}

static bool mmay_formatter_ast_equal(
    const MarimayAstNode *left,
    const MarimaySourceSnapshot *left_source,
    const MarimayAstNode *right,
    const MarimaySourceSnapshot *right_source
) {
    return mmay_formatter_ast_equal_internal(
        left,
        left_source,
        right,
        right_source,
        true
    );
}

static bool mmay_formatter_ast_root_element_is_import(
    const MarimayAstElement *element
) {
    return element != NULL
        && element->kind == MMAY_AST_ELEMENT_NODE
        && element->as.node->kind == MMAY_SYNTAX_IMPORT_DECL;
}

static bool mmay_formatter_ast_root_token_equal(
    const MarimaySyntaxToken *left,
    const MarimaySourceSnapshot *left_source,
    const MarimaySyntaxToken *right,
    const MarimaySourceSnapshot *right_source
) {
    return mmay_formatter_ast_tokens_equal(
        left,
        left_source,
        right,
        right_source
    );
}

bool mmay_formatter_ast_semantically_equal(
    const MarimayAstNode *left,
    const MarimaySourceSnapshot *left_source,
    const MarimayAstNode *right,
    const MarimaySourceSnapshot *right_source
) {
    bool *matched = NULL;
    size_t left_index = 0;
    size_t right_index = 0;
    size_t left_imports = 0;
    size_t right_imports = 0;
    bool equal = true;
    if (left == NULL || right == NULL || left->kind != MMAY_SYNTAX_ROOT
        || right->kind != MMAY_SYNTAX_ROOT) {
        return mmay_formatter_ast_equal(
            left,
            left_source,
            right,
            right_source
        );
    }
    matched = calloc(right->child_count, sizeof(bool));
    if (matched == NULL && right->child_count != 0) {
        return false;
    }
    for (left_index = 0; left_index < left->child_count; left_index += 1) {
        const MarimayAstElement *element = mmay_ast_child(left, left_index);
        if (mmay_formatter_ast_root_element_is_import(element)) {
            bool found = false;
            left_imports += 1;
            for (right_index = 0;
                right_index < right->child_count;
                right_index += 1) {
                const MarimayAstElement *candidate = mmay_ast_child(
                    right,
                    right_index
                );
                if (!matched[right_index]
                    && mmay_formatter_ast_root_element_is_import(candidate)
                    && mmay_formatter_ast_equal_internal(
                        element->as.node,
                        left_source,
                        candidate->as.node,
                        right_source,
                        false
                    )) {
                    matched[right_index] = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                equal = false;
                break;
            }
        }
    }
    for (right_index = 0; equal && right_index < right->child_count;
        right_index += 1) {
        if (mmay_formatter_ast_root_element_is_import(
                mmay_ast_child(right, right_index)
            )) {
            right_imports += 1;
        }
    }
    equal = equal && left_imports == right_imports;
    left_index = 0;
    right_index = 0;
    while (equal) {
        const MarimayAstElement *left_element = NULL;
        const MarimayAstElement *right_element = NULL;
        while (left_index < left->child_count) {
            left_element = mmay_ast_child(left, left_index);
            left_index += 1;
            if (!mmay_formatter_ast_root_element_is_import(left_element)) {
                break;
            }
            left_element = NULL;
        }
        while (right_index < right->child_count) {
            right_element = mmay_ast_child(right, right_index);
            right_index += 1;
            if (!mmay_formatter_ast_root_element_is_import(right_element)) {
                break;
            }
            right_element = NULL;
        }
        if (left_element == NULL || right_element == NULL) {
            equal = left_element == NULL && right_element == NULL;
            break;
        }
        if (left_element->kind != right_element->kind) {
            equal = false;
        } else if (left_element->kind == MMAY_AST_ELEMENT_NODE) {
            equal = mmay_formatter_ast_equal_internal(
                left_element->as.node,
                left_source,
                right_element->as.node,
                right_source,
                false
            );
        } else {
            equal = mmay_formatter_ast_root_token_equal(
                &left_element->as.token,
                left_source,
                &right_element->as.token,
                right_source
            );
        }
    }
    free(matched);
    return equal;
}

static bool mmay_formatter_comment_kind(MarimayTokenKind kind) {
    return kind == MMAY_TOKEN_LINE_COMMENT
        || kind == MMAY_TOKEN_DOC_LINE_COMMENT
        || kind == MMAY_TOKEN_BLOCK_COMMENT
        || kind == MMAY_TOKEN_DOC_BLOCK_COMMENT;
}

static unsigned char mmay_formatter_normalized_comment_byte(
    const unsigned char *bytes,
    size_t length,
    size_t *index
) {
    unsigned char byte = bytes[*index];
    *index += 1;
    if (byte == '\r') {
        if (*index < length && bytes[*index] == '\n') {
            *index += 1;
        }
        return '\n';
    }
    return byte;
}

static bool mmay_formatter_comment_text_equal(
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length
) {
    size_t left_index = 0;
    size_t right_index = 0;
    while (left_index < left_length && right_index < right_length) {
        if (mmay_formatter_normalized_comment_byte(
                left,
                left_length,
                &left_index
            ) != mmay_formatter_normalized_comment_byte(
                right,
                right_length,
                &right_index
            )) {
            return false;
        }
    }
    return left_index == left_length && right_index == right_length;
}

bool mmay_formatter_comments_equal(
    const MarimayLexResult *left,
    const MarimayLexResult *right
) {
    size_t left_index = 0;
    size_t right_index = 0;
    const unsigned char *left_bytes = mmay_source_bytes(left->snapshot);
    const unsigned char *right_bytes = mmay_source_bytes(right->snapshot);
    for (;;) {
        const MarimayToken *left_token = NULL;
        const MarimayToken *right_token = NULL;
        while (left_index < left->tokens.length) {
            left_token = mmay_lex_token(left, left_index);
            left_index += 1;
            if (left_token == NULL
                || mmay_formatter_comment_kind(left_token->kind)) {
                break;
            }
            left_token = NULL;
        }
        while (right_index < right->tokens.length) {
            right_token = mmay_lex_token(right, right_index);
            right_index += 1;
            if (right_token == NULL
                || mmay_formatter_comment_kind(right_token->kind)) {
                break;
            }
            right_token = NULL;
        }
        if (left_token == NULL || right_token == NULL) {
            return left_token == NULL && right_token == NULL;
        }
        if (left_token->kind != right_token->kind
            || !mmay_formatter_comment_text_equal(
                left_bytes + left_token->start,
                left_token->end - left_token->start,
                right_bytes + right_token->start,
                right_token->end - right_token->start
            )) {
            return false;
        }
    }
}

bool mmay_formatter_read_file(
    const char *path,
    unsigned char *bytes,
    size_t capacity,
    size_t *length
) {
    FILE *file = fopen(path, "rb");
    size_t count = 0;
    bool ok = false;
    int close_status = 0;
    if (file == NULL) {
        return false;
    }
    count = fread(bytes, 1, capacity, file);
    ok = ferror(file) == 0 && feof(file) != 0;
    close_status = fclose(file);
    ok = ok && close_status == 0;
    if (ok) {
        *length = count;
    }
    return ok;
}
