#include "checker_expression_api.h"
#include "checker_expr_calls_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_expr_records_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_expr_interfaces_api.h"
#include "checker_lookup_api.h"
#include "checker_expr_try_api.h"
#include "checker_expr_numeric_api.h"





bool ruda_checker_private_compile_expression_raw(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaAstNode *inner = NULL;
    bool enum_resolved = false;
    if (result == NULL || node == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->register_id = RUDA_MIR_NO_REGISTER;
    if (node->kind == RUDA_SYNTAX_EXPRESSION
        || node->kind == RUDA_SYNTAX_PAREN_GROUP) {
        inner = ruda_checker_private_expression_only_node(node);
        if (inner == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        return ruda_checker_private_compile_expression_raw(
            function, inner, expected, result
        );
    }
    if (node->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        return ruda_checker_private_compile_name(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_LITERAL_EXPRESSION) {
        return ruda_checker_private_compile_literal(function, node, expected, result);
    }
    if (node->kind == RUDA_SYNTAX_ARRAY_EXPRESSION) {
        return ruda_compile_array_literal(function, node, expected, result);
    }
    if (node->kind == RUDA_SYNTAX_INDEX_EXPRESSION) {
        return ruda_compile_array_index(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_UNARY_EXPRESSION) {
        return ruda_checker_private_compile_unary(function, node, expected, result);
    }
    if (node->kind == RUDA_SYNTAX_BINARY_EXPRESSION) {
        return ruda_checker_private_compile_binary(function, node, expected, result);
    }
    if (node->kind == RUDA_SYNTAX_CALL_EXPRESSION) {
        return ruda_checker_private_compile_call(
            function, node, expected, result
        );
    }
    if (node->kind == RUDA_SYNTAX_RECORD_EXPRESSION) {
        if (!ruda_checker_private_compile_enum_named(
                function,
                node,
                expected,
                &enum_resolved,
                result
            )) {
            return false;
        }
        if (enum_resolved) {
            return true;
        }
        return ruda_checker_private_compile_record_construction(
            function, node, expected, result
        );
    }
    if (node->kind == RUDA_SYNTAX_MEMBER_EXPRESSION) {
        if (!ruda_checker_private_compile_enum_unit(
                function,
                node,
                expected,
                &enum_resolved,
                result
            )) {
            return false;
        }
        if (enum_resolved) {
            return true;
        }
        return ruda_checker_private_compile_record_member(function, node, result);
    }
    if (node->kind == RUDA_SYNTAX_MATCH_EXPRESSION) {
        return ruda_checker_private_compile_enum_match(
            function,
            node,
            expected,
            result
        );
    }
    if (node->kind == RUDA_SYNTAX_TRY_EXPRESSION) {
        return ruda_checker_private_compile_try_expression(
            function, node, expected, result
        );
    }
    if (node->kind == RUDA_SYNTAX_CONVERSION_EXPRESSION) {
        return ruda_checker_private_compile_integer_conversion(
            function, node, expected, result
        );
    }
    ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

bool ruda_compile_expression(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    return ruda_checker_private_compile_expression_raw(
        function, node, expected, result
    ) && ruda_checker_private_maybe_box_interface(
        function, node, expected, result
    );
}
