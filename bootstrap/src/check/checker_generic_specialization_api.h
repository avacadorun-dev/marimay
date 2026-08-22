#ifndef CHECKER_GENERIC_SPECIALIZATION_API_H
#define CHECKER_GENERIC_SPECIALIZATION_API_H

#include "checker_generic_types_api.h"
#include "package_imports_api.h"

const MarimayCheckFunctionDeclaration *mmay_checker_private_find_function_by_symbol(
    const MarimayChecker *checker,
    MarimaySymbolId symbol,
    size_t *index
);

bool mmay_checker_private_prepare_generic_arguments(
    MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration,
    const MarimayTypeId *actual_types,
    size_t actual_count,
    const MarimayVector *explicit_arguments,
    MarimayVector *arguments
);

bool mmay_checker_private_instantiate_function(
    MarimayChecker *checker,
    size_t base_index,
    const MarimayVector *arguments,
    MarimayCheckFunctionDeclaration **result
);

#endif
