#include "checker_expr_map_error_api.h"
#include "checker_core_api.h"
#include "checker_expression_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "checker_mapper_api.h"
#include "checker_match_pattern_api.h"
#include "checker_statement_api.h"

static bool mmay_checker_private_map_error_result_enum(
    MarimayFunctionChecker *function,
    MarimayTypeId type,
    MarimayCheckEnumDeclaration **result
) {
    size_t index;
    const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(
        &function->checker->result->types, type
    );
    if (descriptor == NULL
        || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || descriptor->argument_count != 2) {
        return false;
    }
    /* Only the stable prelude declaration can qualify. General enum lookup
     * may allocate an imported view even when this context is ineligible. */
    for (index = 0; index < function->checker->prelude_enums.length; index += 1) {
        MarimayCheckEnumDeclaration *declaration = mmay_vector_at(
            &function->checker->prelude_enums, index);
        if (declaration != NULL && declaration->is_builtin
            && declaration->declaration_valid && declaration->variants_resolved
            && declaration->owned_name.length == sizeof("Result") - 1
            && memcmp(declaration->owned_name.data, "Result", sizeof("Result") - 1) == 0
            && declaration->type_parameters.length == 2
            && mmay_symbol_id_equal(descriptor->symbol, declaration->symbol)) {
            *result = declaration;
            return true;
        }
    }
    return false;
}

MarimayTypeId mmay_checker_private_map_error_operand_context(
    MarimayFunctionChecker *function,
    const MarimayAstNode *operand,
    const MarimayAstNode **target
) {
    const MarimayAstNode *member = NULL, *receiver = NULL;
    const MarimaySyntaxToken *name = NULL;
    MarimayCheckEnumDeclaration *result_enum = NULL;
    if (target == NULL) return MMAY_TYPE_INVALID;
    *target = NULL;
    while (operand != NULL && (operand->kind == MMAY_SYNTAX_EXPRESSION
            || operand->kind == MMAY_SYNTAX_PAREN_GROUP)) {
        const MarimayAstNode *inner = mmay_checker_private_expression_only_node(operand);
        if (inner == operand) return MMAY_TYPE_INVALID;
        operand = inner;
    }
    if (function == NULL || operand == NULL
        || operand->kind != MMAY_SYNTAX_CALL_EXPRESSION) return MMAY_TYPE_INVALID;
    member = mmay_checker_private_check_child_node(operand, MMAY_SYNTAX_MEMBER_EXPRESSION, 0);
    if (!mmay_checker_private_member_parts(member, &receiver, &name) || name == NULL
        || !mmay_checker_private_check_text_equal(function->checker, name->start, name->end, "mapError")
        || !mmay_checker_private_map_error_result_enum(function, function->result_type, &result_enum)) {
        return MMAY_TYPE_INVALID;
    }
    *target = operand;
    return mmay_type_descriptor(&function->checker->result->types,
        function->result_type)->arguments[1];
}

bool mmay_checker_private_compile_map_error(
    MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId expected, bool *resolved, MarimayExpressionValue *result
) {
    const MarimayAstNode *member = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0);
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_ARGUMENTS, 0);
    const MarimayAstNode *receiver_node = NULL, *mapper_node = NULL;
    const MarimaySyntaxToken *name = NULL;
    MarimayCheckEnumDeclaration *input_enum = NULL, *output_enum = NULL;
    MarimayCheckEnumDeclaration concrete_input, concrete_output;
    MarimayErrorMapper mapper;
    MarimayExpressionValue receiver, is_error, required_input_error, input_error, mapped_error;
    MarimayExpressionValue input_success, required_input_success, output_success, output_error;
    MarimayMirEnumArgument output_argument;
    MarimayTypeId output_arguments[2], output_type = MMAY_TYPE_INVALID;
    MarimayTypeId expected_error_type = MMAY_TYPE_INVALID, input_error_type;
    MarimayMirEnumId input_layout, output_layout;
    MarimayMirRegister target;
    MarimayMirBlockId error_block, success_block, join_block;
    size_t start = node->start, end = node->end, count = 0;
    *resolved = false;
    if (!mmay_checker_private_member_parts(member, &receiver_node, &name) || name == NULL
        || !mmay_checker_private_check_text_equal(function->checker, name->start, name->end, "mapError")) return true;
    *resolved = true;
    for (size_t i = 0; arguments != NULL && i < arguments->child_count; i += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, i);
        if (element->kind == MMAY_AST_ELEMENT_NODE) { mapper_node = element->as.node; count += 1; }
    }
    if (count != 1) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "mapError requires exactly one mapper argument", start, end); return false;
    }
    if (!mmay_compile_expression(function, receiver_node, MMAY_TYPE_INVALID, &receiver)) return false;
    if (mmay_expression_is_never(function, &receiver)) { *result = receiver; return true; }
    if (!mmay_checker_private_map_error_result_enum(function, receiver.type, &input_enum)) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "mapError receiver must have type Result<T,E>", receiver_node->start, receiver_node->end); return false;
    }
    {
        input_error_type = mmay_type_descriptor(
            &function->checker->result->types, receiver.type)->arguments[1];
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
        if (expected_error_type == MMAY_TYPE_INVALID && node == function->try_operand) {
            expected_error_type = function->try_error_type;
        }
    }
    if (!mmay_error_mapper_prepare(function, mapper_node, input_error_type, expected_error_type, &mapper)) return false;
    if (!mapper.is_constructor && mmay_expression_is_never(function, &mapper.value)) { *result = mapper.value; return true; }
    output_arguments[0] = mmay_type_descriptor(&function->checker->result->types, receiver.type)->arguments[0];
    output_arguments[1] = mapper.result_type;
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
    if (!mmay_error_mapper_invoke(function, mapper_node, &mapper, input_error, &mapped_error)) return false;
    if (!mmay_expression_is_never(function, &mapped_error)) {
        output_argument.register_id = mapped_error.register_id;
        output_argument.type = mapped_error.type;
        bool has_payload = mapped_error.type != MMAY_TYPE_VOID;
        if (!mmay_checker_private_function_emit_enum_construction(
                function, &concrete_output, 1, has_payload ? &output_argument : NULL, has_payload ? 1 : 0,
                start, end, &output_error)
            || !mmay_checker_private_function_emit_copy(function, target, output_error.register_id, start, end)
            || !mmay_checker_private_function_set_goto(function, function->block, join_block, start, end)) return false;
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
