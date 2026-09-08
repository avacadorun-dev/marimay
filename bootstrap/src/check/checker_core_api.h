#ifndef CHECKER_CORE_API_H
#define CHECKER_CORE_API_H

#include "checker_internal.h"
#include "checker_function_context.h"

 bool mmay_checker_private_evaluate_constant(
    MarimayChecker *checker,
    MarimayCheckConstantDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

 bool mmay_checker_private_evaluate_type_alias(
    MarimayChecker *checker,
    MarimayCheckTypeAliasDeclaration *declaration,
    size_t use_start,
    size_t use_end
);

void mmay_checker_fail(MarimayChecker *checker, MarimayCheckStatus status);
bool mmay_checker_add_warning(MarimayChecker *checker, const char *code,
    const char *message, size_t start, size_t end);
void mmay_checker_fail_incomplete(
    MarimayChecker *checker,
    const MarimayAstNode *node,
    const char *construct
);
void mmay_checker_set_incomplete_context(
    MarimayChecker *checker,
    const MarimayAstNode *node
);
 void mmay_checker_private_expression_set_never(
    MarimayFunctionChecker *function,
    MarimayExpressionValue *result
);
 bool mmay_checker_private_checker_set_ast(
    MarimayChecker *checker,
    const MarimayAstTree *ast
);
 bool mmay_checker_private_check_span_valid(
    const MarimayChecker *checker,
    size_t start,
    size_t end
);
 bool mmay_checker_private_check_text_equal(
    const MarimayChecker *checker,
    size_t start,
    size_t end,
    const char *text
);
 bool mmay_checker_private_check_names_equal(
    const MarimayChecker *checker,
    MarimayCheckName left,
    MarimayCheckName right
);
 bool mmay_checker_private_check_name_bytes(
    MarimayCheckName name,
    const unsigned char **bytes,
    size_t *length
);
 const MarimayCheckTopLevelEntry *mmay_checker_private_checker_find_top_level(
    const MarimayChecker *checker,
    MarimayCheckName name
);
 MarimayCheckConstantDeclaration *mmay_checker_private_checker_find_constant(
    MarimayChecker *checker,
    MarimayCheckName name
);
 MarimayCheckTypeAliasDeclaration *mmay_checker_private_checker_find_type_alias(
    MarimayChecker *checker,
    MarimayCheckName name
);
 MarimayCheckRecordDeclaration *mmay_checker_private_checker_find_record(
    MarimayChecker *checker,
    MarimayCheckName name
);
 MarimayCheckRecordDeclaration *mmay_checker_private_checker_find_record_by_type(
    MarimayChecker *checker,
    MarimayTypeId type
);
 MarimayCheckRecordFieldDeclaration *mmay_checker_private_checker_find_record_field(
    MarimayChecker *checker,
    MarimayCheckRecordDeclaration *record,
    MarimayCheckName name,
    size_t *field_index
);
 MarimayCheckEnumDeclaration *mmay_checker_private_checker_find_enum(
    MarimayChecker *checker,
    MarimayCheckName name
);
 MarimayCheckEnumDeclaration *mmay_checker_private_checker_find_enum_by_type(
    MarimayChecker *checker,
    MarimayTypeId type
);
 MarimayCheckInterfaceDeclaration *mmay_checker_private_checker_find_interface(
    MarimayChecker *checker,
    MarimayCheckName name
);
 MarimayCheckInterfaceDeclaration *mmay_checker_private_checker_find_interface_by_type(
    MarimayChecker *checker,
    MarimayTypeId type
);
 MarimayCheckEnumVariantDeclaration *mmay_checker_private_checker_find_enum_variant(
    MarimayChecker *checker,
    MarimayCheckEnumDeclaration *enum_declaration,
    MarimayCheckName name,
    size_t *variant_index
);
 MarimayCheckEnumPayloadItemDeclaration *mmay_checker_private_checker_find_enum_payload(
    MarimayChecker *checker,
    MarimayCheckEnumVariantDeclaration *variant,
    MarimayCheckName name,
    size_t *payload_index
);
 const MarimayAstNode *mmay_checker_private_check_child_node(
    const MarimayAstNode *node,
    MarimaySyntaxKind kind,
    size_t ordinal
);
 const MarimayAstNode *mmay_checker_private_check_child_node_any(
    const MarimayAstNode *node,
    size_t ordinal
);
 const MarimaySyntaxToken *mmay_checker_private_check_child_token(
    const MarimayAstNode *node,
    MarimayTokenKind kind,
    size_t ordinal
);
 const MarimaySyntaxToken *mmay_checker_private_check_first_token(
    const MarimayAstNode *node
);
 const MarimaySyntaxToken *mmay_checker_private_check_attribute_name(
    const MarimayAstNode *attribute
);
 bool mmay_checker_private_check_attribute_is(
    const MarimayChecker *checker,
    const MarimayAstNode *attribute,
    const char *name
);
 bool mmay_checker_private_check_source_is_test_file(const MarimayChecker *checker);
 bool mmay_checker_private_checker_add_owned_diagnostic(
    MarimayChecker *checker,
    const char *code,
    const unsigned char *message,
    size_t message_length,
    size_t start,
    size_t end
);
#endif
