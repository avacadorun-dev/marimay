#ifndef MMAY_CHECK_CHECKER_MATCH_TYPES_H
#define MMAY_CHECK_CHECKER_MATCH_TYPES_H

#include "checker_function_context.h"

typedef struct MarimayCheckEnumMatchBinding {
    MarimayCheckName name;
    MarimayTypeId type;
} MarimayCheckEnumMatchBinding;

typedef struct MarimayCheckEnumMatchAlternative {
    const MarimayAstNode *pattern;
    MarimayCheckEnumVariantDeclaration *variant;
    MarimayTypeId enum_type;
    MarimayCheckName catch_binding;
    size_t variant_index;
    bool is_catch_all;
    bool has_catch_binding;
    bool has_refutable_nested_pattern;
} MarimayCheckEnumMatchAlternative;

typedef struct MarimayCheckEnumMatchArm {
    const MarimayAstNode *node;
    const MarimayAstNode *pattern;
    const MarimayAstNode *guard;
    const MarimayAstNode *body;
    MarimayVector alternatives;
    MarimayVector bindings;
    bool body_is_block;
} MarimayCheckEnumMatchArm;

typedef struct MarimayCheckScalarMatchAlternative {
    const MarimayAstNode *pattern;
    MarimayCheckName catch_binding;
    MarimayMirStringId string_constant;
    int64_t integer;
    bool literal;
    bool is_catch_all;
    bool has_catch_binding;
} MarimayCheckScalarMatchAlternative;

typedef struct MarimayCheckScalarMatchArm {
    const MarimayAstNode *node;
    const MarimayAstNode *pattern;
    const MarimayAstNode *guard;
    const MarimayAstNode *body;
    MarimayVector alternatives;
    MarimayVector bindings;
    bool body_is_block;
} MarimayCheckScalarMatchArm;

typedef struct MarimayCheckRecordMatchAlternative {
    const MarimayAstNode *pattern;
    MarimayCheckName catch_binding;
    bool is_record_pattern;
    bool has_catch_binding;
} MarimayCheckRecordMatchAlternative;

typedef struct MarimayCheckRecordMatchArm {
    const MarimayAstNode *node;
    const MarimayAstNode *pattern;
    const MarimayAstNode *guard;
    const MarimayAstNode *body;
    MarimayVector alternatives;
    MarimayVector bindings;
    bool body_is_block;
} MarimayCheckRecordMatchArm;


#endif
