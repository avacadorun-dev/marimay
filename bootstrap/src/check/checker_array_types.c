#include "checker_array_types_api.h"

#include "checker_core_api.h"
#include "checker_lookup_api.h"

const MarimayAstNode *mmay_checker_private_array_type_element(
    const MarimayAstNode *node
) {
    const MarimayAstElement *open = NULL;
    const MarimayAstElement *close = NULL;
    const MarimayAstElement *element = NULL;
    if (node == NULL || node->kind != MMAY_SYNTAX_TYPE
        || node->child_count != 3) {
        return NULL;
    }
    open = mmay_ast_child(node, 0);
    close = mmay_ast_child(node, 1);
    element = mmay_ast_child(node, 2);
    return open != NULL && close != NULL && element != NULL
        && open->kind == MMAY_AST_ELEMENT_TOKEN
        && open->as.token.kind == MMAY_TOKEN_LEFT_BRACKET
        && close->kind == MMAY_AST_ELEMENT_TOKEN
        && close->as.token.kind == MMAY_TOKEN_RIGHT_BRACKET
        && element->kind == MMAY_AST_ELEMENT_NODE
        && element->as.node != NULL
        && element->as.node->kind == MMAY_SYNTAX_TYPE
            ? element->as.node : NULL;
}

bool mmay_checker_private_resolve_array_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result,
    bool *resolved
) {
    const MarimayAstNode *element = NULL;
    MarimayTypeId element_type = MMAY_TYPE_INVALID;
    if (checker == NULL || node == NULL || result == NULL || resolved == NULL
        || node->kind != MMAY_SYNTAX_TYPE) {
        mmay_checker_fail(checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    element = mmay_checker_private_array_type_element(node);
    if (element == NULL) {
        return true;
    }
    *resolved = true;
    if (!mmay_checker_private_check_exact_type(
            checker,
            element,
            &element_type
        )) {
        *result = MMAY_TYPE_INVALID;
        return false;
    }
    if (!mmay_type_intern_array(
            &checker->result->types,
            element_type,
            result
        )) {
        mmay_checker_fail(
            checker,
            checker->result->types.descriptors.length
                    >= checker->options.type_options.max_types
                        - MMAY_TYPE_FIRST_DYNAMIC
                ? MMAY_CHECK_LIMIT_REACHED
                : MMAY_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    return true;
}
