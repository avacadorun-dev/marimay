#ifndef CHECKER_COLLECTION_TYPES_API_H
#define CHECKER_COLLECTION_TYPES_API_H

#include "checker_internal.h"

/* D079 compiler-known Map<K, V> and Set<T> type spellings. */
bool mmay_checker_private_resolve_collection_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result,
    bool *resolved
);

#endif
