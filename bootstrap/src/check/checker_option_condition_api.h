#ifndef MMAY_CHECKER_OPTION_CONDITION_API_H
#define MMAY_CHECKER_OPTION_CONDITION_API_H

#include "checker_function_context.h"

/* A checked header retained only until its successful lexical body is lowered. */
typedef struct MarimayOptionCondition {
    const MarimaySyntaxToken *name;
    MarimayExpressionValue option;
    MarimayTypeId payload_type;
    MarimayMirEnumId layout;
    bool mutable_binding;
} MarimayOptionCondition;

const MarimayAstNode *mmay_checker_private_condition_node(const MarimayAstNode *statement);
bool mmay_checker_private_compile_option_condition(
    MarimayFunctionChecker *function, const MarimayAstNode *condition,
    MarimayExpressionValue *test, MarimayOptionCondition *binding
);
bool mmay_checker_private_compile_condition_body(
    MarimayFunctionChecker *function, const MarimayAstNode *body,
    const MarimayOptionCondition *binding
);

#endif
