#include "semantic_oracle.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/base/hash.h"
#include "mmay/base/utf8.h"
#include "mmay/base/vector.h"

typedef struct MarimayFuzzDigest {
    unsigned char bytes[32];
} MarimayFuzzDigest;

static bool mmay_fuzz_append(MarimayVector *bytes, const void *data, size_t length) {
    return length == 0 || mmay_vector_append(bytes, data, length);
}

static bool mmay_fuzz_append_u64(MarimayVector *bytes, uint64_t value) {
    unsigned char encoded[8];
    size_t index = 0;
    for (index = 0; index < sizeof(encoded); index += 1) {
        encoded[index] = (unsigned char)(value & UINT64_C(0xff));
        value >>= 8;
    }
    return mmay_fuzz_append(bytes, encoded, sizeof(encoded));
}

static int mmay_fuzz_hex_digit(unsigned char byte) {
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

static bool mmay_fuzz_decode_string(
    const MarimaySyntaxToken *token,
    const MarimaySourceSnapshot *source,
    MarimayVector *decoded
) {
    const unsigned char *bytes = mmay_source_bytes(source);
    size_t source_length = mmay_source_length(source);
    size_t cursor = 0;
    size_t end = 0;
    bool raw = false;
    if (!mmay_vector_init(decoded, mmay_system_allocator(), sizeof(unsigned char))
        || token->synthetic || token->end > source_length) {
        return false;
    }
    if (token->kind == MMAY_TOKEN_STRING && token->end >= token->start + 2
        && bytes[token->start] == '"' && bytes[token->end - 1] == '"') {
        cursor = token->start + 1;
    } else if (token->kind == MMAY_TOKEN_RAW_STRING
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
            if (!mmay_vector_push(decoded, &byte)) {
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
            if (!mmay_vector_push(decoded, &byte)) {
                return false;
            }
        } else if (byte == 'n' || byte == 'r' || byte == 't'
            || byte == '0') {
            unsigned char value = byte == 'n' ? '\n'
                : (byte == 'r' ? '\r' : (byte == 't' ? '\t' : 0));
            if (!mmay_vector_push(decoded, &value)) {
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
                int digit = mmay_fuzz_hex_digit(bytes[cursor]);
                if (digit < 0 || digits >= 6) {
                    return false;
                }
                scalar = (scalar << 4) | (uint32_t)digit;
                digits += 1;
                cursor += 1;
            }
            if (digits == 0 || cursor >= end || bytes[cursor] != '}'
                || !mmay_utf8_encode(scalar, encoded, &encoded_length)
                || !mmay_vector_append(decoded, encoded, encoded_length)) {
                return false;
            }
            cursor += 1;
        } else {
            return false;
        }
    }
    return true;
}

static bool mmay_fuzz_list_kind(MarimaySyntaxKind kind) {
    return kind == MMAY_SYNTAX_ARGUMENTS
        || kind == MMAY_SYNTAX_PARAMETERS
        || kind == MMAY_SYNTAX_IMPORT_CLAUSE
        || kind == MMAY_SYNTAX_ARRAY_EXPRESSION
        || kind == MMAY_SYNTAX_ENUM_DECL
        || kind == MMAY_SYNTAX_ENUM_VARIANT
        || kind == MMAY_SYNTAX_MATCH_EXPRESSION
        || kind == MMAY_SYNTAX_NAMED_PAYLOAD
        || kind == MMAY_SYNTAX_RECORD_DECL
        || kind == MMAY_SYNTAX_RECORD_EXPRESSION
        || kind == MMAY_SYNTAX_RECORD_FIELDS
        || kind == MMAY_SYNTAX_PAREN_GROUP
        || kind == MMAY_SYNTAX_BRACKET_GROUP
        || kind == MMAY_SYNTAX_POSITIONAL_PATTERN
        || kind == MMAY_SYNTAX_TUPLE_PATTERN
        || kind == MMAY_SYNTAX_NAMED_PATTERN
        || kind == MMAY_SYNTAX_PATTERN_LIST
        || kind == MMAY_SYNTAX_PATTERN_FIELDS;
}

static bool mmay_fuzz_trailing_comma(
    const MarimayAstNode *parent,
    size_t index
) {
    const MarimayAstElement *element = mmay_ast_child(parent, index);
    const MarimayAstElement *next = mmay_ast_child(parent, index + 1);
    if (!mmay_fuzz_list_kind(parent->kind) || element == NULL
        || element->kind != MMAY_AST_ELEMENT_TOKEN
        || element->as.token.kind != MMAY_TOKEN_COMMA) {
        return false;
    }
    if ((parent->kind == MMAY_SYNTAX_PATTERN_LIST
            || parent->kind == MMAY_SYNTAX_PATTERN_FIELDS)
        && index + 1 == parent->child_count) {
        return true;
    }
    return next != NULL && next->kind == MMAY_AST_ELEMENT_TOKEN
        && (next->as.token.kind == MMAY_TOKEN_RIGHT_PAREN
            || next->as.token.kind == MMAY_TOKEN_RIGHT_BRACKET
            || next->as.token.kind == MMAY_TOKEN_RIGHT_BRACE);
}

static bool mmay_fuzz_serialize_node(
    const MarimayAstNode *node,
    const MarimaySourceSnapshot *source,
    MarimayVector *output,
    size_t depth
);

static bool mmay_fuzz_serialize_token(
    const MarimaySyntaxToken *token,
    const MarimaySourceSnapshot *source,
    MarimayVector *output
) {
    static const unsigned char token_marker = 'T';
    const unsigned char *source_bytes = mmay_source_bytes(source);
    size_t source_length = mmay_source_length(source);
    size_t length = 0;
    bool string = token->kind == MMAY_TOKEN_STRING
        || token->kind == MMAY_TOKEN_RAW_STRING;
    if (token->synthetic || token->end < token->start
        || token->end > source_length
        || !mmay_fuzz_append(output, &token_marker, 1)) {
        return false;
    }
    if (string) {
        MarimayVector decoded;
        static const uint64_t string_tag = UINT64_C(0xffffffffffffffff);
        bool ok = false;
        memset(&decoded, 0, sizeof(decoded));
        if (!mmay_fuzz_decode_string(token, source, &decoded)) {
            mmay_vector_dispose(&decoded);
            return false;
        }
        ok = mmay_fuzz_append_u64(output, string_tag)
            && mmay_fuzz_append_u64(output, (uint64_t)decoded.length)
            && mmay_fuzz_append(output, decoded.data, decoded.length);
        mmay_vector_dispose(&decoded);
        return ok;
    }
    length = token->end - token->start;
    return mmay_fuzz_append_u64(output, (uint64_t)token->kind)
        && mmay_fuzz_append_u64(output, (uint64_t)length)
        && mmay_fuzz_append(
            output,
            source_bytes + token->start,
            length
        );
}

static bool mmay_fuzz_import_element(const MarimayAstElement *element) {
    return element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
        && element->as.node->kind == MMAY_SYNTAX_IMPORT_DECL;
}

static int mmay_fuzz_digest_compare(const void *left, const void *right) {
    const MarimayFuzzDigest *first = left;
    const MarimayFuzzDigest *second = right;
    return memcmp(first->bytes, second->bytes, sizeof(first->bytes));
}

static bool mmay_fuzz_serialize_root(
    const MarimayAstNode *node,
    const MarimaySourceSnapshot *source,
    MarimayVector *output,
    size_t depth
) {
    MarimayVector imports;
    size_t index = 0;
    size_t import_bytes = 0;
    bool ok = false;
    if (!mmay_vector_init(
            &imports,
            mmay_system_allocator(),
            sizeof(MarimayFuzzDigest)
        )) {
        return false;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (mmay_fuzz_import_element(element)) {
            MarimayVector serialized;
            MarimayFuzzDigest digest;
            memset(&serialized, 0, sizeof(serialized));
            if (!mmay_vector_init(
                    &serialized,
                    mmay_system_allocator(),
                    sizeof(unsigned char)
                ) || !mmay_fuzz_serialize_node(
                    element->as.node,
                    source,
                    &serialized,
                    depth + 1
                ) || !mmay_sha256(
                    serialized.data,
                    serialized.length,
                    digest.bytes
                ) || !mmay_vector_push(&imports, &digest)) {
                mmay_vector_dispose(&serialized);
                goto cleanup;
            }
            mmay_vector_dispose(&serialized);
        }
    }
    if (imports.length > 1) {
        qsort(
            imports.data,
            imports.length,
            sizeof(MarimayFuzzDigest),
            mmay_fuzz_digest_compare
        );
    }
    if (!mmay_size_multiply(
            imports.length,
            sizeof(MarimayFuzzDigest),
            &import_bytes
        ) || !mmay_fuzz_append_u64(output, (uint64_t)imports.length)
        || !mmay_fuzz_append(
            output,
            imports.data,
            import_bytes
        )) {
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (mmay_fuzz_import_element(element)) {
            continue;
        }
        if (element == NULL
            || (element->kind == MMAY_AST_ELEMENT_NODE
                && !mmay_fuzz_serialize_node(
                    element->as.node,
                    source,
                    output,
                    depth + 1
                ))
            || (element->kind == MMAY_AST_ELEMENT_TOKEN
                && !mmay_fuzz_serialize_token(
                    &element->as.token,
                    source,
                    output
                ))) {
            goto cleanup;
        }
    }
    ok = true;

cleanup:
    mmay_vector_dispose(&imports);
    return ok;
}

static bool mmay_fuzz_serialize_node(
    const MarimayAstNode *node,
    const MarimaySourceSnapshot *source,
    MarimayVector *output,
    size_t depth
) {
    static const unsigned char node_marker = 'N';
    static const unsigned char end_marker = 'E';
    size_t index = 0;
    if (node == NULL || depth > 512
        || !mmay_fuzz_append(output, &node_marker, 1)
        || !mmay_fuzz_append_u64(output, (uint64_t)node->kind)) {
        return false;
    }
    if (node->kind == MMAY_SYNTAX_ROOT) {
        if (!mmay_fuzz_serialize_root(node, source, output, depth)) {
            return false;
        }
    } else {
        for (index = 0; index < node->child_count; index += 1) {
            const MarimayAstElement *element = mmay_ast_child(node, index);
            if (mmay_fuzz_trailing_comma(node, index)) {
                continue;
            }
            if (element == NULL
                || (element->kind == MMAY_AST_ELEMENT_NODE
                    && !mmay_fuzz_serialize_node(
                        element->as.node,
                        source,
                        output,
                        depth + 1
                    ))
                || (element->kind == MMAY_AST_ELEMENT_TOKEN
                    && !mmay_fuzz_serialize_token(
                        &element->as.token,
                        source,
                        output
                    ))) {
                return false;
            }
        }
    }
    return mmay_fuzz_append(output, &end_marker, 1);
}

bool mmay_fuzz_ast_semantically_equal(
    const MarimayAstTree *left,
    const MarimaySourceSnapshot *left_source,
    const MarimayAstTree *right,
    const MarimaySourceSnapshot *right_source
) {
    MarimayVector left_bytes;
    MarimayVector right_bytes;
    bool equal = false;
    memset(&left_bytes, 0, sizeof(left_bytes));
    memset(&right_bytes, 0, sizeof(right_bytes));
    if (left == NULL || right == NULL || !left->initialized
        || !right->initialized || left->root == NULL || right->root == NULL
        || !mmay_vector_init(
            &left_bytes,
            mmay_system_allocator(),
            sizeof(unsigned char)
        ) || !mmay_vector_init(
            &right_bytes,
            mmay_system_allocator(),
            sizeof(unsigned char)
        ) || !mmay_fuzz_serialize_node(
            left->root,
            left_source,
            &left_bytes,
            0
        ) || !mmay_fuzz_serialize_node(
            right->root,
            right_source,
            &right_bytes,
            0
        )) {
        mmay_vector_dispose(&right_bytes);
        mmay_vector_dispose(&left_bytes);
        return false;
    }
    equal = left_bytes.length == right_bytes.length
        && (left_bytes.length == 0
            || memcmp(
                left_bytes.data,
                right_bytes.data,
                left_bytes.length
            ) == 0);
    mmay_vector_dispose(&right_bytes);
    mmay_vector_dispose(&left_bytes);
    return equal;
}
