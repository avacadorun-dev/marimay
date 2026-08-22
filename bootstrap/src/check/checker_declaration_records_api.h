#ifndef CHECKER_DECLARATION_RECORDS_API_H
#define CHECKER_DECLARATION_RECORDS_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 bool mmay_checker_private_collect_record_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
 bool mmay_checker_private_collect_enum_declaration(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
 bool mmay_checker_private_record_field_name_is_duplicate(
    const MarimayChecker *checker,
    const MarimayCheckRecordDeclaration *declaration,
    MarimayCheckName name
);
 bool mmay_checker_private_resolve_record_fields(
    MarimayChecker *checker,
    MarimayCheckRecordDeclaration *declaration
);
 bool mmay_checker_private_resolve_all_record_fields(MarimayChecker *checker);
#endif
