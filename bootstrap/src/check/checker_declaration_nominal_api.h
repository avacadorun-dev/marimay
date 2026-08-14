#ifndef CHECKER_DECLARATION_NOMINAL_API_H
#define CHECKER_DECLARATION_NOMINAL_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 bool ruda_checker_private_collect_type_alias_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
);
 bool ruda_checker_private_evaluate_type_alias(
    RudaChecker *checker,
    RudaCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
);
 bool ruda_checker_private_evaluate_all_type_aliases(RudaChecker *checker);
 bool ruda_checker_private_publish_type_aliases(RudaChecker *checker);
#endif
