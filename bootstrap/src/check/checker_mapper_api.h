#ifndef MMAY_CHECKER_MAPPER_API_H
#define MMAY_CHECKER_MAPPER_API_H
#include "checker_function_context.h"

typedef struct MarimayErrorMapper {
    MarimayExpressionValue value;
    MarimayCheckEnumDeclaration constructor;
    MarimayTypeId result_type;
    size_t variant;
    bool is_constructor;
} MarimayErrorMapper;

bool mmay_error_mapper_prepare(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId input, MarimayTypeId context, MarimayErrorMapper *mapper);
bool mmay_error_mapper_invoke(MarimayFunctionChecker *function, const MarimayAstNode *node,
    const MarimayErrorMapper *mapper, MarimayExpressionValue argument, MarimayExpressionValue *result);
#endif
