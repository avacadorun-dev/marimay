#include "semantic_oracle.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/base/hash.h"
#include "ruda/base/utf8.h"
#include "ruda/base/vector.h"

typedef struct RudaFuzzDigest {
    unsigned char bytes[32];
} RudaFuzzDigest;

static bool ruda_fuzz_append(RudaVector *bytes, const void *data, size_t length) {
    return length == 0 || ruda_vector_append(bytes, data, length);
}

static bool ruda_fuzz_append_u64(RudaVector *bytes, uint64_t value) {
    unsigned char encoded[8];
    size_t index = 0;
    for (index = 0; index < sizeof(encoded); index += 1) {
        encoded[index] = (unsigned char)(value & UINT64_C(0xff));
        value >>= 8;
    }
    return ruda_fuzz_append(bytes, encoded, sizeof(encoded));
}

static int ruda_fuzz_hex_digit(unsigned char byte) {
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

static bool ruda_fuzz_decode_string(
    const RudaSyntaxToken *token,
    const RudaSourceSnapshot *source,
    RudaVector *decoded
) {
    const unsigned char *bytes = ruda_source_bytes(source);
    size_t source_length = ruda_source_length(source);
    size_t cursor = 0;
    size_t end = 0;
    bool raw = false;
    if (!ruda_vector_init(decoded, ruda_system_allocator(), sizeof(unsigned char))
        || token->synthetic || token->end > source_length) {
        return false;
    }
    if (token->kind == RUDA_TOKEN_STRING && token->end >= token->start + 2
        && bytes[token->start] == '"' && bytes[token->end - 1] == '"') {
        cursor = token->start + 1;
    } else if (token->kind == RUDA_TOKEN_RAW_STRING
        && token->end >= token->start + 3
        && bytes[token->start] == 'r' && bytes[token->start + 1] == '"'
        && bytes[token->end - 1] == '"') {
        cursor = token->start + 2;
        raw = true;
    } else {
        return false;
    }
    end = token->end - 1;
    while (cursor < end) {
        unsigned char byte = bytes[cursor];
        cursor += 1;
        if (raw || byte != '\\') {
            if (!ruda_vector_push(decoded, &byte)) {
                return false;
            }
            continue;
        }
        if (cursor >= end) {
            return false;
        }
        byte = bytes[cursor];
        cursor += 1;
        if (byte == '\\' || byte == '"' || byte == '\'') {
            if (!ruda_vector_push(decoded, &byte)) {
                return false;
            }
        } else if (byte == 'n' || byte == 'r' || byte == 't'
            || byte == '0') {
            unsigned char value = byte == 'n' ? '\n'
                : (byte == 'r' ? '\r' : (byte == 't' ? '\t' : 0));
            if (!ruda_vector_push(decoded, &value)) {
                return false;
            }
        } else if (byte == 'u') {
            uint32_t scalar = 0;
            unsigned char encoded[4];
            size_t encoded_length = 0;
            size_t digits = 0;
            if (cursor >= end || bytes[cursor] != '{') {
                return false;
            }
            cursor += 1;
            while (cursor < end && bytes[cursor] != '}') {
                int digit = ruda_fuzz_hex_digit(bytes[cursor]);
                if (digit < 0 || digits >= 6) {
                    return false;
                }
                scalar = (scalar << 4) | (uint32_t)digit;
                digits += 1;
                cursor += 1;
            }
            if (digits == 0 || cursor >= end || bytes[cursor] != '}'
                || !ruda_utf8_encode(scalar, encoded, &encoded_length)
                || !ruda_vector_append(decoded, encoded, encoded_length)) {
                return false;
            }
            cursor += 1;
        } else {
            return false;
        }
    }
    return true;
}

static bool ruda_fuzz_list_kind(RudaSyntaxKind kind) {
    return kind == RUDA_SYNTAX_ARGUMENTS
        || kind == RUDA_SYNTAX_PARAMETERS
        || kind == RUDA_SYNTAX_IMPORT_CLAUSE
        || kind == RUDA_SYNTAX_ARRAY_EXPRESSION
        || kind == RUDA_SYNTAX_RECORD_EXPRESSION
        || kind == RUDA_SYNTAX_RECORD_FIELDS
        || kind == RUDA_SYNTAX_PAREN_GROUP
        || kind == RUDA_SYNTAX_BRACKET_GROUP
        || kind == RUDA_SYNTAX_POSITIONAL_PATTERN
        || kind == RUDA_SYNTAX_TUPLE_PATTERN
        || kind == RUDA_SYNTAX_NAMED_PATTERN
        || kind == RUDA_SYNTAX_PATTERN_LIST
        || kind == RUDA_SYNTAX_PATTERN_FIELDS;
}

static bool ruda_fuzz_trailing_comma(
    const RudaAstNode *parent,
    size_t index
) {
    const RudaAstElement *element = ruda_ast_child(parent, index);
    const RudaAstElement *next = ruda_ast_child(parent, index + 1);
    if (!ruda_fuzz_list_kind(parent->kind) || element == NULL
        || element->kind != RUDA_AST_ELEMENT_TOKEN
        || element->as.token.kind != RUDA_TOKEN_COMMA) {
        return false;
    }
    if ((parent->kind == RUDA_SYNTAX_PATTERN_LIST
            || parent->kind == RUDA_SYNTAX_PATTERN_FIELDS)
        && index + 1 == parent->child_count) {
        return true;
    }
    return next != NULL && next->kind == RUDA_AST_ELEMENT_TOKEN
        && (next->as.token.kind == RUDA_TOKEN_RIGHT_PAREN
            || next->as.token.kind == RUDA_TOKEN_RIGHT_BRACKET
            || next->as.token.kind == RUDA_TOKEN_RIGHT_BRACE);
}

static bool ruda_fuzz_serialize_node(
    const RudaAstNode *node,
    const RudaSourceSnapshot *source,
    RudaVector *output,
    size_t depth
);

static bool ruda_fuzz_serialize_token(
    const RudaSyntaxToken *token,
    const RudaSourceSnapshot *source,
    RudaVector *output
) {
    static const unsigned char token_marker = 'T';
    const unsigned char *source_bytes = ruda_source_bytes(source);
    size_t source_length = ruda_source_length(source);
    size_t length = 0;
    bool string = token->kind == RUDA_TOKEN_STRING
        || token->kind == RUDA_TOKEN_RAW_STRING;
    if (token->synthetic || token->end < token->start
        || token->end > source_length
        || !ruda_fuzz_append(output, &token_marker, 1)) {
        return false;
    }
    if (string) {
        RudaVector decoded;
        static const uint64_t string_tag = UINT64_C(0xffffffffffffffff);
        bool ok = false;
        memset(&decoded, 0, sizeof(decoded));
        if (!ruda_fuzz_decode_string(token, source, &decoded)) {
            ruda_vector_dispose(&decoded);
            return false;
        }
        ok = ruda_fuzz_append_u64(output, string_tag)
            && ruda_fuzz_append_u64(output, (uint64_t)decoded.length)
            && ruda_fuzz_append(output, decoded.data, decoded.length);
        ruda_vector_dispose(&decoded);
        return ok;
    }
    length = token->end - token->start;
    return ruda_fuzz_append_u64(output, (uint64_t)token->kind)
        && ruda_fuzz_append_u64(output, (uint64_t)length)
        && ruda_fuzz_append(
            output,
            source_bytes + token->start,
            length
        );
}

static bool ruda_fuzz_import_element(const RudaAstElement *element) {
    return element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
        && element->as.node->kind == RUDA_SYNTAX_IMPORT_DECL;
}

static int ruda_fuzz_digest_compare(const void *left, const void *right) {
    const RudaFuzzDigest *first = left;
    const RudaFuzzDigest *second = right;
    return memcmp(first->bytes, second->bytes, sizeof(first->bytes));
}

static bool ruda_fuzz_serialize_root(
    const RudaAstNode *node,
    const RudaSourceSnapshot *source,
    RudaVector *output,
    size_t depth
) {
    RudaVector imports;
    size_t index = 0;
    size_t import_bytes = 0;
    bool ok = false;
    if (!ruda_vector_init(
            &imports,
            ruda_system_allocator(),
            sizeof(RudaFuzzDigest)
        )) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (ruda_fuzz_import_element(element)) {
            RudaVector serialized;
            RudaFuzzDigest digest;
            memset(&serialized, 0, sizeof(serialized));
            if (!ruda_vector_init(
                    &serialized,
                    ruda_system_allocator(),
                    sizeof(unsigned char)
                ) || !ruda_fuzz_serialize_node(
                    element->as.node,
                    source,
                    &serialized,
                    depth + 1
                ) || !ruda_sha256(
                    serialized.data,
                    serialized.length,
                    digest.bytes
                ) || !ruda_vector_push(&imports, &digest)) {
                ruda_vector_dispose(&serialized);
                goto cleanup;
            }
            ruda_vector_dispose(&serialized);
        }
    }
    if (imports.length > 1) {
        qsort(
            imports.data,
            imports.length,
            sizeof(RudaFuzzDigest),
            ruda_fuzz_digest_compare
        );
    }
    if (!ruda_size_multiply(
            imports.length,
            sizeof(RudaFuzzDigest),
            &import_bytes
        ) || !ruda_fuzz_append_u64(output, (uint64_t)imports.length)
        || !ruda_fuzz_append(
            output,
            imports.data,
            import_bytes
        )) {
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (ruda_fuzz_import_element(element)) {
            continue;
        }
        if (element == NULL
            || (element->kind == RUDA_AST_ELEMENT_NODE
                && !ruda_fuzz_serialize_node(
                    element->as.node,
                    source,
                    output,
                    depth + 1
                ))
            || (element->kind == RUDA_AST_ELEMENT_TOKEN
                && !ruda_fuzz_serialize_token(
                    &element->as.token,
                    source,
                    output
                ))) {
            goto cleanup;
        }
    }
    ok = true;

cleanup:
    ruda_vector_dispose(&imports);
    return ok;
}

