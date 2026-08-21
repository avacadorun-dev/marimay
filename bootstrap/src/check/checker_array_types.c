#include "checker_array_types_api.h"

#include "checker_core_api.h"
#include "checker_lookup_api.h"

const RudaAstNode *ruda_checker_private_array_type_element(
    const RudaAstNode *node
) {
    const RudaAstElement *open = NULL;
    const RudaAstElement *close = NULL;
    const RudaAstElement *element = NULL;
    if (node == NULL || node->kind != RUDA_SYNTAX_TYPE
        || node->child_count != 3) {
        return NULL;
    }
    open = ruda_ast_child(node, 0);
    close = ruda_ast_child(node, 1);
    element = ruda_ast_child(node, 2);
    return open != NULL && close != NULL && element != NULL
        && open->kind == RUDA_AST_ELEMENT_TOKEN
        && open->as.token.kind == RUDA_TOKEN_LEFT_BRACKET
        && close->kind == RUDA_AST_ELEMENT_TOKEN
        && close->as.token.kind == RUDA_TOKEN_RIGHT_BRACKET
        && element->kind == RUDA_AST_ELEMENT_NODE
        && element->as.node != NULL
        && element->as.node->kind == RUDA_SYNTAX_TYPE
            ? element->as.node : NULL;
}

bool ruda_checker_private_resolve_array_type(
    RudaChecker *checker,
    const RudaAstNode *node,
    RudaTypeId *result,
    bool *resolved
) {
    const RudaAstNode *element = NULL;
    RudaTypeId element_type = RUDA_TYPE_INVALID;
    if (checker == NULL || node == NULL || result == NULL || resolved == NULL
        || node->kind != RUDA_SYNTAX_TYPE) {
        ruda_checker_fail(checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    element = ruda_checker_private_array_type_element(node);
    if (element == NULL) {
        return true;
    }
    *resolved = true;
    if (!ruda_checker_private_check_exact_type(
            checker,
            element,
            &element_type
        )) {
        *result = RUDA_TYPE_INVALID;
        return false;
    }
    if (!ruda_type_intern_array(
            &checker->result->types,
            element_type,
            result
        )) {
        ruda_checker_fail(
            checker,
            checker->result->types.descriptors.length
                    >= checker->options.type_options.max_types
                        - RUDA_TYPE_FIRST_DYNAMIC
                ? RUDA_CHECK_LIMIT_REACHED
                : RUDA_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    return true;
}
