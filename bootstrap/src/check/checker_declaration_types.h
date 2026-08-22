#ifndef MMAY_CHECK_CHECKER_DECLARATION_TYPES_H
#define MMAY_CHECK_CHECKER_DECLARATION_TYPES_H

#include "mmay/check/checker.h"

typedef struct MarimayCheckName {
    const MarimaySourceSnapshot *snapshot;
    size_t start;
    size_t end;
} MarimayCheckName;

typedef struct MarimayCheckTypeParameterDeclaration {
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimayTypeId type;
    MarimayVector constraints;
    bool name_initialized;
    bool constraints_initialized;
} MarimayCheckTypeParameterDeclaration;

typedef enum MarimayCheckConstantState {
    MMAY_CHECK_CONSTANT_UNEVALUATED,
    MMAY_CHECK_CONSTANT_EVALUATING,
    MMAY_CHECK_CONSTANT_EVALUATED,
    MMAY_CHECK_CONSTANT_INVALID
} MarimayCheckConstantState;

typedef struct MarimayCheckConstantDeclaration {
    const MarimayAstTree *ast;
    const MarimayAstNode *node;
    const MarimayAstNode *initializer;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    MarimayMirValue value;
    MarimayCheckConstantState state;
    bool name_initialized;
    bool declaration_valid;
    bool is_public;
} MarimayCheckConstantDeclaration;

typedef enum MarimayCheckTypeAliasState {
    MMAY_CHECK_TYPE_ALIAS_UNRESOLVED,
    MMAY_CHECK_TYPE_ALIAS_RESOLVING,
    MMAY_CHECK_TYPE_ALIAS_RESOLVED,
    MMAY_CHECK_TYPE_ALIAS_INVALID
} MarimayCheckTypeAliasState;

typedef struct MarimayCheckTypeAliasDeclaration {
    const MarimayAstTree *ast;
    const MarimayAstNode *node;
    const MarimayAstNode *target;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    MarimayCheckTypeAliasState state;
    bool name_initialized;
    bool declaration_valid;
    bool is_public;
} MarimayCheckTypeAliasDeclaration;

typedef struct MarimayCheckRecordFieldDeclaration {
    const MarimayAstNode *node;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimayTypeId type;
    bool name_initialized;
    bool is_public;
    bool is_mutable;
} MarimayCheckRecordFieldDeclaration;

typedef struct MarimayCheckRecordDeclaration {
    const MarimayAstTree *ast;
    const MarimayAstNode *node;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    MarimayVector fields;
    MarimayVector type_parameters;
    bool name_initialized;
    bool fields_initialized;
    bool is_public;
    bool declaration_valid;
    bool fields_resolved;
    bool type_parameters_initialized;
    bool is_generic;
} MarimayCheckRecordDeclaration;

typedef struct MarimayCheckEnumPayloadItemDeclaration {
    const MarimayAstNode *node;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimayTypeId type;
    bool name_initialized;
    bool has_name;
} MarimayCheckEnumPayloadItemDeclaration;

typedef struct MarimayCheckEnumVariantDeclaration {
    const MarimayAstNode *node;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimayCheckedEnumVariantKind kind;
    MarimayVector payload;
    bool name_initialized;
    bool payload_initialized;
} MarimayCheckEnumVariantDeclaration;

typedef struct MarimayCheckEnumDeclaration {
    const MarimayAstTree *ast;
    const MarimayAstNode *node;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    MarimayVector variants;
    MarimayVector type_parameters;
    bool name_initialized;
    bool variants_initialized;
    bool is_public;
    bool declaration_valid;
    bool variants_resolved;
    bool type_parameters_initialized;
    bool is_generic;
    bool is_builtin;
} MarimayCheckEnumDeclaration;

typedef struct MarimayCheckInterfaceMethodDeclaration {
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    const MarimayAstNode *node;
    MarimayVector parameter_types;
    MarimayTypeId result_type;
    bool name_initialized;
    bool parameter_types_initialized;
} MarimayCheckInterfaceMethodDeclaration;

typedef struct MarimayCheckInterfaceDeclaration {
    const MarimayAstTree *ast;
    const MarimayAstNode *node;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    MarimayVector methods;
    bool name_initialized;
    bool methods_initialized;
    bool methods_resolved;
    bool declaration_valid;
    bool is_public;
} MarimayCheckInterfaceDeclaration;

typedef struct MarimayCheckFunctionDeclaration {
    const MarimayAstTree *ast;
    const MarimayAstNode *node;
    const MarimayAstNode *block;
    MarimayCheckName name;
    MarimayOwnedBytes owned_name;
    MarimayVector parameter_names;
    MarimayVector parameter_types;
    MarimayVector type_parameters;
    MarimayVector generic_arguments;
    MarimaySymbolId symbol;
    MarimaySymbolId receiver_symbol;
    MarimayTypeId result_type;
    MarimayTypeId signature_type;
    MarimayTypeId receiver_type;
    bool name_initialized;
    bool parameter_names_initialized;
    bool parameter_types_initialized;
    bool signature_valid;
    bool dot_eligible;
    bool is_public;
    bool is_unsafe;
    bool is_external;
    bool is_test;
    bool is_fuzz;
    bool type_parameters_initialized;
    bool generic_arguments_initialized;
    bool is_generic;
    bool is_instance;
    size_t generic_base_index;
} MarimayCheckFunctionDeclaration;

typedef enum MarimayCheckTopLevelKind {
    MMAY_CHECK_TOP_LEVEL_FUNCTION,
    MMAY_CHECK_TOP_LEVEL_CONSTANT,
    MMAY_CHECK_TOP_LEVEL_TYPE_ALIAS,
    MMAY_CHECK_TOP_LEVEL_RECORD,
    MMAY_CHECK_TOP_LEVEL_ENUM,
    MMAY_CHECK_TOP_LEVEL_INTERFACE
} MarimayCheckTopLevelKind;

typedef struct MarimayCheckTopLevelEntry {
    MarimayCheckTopLevelKind kind;
    size_t index;
    size_t function_count;
} MarimayCheckTopLevelEntry;

typedef enum MarimayCheckCallResolution {
    MMAY_CHECK_CALL_FOUND,
    MMAY_CHECK_CALL_UNKNOWN_NAME,
    MMAY_CHECK_CALL_NOT_FUNCTION,
    MMAY_CHECK_CALL_RECEIVER_REQUIRED,
    MMAY_CHECK_CALL_RECEIVER_NOT_OWNED,
    MMAY_CHECK_CALL_NOT_DOT_ELIGIBLE,
    MMAY_CHECK_CALL_OTHER_NOMINAL,
    MMAY_CHECK_CALL_UNKNOWN_IMPORT,
    MMAY_CHECK_CALL_PRIVATE_IMPORT,
    MMAY_CHECK_CALL_INTERNAL_ERROR
} MarimayCheckCallResolution;

typedef struct MarimayCheckSymbolSeed {
    MarimayCheckName name;
    MarimaySymbolId symbol;
    MarimayTypeId type;
    MarimayCheckedSymbolKind kind;
    bool is_public;
} MarimayCheckSymbolSeed;

#endif
