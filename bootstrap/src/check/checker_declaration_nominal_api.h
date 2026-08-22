#ifndef CHECKER_DECLARATION_NOMINAL_API_H
#define CHECKER_DECLARATION_NOMINAL_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 bool mmay_checker_private_collect_type_alias_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
 bool mmay_checker_private_evaluate_type_alias(
    MarimayChecker *checker,
    MarimayCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
);
 bool mmay_checker_private_evaluate_all_type_aliases(MarimayChecker *checker);
 bool mmay_checker_private_publish_type_aliases(MarimayChecker *checker);
#endif
