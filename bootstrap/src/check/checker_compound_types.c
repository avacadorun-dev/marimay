#include "checker_compound_types_api.h"

#include "checker_lookup_api.h"

static void mmay_checker_compound_type_intern_failed(
    MarimayChecker *checker
) {
    const size_t max_types = checker->options.type_options.max_types;
    const size_t dynamic_types = checker->result->types.descriptors.length;
    const bool at_limit = max_types <= MMAY_TYPE_FIRST_DYNAMIC
        || dynamic_types >= max_types - MMAY_TYPE_FIRST_DYNAMIC;
    mmay_checker_fail(
        checker,
        at_limit ? MMAY_CHECK_LIMIT_REACHED : MMAY_CHECK_ALLOCATION_FAILED
    );
}

bool mmay_checker_private_resolve_compound_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result,
    bool *resolved
) {
    MarimayTypeId *parts = NULL;
    size_t part_count = 0;
    size_t part_bytes = 0;
    size_t cursor = 0;
    size_t index = 0;
    bool parenthesized = false;
    bool function_type = false;
    bool accepted = false;
    if (checker == NULL || node == NULL || result == NULL
        || resolved == NULL || node->kind != MMAY_SYNTAX_TYPE) {
        if (checker != NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element == NULL) {
            mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (element->kind == MMAY_AST_ELEMENT_TOKEN) {
            parenthesized = parenthesized
                || element->as.token.kind == MMAY_TOKEN_LEFT_PAREN;
            function_type = function_type
                || element->as.token.kind == MMAY_TOKEN_FAT_ARROW;
        } else if (element->as.node != NULL
            && element->as.node->kind == MMAY_SYNTAX_TYPE) {
            if (part_count == SIZE_MAX) {
                mmay_checker_fail(checker, MMAY_CHECK_LIMIT_REACHED);
                goto cleanup;
            }
            part_count += 1;
        }
    }
    if (!parenthesized) {
        return true;
    }
    *resolved = true;
    if ((!function_type && part_count == 0)
        || (function_type && part_count == 0)
        || !mmay_size_multiply(
            part_count,
            sizeof(MarimayTypeId),
            &part_bytes
        )) {
        mmay_checker_fail(
            checker,
            part_count == 0
                ? MMAY_CHECK_INVALID_ARGUMENT
                : MMAY_CHECK_LIMIT_REACHED
        );
        goto cleanup;
    }
    parts = mmay_allocate(&checker->result->allocator, part_bytes);
    if (parts == NULL) {
        mmay_checker_fail(checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    for (index = 0; index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind == MMAY_SYNTAX_TYPE) {
            if (cursor >= part_count
                || !mmay_checker_private_check_exact_type(
                    checker,
                    element->as.node,
                    &parts[cursor]
                )) {
                goto cleanup;
            }
            cursor += 1;
        }
    }
    if (cursor != part_count) {
        mmay_checker_fail(checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    accepted = function_type
        ? mmay_type_intern_function(
            &checker->result->types,
            parts,
            part_count - 1,
            parts[part_count - 1],
            result
        )
        : mmay_type_intern_tuple(
            &checker->result->types,
            parts,
            part_count,
            result
        );
    if (!accepted) {
        mmay_checker_compound_type_intern_failed(checker);
    }

cleanup:
    mmay_deallocate(&checker->result->allocator, parts, part_bytes);
    return accepted && !mmay_checker_has_failed(checker);
}
