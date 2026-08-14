#ifndef CHECKER_DECLARATION_CONSTANTS_API_H
#define CHECKER_DECLARATION_CONSTANTS_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 bool ruda_checker_private_evaluate_constant(
    RudaChecker *checker,
    RudaCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
);
 bool ruda_checker_private_evaluate_all_constants(RudaChecker *checker);
 bool ruda_checker_private_publish_constants(RudaChecker *checker);
#endif
