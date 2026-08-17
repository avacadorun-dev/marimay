#ifndef CHECKER_GENERIC_TYPES_API_H
#define CHECKER_GENERIC_TYPES_API_H

#include "checker_function_context.h"

bool ruda_checker_private_collect_type_parameters(
    RudaChecker *checker,
    const RudaAstNode *declaration_node,
    RudaVector *parameters
);

const RudaCheckTypeParameterDeclaration *ruda_checker_private_find_type_parameter(
    const RudaChecker *checker,
    RudaCheckName name
);

bool ruda_checker_private_resolve_type_parameter(
    const RudaChecker *checker,
    RudaCheckName name,
    RudaTypeId *result
);

bool ruda_checker_private_substitute_type(
    RudaChecker *checker,
    RudaTypeId type,
    const RudaVector *parameters,
    const RudaVector *arguments,
    RudaTypeId *result
);

bool ruda_checker_private_substitute_function_type(
    RudaFunctionChecker *function,
    RudaTypeId type,
    RudaTypeId *result
);

bool ruda_checker_private_substitute_nominal_member_type(
    RudaFunctionChecker *function,
    RudaTypeId nominal_type,
    const RudaVector *parameters,
    RudaTypeId member_type,
    RudaTypeId *result
);

bool ruda_checker_private_parse_type_arguments(
    RudaChecker *checker,
    const RudaAstNode *type_arguments,
    RudaVector *arguments
);

bool ruda_checker_private_intern_generic_nominal(
    RudaChecker *checker,
    RudaTypeKind kind,
    RudaSymbolId symbol,
    const RudaVector *arguments,
    RudaTypeId *result
);

bool ruda_checker_private_type_matches_nominal(
    const RudaChecker *checker,
    RudaTypeId type,
    RudaSymbolId symbol,
    size_t argument_count
);

#endif
