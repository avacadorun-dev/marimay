#include "checker_expr_map_error_api.h"

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_expression_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"
#include "checker_statement_api.h"

static bool ruda_checker_private_map_error_call(
    RudaFunctionChecker *function,
    const RudaCheckCallable *mapper,
    RudaMirRegister argument_register,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirCallArgument argument;
    RudaMirCallId call = RUDA_MIR_NO_CALL;
    RudaMirInstruction instruction;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    if (mapper == NULL || result == NULL || mapper->parameter_count != 1) {
        return false;
    }
    if (function->mir.calls.length >= function->mir.options.max_calls
        || function->mir.call_arguments.length
            >= function->mir.options.max_call_arguments) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    argument.register_id = argument_register;
    argument.type = mapper->parameter_types[0];
    if (!ruda_mir_add_call(
            &function->mir,
            mapper->symbol,
            mapper->result_type,
            &argument,
            1,
            &call
        ) || !ruda_function_add_register(
            function, mapper->result_type, &target
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CALL;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.call = call;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = mapper->result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool ruda_checker_private_map_error_result_enum(
    RudaFunctionChecker *function,
    RudaTypeId type,
    RudaCheckEnumDeclaration **result
) {
    RudaCheckEnumDeclaration *declaration =
        ruda_checker_private_checker_find_enum_by_type(
            function->checker, type
        );
    const RudaTypeDescriptor *descriptor = ruda_type_descriptor(
        &function->checker->result->types, type
    );
    if (declaration == NULL || descriptor == NULL
        || descriptor->kind != RUDA_TYPE_KIND_NOMINAL
        || descriptor->argument_count != 2
        || declaration->type_parameters.length != 2) {
        return false;
    }
    *result = declaration;
    return true;
}

bool ruda_checker_private_compile_map_error(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *member = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const RudaAstNode *arguments_node = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_ARGUMENTS, 0
    );
    const RudaAstNode *receiver_node = NULL;
    const RudaAstNode *mapper_node = NULL;
    const RudaSyntaxToken *member_token = NULL;
    const RudaSyntaxToken *mapper_token = NULL;
    RudaCheckName mapper_name;
    RudaCheckCallable mapper;
    RudaCheckEnumDeclaration *input_enum = NULL;
    RudaCheckEnumDeclaration *output_enum = NULL;
    RudaCheckEnumDeclaration concrete_input;
    RudaCheckEnumDeclaration concrete_output;
    RudaExpressionValue receiver;
    RudaExpressionValue is_error;
    RudaExpressionValue required_input_error;
    RudaExpressionValue input_success;
    RudaExpressionValue required_input_success;
    RudaExpressionValue input_error;
    RudaExpressionValue mapped_error;
    RudaExpressionValue output_success;
    RudaExpressionValue output_error;
    RudaMirEnumArgument output_argument;
    RudaTypeId output_arguments[2];
    RudaTypeId output_type = RUDA_TYPE_INVALID;
    RudaMirEnumId input_layout = RUDA_MIR_NO_ENUM;
    RudaMirEnumId output_layout = RUDA_MIR_NO_ENUM;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirBlockId error_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId success_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId join_block = RUDA_MIR_NO_BLOCK;
    RudaCheckCallResolution resolution;
    size_t start = node != NULL ? node->start : 0;
    size_t end = node != NULL ? node->end : start;
    size_t argument_count = 0;
    memset(&required_input_error, 0, sizeof(required_input_error));
    memset(&required_input_success, 0, sizeof(required_input_success));
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (!ruda_checker_private_member_parts(
            member, &receiver_node, &member_token
        ) || member_token == NULL || !ruda_checker_private_check_text_equal(
            function->checker,
            member_token->start,
            member_token->end,
            "mapError"
        )) {
        return true;
    }
    *resolved = true;
    if (receiver_node == NULL || arguments_node == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (argument_count = 0; argument_count < arguments_node->child_count;
        argument_count += 1) {
        const RudaAstElement *element = ruda_ast_child(
            arguments_node, argument_count
        );
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (mapper_node != NULL) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_CALL_ARITY,
                    "mapError requires exactly one mapper argument",
                    arguments_node->start,
                    arguments_node->end
                );
                return false;
            }
            mapper_node = element->as.node;
        }
    }
    if (mapper_node == NULL || mapper_node->kind != RUDA_SYNTAX_EXPRESSION) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_CALL_ARITY,
            "mapError requires exactly one mapper argument",
            arguments_node->start,
            arguments_node->end
        );
        return false;
    }
    mapper_node = ruda_checker_private_expression_only_node(mapper_node);
    mapper_token = ruda_checker_private_check_child_token(
        mapper_node, RUDA_TOKEN_IDENTIFIER, 0
    );
    if (mapper_token == NULL || mapper_token->synthetic) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError mapper must be a visible function name",
            node->start,
            node->end
        );
        return false;
    }
    if (!ruda_compile_expression(
            function, receiver_node, RUDA_TYPE_INVALID, &receiver
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    input_enum = NULL;
    if (!ruda_checker_private_map_error_result_enum(
            function, receiver.type, &input_enum
        )) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError receiver must have type Result<T,E>",
            receiver_node->start,
            receiver_node->end
        );
        return false;
    }
    mapper_name.snapshot = function->checker->ast->snapshot;
    mapper_name.start = mapper_token->start;
    mapper_name.end = mapper_token->end;
    resolution = ruda_check_resolve_call(
        function->checker, mapper_name, false, RUDA_TYPE_INVALID, &mapper
    );
    if (resolution != RUDA_CHECK_CALL_FOUND || mapper.parameter_count != 1) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            resolution == RUDA_CHECK_CALL_UNKNOWN_NAME
                ? RUDA_DIAGNOSTIC_UNKNOWN_NAME : RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError mapper must be a unary visible function",
            mapper_name.start,
            mapper_name.end
        );
        return false;
    }
    if (!ruda_check_record_occurrence(
            function->checker, mapper_name, mapper.symbol
        )) {
        return false;
    }
    {
        const RudaTypeDescriptor *input_descriptor = ruda_type_descriptor(
            &function->checker->result->types, receiver.type
        );
        if (input_descriptor == NULL
            || mapper.parameter_types[0] != input_descriptor->arguments[1]) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "mapError mapper parameter must match the Result error type",
                mapper_name.start,
                mapper_name.end
            );
            return false;
        }
        output_arguments[0] = input_descriptor->arguments[0];
        output_arguments[1] = mapper.result_type;
    }
    if (!ruda_checker_private_intern_generic_nominal(
            function->checker,
            RUDA_TYPE_KIND_NOMINAL,
            input_enum->symbol,
            &(RudaVector){
                .data = output_arguments,
                .length = 2,
                .capacity = 2,
                .element_size = sizeof(RudaTypeId)
            },
            &output_type
        ) || (expected != RUDA_TYPE_INVALID && expected != output_type)) {
        if (expected != RUDA_TYPE_INVALID && expected != output_type) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "mapError result does not match the expected type",
                node->start,
                node->end
            );
        } else {
            ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        }
        return false;
    }
    if (!ruda_checker_private_map_error_result_enum(
            function, output_type, &output_enum
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    concrete_input = *input_enum;
    concrete_input.type = receiver.type;
    concrete_output = *output_enum;
    concrete_output.type = output_type;
    if (!ruda_checker_private_function_enum_layout(
            function, &concrete_input, &input_layout
        ) || !ruda_checker_private_function_enum_layout(
            function, &concrete_output, &output_layout
        ) || !ruda_checker_private_function_emit_enum_access(
            function, RUDA_MIR_ENUM_IS_VARIANT, RUDA_TYPE_BOOL,
            receiver.register_id, input_layout, 1, 0, start, end, &is_error
        ) || !ruda_checker_private_function_add_block(function, &error_block)
        || !ruda_checker_private_function_add_block(function, &success_block)
        || !ruda_checker_private_function_add_block(function, &join_block)
        || !ruda_checker_private_function_set_branch(
            function, function->block, is_error.register_id,
            error_block, success_block, start, end
        ) || !ruda_function_add_register(function, output_type, &target)) {
        return false;
    }
    function->block = error_block;
    function->terminated = false;
    if (!ruda_checker_private_function_emit_enum_access(
            function, RUDA_MIR_ENUM_REQUIRE_VARIANT,
            receiver.type, receiver.register_id, input_layout, 1, 0,
            start, end, &required_input_error
        ) || !ruda_checker_private_function_emit_enum_access(
            function, RUDA_MIR_ENUM_GET_PAYLOAD,
            ruda_type_descriptor(&function->checker->result->types, receiver.type)
                ->arguments[1], required_input_error.register_id, input_layout, 1, 0,
            start, end, &input_error
        ) || !ruda_checker_private_map_error_call(
            function, &mapper, input_error.register_id, start, end, &mapped_error
        )) {
        return false;
    }
    output_argument.register_id = mapped_error.register_id;
    output_argument.type = mapped_error.type;
    if (!ruda_checker_private_function_emit_enum_construction(
            function, &concrete_output, 1, &output_argument, 1,
            start, end, &output_error
        ) || !ruda_checker_private_function_emit_copy(
            function, target, output_error.register_id, start, end
        ) || !ruda_checker_private_function_set_goto(
            function, function->block, join_block, start, end
        )) {
        return false;
    }
    function->block = success_block;
    function->terminated = false;
    if (output_arguments[0] == RUDA_TYPE_VOID) {
        if (!ruda_checker_private_function_emit_enum_construction(
                function, &concrete_output, 0, NULL, 0,
                start, end, &output_success
            )) {
            return false;
        }
    } else if (!ruda_checker_private_function_emit_enum_access(
            function, RUDA_MIR_ENUM_REQUIRE_VARIANT,
            receiver.type, receiver.register_id, input_layout, 0, 0,
            start, end, &required_input_success
        ) || !ruda_checker_private_function_emit_enum_access(
            function, RUDA_MIR_ENUM_GET_PAYLOAD, output_arguments[0],
            required_input_success.register_id, input_layout, 0, 0, start, end,
            &input_success
        ) || !ruda_checker_private_function_emit_enum_construction(
            function, &concrete_output, 0,
            &(RudaMirEnumArgument){
                .register_id = input_success.register_id,
                .type = input_success.type
            },
            1, start, end, &output_success
        )) {
        return false;
    }
    if (!ruda_checker_private_function_emit_copy(
            function, target, output_success.register_id, start, end
        ) || !ruda_checker_private_function_set_goto(
            function, function->block, join_block, start, end
        )) {
        return false;
    }
    function->block = join_block;
    function->terminated = false;
    result->type = output_type;
    result->register_id = target;
    result->valid = true;
    return true;
}
