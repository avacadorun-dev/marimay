#include "checker_declaration_enums_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_lookup_api.h"





#include "checker_internal.h"

 bool ruda_checker_private_enum_variant_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckEnumDeclaration *declaration,
    RudaCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->variants.length; index += 1) {
        const RudaCheckEnumVariantDeclaration *variant = ruda_vector_at_const(
            &declaration->variants,
            index
        );
        if (variant != NULL
            && ruda_checker_private_check_names_equal(checker, variant->name, name)) {
            return true;
        }
    }
    return false;
}

 bool ruda_checker_private_enum_payload_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckEnumVariantDeclaration *variant,
    RudaCheckName name
) {
    size_t index = 0;
    for (index = 0; index < variant->payload.length; index += 1) {
        const RudaCheckEnumPayloadItemDeclaration *item = ruda_vector_at_const(
            &variant->payload,
            index
        );
        if (item != NULL && item->has_name
            && ruda_checker_private_check_names_equal(checker, item->name, name)) {
            return true;
        }
    }
    return false;
}

 bool ruda_checker_private_resolve_enum_payload_item(
    RudaChecker *checker,
    const RudaCheckEnumDeclaration *declaration,
    RudaCheckEnumVariantDeclaration *variant,
    const RudaAstNode *node,
    bool has_name,
    bool *valid
) {
    const RudaAstNode *type_node = has_name
        ? ruda_checker_private_check_child_node(node, RUDA_SYNTAX_TYPE, 0)
        : node;
    const RudaSyntaxToken *name_token = has_name
        ? ruda_checker_private_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 0)
        : NULL;
    RudaCheckEnumPayloadItemDeclaration item;
    memset(&item, 0, sizeof(item));
    if (checker == NULL || declaration == NULL || variant == NULL
        || node == NULL || valid == NULL || type_node == NULL
        || type_node->kind != RUDA_SYNTAX_TYPE
        || (has_name && (name_token == NULL || name_token->synthetic))) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    item.node = node;
    item.type = RUDA_TYPE_INVALID;
    item.has_name = has_name;
    if (has_name) {
        item.name.snapshot = checker->ast->snapshot;
        item.name.start = name_token->start;
        item.name.end = name_token->end;
        if (ruda_checker_private_enum_payload_name_is_duplicate(
                checker,
                variant,
                item.name
            )) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "payload field name is already declared in this variant",
                item.name.start,
                item.name.end
            );
            *valid = false;
            return !ruda_checker_has_failed(checker);
        }
        if (!ruda_owned_bytes_copy(
                &item.owned_name,
                checker->result->allocator,
                checker->source + item.name.start,
                item.name.end - item.name.start
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        item.name_initialized = true;
    }
    if (!ruda_checker_private_check_exact_type(checker, type_node, &item.type)) {
        if (ruda_checker_has_failed(checker)) {
            ruda_checker_private_enum_payload_item_declaration_dispose(&item);
            return false;
        }
        *valid = false;
    } else if (declaration->is_public
        && !ruda_checker_private_check_public_type_reference(checker, type_node)) {
        if (ruda_checker_has_failed(checker)) {
            ruda_checker_private_enum_payload_item_declaration_dispose(&item);
            return false;
        }
        *valid = false;
    }
    if (!ruda_vector_push(&variant->payload, &item)) {
        ruda_checker_private_enum_payload_item_declaration_dispose(&item);
        ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memset(&item, 0, sizeof(item));
    return true;
}

 bool ruda_checker_private_resolve_enum_variants(
    RudaChecker *checker,
    RudaCheckEnumDeclaration *declaration
) {
    size_t child_index = 0;
    size_t variant_count = 0;
    bool valid = true;
    if (declaration == NULL || !declaration->declaration_valid
        || !declaration->variants_initialized
        || !ruda_checker_private_checker_set_ast(checker, declaration->ast)) {
        ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (child_index = 0; child_index < declaration->node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            declaration->node,
            child_index
        );
        const RudaSyntaxToken *name_token = NULL;
        const RudaAstNode *payload_node = NULL;
        RudaCheckEnumVariantDeclaration variant;
        size_t payload_index = 0;
        size_t payload_count = 0;
        if (element == NULL) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE
            || element->as.node == NULL
            || element->as.node->kind != RUDA_SYNTAX_ENUM_VARIANT) {
            continue;
        }
        if (variant_count >= checker->options.max_variants_per_enum) {
            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
            return false;
        }
        variant_count += 1;
        memset(&variant, 0, sizeof(variant));
        variant.node = element->as.node;
        name_token = ruda_checker_private_check_child_token(
            variant.node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (name_token == NULL || name_token->synthetic) {
            ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        variant.name.snapshot = checker->ast->snapshot;
        variant.name.start = name_token->start;
        variant.name.end = name_token->end;
        if (ruda_checker_private_enum_variant_name_is_duplicate(
                checker,
                declaration,
                variant.name
            )) {
            (void)ruda_checker_add_diagnostic(
                checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "variant name is already declared in this enum",
                variant.name.start,
                variant.name.end
            );
            valid = false;
            if (ruda_checker_has_failed(checker)) {
                return false;
            }
            continue;
        }
        if (!ruda_owned_bytes_copy(
                &variant.owned_name,
                checker->result->allocator,
                checker->source + variant.name.start,
                variant.name.end - variant.name.start
            )) {
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        variant.name_initialized = true;
        if (!ruda_vector_init(
                &variant.payload,
                checker->result->allocator,
                sizeof(RudaCheckEnumPayloadItemDeclaration)
            )) {
            ruda_checker_private_enum_variant_declaration_dispose(&variant);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        variant.payload_initialized = true;
        payload_node = ruda_checker_private_check_child_node(
            variant.node,
            RUDA_SYNTAX_POSITIONAL_PAYLOAD,
            0
        );
        if (payload_node != NULL) {
            variant.kind = RUDA_CHECKED_ENUM_VARIANT_POSITIONAL;
            for (payload_index = 0; payload_index < payload_node->child_count;
                payload_index += 1) {
                const RudaAstElement *payload = ruda_ast_child(
                    payload_node,
                    payload_index
                );
                if (payload != NULL && payload->kind == RUDA_AST_ELEMENT_NODE
                    && payload->as.node != NULL
                    && payload->as.node->kind == RUDA_SYNTAX_TYPE) {
                    if (payload_count
                            >= checker->options.max_payload_items_per_variant) {
                        ruda_checker_private_enum_variant_declaration_dispose(&variant);
                        ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
                        return false;
                    }
                    payload_count += 1;
                    if (!ruda_checker_private_resolve_enum_payload_item(
                            checker,
                            declaration,
                            &variant,
                            payload->as.node,
                            false,
                            &valid
                        )) {
                        ruda_checker_private_enum_variant_declaration_dispose(&variant);
                        return false;
                    }
                }
            }
        } else {
            payload_node = ruda_checker_private_check_child_node(
                variant.node,
                RUDA_SYNTAX_NAMED_PAYLOAD,
                0
            );
            if (payload_node != NULL) {
                variant.kind = RUDA_CHECKED_ENUM_VARIANT_NAMED;
                for (payload_index = 0;
                    payload_index < payload_node->child_count;
                    payload_index += 1) {
                    const RudaAstElement *payload = ruda_ast_child(
                        payload_node,
                        payload_index
                    );
                    if (payload != NULL
                        && payload->kind == RUDA_AST_ELEMENT_NODE
                        && payload->as.node != NULL
                        && payload->as.node->kind == RUDA_SYNTAX_PAYLOAD_FIELD) {
                        if (payload_count >= checker->options
                                .max_payload_items_per_variant) {
                            ruda_checker_private_enum_variant_declaration_dispose(&variant);
                            ruda_checker_fail(checker, RUDA_CHECK_LIMIT_REACHED);
                            return false;
                        }
                        payload_count += 1;
                        if (!ruda_checker_private_resolve_enum_payload_item(
                                checker,
                                declaration,
                                &variant,
                                payload->as.node,
                                true,
                                &valid
                            )) {
                            ruda_checker_private_enum_variant_declaration_dispose(&variant);
                            return false;
                        }
                    }
                }
            } else {
                variant.kind = RUDA_CHECKED_ENUM_VARIANT_UNIT;
            }
        }
        if (!ruda_vector_push(&declaration->variants, &variant)) {
            ruda_checker_private_enum_variant_declaration_dispose(&variant);
            ruda_checker_fail(checker, RUDA_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&variant, 0, sizeof(variant));
    }
    declaration->variants_resolved = valid;
    return valid && !ruda_checker_has_failed(checker);
}

 bool ruda_checker_private_resolve_all_enum_variants(RudaChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->enums.length
        && !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        RudaCheckEnumDeclaration *declaration = ruda_vector_at(
            &checker->enums,
            index
        );
        if (declaration == NULL || !ruda_checker_private_resolve_enum_variants(
                checker,
                declaration
            )) {
            if (!ruda_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                ruda_checker_fail(checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !ruda_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}
