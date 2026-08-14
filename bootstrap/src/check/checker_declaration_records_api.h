#ifndef CHECKER_DECLARATION_RECORDS_API_H
#define CHECKER_DECLARATION_RECORDS_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 bool ruda_checker_private_collect_record_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
);
 bool ruda_checker_private_collect_enum_declaration(
    RudaChecker *checker,
    const RudaAstNode *node
);
 bool ruda_checker_private_record_field_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckRecordDeclaration *declaration,
    RudaCheckName name
);
 bool ruda_checker_private_resolve_record_fields(
    RudaChecker *checker,
    RudaCheckRecordDeclaration *declaration
);
 bool ruda_checker_private_resolve_all_record_fields(RudaChecker *checker);
#endif
