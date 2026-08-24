#ifndef MMAY_CHECKER_COMPOUND_TYPES_API_H
#define MMAY_CHECKER_COMPOUND_TYPES_API_H

#include "checker_internal.h"

bool mmay_checker_private_resolve_compound_type(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    MarimayTypeId *result,
    bool *resolved
);

#endif
