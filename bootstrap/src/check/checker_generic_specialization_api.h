#ifndef CHECKER_GENERIC_SPECIALIZATION_API_H
#define CHECKER_GENERIC_SPECIALIZATION_API_H

#include "checker_generic_types_api.h"
#include "package_imports_api.h"

const RudaCheckFunctionDeclaration *ruda_checker_private_find_function_by_symbol(
    const RudaChecker *checker,
    RudaSymbolId symbol,
    size_t *index
);

bool ruda_checker_private_prepare_generic_arguments(
    RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    const RudaTypeId *actual_types,
    size_t actual_count,
    const RudaVector *explicit_arguments,
    RudaVector *arguments
);

bool ruda_checker_private_instantiate_function(
    RudaChecker *checker,
    size_t base_index,
    const RudaVector *arguments,
    RudaCheckFunctionDeclaration **result
);

#endif
