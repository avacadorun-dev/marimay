#include "checker_declaration_enums_api.h"
#include "checker_core_api.h"
#include "checker_declaration_lifecycle_api.h"
#include "checker_lookup_api.h"
#include "checker_generic_types_api.h"





#include "checker_internal.h"

 bool mmay_checker_private_enum_variant_name_is_duplicate(
    const MarimayChecker *checker,
    const MarimayCheckEnumDeclaration *declaration,
    MarimayCheckName name
) {
    size_t index = 0;
    for (index = 0; index < declaration->variants.length; index += 1) {
        const MarimayCheckEnumVariantDeclaration *variant = mmay_vector_at_const(
            &declaration->variants,
            index
        );
        if (variant != NULL
            && mmay_checker_private_check_names_equal(checker, variant->name, name)) {
            return true;
        }
    }
    return false;
}

 bool mmay_checker_private_enum_payload_name_is_duplicate(
    const MarimayChecker *checker,
    const MarimayCheckEnumVariantDeclaration *variant,
    MarimayCheckName name
) {
    size_t index = 0;
    for (index = 0; index < variant->payload.length; index += 1) {
        const MarimayCheckEnumPayloadItemDeclaration *item = mmay_vector_at_const(
            &variant->payload,
            index
        );
        if (item != NULL && item->has_name
            && mmay_checker_private_check_names_equal(checker, item->name, name)) {
            return true;
        }
    }
    return false;
}

 bool mmay_checker_private_resolve_enum_payload_item(
    MarimayChecker *checker,
    const MarimayCheckEnumDeclaration *declaration,
    MarimayCheckEnumVariantDeclaration *variant,
    const MarimayAstNode *node,
    bool has_name,
    bool *valid
) {
    const MarimayAstNode *type_node = has_name
        ? mmay_checker_private_check_child_node(node, MMAY_SYNTAX_TYPE, 0)
        : node;
    const MarimaySyntaxToken *name_token = has_name
        ? mmay_checker_private_check_child_token(node, MMAY_TOKEN_IDENTIFIER, 0)
        : NULL;
    MarimayCheckEnumPayloadItemDeclaration item;
    memset(&item, 0, sizeof(item));
    if (checker == NULL || declaration == NULL || variant == NULL
        || node == NULL || valid == NULL || type_node == NULL
        || type_node->kind != MMAY_SYNTAX_TYPE
        || (has_name && (name_token == NULL || name_token->synthetic))) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    item.node = node;
    item.type = MMAY_TYPE_INVALID;
    item.has_name = has_name;
    if (has_name) {
        item.name.snapshot = checker->ast->snapshot;
        item.name.start = name_token->start;
        item.name.end = name_token->end;
        if (mmay_checker_private_enum_payload_name_is_duplicate(
                checker,
                variant,
                item.name
            )) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_DUPLICATE_NAME,
                "payload field name is already declared in this variant",
                item.name.start,
                item.name.end
            );
            *valid = false;
            return !mmay_checker_has_failed(checker);
        }
        if (!mmay_owned_bytes_copy(
                &item.owned_name,
                checker->result->allocator,
                checker->source + item.name.start,
                item.name.end - item.name.start
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        item.name_initialized = true;
    }
    if (!mmay_checker_private_check_exact_type(checker, type_node, &item.type)) {
        if (mmay_checker_has_failed(checker)) {
            mmay_checker_private_enum_payload_item_declaration_dispose(&item);
            return false;
        }
        *valid = false;
    } else if (declaration->is_public
        && !mmay_checker_private_check_public_type_reference(checker, type_node)) {
        if (mmay_checker_has_failed(checker)) {
            mmay_checker_private_enum_payload_item_declaration_dispose(&item);
            return false;
        }
        *valid = false;
    }
    if (!mmay_vector_push(&variant->payload, &item)) {
        mmay_checker_private_enum_payload_item_declaration_dispose(&item);
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memset(&item, 0, sizeof(item));
    return true;
}

 bool mmay_checker_private_resolve_enum_variants(
    MarimayChecker *checker,
    MarimayCheckEnumDeclaration *declaration
) {
    size_t child_index = 0;
    size_t variant_count = 0;
    bool valid = true;
    if (declaration == NULL || !declaration->declaration_valid
        || !declaration->variants_initialized
        || !mmay_checker_private_checker_set_ast(checker, declaration->ast)) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    checker->active_type_parameters = &declaration->type_parameters;
    for (child_index = 0; child_index < declaration->node->child_count;
        child_index += 1) {
        const MarimayAstElement *element = mmay_ast_child(
            declaration->node,
            child_index
        );
        const MarimaySyntaxToken *name_token = NULL;
        const MarimayAstNode *payload_node = NULL;
        MarimayCheckEnumVariantDeclaration variant;
        size_t payload_index = 0;
        size_t payload_count = 0;
        if (element == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind != MMAY_AST_ELEMENT_NODE
            || element->as.node == NULL
            || element->as.node->kind != MMAY_SYNTAX_ENUM_VARIANT) {
            continue;
        }
        if (variant_count >= checker->options.max_variants_per_enum) {
            mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
            return false;
        }
        variant_count += 1;
        memset(&variant, 0, sizeof(variant));
        variant.node = element->as.node;
        name_token = mmay_checker_private_check_child_token(
            variant.node,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
        if (name_token == NULL || name_token->synthetic) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        variant.name.snapshot = checker->ast->snapshot;
        variant.name.start = name_token->start;
        variant.name.end = name_token->end;
        if (mmay_checker_private_enum_variant_name_is_duplicate(
                checker,
                declaration,
                variant.name
            )) {
            (void)mmay_checker_add_diagnostic(
                checker,
                MMAY_DIAGNOSTIC_DUPLICATE_NAME,
                "variant name is already declared in this enum",
                variant.name.start,
                variant.name.end
            );
            valid = false;
            if (mmay_checker_has_failed(checker)) {
                return false;
            }
            continue;
        }
        if (!mmay_owned_bytes_copy(
                &variant.owned_name,
                checker->result->allocator,
                checker->source + variant.name.start,
                variant.name.end - variant.name.start
            )) {
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        variant.name_initialized = true;
        if (!mmay_vector_init(
                &variant.payload,
                checker->result->allocator,
                sizeof(MarimayCheckEnumPayloadItemDeclaration)
            )) {
            mmay_checker_private_enum_variant_declaration_dispose(&variant);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        variant.payload_initialized = true;
        payload_node = mmay_checker_private_check_child_node(
            variant.node,
            MMAY_SYNTAX_POSITIONAL_PAYLOAD,
            0
        );
        if (payload_node != NULL) {
            variant.kind = MMAY_CHECKED_ENUM_VARIANT_POSITIONAL;
            for (payload_index = 0; payload_index < payload_node->child_count;
                payload_index += 1) {
                const MarimayAstElement *payload = mmay_ast_child(
                    payload_node,
                    payload_index
                );
                if (payload != NULL && payload->kind == MMAY_AST_ELEMENT_NODE
                    && payload->as.node != NULL
                    && payload->as.node->kind == MMAY_SYNTAX_TYPE) {
                    if (payload_count
                            >= checker->options.max_payload_items_per_variant) {
                        mmay_checker_private_enum_variant_declaration_dispose(&variant);
                        mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
                        return false;
                    }
                    payload_count += 1;
                    if (!mmay_checker_private_resolve_enum_payload_item(
                            checker,
                            declaration,
                            &variant,
                            payload->as.node,
                            false,
                            &valid
                        )) {
                        mmay_checker_private_enum_variant_declaration_dispose(&variant);
                        return false;
                    }
                }
            }
        } else {
            payload_node = mmay_checker_private_check_child_node(
                variant.node,
                MMAY_SYNTAX_NAMED_PAYLOAD,
                0
            );
            if (payload_node != NULL) {
                variant.kind = MMAY_CHECKED_ENUM_VARIANT_NAMED;
                for (payload_index = 0;
                    payload_index < payload_node->child_count;
                    payload_index += 1) {
                    const MarimayAstElement *payload = mmay_ast_child(
                        payload_node,
                        payload_index
                    );
                    if (payload != NULL
                        && payload->kind == MMAY_AST_ELEMENT_NODE
                        && payload->as.node != NULL
                        && payload->as.node->kind == MMAY_SYNTAX_PAYLOAD_FIELD) {
                        if (payload_count >= checker->options
                                .max_payload_items_per_variant) {
                            mmay_checker_private_enum_variant_declaration_dispose(&variant);
                            mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
                            return false;
                        }
                        payload_count += 1;
                        if (!mmay_checker_private_resolve_enum_payload_item(
                                checker,
                                declaration,
                                &variant,
                                payload->as.node,
                                true,
                                &valid
                            )) {
                            mmay_checker_private_enum_variant_declaration_dispose(&variant);
                            return false;
                        }
                    }
                }
            } else {
                variant.kind = MMAY_CHECKED_ENUM_VARIANT_UNIT;
            }
        }
        if (!mmay_vector_push(&declaration->variants, &variant)) {
            mmay_checker_private_enum_variant_declaration_dispose(&variant);
            mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
            return false;
        }
        memset(&variant, 0, sizeof(variant));
    }
    if (checker->active_type_parameters == &declaration->type_parameters) {
        checker->active_type_parameters = NULL;
    }
    declaration->variants_resolved = valid;
    return valid && !mmay_checker_has_failed(checker);
}

 bool mmay_checker_private_resolve_all_enum_variants(MarimayChecker *checker) {
    size_t index = 0;
    for (index = 0; index < checker->enums.length
        && !mmay_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0; index += 1) {
        MarimayCheckEnumDeclaration *declaration = mmay_vector_at(
            &checker->enums,
            index
        );
        if (declaration == NULL || !mmay_checker_private_resolve_enum_variants(
                checker,
                declaration
            )) {
            if (!mmay_checker_has_failed(checker)
                && checker->result->diagnostics.length == 0) {
                mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
    }
    return !mmay_checker_has_failed(checker)
        && checker->result->diagnostics.length == 0;
}