static bool ruda_fuzz_serialize_node(
    const RudaAstNode *node,
    const RudaSourceSnapshot *source,
    RudaVector *output,
    size_t depth
) {
    static const unsigned char node_marker = 'N';
    static const unsigned char end_marker = 'E';
    size_t index = 0;
    if (node == NULL || depth > 512
        || !ruda_fuzz_append(output, &node_marker, 1)
        || !ruda_fuzz_append_u64(output, (uint64_t)node->kind)) {
        return false;
    }
    if (node->kind == RUDA_SYNTAX_ROOT) {
        if (!ruda_fuzz_serialize_root(node, source, output, depth)) {
            return false;
        }
    } else {
        for (index = 0; index < node->child_count; index += 1) {
            const RudaAstElement *element = ruda_ast_child(node, index);
            if (ruda_fuzz_trailing_comma(node, index)) {
                continue;
            }
            if (element == NULL
                || (element->kind == RUDA_AST_ELEMENT_NODE
                    && !ruda_fuzz_serialize_node(
                        element->as.node,
                        source,
                        output,
                        depth + 1
                    ))
                || (element->kind == RUDA_AST_ELEMENT_TOKEN
                    && !ruda_fuzz_serialize_token(
                        &element->as.token,
                        source,
                        output
                    ))) {
                return false;
            }
        }
    }
    return ruda_fuzz_append(output, &end_marker, 1);
}

