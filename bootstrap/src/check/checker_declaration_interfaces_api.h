#ifndef CHECKER_DECLARATION_INTERFACES_API_H
#define CHECKER_DECLARATION_INTERFACES_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

bool mmay_checker_private_collect_interface_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
bool mmay_checker_private_resolve_all_interface_methods(MarimayChecker *checker);
bool mmay_checker_private_publish_interfaces(MarimayChecker *checker);

#endif
