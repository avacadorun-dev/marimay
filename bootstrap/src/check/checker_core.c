#include "checker_core_api.h"





#include "ruda/check/checker.h"


#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/generated/diagnostic_codes.h"
#include "ruda/base/f64.h"
#include "ruda/base/string_literal.h"


 bool ruda_checker_private_evaluate_constant(
    RudaChecker *checker,
    RudaCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

 bool ruda_checker_private_evaluate_type_alias(
    RudaChecker *checker,
    RudaCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

void ruda_checker_fail(RudaChecker *checker, RudaCheckStatus status) {
    if (checker->failure == RUDA_CHECK_OK) {
        checker->failure = status;
    }
}

 void ruda_checker_private_expression_set_never(
    RudaFunctionChecker *function,
    RudaExpressionValue *result
) {
    result->type = RUDA_TYPE_NEVER;
    result->register_id = RUDA_MIR_NO_REGISTER;
    result->valid = true;
    function->terminated = true;
}

bool ruda_expression_is_never(
    const RudaFunctionChecker *function,
    const RudaExpressionValue *value
) {
    return function->terminated && value != NULL && value->valid
        && value->type == RUDA_TYPE_NEVER
        && value->register_id == RUDA_MIR_NO_REGISTER;
}

bool ruda_checker_has_failed(const RudaChecker *checker) {
    return checker->failure != RUDA_CHECK_OK;
}

 bool ruda_checker_private_checker_set_ast(
    RudaChecker *checker,
    const RudaAstTree *ast
) {
    if (checker == NULL || ast == NULL || !ast->initialized
        || ast->root == NULL || ast->root->kind != RUDA_SYNTAX_ROOT
        || ast->snapshot == NULL) {
        return false;
    }
    checker->ast = ast;
    checker->source = ruda_source_bytes(ast->snapshot);
    checker->source_length = ruda_source_length(ast->snapshot);
    return checker->source != NULL || checker->source_length == 0;
}

 bool ruda_checker_private_check_span_valid(
    const RudaChecker *checker,
    size_t start,
    size_t end
) {
    return start <= end && end <= checker->source_length;
}

 bool ruda_checker_private_check_text_equal(
    const RudaChecker *checker,
    size_t start,
    size_t end,
    const char *text
) {
    size_t length = text != NULL ? strlen(text) : 0;
    return text != NULL && ruda_checker_private_check_span_valid(checker, start, end)
        && end - start == length
        && memcmp(checker->source + start, text, length) == 0;
}

 bool ruda_checker_private_check_names_equal(
    const RudaChecker *checker,
    RudaCheckName left,
    RudaCheckName right
) {
    size_t left_length = left.end - left.start;
    size_t right_length = right.end - right.start;
    const unsigned char *left_source = left.snapshot != NULL
        ? ruda_source_bytes(left.snapshot)
        : NULL;
    const unsigned char *right_source = right.snapshot != NULL
        ? ruda_source_bytes(right.snapshot)
        : NULL;
    (void)checker;
    return left_source != NULL && right_source != NULL
        && left.start <= left.end
        && left.end <= ruda_source_length(left.snapshot)
        && right.start <= right.end
        && right.end <= ruda_source_length(right.snapshot)
        && left_length == right_length
        && memcmp(
            left_source + left.start,
            right_source + right.start,
            left_length
        ) == 0;
}

 bool ruda_checker_private_check_name_bytes(
    RudaCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    const unsigned char *source = name.snapshot != NULL
        ? ruda_source_bytes(name.snapshot)
        : NULL;
    if (bytes == NULL || length == NULL || source == NULL
        || name.start > name.end
        || name.end > ruda_source_length(name.snapshot)) {
        return false;
    }
    *bytes = source + name.start;
    *length = name.end - name.start;
    return true;
}

 const RudaCheckTopLevelEntry *ruda_checker_private_checker_find_top_level(
    const RudaChecker *checker,
    RudaCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    if (!ruda_checker_private_check_name_bytes(name, &bytes, &length)) {
        return NULL;
    }
    return ruda_map_get_const(&checker->top_level_index, bytes, length);
}

 RudaCheckConstantDeclaration *ruda_checker_private_checker_find_constant(
    RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_private_checker_find_top_level(
        checker,
        name
    );
    RudaCheckConstantDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_CONSTANT) {
        declaration = ruda_vector_at(&checker->constants, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && ruda_checker_private_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

 RudaCheckTypeAliasDeclaration *ruda_checker_private_checker_find_type_alias(
    RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_private_checker_find_top_level(
        checker,
        name
    );
    RudaCheckTypeAliasDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_TYPE_ALIAS) {
        declaration = ruda_vector_at(&checker->type_aliases, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && ruda_checker_private_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

 RudaCheckRecordDeclaration *ruda_checker_private_checker_find_record(
    RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_private_checker_find_top_level(
        checker,
        name
    );
    RudaCheckRecordDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_RECORD) {
        declaration = ruda_vector_at(&checker->records, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && ruda_checker_private_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

 RudaCheckRecordDeclaration *ruda_checker_private_checker_find_record_by_type(
    RudaChecker *checker,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->records.length;
        index += 1) {
        RudaCheckRecordDeclaration *declaration = ruda_vector_at(
            &checker->records,
            index
        );
        if (declaration != NULL && declaration->declaration_valid
            && declaration->fields_resolved && declaration->type == type) {
            return declaration;
        }
    }
    return NULL;
}

 RudaCheckRecordFieldDeclaration *ruda_checker_private_checker_find_record_field(
    RudaChecker *checker,
    RudaCheckRecordDeclaration *record,
    RudaCheckName name,
    size_t *field_index
) {
    size_t index = 0;
    for (index = 0; record != NULL && index < record->fields.length;
        index += 1) {
        RudaCheckRecordFieldDeclaration *field = ruda_vector_at(
            &record->fields,
            index
        );
        if (field != NULL && ruda_checker_private_check_names_equal(
                checker,
                field->name,
                name
            )) {
            if (field_index != NULL) {
                *field_index = index;
            }
            return field;
        }
    }
    return NULL;
}

 RudaCheckEnumDeclaration *ruda_checker_private_checker_find_enum(
    RudaChecker *checker,
    RudaCheckName name
) {
    const RudaCheckTopLevelEntry *entry = ruda_checker_private_checker_find_top_level(
        checker,
        name
    );
    RudaCheckEnumDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == RUDA_CHECK_TOP_LEVEL_ENUM) {
        declaration = ruda_vector_at(&checker->enums, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && ruda_checker_private_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

 RudaCheckEnumDeclaration *ruda_checker_private_checker_find_enum_by_type(
    RudaChecker *checker,
    RudaTypeId type
) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->enums.length;
        index += 1) {
        RudaCheckEnumDeclaration *declaration = ruda_vector_at(
            &checker->enums,
            index
        );
        if (declaration != NULL && declaration->declaration_valid
            && declaration->variants_resolved
            && declaration->type == type) {
            return declaration;
        }
    }
    return NULL;
}

 RudaCheckEnumVariantDeclaration *ruda_checker_private_checker_find_enum_variant(
    RudaChecker *checker,
    RudaCheckEnumDeclaration *enum_declaration,
    RudaCheckName name,
    size_t *variant_index
) {
    size_t index = 0;
    for (index = 0; enum_declaration != NULL
        && index < enum_declaration->variants.length; index += 1) {
        RudaCheckEnumVariantDeclaration *variant = ruda_vector_at(
            &enum_declaration->variants,
            index
        );
        if (variant != NULL && ruda_checker_private_check_names_equal(
                checker,
                variant->name,
                name
            )) {
            if (variant_index != NULL) {
                *variant_index = index;
            }
            return variant;
        }
    }
    return NULL;
}

 RudaCheckEnumPayloadItemDeclaration *ruda_checker_private_checker_find_enum_payload(
    RudaChecker *checker,
    RudaCheckEnumVariantDeclaration *variant,
    RudaCheckName name,
    size_t *payload_index
) {
    size_t index = 0;
    for (index = 0; variant != NULL && index < variant->payload.length;
        index += 1) {
        RudaCheckEnumPayloadItemDeclaration *payload = ruda_vector_at(
            &variant->payload,
            index
        );
        if (payload != NULL && payload->has_name
            && ruda_checker_private_check_names_equal(checker, payload->name, name)) {
            if (payload_index != NULL) {
                *payload_index = index;
            }
            return payload;
        }
    }
    return NULL;
}

 const RudaAstNode *ruda_checker_private_check_child_node(
    const RudaAstNode *node,
    RudaSyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found == ordinal) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

 const RudaAstNode *ruda_checker_private_check_child_node_any(
    const RudaAstNode *node,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (found == ordinal) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

 const RudaSyntaxToken *ruda_checker_private_check_child_token(
    const RudaAstNode *node,
    RudaTokenKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_TOKEN
            && element->as.token.kind == kind) {
            if (found == ordinal) {
                return &element->as.token;
            }
            found += 1;
        }
    }
    return NULL;
}

 const RudaSyntaxToken *ruda_checker_private_check_first_token(
    const RudaAstNode *node
) {
    size_t index = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_TOKEN) {
            return &element->as.token;
        }
    }
    return NULL;
}

 const RudaSyntaxToken *ruda_checker_private_check_attribute_name(
    const RudaAstNode *attribute
) {
    return attribute != NULL && attribute->kind == RUDA_SYNTAX_ATTRIBUTE
        ? ruda_checker_private_check_child_token(attribute, RUDA_TOKEN_IDENTIFIER, 0)
        : NULL;
}

 bool ruda_checker_private_check_attribute_is(
    const RudaChecker *checker,
    const RudaAstNode *attribute,
    const char *name
) {
    const RudaSyntaxToken *token = ruda_checker_private_check_attribute_name(attribute);
    return token != NULL && !token->synthetic
        && ruda_checker_private_check_text_equal(checker, token->start, token->end, name);
}

 bool ruda_checker_private_check_source_is_test_file(const RudaChecker *checker) {
    static const char suffix[] = "_test.ruda";
    size_t path_length = checker != NULL && checker->ast != NULL
        ? ruda_source_path_length(checker->ast->snapshot)
        : 0;
    const char *path = checker != NULL && checker->ast != NULL
        ? ruda_source_path(checker->ast->snapshot)
        : NULL;
    return path != NULL && path_length >= sizeof(suffix) - 1
        && memcmp(
            path + path_length - (sizeof(suffix) - 1),
            suffix,
            sizeof(suffix) - 1
        ) == 0;
}

bool ruda_checker_add_diagnostic(
    RudaChecker *checker,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    RudaCheckDiagnostic diagnostic;
    if (ruda_checker_has_failed(checker)) {
        return false;
    }
    if (!ruda_checker_private_check_span_valid(checker, start, end)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (checker->result->diagnostics.length
        >= checker->options.max_diagnostics) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.snapshot = checker->ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!ruda_vector_push(&checker->result->diagnostics, &diagnostic)) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool ruda_checker_private_checker_add_owned_diagnostic(
    RudaChecker *checker,
    const char *code,
    const unsigned char *message,
    size_t message_length,
    size_t start,
    size_t end
) {
    RudaCheckDiagnostic diagnostic;
    if (ruda_checker_has_failed(checker)) {
        return false;
    }
    if (!ruda_checker_private_check_span_valid(checker, start, end)
        || (message == NULL && message_length != 0)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (checker->result->diagnostics.length
        >= checker->options.max_diagnostics) {
        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    if (!ruda_owned_bytes_copy(
            &diagnostic.owned_message,
            checker->result->allocator,
            message,
            message_length
        )) {
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = (const char *)diagnostic.owned_message.data;
    diagnostic.snapshot = checker->ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!ruda_vector_push(&checker->result->diagnostics, &diagnostic)) {
        ruda_owned_bytes_dispose(&diagnostic.owned_message);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}
