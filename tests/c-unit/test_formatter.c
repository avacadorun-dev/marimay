#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/base/allocator.h"
#include "ruda/base/utf8.h"
#include "ruda/format/formatter.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

#include "semantic_oracle.h"

typedef struct RudaFormatterFixture {
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot;
    RudaLexResult lexed;
    RudaParseResult parsed;
} RudaFormatterFixture;

typedef struct RudaFormatterStringIterator {
    const unsigned char *source;
    size_t cursor;
    size_t end;
    unsigned char pending[4];
    size_t pending_cursor;
    size_t pending_length;
    bool raw;
} RudaFormatterStringIterator;

static int ruda_formatter_test_hex_digit(unsigned char byte) {
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

static bool ruda_formatter_string_iterator_init(
    RudaFormatterStringIterator *iterator,
    const RudaSyntaxToken *token,
    const RudaSourceSnapshot *source
) {
    const unsigned char *bytes = ruda_source_bytes(source);
    size_t length = ruda_source_length(source);
    memset(iterator, 0, sizeof(*iterator));
    if (token->synthetic || token->end > length) {
        return false;
    }
    iterator->source = bytes;
    if (token->kind == RUDA_TOKEN_STRING && token->end >= token->start + 2
        && bytes[token->start] == '"' && bytes[token->end - 1] == '"') {
        iterator->cursor = token->start + 1;
        iterator->end = token->end - 1;
        return true;
    }
    if (token->kind == RUDA_TOKEN_RAW_STRING
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

static bool ruda_formatter_string_iterator_next(
    RudaFormatterStringIterator *iterator,
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
            int digit = ruda_formatter_test_hex_digit(
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
            || !ruda_utf8_encode(
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

static bool ruda_formatter_string_tokens_equal(
    const RudaSyntaxToken *left,
    const RudaSourceSnapshot *left_source,
    const RudaSyntaxToken *right,
    const RudaSourceSnapshot *right_source
) {
    RudaFormatterStringIterator left_iterator;
    RudaFormatterStringIterator right_iterator;
    if (!ruda_formatter_string_iterator_init(
            &left_iterator,
            left,
            left_source
        ) || !ruda_formatter_string_iterator_init(
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
        if (!ruda_formatter_string_iterator_next(
                &left_iterator,
                &left_byte,
                &left_present
            ) || !ruda_formatter_string_iterator_next(
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

static bool ruda_formatter_ast_tokens_equal(
    const RudaSyntaxToken *left,
    const RudaSourceSnapshot *left_source,
    const RudaSyntaxToken *right,
    const RudaSourceSnapshot *right_source
) {
    size_t left_length = left->end - left->start;
    size_t right_length = right->end - right->start;
    bool left_string = left->kind == RUDA_TOKEN_STRING
        || left->kind == RUDA_TOKEN_RAW_STRING;
    bool right_string = right->kind == RUDA_TOKEN_STRING
        || right->kind == RUDA_TOKEN_RAW_STRING;
    if (left->synthetic != right->synthetic) {
        return false;
    }
    if (left_string || right_string) {
        return left_string && right_string
            && ruda_formatter_string_tokens_equal(
                left,
                left_source,
                right,
                right_source
            );
    }
    return left->kind == right->kind && left_length == right_length
        && memcmp(
            ruda_source_bytes(left_source) + left->start,
            ruda_source_bytes(right_source) + right->start,
            left_length
        ) == 0;
}

static bool ruda_formatter_fixture_init(
    RudaFormatterFixture *fixture,
    const char *path,
    const unsigned char *bytes,
    size_t length
) {
    memset(fixture, 0, sizeof(*fixture));
    if (!ruda_source_manager_init(&fixture->sources, ruda_system_allocator())
        || ruda_source_manager_add(
            &fixture->sources,
            path,
            strlen(path),
            bytes,
            length,
            &fixture->snapshot
        ) != RUDA_SOURCE_OK
        || ruda_lex_snapshot(
            fixture->snapshot,
            ruda_system_allocator(),
            ruda_lex_default_options(),
            &fixture->lexed
        ) != RUDA_LEX_OK
        || ruda_parse_file(
            &fixture->lexed,
            ruda_system_allocator(),
            ruda_parse_default_options(),
            &fixture->parsed
        ) != RUDA_PARSE_OK) {
        if (fixture->lexed.initialized) {
            ruda_lex_result_dispose(&fixture->lexed);
        }
        ruda_source_manager_dispose(&fixture->sources);
        return false;
    }
    return true;
}

static void ruda_formatter_fixture_dispose(RudaFormatterFixture *fixture) {
    ruda_parse_result_dispose(&fixture->parsed);
    ruda_lex_result_dispose(&fixture->lexed);
    ruda_source_manager_dispose(&fixture->sources);
}

static bool ruda_formatter_ast_equal_internal(
    const RudaAstNode *left,
    const RudaSourceSnapshot *left_source,
    const RudaAstNode *right,
    const RudaSourceSnapshot *right_source,
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
        const RudaAstElement *left_element = ruda_ast_child(left, left_index);
        const RudaAstElement *right_element = ruda_ast_child(right, right_index);
        bool list_kind = left->kind == RUDA_SYNTAX_ARGUMENTS
            || left->kind == RUDA_SYNTAX_PARAMETERS
            || left->kind == RUDA_SYNTAX_IMPORT_CLAUSE
            || left->kind == RUDA_SYNTAX_ARRAY_EXPRESSION
            || left->kind == RUDA_SYNTAX_RECORD_EXPRESSION
            || left->kind == RUDA_SYNTAX_RECORD_FIELDS
            || left->kind == RUDA_SYNTAX_PAREN_GROUP
            || left->kind == RUDA_SYNTAX_BRACKET_GROUP
            || left->kind == RUDA_SYNTAX_POSITIONAL_PATTERN
            || left->kind == RUDA_SYNTAX_TUPLE_PATTERN
            || left->kind == RUDA_SYNTAX_NAMED_PATTERN
            || left->kind == RUDA_SYNTAX_PATTERN_LIST
            || left->kind == RUDA_SYNTAX_PATTERN_FIELDS;
        bool left_trailing_comma = false;
        bool right_trailing_comma = false;
        if (list_kind && left_element != NULL
            && left_element->kind == RUDA_AST_ELEMENT_TOKEN
            && left_element->as.token.kind == RUDA_TOKEN_COMMA
            && left_index + 1 < left->child_count) {
            const RudaAstElement *after = ruda_ast_child(left, left_index + 1);
            left_trailing_comma = after != NULL
                && after->kind == RUDA_AST_ELEMENT_TOKEN
                && (after->as.token.kind == RUDA_TOKEN_RIGHT_PAREN
                    || after->as.token.kind == RUDA_TOKEN_RIGHT_BRACKET
                    || after->as.token.kind == RUDA_TOKEN_RIGHT_BRACE);
        } else if ((left->kind == RUDA_SYNTAX_PATTERN_LIST
                || left->kind == RUDA_SYNTAX_PATTERN_FIELDS)
            && left_element != NULL
            && left_element->kind == RUDA_AST_ELEMENT_TOKEN
            && left_element->as.token.kind == RUDA_TOKEN_COMMA
            && left_index + 1 == left->child_count) {
            left_trailing_comma = true;
        }
        if (list_kind && right_element != NULL
            && right_element->kind == RUDA_AST_ELEMENT_TOKEN
            && right_element->as.token.kind == RUDA_TOKEN_COMMA
            && right_index + 1 < right->child_count) {
            const RudaAstElement *after = ruda_ast_child(right, right_index + 1);
            right_trailing_comma = after != NULL
                && after->kind == RUDA_AST_ELEMENT_TOKEN
                && (after->as.token.kind == RUDA_TOKEN_RIGHT_PAREN
                    || after->as.token.kind == RUDA_TOKEN_RIGHT_BRACKET
                    || after->as.token.kind == RUDA_TOKEN_RIGHT_BRACE);
        } else if ((right->kind == RUDA_SYNTAX_PATTERN_LIST
                || right->kind == RUDA_SYNTAX_PATTERN_FIELDS)
            && right_element != NULL
            && right_element->kind == RUDA_AST_ELEMENT_TOKEN
            && right_element->as.token.kind == RUDA_TOKEN_COMMA
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
        if (left_element->kind == RUDA_AST_ELEMENT_NODE) {
            if (!ruda_formatter_ast_equal_internal(
                    left_element->as.node,
                    left_source,
                    right_element->as.node,
                    right_source,
                    report
                )) {
                return false;
            }
        } else {
            const RudaSyntaxToken *left_token = &left_element->as.token;
            const RudaSyntaxToken *right_token = &right_element->as.token;
            if (!ruda_formatter_ast_tokens_equal(
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

static bool ruda_formatter_ast_equal(
    const RudaAstNode *left,
    const RudaSourceSnapshot *left_source,
    const RudaAstNode *right,
    const RudaSourceSnapshot *right_source
) {
    return ruda_formatter_ast_equal_internal(
        left,
        left_source,
        right,
        right_source,
        true
    );
}

static bool ruda_formatter_ast_root_element_is_import(
    const RudaAstElement *element
) {
    return element != NULL
        && element->kind == RUDA_AST_ELEMENT_NODE
        && element->as.node->kind == RUDA_SYNTAX_IMPORT_DECL;
}

static bool ruda_formatter_ast_root_token_equal(
    const RudaSyntaxToken *left,
    const RudaSourceSnapshot *left_source,
    const RudaSyntaxToken *right,
    const RudaSourceSnapshot *right_source
) {
    return ruda_formatter_ast_tokens_equal(
        left,
        left_source,
        right,
        right_source
    );
}

static bool ruda_formatter_ast_semantically_equal(
    const RudaAstNode *left,
    const RudaSourceSnapshot *left_source,
    const RudaAstNode *right,
    const RudaSourceSnapshot *right_source
) {
    bool *matched = NULL;
    size_t left_index = 0;
    size_t right_index = 0;
    size_t left_imports = 0;
    size_t right_imports = 0;
    bool equal = true;
    if (left == NULL || right == NULL || left->kind != RUDA_SYNTAX_ROOT
        || right->kind != RUDA_SYNTAX_ROOT) {
        return ruda_formatter_ast_equal(
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
        const RudaAstElement *element = ruda_ast_child(left, left_index);
        if (ruda_formatter_ast_root_element_is_import(element)) {
            bool found = false;
            left_imports += 1;
            for (right_index = 0;
                right_index < right->child_count;
                right_index += 1) {
                const RudaAstElement *candidate = ruda_ast_child(
                    right,
                    right_index
                );
                if (!matched[right_index]
                    && ruda_formatter_ast_root_element_is_import(candidate)
                    && ruda_formatter_ast_equal_internal(
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
        if (ruda_formatter_ast_root_element_is_import(
                ruda_ast_child(right, right_index)
            )) {
            right_imports += 1;
        }
    }
    equal = equal && left_imports == right_imports;
    left_index = 0;
    right_index = 0;
    while (equal) {
        const RudaAstElement *left_element = NULL;
        const RudaAstElement *right_element = NULL;
        while (left_index < left->child_count) {
            left_element = ruda_ast_child(left, left_index);
            left_index += 1;
            if (!ruda_formatter_ast_root_element_is_import(left_element)) {
                break;
            }
            left_element = NULL;
        }
        while (right_index < right->child_count) {
            right_element = ruda_ast_child(right, right_index);
            right_index += 1;
            if (!ruda_formatter_ast_root_element_is_import(right_element)) {
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
        } else if (left_element->kind == RUDA_AST_ELEMENT_NODE) {
            equal = ruda_formatter_ast_equal_internal(
                left_element->as.node,
                left_source,
                right_element->as.node,
                right_source,
                false
            );
        } else {
            equal = ruda_formatter_ast_root_token_equal(
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

static bool ruda_formatter_comment_kind(RudaTokenKind kind) {
    return kind == RUDA_TOKEN_LINE_COMMENT
        || kind == RUDA_TOKEN_DOC_LINE_COMMENT
        || kind == RUDA_TOKEN_BLOCK_COMMENT
        || kind == RUDA_TOKEN_DOC_BLOCK_COMMENT;
}

static unsigned char ruda_formatter_normalized_comment_byte(
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

static bool ruda_formatter_comment_text_equal(
    const unsigned char *left,
    size_t left_length,
    const unsigned char *right,
    size_t right_length
) {
    size_t left_index = 0;
    size_t right_index = 0;
    while (left_index < left_length && right_index < right_length) {
        if (ruda_formatter_normalized_comment_byte(
                left,
                left_length,
                &left_index
            ) != ruda_formatter_normalized_comment_byte(
                right,
                right_length,
                &right_index
            )) {
            return false;
        }
    }
    return left_index == left_length && right_index == right_length;
}

static bool ruda_formatter_comments_equal(
    const RudaLexResult *left,
    const RudaLexResult *right
) {
    size_t left_index = 0;
    size_t right_index = 0;
    const unsigned char *left_bytes = ruda_source_bytes(left->snapshot);
    const unsigned char *right_bytes = ruda_source_bytes(right->snapshot);
    for (;;) {
        const RudaToken *left_token = NULL;
        const RudaToken *right_token = NULL;
        while (left_index < left->tokens.length) {
            left_token = ruda_lex_token(left, left_index);
            left_index += 1;
            if (left_token == NULL
                || ruda_formatter_comment_kind(left_token->kind)) {
                break;
            }
            left_token = NULL;
        }
        while (right_index < right->tokens.length) {
            right_token = ruda_lex_token(right, right_index);
            right_index += 1;
            if (right_token == NULL
                || ruda_formatter_comment_kind(right_token->kind)) {
                break;
            }
            right_token = NULL;
        }
        if (left_token == NULL || right_token == NULL) {
            return left_token == NULL && right_token == NULL;
        }
        if (left_token->kind != right_token->kind
            || !ruda_formatter_comment_text_equal(
                left_bytes + left_token->start,
                left_token->end - left_token->start,
                right_bytes + right_token->start,
                right_token->end - right_token->start
            )) {
            return false;
        }
    }
}

static bool ruda_formatter_read_file(
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

static void ruda_test_formatter_round_trip(void) {
    static const char source[] =
        "@derive(Debug)\r\n"
        "type  Box<T>{public mutable value:T;}\r\n"
        "fn  compute( a:int,b:int,c:int):Result<int,Error>{\r\n"
        "/*keep\r\n  shape*/const values=[a,b+c];\r\n"
        "for let i=0;i<3;i+=1{values.push(i);}\r\n"
        "return service.load<Result<int,Error>>(values[0],b*c)?.value;\r\n"
        "}\r\n";
    RudaFormatterFixture original;
    RudaFormatterFixture formatted;
    RudaFormatResult first;
    RudaFormatResult second;
    RudaAstTree original_ast;
    RudaAstTree formatted_ast;
    bool original_ready = ruda_formatter_fixture_init(
        &original,
        "unformatted.ruda",
        (const unsigned char *)source,
        strlen(source)
    );
    RUDA_EXPECT(original_ready);
    if (!original_ready) {
        return;
    }
    RUDA_EXPECT(original.lexed.diagnostics.length == 0);
    RUDA_EXPECT(original.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_format_source(
        &original.lexed,
        &original.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &first
    ) == RUDA_FORMAT_OK);
    if (!first.initialized) {
        ruda_formatter_fixture_dispose(&original);
        return;
    }
    RUDA_EXPECT(first.changed);
    RUDA_EXPECT_SNAPSHOT("formatter-canonical.ruda", (const char *)first.output.data);

    RUDA_EXPECT(ruda_formatter_fixture_init(
        &formatted,
        "formatted.ruda",
        first.output.data,
        first.output.length
    ));
    if (!formatted.parsed.initialized) {
        ruda_format_result_dispose(&first);
        ruda_formatter_fixture_dispose(&original);
        return;
    }
    RUDA_EXPECT(formatted.lexed.diagnostics.length == 0);
    RUDA_EXPECT(formatted.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_formatter_comments_equal(&original.lexed, &formatted.lexed));
    RUDA_EXPECT(ruda_format_source(
        &formatted.lexed,
        &formatted.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &second
    ) == RUDA_FORMAT_OK);
    if (second.initialized) {
        RUDA_EXPECT(!second.changed);
        RUDA_EXPECT(second.output.length == first.output.length);
        RUDA_EXPECT(memcmp(
            second.output.data,
            first.output.data,
            first.output.length
        ) == 0);
        ruda_format_result_dispose(&second);
    }

    RUDA_EXPECT(ruda_ast_project(
        &original.parsed.tree,
        ruda_system_allocator(),
        ruda_ast_default_options(),
        &original_ast
    ) == RUDA_AST_OK);
    RUDA_EXPECT(ruda_ast_project(
        &formatted.parsed.tree,
        ruda_system_allocator(),
        ruda_ast_default_options(),
        &formatted_ast
    ) == RUDA_AST_OK);
    if (original_ast.initialized && formatted_ast.initialized) {
        RUDA_EXPECT(ruda_formatter_ast_semantically_equal(
            original_ast.root,
            original.snapshot,
            formatted_ast.root,
            formatted.snapshot
        ));
        RUDA_EXPECT(ruda_fuzz_ast_semantically_equal(
            &original_ast,
            original.snapshot,
            &formatted_ast,
            formatted.snapshot
        ));
        ruda_ast_tree_dispose(&formatted_ast);
        ruda_ast_tree_dispose(&original_ast);
    }
    ruda_formatter_fixture_dispose(&formatted);
    ruda_format_result_dispose(&first);
    ruda_formatter_fixture_dispose(&original);
}

static void ruda_test_formatter_fixture_file(void) {
    static const struct {
        const char *input_path;
        const char *expected_path;
    } fixtures[] = {
        {
            "syntax/parser/all-productions.ruda",
            "syntax/parser/all-productions.ruda"
        },
        {
            "format/types.input.ruda",
            "format/types.expected.ruda"
        },
        {
            "format/imports.input.ruda",
            "format/imports.expected.ruda"
        },
        {
            "format/braced-groups.input.ruda",
            "format/braced-groups.expected.ruda"
        },
        {
            "format/named-enum-payload.input.ruda",
            "format/named-enum-payload.expected.ruda"
        },
        {
            "format/grammar.input.ruda",
            "format/grammar.expected.ruda"
        },
        {
            "format/comments.input.ruda",
            "format/comments.expected.ruda"
        },
        {
            "format/strings.input.ruda",
            "format/strings.expected.ruda"
        },
        {
            "format/matrix-arrays.input.ruda",
            "format/matrix-arrays.expected.ruda"
        }
    };
    unsigned char input[32768];
    unsigned char expected[32768];
    size_t fixture_index = 0;
    for (fixture_index = 0;
        fixture_index < sizeof(fixtures) / sizeof(fixtures[0]);
        fixture_index += 1) {
        size_t input_length = 0;
        size_t expected_length = 0;
        RudaFormatterFixture fixture;
        RudaFormatterFixture formatted;
        RudaFormatResult result;
        RudaFormatResult second;
        RudaAstTree original_ast;
        RudaAstTree formatted_ast;
        bool fixture_ready;
        bool formatted_ready = false;
        memset(&original_ast, 0, sizeof(original_ast));
        memset(&formatted_ast, 0, sizeof(formatted_ast));
        RUDA_EXPECT(ruda_formatter_read_file(
            fixtures[fixture_index].input_path,
            input,
            sizeof(input),
            &input_length
        ));
        RUDA_EXPECT(ruda_formatter_read_file(
            fixtures[fixture_index].expected_path,
            expected,
            sizeof(expected),
            &expected_length
        ));
        fixture_ready = ruda_formatter_fixture_init(
            &fixture,
            fixtures[fixture_index].input_path,
            input,
            input_length
        );
        RUDA_EXPECT(fixture_ready);
        if (!fixture_ready) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        if (fixture.parsed.diagnostics.length != 0) {
            size_t diagnostic_index = 0;
            for (diagnostic_index = 0;
                diagnostic_index < fixture.parsed.diagnostics.length;
                diagnostic_index += 1) {
                const RudaParseDiagnostic *diagnostic = ruda_parse_diagnostic(
                    &fixture.parsed,
                    diagnostic_index
                );
                if (diagnostic != NULL) {
                    fprintf(
                        stderr,
                        "%s:%zu..%zu: %s %s\n",
                        fixtures[fixture_index].input_path,
                        diagnostic->start,
                        diagnostic->end,
                        diagnostic->code,
                        diagnostic->message
                    );
                }
            }
        }
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_format_source(
            &fixture.lexed,
            &fixture.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &result
        ) == RUDA_FORMAT_OK);
        if (result.initialized) {
            if (result.output.length != expected_length
                || memcmp(
                    result.output.data,
                    expected,
                    result.output.length < expected_length
                        ? result.output.length
                        : expected_length
                ) != 0) {
                fprintf(
                    stderr,
                    "--- formatter actual for %s ---\n",
                    fixtures[fixture_index].input_path
                );
                (void)fwrite(
                    result.output.data,
                    1,
                    result.output.length,
                    stderr
                );
            }
            RUDA_EXPECT(result.output.length == expected_length);
            RUDA_EXPECT(result.output.length != expected_length
                || memcmp(result.output.data, expected, expected_length) == 0);
            formatted_ready = ruda_formatter_fixture_init(
                &formatted,
                fixtures[fixture_index].expected_path,
                result.output.data,
                result.output.length
            );
            RUDA_EXPECT(formatted_ready);
            if (formatted_ready) {
                RUDA_EXPECT(formatted.lexed.diagnostics.length == 0);
                RUDA_EXPECT(formatted.parsed.diagnostics.length == 0);
                RUDA_EXPECT(ruda_formatter_comments_equal(
                    &fixture.lexed,
                    &formatted.lexed
                ));
                RUDA_EXPECT(ruda_format_source(
                    &formatted.lexed,
                    &formatted.parsed,
                    ruda_system_allocator(),
                    ruda_format_default_options(),
                    &second
                ) == RUDA_FORMAT_OK);
                if (second.initialized) {
                    RUDA_EXPECT(!second.changed);
                    RUDA_EXPECT(second.output.length == result.output.length);
                    RUDA_EXPECT(second.output.length != result.output.length
                        || memcmp(
                            second.output.data,
                            result.output.data,
                            result.output.length
                        ) == 0);
                    ruda_format_result_dispose(&second);
                }
                RUDA_EXPECT(ruda_ast_project(
                    &fixture.parsed.tree,
                    ruda_system_allocator(),
                    ruda_ast_default_options(),
                    &original_ast
                ) == RUDA_AST_OK);
                RUDA_EXPECT(ruda_ast_project(
                    &formatted.parsed.tree,
                    ruda_system_allocator(),
                    ruda_ast_default_options(),
                    &formatted_ast
                ) == RUDA_AST_OK);
                if (original_ast.initialized && formatted_ast.initialized) {
                    RUDA_EXPECT(ruda_formatter_ast_semantically_equal(
                        original_ast.root,
                        fixture.snapshot,
                        formatted_ast.root,
                        formatted.snapshot
                    ));
                    RUDA_EXPECT(ruda_fuzz_ast_semantically_equal(
                        &original_ast,
                        fixture.snapshot,
                        &formatted_ast,
                        formatted.snapshot
                    ));
                }
                ruda_ast_tree_dispose(&formatted_ast);
                ruda_ast_tree_dispose(&original_ast);
                ruda_formatter_fixture_dispose(&formatted);
            }
            ruda_format_result_dispose(&result);
        }
        ruda_formatter_fixture_dispose(&fixture);
    }
}

static void ruda_test_formatter_long_list(void) {
    static const char source[] =
        "fn main(): void {return compute("
        "firstArgumentWithALongName,secondArgumentWithALongName,"
        "thirdArgumentWithALongName,fourthArgumentWithALongName);}\n";
    RudaFormatterFixture first_fixture;
    RudaFormatterFixture second_fixture;
    RudaFormatResult first;
    RudaFormatResult second;
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &first_fixture,
        "long-list.ruda",
        (const unsigned char *)source,
        strlen(source)
    ));
    if (!first_fixture.parsed.initialized) {
        return;
    }
    RUDA_EXPECT(first_fixture.lexed.diagnostics.length == 0);
    RUDA_EXPECT(first_fixture.parsed.diagnostics.length == 0);
    RUDA_EXPECT(ruda_format_source(
        &first_fixture.lexed,
        &first_fixture.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &first
    ) == RUDA_FORMAT_OK);
    if (!first.initialized) {
        ruda_formatter_fixture_dispose(&first_fixture);
        return;
    }
    RUDA_EXPECT_SNAPSHOT("formatter-long-list.ruda", (const char *)first.output.data);
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &second_fixture,
        "long-list-formatted.ruda",
        first.output.data,
        first.output.length
    ));
    if (second_fixture.parsed.initialized) {
        RUDA_EXPECT(second_fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(second_fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_format_source(
            &second_fixture.lexed,
            &second_fixture.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &second
        ) == RUDA_FORMAT_OK);
        if (second.initialized) {
            RUDA_EXPECT(!second.changed);
            RUDA_EXPECT(second.output.length == first.output.length);
            RUDA_EXPECT(second.output.length != first.output.length
                || memcmp(
                    second.output.data,
                    first.output.data,
                    first.output.length
                ) == 0);
            ruda_format_result_dispose(&second);
        }
        ruda_formatter_fixture_dispose(&second_fixture);
    }
    ruda_format_result_dispose(&first);
    ruda_formatter_fixture_dispose(&first_fixture);
}

static void ruda_test_formatter_example_corpus(void) {
    static const char *paths[] = {
        "examples/concurrency.ruda",
        "examples/service.ruda",
        "examples/tour.ruda",
        "examples/tour_test.ruda"
    };
    unsigned char source[131072];
    size_t path_index = 0;
    for (path_index = 0;
        path_index < sizeof(paths) / sizeof(paths[0]);
        path_index += 1) {
        RudaFormatterFixture original;
        RudaFormatterFixture formatted;
        RudaFormatResult first;
        RudaFormatResult second;
        RudaAstTree original_ast;
        RudaAstTree formatted_ast;
        size_t source_length = 0;
        bool original_ready;
        memset(&original_ast, 0, sizeof(original_ast));
        memset(&formatted_ast, 0, sizeof(formatted_ast));
        RUDA_EXPECT(ruda_formatter_read_file(
            paths[path_index],
            source,
            sizeof(source),
            &source_length
        ));
        original_ready = ruda_formatter_fixture_init(
            &original,
            paths[path_index],
            source,
            source_length
        );
        RUDA_EXPECT(original_ready);
        if (!original_ready) {
            continue;
        }
        RUDA_EXPECT(original.lexed.diagnostics.length == 0);
        RUDA_EXPECT(original.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_format_source(
            &original.lexed,
            &original.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &first
        ) == RUDA_FORMAT_OK);
        if (!first.initialized) {
            ruda_formatter_fixture_dispose(&original);
            continue;
        }
        RUDA_EXPECT(ruda_formatter_fixture_init(
            &formatted,
            paths[path_index],
            first.output.data,
            first.output.length
        ));
        if (!formatted.parsed.initialized) {
            ruda_format_result_dispose(&first);
            ruda_formatter_fixture_dispose(&original);
            continue;
        }
        RUDA_EXPECT(formatted.lexed.diagnostics.length == 0);
        RUDA_EXPECT(formatted.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_formatter_comments_equal(&original.lexed, &formatted.lexed));
        RUDA_EXPECT(ruda_format_source(
            &formatted.lexed,
            &formatted.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &second
        ) == RUDA_FORMAT_OK);
        if (second.initialized) {
            RUDA_EXPECT(!second.changed);
            RUDA_EXPECT(second.output.length == first.output.length);
            RUDA_EXPECT(second.output.length != first.output.length
                || memcmp(
                    second.output.data,
                    first.output.data,
                    first.output.length
                ) == 0);
            ruda_format_result_dispose(&second);
        }
        RUDA_EXPECT(ruda_ast_project(
            &original.parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &original_ast
        ) == RUDA_AST_OK);
        RUDA_EXPECT(ruda_ast_project(
            &formatted.parsed.tree,
            ruda_system_allocator(),
            ruda_ast_default_options(),
            &formatted_ast
        ) == RUDA_AST_OK);
        if (original_ast.initialized && formatted_ast.initialized) {
            bool ast_equal = ruda_formatter_ast_semantically_equal(
                original_ast.root,
                original.snapshot,
                formatted_ast.root,
                formatted.snapshot
            );
            if (!ast_equal) {
                fprintf(
                    stderr,
                    "formatter AST mismatch for %s\n",
                    paths[path_index]
                );
            }
            RUDA_EXPECT(ast_equal);
            RUDA_EXPECT(ruda_fuzz_ast_semantically_equal(
                &original_ast,
                original.snapshot,
                &formatted_ast,
                formatted.snapshot
            ));
        }
        ruda_ast_tree_dispose(&formatted_ast);
        ruda_ast_tree_dispose(&original_ast);
        ruda_formatter_fixture_dispose(&formatted);
        ruda_format_result_dispose(&first);
        ruda_formatter_fixture_dispose(&original);
    }
}

static uint32_t ruda_formatter_random_next(uint32_t *state) {
    uint32_t value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static void ruda_test_formatter_trivia_property(void) {
    static const char *const trivia[] = {
        " ",
        "\t",
        "\n",
        "\r\n",
        " \n\t"
    };
    unsigned char source[131072];
    unsigned char expected[131072];
    unsigned char variant[262144];
    size_t source_length = 0;
    size_t expected_length = 0;
    size_t seed = 0;
    RudaFormatterFixture canonical;
    bool canonical_ready;
    RUDA_EXPECT(ruda_formatter_read_file(
        "format/grammar.input.ruda",
        source,
        sizeof(source),
        &source_length
    ));
    RUDA_EXPECT(ruda_formatter_read_file(
        "format/grammar.expected.ruda",
        expected,
        sizeof(expected),
        &expected_length
    ));
    canonical_ready = ruda_formatter_fixture_init(
        &canonical,
        "grammar-property-source.ruda",
        source,
        source_length
    );
    RUDA_EXPECT(canonical_ready);
    if (!canonical_ready) {
        return;
    }
    for (seed = 1; seed <= 128; seed += 1) {
        RudaFormatterFixture fixture;
        RudaFormatResult formatted;
        size_t token_index = 0;
        size_t variant_length = 0;
        bool first = true;
        bool variant_valid = true;
        uint32_t random_state = (uint32_t)seed * UINT32_C(0x9e3779b9);
        for (token_index = 0;
            token_index < canonical.lexed.tokens.length;
            token_index += 1) {
            const RudaToken *token = ruda_lex_token(&canonical.lexed, token_index);
            size_t token_length = 0;
            if (token == NULL) {
                variant_valid = false;
                break;
            }
            if (token->kind == RUDA_TOKEN_EOF) {
                break;
            }
            if (token->kind == RUDA_TOKEN_BOM
                || token->kind == RUDA_TOKEN_WHITESPACE) {
                continue;
            }
            if (!first) {
                const char *selected = trivia[
                    ruda_formatter_random_next(&random_state)
                        % (sizeof(trivia) / sizeof(trivia[0]))
                ];
                size_t trivia_length = strlen(selected);
                if (trivia_length > sizeof(variant) - variant_length) {
                    variant_valid = false;
                    break;
                }
                memcpy(variant + variant_length, selected, trivia_length);
                variant_length += trivia_length;
            }
            token_length = token->end - token->start;
            if (token_length > sizeof(variant) - variant_length) {
                variant_valid = false;
                break;
            }
            memcpy(
                variant + variant_length,
                source + token->start,
                token_length
            );
            variant_length += token_length;
            first = false;
        }
        RUDA_EXPECT(variant_valid);
        if (!variant_valid) {
            break;
        }
        RUDA_EXPECT(ruda_formatter_fixture_init(
            &fixture,
            "grammar-property.ruda",
            variant,
            variant_length
        ));
        if (!fixture.parsed.initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length == 0);
        RUDA_EXPECT(ruda_format_source(
            &fixture.lexed,
            &fixture.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &formatted
        ) == RUDA_FORMAT_OK);
        if (formatted.initialized) {
            RUDA_EXPECT(formatted.output.length == expected_length);
            RUDA_EXPECT(formatted.output.length != expected_length
                || memcmp(
                    formatted.output.data,
                    expected,
                    expected_length
                ) == 0);
            ruda_format_result_dispose(&formatted);
        }
        ruda_formatter_fixture_dispose(&fixture);
    }
    ruda_formatter_fixture_dispose(&canonical);
}

static void ruda_test_formatter_rejection_limits_and_allocation(void) {
    static const char valid[] = "fn main(): void { return; }\n";
    static const char valid_imports[] =
        "import * as zed from \"z.example/z\";\n"
        "import * as io from \"std/io\";\n";
    static const char valid_strings[] =
        "fn strings(): void { const raw = r\"\\\\server\\share\"; "
        "const control = \"\\u{1}\"; }\n";
    static const char invalid[] = "fn broken(: void {\n";
    RudaFormatterFixture valid_fixture;
    RudaFormatterFixture import_fixture;
    RudaFormatterFixture string_fixture;
    RudaFormatterFixture invalid_fixture;
    RudaFormatResult result;
    RudaFormatOptions options;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &valid_fixture,
        "valid.ruda",
        (const unsigned char *)valid,
        strlen(valid)
    ));
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &import_fixture,
        "imports.ruda",
        (const unsigned char *)valid_imports,
        strlen(valid_imports)
    ));
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &string_fixture,
        "strings.ruda",
        (const unsigned char *)valid_strings,
        strlen(valid_strings)
    ));
    RUDA_EXPECT(ruda_formatter_fixture_init(
        &invalid_fixture,
        "invalid.ruda",
        (const unsigned char *)invalid,
        strlen(invalid)
    ));
    RUDA_EXPECT(ruda_format_source(
        &invalid_fixture.lexed,
        &invalid_fixture.parsed,
        ruda_system_allocator(),
        ruda_format_default_options(),
        &result
    ) == RUDA_FORMAT_SOURCE_INVALID);
    RUDA_EXPECT(!result.initialized);

    options = ruda_format_default_options();
    options.max_tokens = 1;
    RUDA_EXPECT(ruda_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_FORMAT_LIMIT_REACHED);
    options = ruda_format_default_options();
    options.max_document_parts = 1;
    RUDA_EXPECT(ruda_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_FORMAT_LIMIT_REACHED);
    options = ruda_format_default_options();
    options.max_output_bytes = 1;
    RUDA_EXPECT(ruda_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_FORMAT_LIMIT_REACHED);
    options = ruda_format_default_options();
    options.max_nesting = 1;
    RUDA_EXPECT(ruda_format_source(
        &valid_fixture.lexed,
        &valid_fixture.parsed,
        ruda_system_allocator(),
        options,
        &result
    ) == RUDA_FORMAT_LIMIT_REACHED);

    for (failure_index = 0; failure_index < 64; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaFormatStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_format_source(
            &valid_fixture.lexed,
            &valid_fixture.parsed,
            allocator,
            ruda_format_default_options(),
            &result
        );
        if (status == RUDA_FORMAT_OK) {
            reached_success = true;
            ruda_format_result_dispose(&result);
        } else {
            RUDA_EXPECT(status == RUDA_FORMAT_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 128; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaFormatStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_format_source(
            &import_fixture.lexed,
            &import_fixture.parsed,
            allocator,
            ruda_format_default_options(),
            &result
        );
        if (status == RUDA_FORMAT_OK) {
            reached_success = true;
            ruda_format_result_dispose(&result);
        } else {
            RUDA_EXPECT(status == RUDA_FORMAT_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);

    reached_success = false;
    for (failure_index = 0; failure_index < 192; failure_index += 1) {
        RudaFailAllocator state;
        RudaAllocator allocator;
        RudaFormatStatus status;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), failure_index);
        allocator = ruda_fail_allocator(&state);
        status = ruda_format_source(
            &string_fixture.lexed,
            &string_fixture.parsed,
            allocator,
            ruda_format_default_options(),
            &result
        );
        if (status == RUDA_FORMAT_OK) {
            reached_success = true;
            ruda_format_result_dispose(&result);
        } else {
            RUDA_EXPECT(status == RUDA_FORMAT_ALLOCATION_FAILED);
            RUDA_EXPECT(!result.initialized);
        }
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) {
            break;
        }
    }
    RUDA_EXPECT(reached_success);
    ruda_formatter_fixture_dispose(&invalid_fixture);
    ruda_formatter_fixture_dispose(&string_fixture);
    ruda_formatter_fixture_dispose(&import_fixture);
    ruda_formatter_fixture_dispose(&valid_fixture);
}

static void ruda_test_formatter_rejects_invalid_types(void) {
    static const char *const invalid[] = {
        "type Missing = ; fn after(): void {}",
        "type EmptyArguments = Array<>; fn after(): void {}",
        "type Qualified = pkg.; fn after(): void {}",
        "type EmptyTuple = []; fn after(): void {}",
        "type NotFunction = (int); fn after(): void {}",
        "type Extra = int string; fn after(): void {}",
        "type MissingClose = Map<string, Array<int>; fn after(): void {}",
        "type ExtraClose = Array<int>>; fn after(): void {}",
        "fn emptyParameters<>(value: int): int { return value; }",
        "fn missingConstraint<T: >(value: T): T { return value; }"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        RudaFormatterFixture fixture;
        RudaFormatResult result;
        bool initialized = ruda_formatter_fixture_init(
            &fixture,
            "invalid-type.ruda",
            (const unsigned char *)invalid[index],
            strlen(invalid[index])
        );
        RUDA_EXPECT(initialized);
        if (!initialized) {
            continue;
        }
        RUDA_EXPECT(fixture.lexed.diagnostics.length == 0);
        RUDA_EXPECT(fixture.parsed.diagnostics.length != 0);
        RUDA_EXPECT(ruda_format_source(
            &fixture.lexed,
            &fixture.parsed,
            ruda_system_allocator(),
            ruda_format_default_options(),
            &result
        ) == RUDA_FORMAT_SOURCE_INVALID);
        RUDA_EXPECT(!result.initialized);
        ruda_formatter_fixture_dispose(&fixture);
    }
}

void ruda_test_formatter(void) {
    ruda_test_formatter_round_trip();
    ruda_test_formatter_fixture_file();
    ruda_test_formatter_long_list();
    ruda_test_formatter_example_corpus();
    ruda_test_formatter_trivia_property();
    ruda_test_formatter_rejection_limits_and_allocation();
    ruda_test_formatter_rejects_invalid_types();
}
