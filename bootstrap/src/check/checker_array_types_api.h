#ifndef CHECKER_ARRAY_TYPES_API_H
#define CHECKER_ARRAY_TYPES_API_H

#include "checker_internal.h"

const RudaAstNode *ruda_checker_private_array_type_element(
    const RudaAstNode *node
);

/* D054 prefix spelling; resolved tells the general type owner to stop. */
bool ruda_checker_private_resolve_array_type(
    RudaChecker *checker,
    const RudaAstNode *node,
    RudaTypeId *result,
    bool *resolved
);

#endif
