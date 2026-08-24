#include "checker_expression_api.h"
#include "checker_expr_calls_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_expr_records_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_expr_interfaces_api.h"
#include "checker_lookup_api.h"
#include "checker_expr_try_api.h"
#include "checker_expr_numeric_api.h"
#include "checker_expr_string_api.h"
#include "checker_expr_tuples_api.h"

bool mmay_checker_private_compile_expression_raw(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *inner = NULL;
    bool enum_resolved = false;
    if (result == NULL || node == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(result, 0, sizeof(*result));
    result->register_id = MMAY_MIR_NO_REGISTER;
    if (node->kind == MMAY_SYNTAX_EXPRESSION
        || node->kind == MMAY_SYNTAX_PAREN_GROUP) {
        inner = mmay_checker_private_expression_only_node(node);
        if (inner == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        return mmay_checker_private_compile_expression_raw(
            function, inner, expected, result
        );
    }
    if (node->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        return mmay_checker_private_compile_name(
            function,
            node,
            expected,
            result
        );
    }
    if (node->kind == MMAY_SYNTAX_LITERAL_EXPRESSION) {
        return mmay_checker_private_compile_literal(function, node, expected, result);
    }
    if (node->kind == MMAY_SYNTAX_INTERPOLATED_EXPRESSION) {
        return mmay_checker_private_compile_interpolation(
            function, node, expected, result
        );
    }
    if (node->kind == MMAY_SYNTAX_ARRAY_EXPRESSION) {
        return mmay_compile_array_literal(function, node, expected, result);
    }
    if (node->kind == MMAY_SYNTAX_TUPLE_EXPRESSION) {
        return mmay_checker_private_compile_tuple(
            function,
            node,
            expected,
            result
        );
    }
    if (node->kind == MMAY_SYNTAX_INDEX_EXPRESSION) {
        return mmay_compile_array_index(function, node, result);
    }
    if (node->kind == MMAY_SYNTAX_UNARY_EXPRESSION) {
        return mmay_checker_private_compile_unary(function, node, expected, result);
    }
    if (node->kind == MMAY_SYNTAX_BINARY_EXPRESSION) {
        return mmay_checker_private_compile_binary(function, node, expected, result);
    }
    if (node->kind == MMAY_SYNTAX_CALL_EXPRESSION) {
        return mmay_checker_private_compile_call(
            function, node, expected, result
        );
    }
    if (node->kind == MMAY_SYNTAX_RECORD_EXPRESSION) {
        if (!mmay_checker_private_compile_enum_named(
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
        return mmay_checker_private_compile_record_construction(
            function, node, expected, result
        );
    }
    if (node->kind == MMAY_SYNTAX_MEMBER_EXPRESSION) {
        if (!mmay_checker_private_compile_enum_unit(
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
        return mmay_checker_private_compile_record_member(function, node, result);
    }
    if (node->kind == MMAY_SYNTAX_MATCH_EXPRESSION) {
        return mmay_checker_private_compile_enum_match(
            function,
            node,
            expected,
            result
        );
    }
    if (node->kind == MMAY_SYNTAX_TRY_EXPRESSION) {
        return mmay_checker_private_compile_try_expression(
            function, node, expected, result
        );
    }
    if (node->kind == MMAY_SYNTAX_CONVERSION_EXPRESSION) {
        return mmay_checker_private_compile_integer_conversion(
            function, node, expected, result
        );
    }
    if (node->kind == MMAY_SYNTAX_RANGE_EXPRESSION) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "integer ranges are valid only as the direct iterable of a for loop",
            node->start,
            node->end
        );
        return false;
    }
    mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

bool mmay_compile_expression(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    return mmay_checker_private_compile_expression_raw(
        function, node, expected, result
    ) && mmay_checker_private_maybe_box_interface(
        function, node, expected, result
    );
}
