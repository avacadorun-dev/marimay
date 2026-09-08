#include "checker_enum_bottom_api.h"
#include "checker_expr_try_api.h"

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_expr_map_error_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "checker_statement_api.h"

bool mmay_checker_private_compile_try_expression(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *operand_node = mmay_checker_private_check_child_node_any(
        node, 0
    );
    MarimayExpressionValue operand;
    MarimayCheckEnumDeclaration *result_enum = NULL;
    MarimayCheckEnumDeclaration concrete_result;
    const MarimayTypeDescriptor *operand_descriptor = NULL;
    const MarimayTypeDescriptor *result_descriptor = NULL;
    MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
    MarimayMirBlockId error_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId success_block = MMAY_MIR_NO_BLOCK;
    MarimayExpressionValue is_error;
    MarimayExpressionValue error_value;
    MarimayExpressionValue required_error;
    MarimayExpressionValue propagated;
    MarimayExpressionValue success_value;
    MarimayExpressionValue required_success;
    MarimayMirEnumArgument error_argument;
    size_t start = node != NULL ? node->start : 0;
    size_t end = node != NULL ? node->end : start;
    (void)expected;
    if (function == NULL || node == NULL || operand_node == NULL || result == NULL) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    memset(&operand, 0, sizeof(operand));
    memset(&is_error, 0, sizeof(is_error));
    memset(&error_value, 0, sizeof(error_value));
    memset(&required_error, 0, sizeof(required_error));
    memset(&propagated, 0, sizeof(propagated));
    memset(&success_value, 0, sizeof(success_value));
    memset(&required_success, 0, sizeof(required_success));
    memset(&error_argument, 0, sizeof(error_argument));
    {
        const MarimayAstNode *previous_operand = function->try_operand;
        MarimayTypeId previous_error = function->try_error_type;
        bool compiled;
        function->try_error_type = mmay_checker_private_map_error_operand_context(
            function, operand_node, &function->try_operand);
        compiled = mmay_compile_expression(function, operand_node, MMAY_TYPE_INVALID, &operand);
        function->try_operand = previous_operand;
        function->try_error_type = previous_error;
        if (!compiled) return false;
    }
    if (mmay_expression_is_never(function, &operand)) {
        *result = operand;
        return true;
    }
    operand_descriptor = mmay_type_descriptor(
        &function->checker->result->types, operand.type
    );
    result_descriptor = mmay_type_descriptor(
        &function->checker->result->types, function->result_type
    );
    result_enum = mmay_checker_private_checker_find_enum_by_type(
        function->checker, operand.type
    );
    if (operand_descriptor == NULL || result_descriptor == NULL
        || result_enum == NULL || !result_enum->is_builtin
        || result_enum->owned_name.length != sizeof("Result") - 1
        || memcmp(result_enum->owned_name.data, "Result", sizeof("Result") - 1) != 0
        || operand_descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || result_descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || operand_descriptor->argument_count != 2
        || result_descriptor->argument_count != 2
        || !mmay_symbol_id_equal(
            operand_descriptor->symbol, result_enum->symbol
        )
        || !mmay_symbol_id_equal(
            result_descriptor->symbol, result_enum->symbol
        )
        || operand_descriptor->arguments[1] != result_descriptor->arguments[1]) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "postfix '?' requires Result<T,E> and an enclosing Result<U,E> with the same E",
            start,
            end
        );
        return false;
    }
    concrete_result = *result_enum;
    concrete_result.type = operand.type;
    if (!mmay_checker_private_function_enum_layout(
            function, &concrete_result, &enum_id
        ) || !mmay_checker_private_function_emit_enum_access(
            function,
            MMAY_MIR_ENUM_IS_VARIANT,
            MMAY_TYPE_BOOL,
            operand.register_id,
            enum_id,
            1,
            0,
            start,
            end,
            &is_error
        ) || !mmay_checker_private_function_add_block(
            function, &error_block
        ) || !mmay_checker_private_function_add_block(
            function, &success_block
        ) || !mmay_checker_private_function_set_branch(
            function,
            function->block,
            is_error.register_id,
            error_block,
            success_block,
            start,
            end
        )) {
        return false;
    }
    function->block = error_block;
    function->terminated = false;
    if (operand_descriptor->arguments[1] == MMAY_TYPE_NEVER) {
        if (!mmay_enum_bottom_edge(function, start, end, &error_value)) return false;
    } else {
        if (!mmay_checker_private_function_emit_enum_access(
                function,
                MMAY_MIR_ENUM_REQUIRE_VARIANT,
                operand.type,
                operand.register_id,
                enum_id,
                1,
                0,
                start,
                end,
                &required_error
            ) || !mmay_checker_private_function_emit_enum_access(
                function,
                MMAY_MIR_ENUM_GET_PAYLOAD,
                operand_descriptor->arguments[1],
                required_error.register_id,
                enum_id,
                1,
                0,
                start,
                end,
                &error_value
            )) {
            return false;
        }
        concrete_result.type = function->result_type;
        error_argument.register_id = error_value.register_id;
        error_argument.type = error_value.type;
        if (!mmay_checker_private_function_emit_enum_construction(
                function,
                &concrete_result,
                1,
                &error_argument,
                1,
                start,
                end,
                &propagated
            ) || !mmay_checker_private_function_set_return(
                function, propagated.register_id, start, end
            )) {
            return false;
        }
    }
    function->block = success_block;
    function->terminated = false;
    if (!mmay_checker_private_function_emit_enum_access(
            function,
            MMAY_MIR_ENUM_REQUIRE_VARIANT,
            operand.type,
            operand.register_id,
            enum_id,
            0,
            0,
            start,
            end,
            &required_success
        )) {
        return false;
    }
    if (operand_descriptor->arguments[0] == MMAY_TYPE_NEVER) {
        return mmay_enum_bottom_edge(function, start, end, result);
    }
    if (operand_descriptor->arguments[0] == MMAY_TYPE_VOID) {
        result->type = MMAY_TYPE_VOID;
        result->register_id = MMAY_MIR_NO_REGISTER;
        result->valid = true;
        return true;
    }
    if (!mmay_checker_private_function_emit_enum_access(
            function,
            MMAY_MIR_ENUM_GET_PAYLOAD,
            operand_descriptor->arguments[0],
            required_success.register_id,
            enum_id,
            0,
            0,
            start,
            end,
            &success_value
        )) {
        return false;
    }
    *result = success_value;
    return true;
}
