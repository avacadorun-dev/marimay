#include "checker_expr_try_api.h"

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "checker_statement_api.h"

bool ruda_checker_private_compile_try_expression(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaAstNode *operand_node = ruda_checker_private_check_child_node_any(
        node, 0
    );
    RudaExpressionValue operand;
    RudaCheckEnumDeclaration *result_enum = NULL;
    RudaCheckEnumDeclaration concrete_result;
    const RudaTypeDescriptor *operand_descriptor = NULL;
    const RudaTypeDescriptor *result_descriptor = NULL;
    RudaMirEnumId enum_id = RUDA_MIR_NO_ENUM;
    RudaMirBlockId error_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId success_block = RUDA_MIR_NO_BLOCK;
    RudaExpressionValue is_error;
    RudaExpressionValue error_value;
    RudaExpressionValue required_error;
    RudaExpressionValue propagated;
    RudaExpressionValue success_value;
    RudaExpressionValue required_success;
    RudaMirEnumArgument error_argument;
    size_t start = node != NULL ? node->start : 0;
    size_t end = node != NULL ? node->end : start;
    (void)expected;
    if (function == NULL || node == NULL || operand_node == NULL || result == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
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
    if (!ruda_compile_expression(
            function, operand_node, RUDA_TYPE_INVALID, &operand
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &operand)) {
        *result = operand;
        return true;
    }
    operand_descriptor = ruda_type_descriptor(
        &function->checker->result->types, operand.type
    );
    result_descriptor = ruda_type_descriptor(
        &function->checker->result->types, function->result_type
    );
    result_enum = ruda_checker_private_checker_find_enum_by_type(
        function->checker, operand.type
    );
    if (operand_descriptor == NULL || result_descriptor == NULL
        || result_enum == NULL
        || operand_descriptor->kind != RUDA_TYPE_KIND_NOMINAL
        || result_descriptor->kind != RUDA_TYPE_KIND_NOMINAL
        || operand_descriptor->argument_count != 2
        || result_descriptor->argument_count != 2
        || !ruda_symbol_id_equal(
            operand_descriptor->symbol, result_enum->symbol
        )
        || !ruda_symbol_id_equal(
            result_descriptor->symbol, result_enum->symbol
        )
        || operand_descriptor->arguments[1] != result_descriptor->arguments[1]) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "postfix '?' requires Result<T,E> and an enclosing Result<U,E> with the same E",
            start,
            end
        );
        return false;
    }
    concrete_result = *result_enum;
    concrete_result.type = operand.type;
    if (!ruda_checker_private_function_enum_layout(
            function, &concrete_result, &enum_id
        ) || !ruda_checker_private_function_emit_enum_access(
            function,
            RUDA_MIR_ENUM_IS_VARIANT,
            RUDA_TYPE_BOOL,
            operand.register_id,
            enum_id,
            1,
            0,
            start,
            end,
            &is_error
        ) || !ruda_checker_private_function_add_block(
            function, &error_block
        ) || !ruda_checker_private_function_add_block(
            function, &success_block
        ) || !ruda_checker_private_function_set_branch(
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
    if (!ruda_checker_private_function_emit_enum_access(
            function,
            RUDA_MIR_ENUM_REQUIRE_VARIANT,
            operand.type,
            operand.register_id,
            enum_id,
            1,
            0,
            start,
            end,
            &required_error
        ) || !ruda_checker_private_function_emit_enum_access(
            function,
            RUDA_MIR_ENUM_GET_PAYLOAD,
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
    if (!ruda_checker_private_function_emit_enum_construction(
            function,
            &concrete_result,
            1,
            &error_argument,
            1,
            start,
            end,
            &propagated
        ) || !ruda_checker_private_function_set_return(
            function, propagated.register_id, start, end
        )) {
        return false;
    }
    function->block = success_block;
    function->terminated = false;
    if (!ruda_checker_private_function_emit_enum_access(
            function,
            RUDA_MIR_ENUM_REQUIRE_VARIANT,
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
    if (operand_descriptor->arguments[0] == RUDA_TYPE_VOID) {
        result->type = RUDA_TYPE_VOID;
        result->register_id = RUDA_MIR_NO_REGISTER;
        result->valid = true;
        return true;
    }
    if (!ruda_checker_private_function_emit_enum_access(
            function,
            RUDA_MIR_ENUM_GET_PAYLOAD,
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
