#ifndef CHECKER_GENERIC_TYPES_API_H
#define CHECKER_GENERIC_TYPES_API_H

#include "checker_function_context.h"

bool mmay_checker_private_collect_type_parameters(
    MarimayChecker *checker,
    const MarimayAstNode *declaration_node,
    MarimayVector *parameters
);

const MarimayCheckTypeParameterDeclaration *mmay_checker_private_find_type_parameter(
    const MarimayChecker *checker,
    MarimayCheckName name
);

bool mmay_checker_private_resolve_type_parameter(
    const MarimayChecker *checker,
    MarimayCheckName name,
    MarimayTypeId *result
);

bool mmay_checker_private_substitute_type(
    MarimayChecker *checker,
    MarimayTypeId type,
    const MarimayVector *parameters,
    const MarimayVector *arguments,
    MarimayTypeId *result
);

bool mmay_checker_private_substitute_function_type(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayTypeId *result
);

bool mmay_checker_private_substitute_nominal_member_type(
    MarimayFunctionChecker *function,
    MarimayTypeId nominal_type,
    const MarimayVector *parameters,
    MarimayTypeId member_type,
    MarimayTypeId *result
);

bool mmay_checker_private_parse_type_arguments(
    MarimayChecker *checker,
    const MarimayAstNode *type_arguments,
    MarimayVector *arguments
);

bool mmay_checker_private_intern_generic_nominal(
    MarimayChecker *checker,
    MarimayTypeKind kind,
    MarimaySymbolId symbol,
    const MarimayVector *arguments,
    MarimayTypeId *result
);

bool mmay_checker_private_type_matches_nominal(
    const MarimayChecker *checker,
    MarimayTypeId type,
    MarimaySymbolId symbol,
    size_t argument_count
);

#endif
