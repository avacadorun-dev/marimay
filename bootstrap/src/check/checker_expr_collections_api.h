#ifndef CHECKER_EXPR_COLLECTIONS_API_H
#define CHECKER_EXPR_COLLECTIONS_API_H

#include "checker_function_context.h"

bool mmay_checker_private_validate_generic_collection_keys(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
bool mmay_checker_private_compile_collection_constructor(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
);
bool mmay_checker_private_compile_collection_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *receiver_node,
    const MarimayExpressionValue *receiver,
    bool *resolved,
    MarimayExpressionValue *result
);
bool mmay_checker_private_collection_layout(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    size_t start,
    size_t end,
    MarimayMirCollectionId *layout,
    MarimayTypeId *key,
    MarimayTypeId *value,
    bool *is_set
);

#endif
