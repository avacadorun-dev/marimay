#include "checker_expr_map_error_api.h"

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_expression_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"
#include "package_imports_api.h"
#include "checker_statement_api.h"

static bool mmay_checker_private_map_error_closure_parts(
    MarimayFunctionChecker *function,
    const MarimayAstNode *closure,
    MarimayCheckName *parameter_name,
    MarimayTypeId *declared_parameter_type,
    MarimayTypeId *declared_result_type,
    const MarimayAstNode **body
) {
    const MarimayAstNode *parameters = mmay_checker_private_check_child_node(
        closure, MMAY_SYNTAX_PARAMETERS, 0
    );
    const MarimayAstNode *parameter = mmay_checker_private_check_child_node(
        parameters, MMAY_SYNTAX_PARAMETER, 0
    );
    const MarimayAstNode *parameter_type = mmay_checker_private_check_child_node(
        parameter, MMAY_SYNTAX_TYPE, 0
    );
    const MarimayAstNode *result_type = mmay_checker_private_check_child_node(
        closure, MMAY_SYNTAX_TYPE, 0
    );
    const MarimaySyntaxToken *name = mmay_checker_private_check_child_token(
        parameter, MMAY_TOKEN_IDENTIFIER, 0
    );
    if (parameters == NULL || parameter == NULL || name == NULL
        || name->synthetic || mmay_checker_private_check_child_node(
            parameters, MMAY_SYNTAX_PARAMETER, 1
        ) != NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CALL_ARITY,
            "mapError closure requires exactly one parameter",
            closure->start,
            closure->end
        );
        return false;
    }
    *body = mmay_checker_private_check_child_node(
        closure, MMAY_SYNTAX_EXPRESSION, 0
    );
    if (*body == NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError contextual closure must have an expression body",
            closure->start,
            closure->end
        );
        return false;
    }
    parameter_name->snapshot = function->checker->ast->snapshot;
    parameter_name->start = name->start;
    parameter_name->end = name->end;
    *declared_parameter_type = MMAY_TYPE_INVALID;
    *declared_result_type = MMAY_TYPE_INVALID;
    return (parameter_type == NULL || mmay_checker_private_check_exact_type(
            function->checker, parameter_type, declared_parameter_type
        )) && (result_type == NULL || mmay_checker_private_check_exact_type(
            function->checker, result_type, declared_result_type
        ));
}

static bool mmay_checker_private_map_error_compile_closure(
    MarimayFunctionChecker *function,
    MarimayCheckName parameter_name,
    const MarimayAstNode *body,
    MarimayTypeId parameter_type,
    MarimayTypeId result_type,
    MarimayExpressionValue argument,
    MarimayExpressionValue *result
) {
    MarimayCheckLocal *local = NULL;
    size_t previous_depth = function->scope_depth;
    bool accepted = false;
    function->scope_depth = previous_depth + 1;
    if (mmay_checker_private_function_add_local(
            function, parameter_name, parameter_type, false, true, &local
        ) && local != NULL
        && mmay_checker_private_function_emit_copy(
            function, local->register_id, argument.register_id,
            body->start, body->end
        ) && mmay_compile_expression(function, body, result_type, result)
        && result->type == result_type) {
        accepted = true;
    } else if (!mmay_checker_has_failed(function->checker)
        && (result == NULL || result->type != result_type)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError closure result does not match its contextual type",
            body->start,
            body->end
        );
    }
    function->scope_depth = previous_depth;
    return accepted;
}

