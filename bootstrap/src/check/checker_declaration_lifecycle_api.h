#ifndef CHECKER_DECLARATION_LIFECYCLE_API_H
#define CHECKER_DECLARATION_LIFECYCLE_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

 void ruda_checker_private_function_declaration_dispose(
    RudaCheckFunctionDeclaration *declaration
);
 void ruda_checker_private_constant_declaration_dispose(
    RudaCheckConstantDeclaration *declaration
);
 void ruda_checker_private_type_alias_declaration_dispose(
    RudaCheckTypeAliasDeclaration *declaration
);
 void ruda_checker_private_record_field_declaration_dispose(
    RudaCheckRecordFieldDeclaration *field
);
 void ruda_checker_private_record_declaration_dispose(
    RudaCheckRecordDeclaration *declaration
);
 void ruda_checker_private_enum_payload_item_declaration_dispose(
    RudaCheckEnumPayloadItemDeclaration *item
);
 void ruda_checker_private_enum_variant_declaration_dispose(
    RudaCheckEnumVariantDeclaration *variant
);
 void ruda_checker_private_enum_declaration_dispose(
    RudaCheckEnumDeclaration *declaration
);
 bool ruda_checker_private_declaration_name_is_duplicate(
    const RudaChecker *checker,
    RudaCheckName name
);
 bool ruda_checker_private_parameter_name_is_duplicate(
    const RudaChecker *checker,
    const RudaCheckFunctionDeclaration *declaration,
    RudaCheckName name
);
 bool ruda_checker_private_check_name_is_primitive_type(
    const RudaChecker *checker,
    RudaCheckName name
);
 bool ruda_checker_private_check_public_type_reference(
    RudaChecker *checker,
    const RudaAstNode *node
);
#endif
