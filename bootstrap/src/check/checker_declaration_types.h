#ifndef RUDA_CHECK_CHECKER_DECLARATION_TYPES_H
#define RUDA_CHECK_CHECKER_DECLARATION_TYPES_H

#include "ruda/check/checker.h"

typedef struct RudaCheckName {
    const RudaSourceSnapshot *snapshot;
    size_t start;
    size_t end;
} RudaCheckName;

typedef enum RudaCheckConstantState {
    RUDA_CHECK_CONSTANT_UNEVALUATED,
    RUDA_CHECK_CONSTANT_EVALUATING,
    RUDA_CHECK_CONSTANT_EVALUATED,
    RUDA_CHECK_CONSTANT_INVALID
} RudaCheckConstantState;

typedef struct RudaCheckConstantDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    const RudaAstNode *initializer;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaMirValue value;
    RudaCheckConstantState state;
    bool name_initialized;
    bool declaration_valid;
    bool is_public;
} RudaCheckConstantDeclaration;

typedef enum RudaCheckTypeAliasState {
    RUDA_CHECK_TYPE_ALIAS_UNRESOLVED,
    RUDA_CHECK_TYPE_ALIAS_RESOLVING,
    RUDA_CHECK_TYPE_ALIAS_RESOLVED,
    RUDA_CHECK_TYPE_ALIAS_INVALID
} RudaCheckTypeAliasState;

typedef struct RudaCheckTypeAliasDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    const RudaAstNode *target;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaCheckTypeAliasState state;
    bool name_initialized;
    bool declaration_valid;
    bool is_public;
} RudaCheckTypeAliasDeclaration;

typedef struct RudaCheckRecordFieldDeclaration {
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaTypeId type;
    bool name_initialized;
    bool is_public;
    bool is_mutable;
} RudaCheckRecordFieldDeclaration;

typedef struct RudaCheckRecordDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaVector fields;
    bool name_initialized;
    bool fields_initialized;
    bool is_public;
    bool declaration_valid;
    bool fields_resolved;
} RudaCheckRecordDeclaration;

typedef struct RudaCheckEnumPayloadItemDeclaration {
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaTypeId type;
    bool name_initialized;
    bool has_name;
} RudaCheckEnumPayloadItemDeclaration;

typedef struct RudaCheckEnumVariantDeclaration {
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaCheckedEnumVariantKind kind;
    RudaVector payload;
    bool name_initialized;
    bool payload_initialized;
} RudaCheckEnumVariantDeclaration;

typedef struct RudaCheckEnumDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaVector variants;
    bool name_initialized;
    bool variants_initialized;
    bool is_public;
    bool declaration_valid;
    bool variants_resolved;
} RudaCheckEnumDeclaration;

typedef struct RudaCheckFunctionDeclaration {
    const RudaAstTree *ast;
    const RudaAstNode *node;
    const RudaAstNode *block;
    RudaCheckName name;
    RudaOwnedBytes owned_name;
    RudaVector parameter_names;
    RudaVector parameter_types;
    RudaSymbolId symbol;
    RudaSymbolId receiver_symbol;
    RudaTypeId result_type;
    RudaTypeId signature_type;
    RudaTypeId receiver_type;
    bool name_initialized;
    bool parameter_names_initialized;
    bool parameter_types_initialized;
    bool signature_valid;
    bool dot_eligible;
    bool is_public;
    bool is_test;
    bool is_fuzz;
} RudaCheckFunctionDeclaration;

typedef enum RudaCheckTopLevelKind {
    RUDA_CHECK_TOP_LEVEL_FUNCTION,
    RUDA_CHECK_TOP_LEVEL_CONSTANT,
    RUDA_CHECK_TOP_LEVEL_TYPE_ALIAS,
    RUDA_CHECK_TOP_LEVEL_RECORD,
    RUDA_CHECK_TOP_LEVEL_ENUM
} RudaCheckTopLevelKind;

typedef struct RudaCheckTopLevelEntry {
    RudaCheckTopLevelKind kind;
    size_t index;
    size_t function_count;
} RudaCheckTopLevelEntry;

typedef enum RudaCheckCallResolution {
    RUDA_CHECK_CALL_FOUND,
    RUDA_CHECK_CALL_UNKNOWN_NAME,
    RUDA_CHECK_CALL_NOT_FUNCTION,
    RUDA_CHECK_CALL_RECEIVER_REQUIRED,
    RUDA_CHECK_CALL_RECEIVER_NOT_OWNED,
    RUDA_CHECK_CALL_NOT_DOT_ELIGIBLE,
    RUDA_CHECK_CALL_OTHER_NOMINAL,
    RUDA_CHECK_CALL_INTERNAL_ERROR
} RudaCheckCallResolution;

typedef struct RudaCheckSymbolSeed {
    RudaCheckName name;
    RudaSymbolId symbol;
    RudaTypeId type;
    RudaCheckedSymbolKind kind;
    bool is_public;
} RudaCheckSymbolSeed;

#endif