static bool mmay_checker_private_map_error_call(
    MarimayFunctionChecker *function,
    const MarimayCheckCallable *mapper,
    MarimayMirRegister argument_register,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirCallArgument argument;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirInstruction instruction;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    if (mapper == NULL || result == NULL || mapper->parameter_count != 1) {
        return false;
    }
    if (function->mir.calls.length >= function->mir.options.max_calls
        || function->mir.call_arguments.length
            >= function->mir.options.max_call_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    argument.register_id = argument_register;
    argument.type = mapper->parameter_types[0];
    if (!mmay_mir_add_call(
            &function->mir,
            mapper->symbol,
            mapper->result_type,
            &argument,
            1,
            &call
        ) || !mmay_function_add_register(
            function, mapper->result_type, &target
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CALL;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.call = call;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = mapper->result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool mmay_checker_private_map_error_result_enum(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayCheckEnumDeclaration **result
) {
    MarimayCheckEnumDeclaration *declaration =
        mmay_checker_private_checker_find_enum_by_type(
            function->checker, type
        );
    const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(
        &function->checker->result->types, type
    );
    if (declaration == NULL || descriptor == NULL
        || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || descriptor->argument_count != 2
        || declaration->type_parameters.length != 2) {
        return false;
    }
    *result = declaration;
    return true;
}

bool mmay_checker_private_compile_map_error(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *member = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const MarimayAstNode *arguments_node = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    const MarimayAstNode *receiver_node = NULL;
    const MarimayAstNode *mapper_node = NULL;
    const MarimayAstNode *closure_body = NULL;
    const MarimaySyntaxToken *member_token = NULL;
    const MarimaySyntaxToken *mapper_token = NULL;
    MarimayCheckName mapper_name;
    MarimayCheckName mapper_namespace;
    MarimayCheckName closure_parameter_name;
    MarimayCheckCallable mapper;
    MarimayCheckEnumDeclaration *mapper_enum = NULL;
    MarimayCheckEnumVariantDeclaration *mapper_variant = NULL;
    MarimayCheckEnumDeclaration concrete_mapper;
    MarimayCheckEnumDeclaration *input_enum = NULL;
    MarimayCheckEnumDeclaration *output_enum = NULL;
    MarimayCheckEnumDeclaration concrete_input;
    MarimayCheckEnumDeclaration concrete_output;
    MarimayExpressionValue receiver;
    MarimayExpressionValue is_error;
    MarimayExpressionValue required_input_error;
    MarimayExpressionValue input_success;
    MarimayExpressionValue required_input_success;
    MarimayExpressionValue input_error;
    MarimayExpressionValue mapped_error;
    MarimayExpressionValue output_success;
    MarimayExpressionValue output_error;
    MarimayMirEnumArgument output_argument;
    MarimayTypeId output_arguments[2];
    MarimayTypeId mapper_parameter_type = MMAY_TYPE_INVALID;
    MarimayTypeId mapper_result_type = MMAY_TYPE_INVALID;
    MarimayTypeId closure_parameter_type = MMAY_TYPE_INVALID;
    MarimayTypeId closure_result_type = MMAY_TYPE_INVALID;
    MarimayTypeId expected_error_type = MMAY_TYPE_INVALID;
    MarimayTypeId output_type = MMAY_TYPE_INVALID;
    MarimayMirEnumId input_layout = MMAY_MIR_NO_ENUM;
    MarimayMirEnumId output_layout = MMAY_MIR_NO_ENUM;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirBlockId error_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId success_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId join_block = MMAY_MIR_NO_BLOCK;
    MarimayCheckCallResolution resolution;
    size_t start = node != NULL ? node->start : 0;
    size_t end = node != NULL ? node->end : start;
    size_t argument_count = 0;
    size_t mapper_variant_index = 0;
    bool mapper_enum_resolved = false;
    bool mapper_closure = false;
    bool mapper_qualified = false;
    memset(&required_input_error, 0, sizeof(required_input_error));
    memset(&required_input_success, 0, sizeof(required_input_success));
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (!mmay_checker_private_member_parts(
            member, &receiver_node, &member_token
        ) || member_token == NULL || !mmay_checker_private_check_text_equal(
            function->checker,
            member_token->start,
            member_token->end,
            "mapError"
        )) {
        return true;
    }
    *resolved = true;
    if (receiver_node == NULL || arguments_node == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (argument_count = 0; argument_count < arguments_node->child_count;
        argument_count += 1) {
        const MarimayAstElement *element = mmay_ast_child(
            arguments_node, argument_count
        );
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (mapper_node != NULL) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_CALL_ARITY,
                    "mapError requires exactly one mapper argument",
                    arguments_node->start,
                    arguments_node->end
                );
                return false;
            }
            mapper_node = element->as.node;
        }
    }
    if (mapper_node == NULL || mapper_node->kind != MMAY_SYNTAX_EXPRESSION) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CALL_ARITY,
            "mapError requires exactly one mapper argument",
            arguments_node->start,
            arguments_node->end
        );
        return false;
    }
    mapper_node = mmay_checker_private_expression_only_node(mapper_node);
    mapper_token = mmay_checker_private_check_child_token(
        mapper_node, MMAY_TOKEN_IDENTIFIER, 0
    );
    if (mapper_node != NULL
        && mapper_node->kind == MMAY_SYNTAX_MEMBER_EXPRESSION) {
        const MarimayAstNode *namespace_node = NULL;
        const MarimaySyntaxToken *namespace_token = NULL;
        if (mmay_checker_private_member_parts(
                mapper_node, &namespace_node, &mapper_token
            ) && namespace_node != NULL
            && namespace_node->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
            namespace_token = mmay_checker_private_check_child_token(
                namespace_node, MMAY_TOKEN_IDENTIFIER, 0
            );
        }
        if (namespace_token != NULL && !namespace_token->synthetic
            && mmay_check_import_namespace_exists(
                function->checker,
                function->checker->source + namespace_token->start,
                namespace_token->end - namespace_token->start
            )) {
            mapper_namespace.snapshot = function->checker->ast->snapshot;
            mapper_namespace.start = namespace_token->start;
            mapper_namespace.end = namespace_token->end;
            mapper_qualified = true;
        }
    }
    if (!mmay_compile_expression(
            function, receiver_node, MMAY_TYPE_INVALID, &receiver
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    input_enum = NULL;
    if (!mmay_checker_private_map_error_result_enum(
            function, receiver.type, &input_enum
        )) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError receiver must have type Result<T,E>",
            receiver_node->start,
            receiver_node->end
        );
        return false;
    }
    {
        const MarimayTypeDescriptor *expected_descriptor =
            expected != MMAY_TYPE_INVALID
            ? mmay_type_descriptor(
                &function->checker->result->types,
                expected
            )
            : NULL;
        if (expected_descriptor != NULL
            && expected_descriptor->kind == MMAY_TYPE_KIND_NOMINAL
            && expected_descriptor->argument_count == 2
            && mmay_symbol_id_equal(
                expected_descriptor->symbol,
                input_enum->symbol
            )) {
            expected_error_type = expected_descriptor->arguments[1];
        }
    }
    if (mapper_node != NULL
        && mapper_node->kind == MMAY_SYNTAX_CLOSURE_EXPRESSION) {
        mapper_closure = true;
        if (!mmay_checker_private_map_error_closure_parts(
                function,
                mapper_node,
                &closure_parameter_name,
                &closure_parameter_type,
                &closure_result_type,
                &closure_body
            )) {
            return false;
        }
        mapper_result_type = closure_result_type != MMAY_TYPE_INVALID
            ? closure_result_type
            : expected_error_type;
        if (mapper_result_type == MMAY_TYPE_INVALID) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "mapError closure with an omitted result type requires an expected Result type",
                mapper_node->start,
                mapper_node->end
            );
            return false;
        }
    } else if (mapper_node != NULL
        && mapper_node->kind == MMAY_SYNTAX_MEMBER_EXPRESSION
        && !mapper_qualified) {
        const MarimayCheckEnumPayloadItemDeclaration *payload = NULL;
        const MarimayTypeDescriptor *mapper_descriptor = NULL;
        if (!mmay_checker_private_resolve_enum_member(
                function,
                mapper_node,
                expected_error_type,
                &mapper_enum,
                &mapper_variant,
                &mapper_variant_index,
                &mapper_enum_resolved
            )) {
            return false;
        }
        if (!mapper_enum_resolved || mapper_enum == NULL
            || mapper_variant == NULL
            || mapper_variant->kind
                != MMAY_CHECKED_ENUM_VARIANT_POSITIONAL
            || mapper_variant->payload.length != 1) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "mapError enum mapper must be a positional constructor with one payload",
                mapper_node->start,
                mapper_node->end
            );
            return false;
        }
        concrete_mapper = *mapper_enum;
        if (mapper_enum->is_generic) {
            mapper_descriptor = expected_error_type != MMAY_TYPE_INVALID
                ? mmay_type_descriptor(
                    &function->checker->result->types,
                    expected_error_type
                )
                : NULL;
            if (mapper_descriptor == NULL
                || mapper_descriptor->kind != MMAY_TYPE_KIND_NOMINAL
                || !mmay_symbol_id_equal(
                    mapper_descriptor->symbol,
                    mapper_enum->symbol
                )) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "generic enum constructor mapper requires an expected concrete Result error type",
                    mapper_node->start,
                    mapper_node->end
                );
                return false;
            }
            concrete_mapper.type = expected_error_type;
        }
        payload = mmay_vector_at_const(&mapper_variant->payload, 0);
        if (payload == NULL
            || !mmay_checker_private_substitute_nominal_member_type(
                function,
                concrete_mapper.type,
                &mapper_enum->type_parameters,
                payload->type,
                &mapper_parameter_type
            )) {
            return false;
        }
        mapper_result_type = concrete_mapper.type;
    } else {
        if (mapper_token == NULL || mapper_token->synthetic) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "mapError mapper must be a unary function or positional enum constructor",
                node->start,
                node->end
            );
            return false;
        }
    mapper_name.snapshot = function->checker->ast->snapshot;
    mapper_name.start = mapper_token->start;
    mapper_name.end = mapper_token->end;
    resolution = mapper_qualified
        ? mmay_check_resolve_qualified_call(
            function->checker, mapper_namespace, mapper_name,
            false, MMAY_TYPE_INVALID, &mapper
        ) : mmay_check_resolve_call(
            function->checker, mapper_name, false, MMAY_TYPE_INVALID, &mapper
        );
    if (resolution != MMAY_CHECK_CALL_FOUND || mapper.parameter_count != 1) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            resolution == MMAY_CHECK_CALL_UNKNOWN_NAME
                ? MMAY_DIAGNOSTIC_UNKNOWN_NAME
                : resolution == MMAY_CHECK_CALL_UNKNOWN_IMPORT
                ? MMAY_DIAGNOSTIC_UNKNOWN_IMPORT
                : resolution == MMAY_CHECK_CALL_PRIVATE_IMPORT
                ? MMAY_DIAGNOSTIC_PRIVATE_IMPORT
                : MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            resolution == MMAY_CHECK_CALL_PRIVATE_IMPORT
                ? "mapError mapper is package-private"
                : "mapError mapper must be a unary visible function",
            mapper_name.start,
            mapper_name.end
        );
        return false;
    }
    if (!mmay_check_record_occurrence(
            function->checker, mapper_name, mapper.symbol
        )) {
        return false;
    }
        mapper_parameter_type = mapper.parameter_types[0];
        mapper_result_type = mapper.result_type;
    }
    {
        const MarimayTypeDescriptor *input_descriptor = mmay_type_descriptor(
            &function->checker->result->types, receiver.type
        );
        if (input_descriptor == NULL
            || (mapper_closure
                && closure_parameter_type != MMAY_TYPE_INVALID
                && closure_parameter_type != input_descriptor->arguments[1])) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "mapError closure parameter does not match the Result error type",
                mapper_node->start,
                mapper_node->end
            );
            return false;
        }
        if (mapper_closure) {
            mapper_parameter_type = input_descriptor->arguments[1];
        }
        if (mapper_parameter_type != input_descriptor->arguments[1]) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "mapError mapper parameter must match the Result error type",
                mapper_node->start,
                mapper_node->end
            );
            return false;
        }
        output_arguments[0] = input_descriptor->arguments[0];
        output_arguments[1] = mapper_result_type;
    }
    if (!mmay_checker_private_intern_generic_nominal(
            function->checker,
            MMAY_TYPE_KIND_NOMINAL,
            input_enum->symbol,
            &(MarimayVector){
                .data = output_arguments,
                .length = 2,
                .capacity = 2,
                .element_size = sizeof(MarimayTypeId)
            },
            &output_type
        ) || (expected != MMAY_TYPE_INVALID && expected != output_type)) {
        if (expected != MMAY_TYPE_INVALID && expected != output_type) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "mapError result does not match the expected type",
                node->start,
                node->end
            );
        } else {
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        }
        return false;
    }
    if (!mmay_checker_private_map_error_result_enum(
            function, output_type, &output_enum
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    concrete_input = *input_enum;
    concrete_input.type = receiver.type;
    concrete_output = *output_enum;
    concrete_output.type = output_type;
    if (!mmay_checker_private_function_enum_layout(
            function, &concrete_input, &input_layout
        ) || !mmay_checker_private_function_enum_layout(
            function, &concrete_output, &output_layout
        ) || !mmay_checker_private_function_emit_enum_access(
            function, MMAY_MIR_ENUM_IS_VARIANT, MMAY_TYPE_BOOL,
            receiver.register_id, input_layout, 1, 0, start, end, &is_error
        ) || !mmay_checker_private_function_add_block(function, &error_block)
        || !mmay_checker_private_function_add_block(function, &success_block)
        || !mmay_checker_private_function_add_block(function, &join_block)
        || !mmay_checker_private_function_set_branch(
            function, function->block, is_error.register_id,
            error_block, success_block, start, end
        ) || !mmay_function_add_register(function, output_type, &target)) {
        return false;
    }
    function->block = error_block;
    function->terminated = false;
    if (!mmay_checker_private_function_emit_enum_access(
            function, MMAY_MIR_ENUM_REQUIRE_VARIANT,
            receiver.type, receiver.register_id, input_layout, 1, 0,
            start, end, &required_input_error
        ) || !mmay_checker_private_function_emit_enum_access(
            function, MMAY_MIR_ENUM_GET_PAYLOAD,
            mmay_type_descriptor(&function->checker->result->types, receiver.type)
                ->arguments[1], required_input_error.register_id, input_layout, 1, 0,
            start, end, &input_error
        )) {
        return false;
    }
    if (mapper_closure) {
        if (!mmay_checker_private_map_error_compile_closure(
                function,
                closure_parameter_name,
                closure_body,
                mapper_parameter_type,
                mapper_result_type,
                input_error,
                &mapped_error
            )) {
            return false;
        }
    } else if (mapper_enum_resolved) {
        if (!mmay_checker_private_function_emit_enum_construction(
                function,
                &concrete_mapper,
                mapper_variant_index,
                &(MarimayMirEnumArgument){
                    .register_id = input_error.register_id,
                    .type = input_error.type
                },
                1,
                start,
                end,
                &mapped_error
            )) {
            return false;
        }
    } else if (!mmay_checker_private_map_error_call(
            function,
            &mapper,
            input_error.register_id,
            start,
            end,
            &mapped_error
        )) {
        return false;
    }
    output_argument.register_id = mapped_error.register_id;
    output_argument.type = mapped_error.type;
    if (!mmay_checker_private_function_emit_enum_construction(
            function, &concrete_output, 1, &output_argument, 1,
            start, end, &output_error
        ) || !mmay_checker_private_function_emit_copy(
            function, target, output_error.register_id, start, end
        ) || !mmay_checker_private_function_set_goto(
            function, function->block, join_block, start, end
        )) {
        return false;
    }
    function->block = success_block;
    function->terminated = false;
    if (output_arguments[0] == MMAY_TYPE_VOID) {
        if (!mmay_checker_private_function_emit_enum_construction(
                function, &concrete_output, 0, NULL, 0,
                start, end, &output_success
            )) {
            return false;
        }
    } else if (!mmay_checker_private_function_emit_enum_access(
            function, MMAY_MIR_ENUM_REQUIRE_VARIANT,
            receiver.type, receiver.register_id, input_layout, 0, 0,
            start, end, &required_input_success
        ) || !mmay_checker_private_function_emit_enum_access(
            function, MMAY_MIR_ENUM_GET_PAYLOAD, output_arguments[0],
            required_input_success.register_id, input_layout, 0, 0, start, end,
            &input_success
        ) || !mmay_checker_private_function_emit_enum_construction(
            function, &concrete_output, 0,
            &(MarimayMirEnumArgument){
                .register_id = input_success.register_id,
                .type = input_success.type
            },
            1, start, end, &output_success
        )) {
        return false;
    }
    if (!mmay_checker_private_function_emit_copy(
            function, target, output_success.register_id, start, end
        ) || !mmay_checker_private_function_set_goto(
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
