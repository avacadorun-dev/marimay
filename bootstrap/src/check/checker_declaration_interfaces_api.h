#ifndef CHECKER_DECLARATION_INTERFACES_API_H
#define CHECKER_DECLARATION_INTERFACES_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

bool ruda_checker_private_collect_interface_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
);
bool ruda_checker_private_resolve_all_interface_methods(RudaChecker *checker);
bool ruda_checker_private_publish_interfaces(RudaChecker *checker);

#endif
