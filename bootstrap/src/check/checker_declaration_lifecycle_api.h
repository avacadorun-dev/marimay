#ifndef CHECKER_DECLARATION_LIFECYCLE_API_H
#define CHECKER_DECLARATION_LIFECYCLE_API_H

#include "checker_internal.h"
#include "checker_package_context.h"

void mmay_checker_private_function_declaration_dispose(
    MarimayCheckFunctionDeclaration *declaration
);
void mmay_checker_private_type_parameter_vector_dispose(MarimayVector *parameters);
 void mmay_checker_private_constant_declaration_dispose(
    MarimayCheckConstantDeclaration *declaration
);
 void mmay_checker_private_type_alias_declaration_dispose(
    MarimayCheckTypeAliasDeclaration *declaration
);
 void mmay_checker_private_record_field_declaration_dispose(
    MarimayCheckRecordFieldDeclaration *field
);
 void mmay_checker_private_record_declaration_dispose(
    MarimayCheckRecordDeclaration *declaration
);
 void mmay_checker_private_enum_payload_item_declaration_dispose(
    MarimayCheckEnumPayloadItemDeclaration *item
);
 void mmay_checker_private_enum_variant_declaration_dispose(
    MarimayCheckEnumVariantDeclaration *variant
);
 void mmay_checker_private_enum_declaration_dispose(
    MarimayCheckEnumDeclaration *declaration
);
 void mmay_checker_private_interface_method_declaration_dispose(
    MarimayCheckInterfaceMethodDeclaration *method
);
 void mmay_checker_private_interface_declaration_dispose(
    MarimayCheckInterfaceDeclaration *declaration
);
 bool mmay_checker_private_declaration_name_is_duplicate(
    const MarimayChecker *checker,
    MarimayCheckName name
);
 bool mmay_checker_private_parameter_name_is_duplicate(
    const MarimayChecker *checker,
    const MarimayCheckFunctionDeclaration *declaration,
    MarimayCheckName name
);
 bool mmay_checker_private_check_name_is_primitive_type(
    const MarimayChecker *checker,
    MarimayCheckName name
);
 bool mmay_checker_private_check_public_type_reference(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
#endif
