#ifndef CHECKER_CORE_API_H
#define CHECKER_CORE_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool ruda_checker_private_evaluate_constant(
    RudaChecker *checker,
    RudaCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

 bool ruda_checker_private_evaluate_type_alias(
    RudaChecker *checker,
    RudaCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

void ruda_checker_fail(RudaChecker *checker, RudaCheckStatus status);
 void ruda_checker_private_expression_set_never(
    RudaFunctionChecker *function,
    RudaExpressionValue *result
);
 bool ruda_checker_private_checker_set_ast(
    RudaChecker *checker,
    const RudaAstTree *ast
);
 bool ruda_checker_private_check_span_valid(
    const RudaChecker *checker,
    size_t start,
    size_t end
);
 bool ruda_checker_private_check_text_equal(
    const RudaChecker *checker,
    size_t start,
    size_t end,
    const char *text
);
 bool ruda_checker_private_check_names_equal(
    const RudaChecker *checker,
    RudaCheckName left,
    RudaCheckName right
);
 bool ruda_checker_private_check_name_bytes(
    RudaCheckName name,
    const unsigned char **bytes,
    size_t *length
);
 const RudaCheckTopLevelEntry *ruda_checker_private_checker_find_top_level(
    const RudaChecker *checker,
    RudaCheckName name
);
 RudaCheckConstantDeclaration *ruda_checker_private_checker_find_constant(
    RudaChecker *checker,
    RudaCheckName name
);
 RudaCheckTypeAliasDeclaration *ruda_checker_private_checker_find_type_alias(
    RudaChecker *checker,
    RudaCheckName name
);
 RudaCheckRecordDeclaration *ruda_checker_private_checker_find_record(
    RudaChecker *checker,
    RudaCheckName name
);
 RudaCheckRecordDeclaration *ruda_checker_private_checker_find_record_by_type(
    RudaChecker *checker,
    RudaTypeId type
);
 RudaCheckRecordFieldDeclaration *ruda_checker_private_checker_find_record_field(
    RudaChecker *checker,
    RudaCheckRecordDeclaration *record,
    RudaCheckName name,
    size_t *field_index
);
 RudaCheckEnumDeclaration *ruda_checker_private_checker_find_enum(
    RudaChecker *checker,
    RudaCheckName name
);
 RudaCheckEnumDeclaration *ruda_checker_private_checker_find_enum_by_type(
    RudaChecker *checker,
    RudaTypeId type
);
 RudaCheckInterfaceDeclaration *ruda_checker_private_checker_find_interface(
    RudaChecker *checker,
    RudaCheckName name
);
 RudaCheckInterfaceDeclaration *ruda_checker_private_checker_find_interface_by_type(
    RudaChecker *checker,
    RudaTypeId type
);
 RudaCheckEnumVariantDeclaration *ruda_checker_private_checker_find_enum_variant(
    RudaChecker *checker,
    RudaCheckEnumDeclaration *enum_declaration,
    RudaCheckName name,
    size_t *variant_index
);
 RudaCheckEnumPayloadItemDeclaration *ruda_checker_private_checker_find_enum_payload(
    RudaChecker *checker,
    RudaCheckEnumVariantDeclaration *variant,
    RudaCheckName name,
    size_t *payload_index
);
 const RudaAstNode *ruda_checker_private_check_child_node(
    const RudaAstNode *node,
    RudaSyntaxKind kind,
    size_t ordinal
);
 const RudaAstNode *ruda_checker_private_check_child_node_any(
    const RudaAstNode *node,
    size_t ordinal
);
 const RudaSyntaxToken *ruda_checker_private_check_child_token(
    const RudaAstNode *node,
    RudaTokenKind kind,
    size_t ordinal
);
 const RudaSyntaxToken *ruda_checker_private_check_first_token(
    const RudaAstNode *node
);
 const RudaSyntaxToken *ruda_checker_private_check_attribute_name(
    const RudaAstNode *attribute
);
 bool ruda_checker_private_check_attribute_is(
    const RudaChecker *checker,
    const RudaAstNode *attribute,
    const char *name
);
 bool ruda_checker_private_check_source_is_test_file(const RudaChecker *checker);
 bool ruda_checker_private_checker_add_owned_diagnostic(
    RudaChecker *checker,
    const char *code,
    const unsigned char *message,
    size_t message_length,
    size_t start,
    size_t end
);
#endif
