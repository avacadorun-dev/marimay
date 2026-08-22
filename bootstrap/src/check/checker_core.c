#include "checker_core_api.h"





#include "mmay/check/checker.h"


#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/generated/diagnostic_codes.h"
#include "mmay/base/f64.h"
#include "mmay/base/string_literal.h"
#include "checker_generic_types_api.h"
#include "checker_imported_enums_api.h"


 bool mmay_checker_private_evaluate_constant(
    MarimayChecker *checker,
    MarimayCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

 bool mmay_checker_private_evaluate_type_alias(
    MarimayChecker *checker,
    MarimayCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

void mmay_checker_fail(MarimayChecker *checker, MarimayCheckStatus status) {
    if (checker->failure == MMAY_CHECK_OK) {
        checker->failure = status;
    }
}

 void mmay_checker_private_expression_set_never(
    MarimayFunctionChecker *function,
    MarimayExpressionValue *result
) {
    result->type = MMAY_TYPE_NEVER;
    result->register_id = MMAY_MIR_NO_REGISTER;
    result->valid = true;
    function->terminated = true;
}

bool mmay_expression_is_never(
    const MarimayFunctionChecker *function,
    const MarimayExpressionValue *value
) {
    return function->terminated && value != NULL && value->valid
        && value->type == MMAY_TYPE_NEVER
        && value->register_id == MMAY_MIR_NO_REGISTER;
}

bool mmay_checker_has_failed(const MarimayChecker *checker) {
    return checker->failure != MMAY_CHECK_OK;
}

 bool mmay_checker_private_checker_set_ast(
    MarimayChecker *checker,
    const MarimayAstTree *ast
) {
    if (checker == NULL || ast == NULL || !ast->initialized
        || ast->root == NULL || ast->root->kind != MMAY_SYNTAX_ROOT
        || ast->snapshot == NULL) {
        return false;
    }
    checker->ast = ast;
    checker->source = mmay_source_bytes(ast->snapshot);
    checker->source_length = mmay_source_length(ast->snapshot);
    return checker->source != NULL || checker->source_length == 0;
}

 bool mmay_checker_private_check_span_valid(
    const MarimayChecker *checker,
    size_t start,
    size_t end
) {
    return start <= end && end <= checker->source_length;
}

 bool mmay_checker_private_check_text_equal(
    const MarimayChecker *checker,
    size_t start,
    size_t end,
    const char *text
) {
    size_t length = text != NULL ? strlen(text) : 0;
    return text != NULL && mmay_checker_private_check_span_valid(checker, start, end)
        && end - start == length
        && memcmp(checker->source + start, text, length) == 0;
}

 bool mmay_checker_private_check_names_equal(
    const MarimayChecker *checker,
    MarimayCheckName left,
    MarimayCheckName right
) {
    size_t left_length = left.end - left.start;
    size_t right_length = right.end - right.start;
    const unsigned char *left_source = left.snapshot != NULL
        ? mmay_source_bytes(left.snapshot)
        : NULL;
    const unsigned char *right_source = right.snapshot != NULL
        ? mmay_source_bytes(right.snapshot)
        : NULL;
    (void)checker;
    return left_source != NULL && right_source != NULL
        && left.start <= left.end
        && left.end <= mmay_source_length(left.snapshot)
        && right.start <= right.end
        && right.end <= mmay_source_length(right.snapshot)
        && left_length == right_length
        && memcmp(
            left_source + left.start,
            right_source + right.start,
            left_length
        ) == 0;
}

 bool mmay_checker_private_check_name_bytes(
    MarimayCheckName name,
    const unsigned char **bytes,
    size_t *length
) {
    const unsigned char *source = name.snapshot != NULL
        ? mmay_source_bytes(name.snapshot)
        : NULL;
    if (bytes == NULL || length == NULL || source == NULL
        || name.start > name.end
        || name.end > mmay_source_length(name.snapshot)) {
        return false;
    }
    *bytes = source + name.start;
    *length = name.end - name.start;
    return true;
}

 const MarimayCheckTopLevelEntry *mmay_checker_private_checker_find_top_level(
    const MarimayChecker *checker,
    MarimayCheckName name
) {
    const unsigned char *bytes = NULL;
    size_t length = 0;
    if (!mmay_checker_private_check_name_bytes(name, &bytes, &length)) {
        return NULL;
    }
    return mmay_map_get_const(&checker->top_level_index, bytes, length);
}

 MarimayCheckConstantDeclaration *mmay_checker_private_checker_find_constant(
    MarimayChecker *checker,
    MarimayCheckName name
) {
    const MarimayCheckTopLevelEntry *entry = mmay_checker_private_checker_find_top_level(
        checker,
        name
    );
    MarimayCheckConstantDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == MMAY_CHECK_TOP_LEVEL_CONSTANT) {
        declaration = mmay_vector_at(&checker->constants, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && mmay_checker_private_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

 MarimayCheckTypeAliasDeclaration *mmay_checker_private_checker_find_type_alias(
    MarimayChecker *checker,
    MarimayCheckName name
) {
    const MarimayCheckTopLevelEntry *entry = mmay_checker_private_checker_find_top_level(
        checker,
        name
    );
    MarimayCheckTypeAliasDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == MMAY_CHECK_TOP_LEVEL_TYPE_ALIAS) {
        declaration = mmay_vector_at(&checker->type_aliases, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && mmay_checker_private_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

 MarimayCheckRecordDeclaration *mmay_checker_private_checker_find_record(
    MarimayChecker *checker,
    MarimayCheckName name
) {
    const MarimayCheckTopLevelEntry *entry = mmay_checker_private_checker_find_top_level(
        checker,
        name
    );
    MarimayCheckRecordDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == MMAY_CHECK_TOP_LEVEL_RECORD) {
        declaration = mmay_vector_at(&checker->records, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && mmay_checker_private_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    return NULL;
}

 MarimayCheckRecordDeclaration *mmay_checker_private_checker_find_record_by_type(
    MarimayChecker *checker,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->records.length;
        index += 1) {
        MarimayCheckRecordDeclaration *declaration = mmay_vector_at(
            &checker->records,
            index
        );
        if (declaration != NULL && declaration->declaration_valid
            && declaration->fields_resolved
            && (declaration->type == type
                || (declaration->is_generic
                    && mmay_checker_private_type_matches_nominal(
                        checker,
                        type,
                        declaration->symbol,
                        declaration->type_parameters.length
                    )))) {
            return declaration;
        }
    }
    return NULL;
}

 MarimayCheckRecordFieldDeclaration *mmay_checker_private_checker_find_record_field(
    MarimayChecker *checker,
    MarimayCheckRecordDeclaration *record,
    MarimayCheckName name,
    size_t *field_index
) {
    size_t index = 0;
    for (index = 0; record != NULL && index < record->fields.length;
        index += 1) {
        MarimayCheckRecordFieldDeclaration *field = mmay_vector_at(
            &record->fields,
            index
        );
        if (field != NULL && mmay_checker_private_check_names_equal(
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

MarimayCheckEnumDeclaration *mmay_checker_private_checker_find_enum(
    MarimayChecker *checker,
    MarimayCheckName name
) {
    const MarimayCheckTopLevelEntry *entry = mmay_checker_private_checker_find_top_level(
        checker,
        name
    );
    MarimayCheckEnumDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == MMAY_CHECK_TOP_LEVEL_ENUM) {
        declaration = mmay_vector_at(&checker->enums, entry->index);
    }
    if (declaration != NULL && declaration->declaration_valid
        && mmay_checker_private_check_names_equal(checker, declaration->name, name)) {
        return declaration;
    }
    {
        const unsigned char *bytes = NULL;
        size_t length = 0;
        if (mmay_checker_private_check_name_bytes(name, &bytes, &length)) {
            size_t index = 0;
            for (index = 0; checker != NULL
                && index < checker->prelude_enums.length; index += 1) {
                declaration = mmay_vector_at(
                    &checker->prelude_enums, index
                );
                if (declaration != NULL && declaration->is_builtin
                    && declaration->declaration_valid
                    && declaration->owned_name.length == length
                    && memcmp(declaration->owned_name.data, bytes, length) == 0) {
                    return declaration;
                }
            }
        }
    }
    return NULL;
}

 MarimayCheckEnumDeclaration *mmay_checker_private_checker_find_enum_by_type(
    MarimayChecker *checker,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->enums.length;
        index += 1) {
        MarimayCheckEnumDeclaration *declaration = mmay_vector_at(
            &checker->enums,
            index
        );
        if (declaration != NULL && declaration->declaration_valid
            && declaration->variants_resolved
            && (declaration->type == type
                || (declaration->is_generic
                    && mmay_checker_private_type_matches_nominal(
                        checker,
                        type,
                        declaration->symbol,
                        declaration->type_parameters.length
                    )))) {
            return declaration;
        }
    }
    for (index = 0; checker != NULL && index < checker->prelude_enums.length;
        index += 1) {
        MarimayCheckEnumDeclaration *declaration = mmay_vector_at(
            &checker->prelude_enums, index
        );
        if (declaration != NULL && declaration->declaration_valid
            && declaration->variants_resolved
            && (declaration->type == type
                || mmay_checker_private_type_matches_nominal(
                    checker,
                    type,
                    declaration->symbol,
                    declaration->type_parameters.length
                ))) {
            return declaration;
        }
    }
    return mmay_checker_private_imported_enum_declaration_by_type(
        checker, type
    );
}

 MarimayCheckInterfaceDeclaration *mmay_checker_private_checker_find_interface(
    MarimayChecker *checker,
    MarimayCheckName name
) {
    const MarimayCheckTopLevelEntry *entry =
        mmay_checker_private_checker_find_top_level(checker, name);
    MarimayCheckInterfaceDeclaration *declaration = NULL;
    if (entry != NULL && entry->kind == MMAY_CHECK_TOP_LEVEL_INTERFACE) {
        declaration = mmay_vector_at(&checker->interfaces, entry->index);
    }
    return declaration != NULL && declaration->declaration_valid
            && mmay_checker_private_check_names_equal(checker, declaration->name, name)
        ? declaration
        : NULL;
}

 MarimayCheckInterfaceDeclaration *mmay_checker_private_checker_find_interface_by_type(
    MarimayChecker *checker,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; checker != NULL && index < checker->interfaces.length;
        index += 1) {
        MarimayCheckInterfaceDeclaration *declaration = mmay_vector_at(
            &checker->interfaces, index
        );
        if (declaration != NULL && declaration->declaration_valid
            && declaration->type == type) {
            return declaration;
        }
    }
    return NULL;
}

 MarimayCheckEnumVariantDeclaration *mmay_checker_private_checker_find_enum_variant(
    MarimayChecker *checker,
    MarimayCheckEnumDeclaration *enum_declaration,
    MarimayCheckName name,
    size_t *variant_index
) {
    size_t index = 0;
    for (index = 0; enum_declaration != NULL
        && index < enum_declaration->variants.length; index += 1) {
        MarimayCheckEnumVariantDeclaration *variant = mmay_vector_at(
            &enum_declaration->variants,
            index
        );
        const unsigned char *bytes = NULL;
        size_t length = 0;
        bool same_name = variant != NULL
            && mmay_checker_private_check_names_equal(checker, variant->name, name);
        if (variant != NULL && !same_name
            && mmay_checker_private_check_name_bytes(name, &bytes, &length)) {
            same_name = variant->owned_name.length == length
                && memcmp(variant->owned_name.data, bytes, length) == 0;
        }
        if (same_name) {
            if (variant_index != NULL) {
                *variant_index = index;
            }
            return variant;
        }
    }
    return NULL;
}

 MarimayCheckEnumPayloadItemDeclaration *mmay_checker_private_checker_find_enum_payload(
    MarimayChecker *checker,
    MarimayCheckEnumVariantDeclaration *variant,
    MarimayCheckName name,
    size_t *payload_index
) {
    size_t index = 0;
    for (index = 0; variant != NULL && index < variant->payload.length;
        index += 1) {
        MarimayCheckEnumPayloadItemDeclaration *payload = mmay_vector_at(
            &variant->payload,
            index
        );
        const unsigned char *bytes = NULL;
        size_t length = 0;
        bool same_name = payload != NULL && payload->has_name
            && mmay_checker_private_check_names_equal(
                checker, payload->name, name
            );
        if (payload != NULL && payload->has_name && !same_name
            && mmay_checker_private_check_name_bytes(name, &bytes, &length)) {
            same_name = payload->owned_name.length == length
                && memcmp(payload->owned_name.data, bytes, length) == 0;
        }
        if (same_name) {
            if (payload_index != NULL) {
                *payload_index = index;
            }
            return payload;
        }
    }
    return NULL;
}

 const MarimayAstNode *mmay_checker_private_check_child_node(
    const MarimayAstNode *node,
    MarimaySyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (found == ordinal) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

 const MarimayAstNode *mmay_checker_private_check_child_node_any(
    const MarimayAstNode *node,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (found == ordinal) {
                return element->as.node;
            }
            found += 1;
        }
    }
    return NULL;
}

 const MarimaySyntaxToken *mmay_checker_private_check_child_token(
    const MarimayAstNode *node,
    MarimayTokenKind kind,
    size_t ordinal
) {
    size_t index = 0;
    size_t found = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_TOKEN
            && element->as.token.kind == kind) {
            if (found == ordinal) {
                return &element->as.token;
            }
            found += 1;
        }
    }
    return NULL;
}

 const MarimaySyntaxToken *mmay_checker_private_check_first_token(
    const MarimayAstNode *node
) {
    size_t index = 0;
    if (node == NULL) {
        return NULL;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_TOKEN) {
            return &element->as.token;
        }
    }
    return NULL;
}

 const MarimaySyntaxToken *mmay_checker_private_check_attribute_name(
    const MarimayAstNode *attribute
) {
    return attribute != NULL && attribute->kind == MMAY_SYNTAX_ATTRIBUTE
        ? mmay_checker_private_check_child_token(attribute, MMAY_TOKEN_IDENTIFIER, 0)
        : NULL;
}

 bool mmay_checker_private_check_attribute_is(
    const MarimayChecker *checker,
    const MarimayAstNode *attribute,
    const char *name
) {
    const MarimaySyntaxToken *token = mmay_checker_private_check_attribute_name(attribute);
    return token != NULL && !token->synthetic
        && mmay_checker_private_check_text_equal(checker, token->start, token->end, name);
}

 bool mmay_checker_private_check_source_is_test_file(const MarimayChecker *checker) {
    static const char suffix[] = "_test.my";
    size_t path_length = checker != NULL && checker->ast != NULL
        ? mmay_source_path_length(checker->ast->snapshot)
        : 0;
    const char *path = checker != NULL && checker->ast != NULL
        ? mmay_source_path(checker->ast->snapshot)
        : NULL;
    return path != NULL && path_length >= sizeof(suffix) - 1
        && memcmp(
            path + path_length - (sizeof(suffix) - 1),
            suffix,
            sizeof(suffix) - 1
        ) == 0;
}

bool mmay_checker_add_diagnostic(
    MarimayChecker *checker,
    const char *code,
    const char *message,
    size_t start,
    size_t end
) {
    MarimayCheckDiagnostic diagnostic;
    if (mmay_checker_has_failed(checker)) {
        return false;
    }
    if (!mmay_checker_private_check_span_valid(checker, start, end)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (checker->result->diagnostics.length
        >= checker->options.max_diagnostics) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.snapshot = checker->ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!mmay_vector_push(&checker->result->diagnostics, &diagnostic)) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool mmay_checker_private_checker_add_owned_diagnostic(
    MarimayChecker *checker,
    const char *code,
    const unsigned char *message,
    size_t message_length,
    size_t start,
    size_t end
) {
    MarimayCheckDiagnostic diagnostic;
    if (mmay_checker_has_failed(checker)) {
        return false;
    }
    if (!mmay_checker_private_check_span_valid(checker, start, end)
        || (message == NULL && message_length != 0)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (checker->result->diagnostics.length
        >= checker->options.max_diagnostics) {
        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    if (!mmay_owned_bytes_copy(
            &diagnostic.owned_message,
            checker->result->allocator,
            message,
            message_length
        )) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = (const char *)diagnostic.owned_message.data;
    diagnostic.snapshot = checker->ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!mmay_vector_push(&checker->result->diagnostics, &diagnostic)) {
        mmay_owned_bytes_dispose(&diagnostic.owned_message);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}
