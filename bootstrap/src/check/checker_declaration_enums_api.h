#ifndef CHECKER_DECLARATION_ENUMS_API_H
#define CHECKER_DECLARATION_ENUMS_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 bool ruda_checker_private_enum_variant_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckEnumDeclaration *declaration,
    RudaCheckName name
);
 bool ruda_checker_private_enum_payload_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckEnumVariantDeclaration *variant,
    RudaCheckName name
);
 bool ruda_checker_private_resolve_enum_payload_item(
    RudaChecker *checker,
    const RudaCheckEnumDeclaration *declaration,
    RudaCheckEnumVariantDeclaration *variant,
    const RudaAstNode *node,
    bool has_name,
    bool *valid
);
 bool ruda_checker_private_resolve_enum_variants(
    RudaChecker *checker,
    RudaCheckEnumDeclaration *declaration
);
 bool ruda_checker_private_resolve_all_enum_variants(RudaChecker *checker);
#endif
