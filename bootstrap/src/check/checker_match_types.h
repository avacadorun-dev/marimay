#ifndef RUDA_CHECK_CHECKER_MATCH_TYPES_H
#define RUDA_CHECK_CHECKER_MATCH_TYPES_H

#include "checker_function_context.h"

typedef struct RudaCheckEnumMatchBinding {
    RudaCheckName name;
    RudaTypeId type;
} RudaCheckEnumMatchBinding;

typedef struct RudaCheckEnumMatchAlternative {
    const RudaAstNode *pattern;
    RudaCheckEnumVariantDeclaration *variant;
    RudaCheckName catch_binding;
    size_t variant_index;
    bool is_catch_all;
    bool has_catch_binding;
} RudaCheckEnumMatchAlternative;

typedef struct RudaCheckEnumMatchArm {
    const RudaAstNode *node;
    const RudaAstNode *pattern;
    const RudaAstNode *guard;
    const RudaAstNode *body;
    RudaVector alternatives;
    RudaVector bindings;
    bool body_is_block;
} RudaCheckEnumMatchArm;

typedef struct RudaCheckScalarMatchAlternative {
    const RudaAstNode *pattern;
    RudaCheckName catch_binding;
    RudaMirStringId string_constant;
    int64_t integer;
    bool literal;
    bool is_catch_all;
    bool has_catch_binding;
} RudaCheckScalarMatchAlternative;

typedef struct RudaCheckScalarMatchArm {
    const RudaAstNode *node;
    const RudaAstNode *pattern;
    const RudaAstNode *guard;
    const RudaAstNode *body;
    RudaVector alternatives;
    RudaVector bindings;
    bool body_is_block;
} RudaCheckScalarMatchArm;

typedef struct RudaCheckRecordMatchAlternative {
    const RudaAstNode *pattern;
    RudaCheckName catch_binding;
    bool is_record_pattern;
    bool has_catch_binding;
} RudaCheckRecordMatchAlternative;

typedef struct RudaCheckRecordMatchArm {
    const RudaAstNode *node;
    const RudaAstNode *pattern;
    const RudaAstNode *guard;
    const RudaAstNode *body;
    RudaVector alternatives;
    RudaVector bindings;
    bool body_is_block;
} RudaCheckRecordMatchArm;


#endif