bool ruda_fuzz_ast_semantically_equal(
    const RudaAstTree *left,
    const RudaSourceSnapshot *left_source,
    const RudaAstTree *right,
    const RudaSourceSnapshot *right_source
) {
    RudaVector left_bytes;
    RudaVector right_bytes;
    bool equal = false;
    memset(&left_bytes, 0, sizeof(left_bytes));
    memset(&right_bytes, 0, sizeof(right_bytes));
    if (left == NULL || right == NULL || !left->initialized
        || !right->initialized || left->root == NULL || right->root == NULL
        || !ruda_vector_init(
            &left_bytes,
            ruda_system_allocator(),
            sizeof(unsigned char)
        ) || !ruda_vector_init(
            &right_bytes,
            ruda_system_allocator(),
            sizeof(unsigned char)
        ) || !ruda_fuzz_serialize_node(
            left->root,
            left_source,
            &left_bytes,
            0
        ) || !ruda_fuzz_serialize_node(
            right->root,
            right_source,
            &right_bytes,
            0
        )) {
        ruda_vector_dispose(&right_bytes);
        ruda_vector_dispose(&left_bytes);
        return false;
    }
    equal = left_bytes.length == right_bytes.length
        && (left_bytes.length == 0
            || memcmp(
                left_bytes.data,
                right_bytes.data,
                left_bytes.length
            ) == 0);
    ruda_vector_dispose(&right_bytes);
    ruda_vector_dispose(&left_bytes);
    return equal;
}
