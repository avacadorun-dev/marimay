#ifndef CHECKER_DECLARATION_CONSTANTS_API_H
#define CHECKER_DECLARATION_CONSTANTS_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 bool mmay_checker_private_evaluate_constant(
    MarimayChecker *checker,
    MarimayCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
);
 bool mmay_checker_private_evaluate_all_constants(MarimayChecker *checker);
 bool mmay_checker_private_publish_constants(MarimayChecker *checker);
#endif
