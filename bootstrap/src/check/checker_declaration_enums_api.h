#ifndef CHECKER_DECLARATION_ENUMS_API_H
#define CHECKER_DECLARATION_ENUMS_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 bool mmay_checker_private_enum_variant_name_is_duplicate(
    const MarimayChecker *checker,
    const MarimayCheckEnumDeclaration *declaration,
    MarimayCheckName name
);
 bool mmay_checker_private_enum_payload_name_is_duplicate(
    const MarimayChecker *checker,
    const MarimayCheckEnumVariantDeclaration *variant,
    MarimayCheckName name
);
 bool mmay_checker_private_resolve_enum_payload_item(
    MarimayChecker *checker,
    const MarimayCheckEnumDeclaration *declaration,
    MarimayCheckEnumVariantDeclaration *variant,
    const MarimayAstNode *node,
    bool has_name,
    bool *valid
);
 bool mmay_checker_private_resolve_enum_variants(
    MarimayChecker *checker,
    MarimayCheckEnumDeclaration *declaration
);
 bool mmay_checker_private_resolve_all_enum_variants(MarimayChecker *checker);
#endif
