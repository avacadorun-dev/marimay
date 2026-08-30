#include "checker_expr_test_api.h"

#include <string.h>

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_equality_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_expr_string_api.h"
#include "checker_lookup_api.h"

static size_t mmay_checker_test_node_count(
    const MarimayAstNode *node,
    MarimaySyntaxKind kind
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind == kind) {
            count += 1;
        }
    }
    return count;
}

static size_t mmay_checker_test_arguments(
    const MarimayAstNode *arguments,
    const MarimayAstNode **nodes,
    size_t capacity
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            if (count < capacity) nodes[count] = element->as.node;
            count += 1;
        }
    }
    return count;
}

static bool mmay_checker_test_member_is(
    const MarimayFunctionChecker *function,
    const MarimaySyntaxToken *member,
    const char *name
) {
    return member != NULL && !member->synthetic
        && mmay_checker_private_check_text_equal(
            function->checker, member->start, member->end, name
        );
}

static bool mmay_checker_test_assertion_member(
    const MarimayFunctionChecker *function,
    const MarimaySyntaxToken *member
) {
    return mmay_checker_test_member_is(function, member, "equal")
        || mmay_checker_test_member_is(function, member, "notEqual")
        || mmay_checker_test_member_is(function, member, "true")
        || mmay_checker_test_member_is(function, member, "false")
        || mmay_checker_test_member_is(function, member, "some")
        || mmay_checker_test_member_is(function, member, "none")
        || mmay_checker_test_member_is(function, member, "ok")
        || mmay_checker_test_member_is(function, member, "err");
}

static bool mmay_checker_test_emit_assertion(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue condition,
    const char *message,
    MarimayExpressionValue *result
) {
    MarimayExpressionValue message_value;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    memset(&message_value, 0, sizeof(message_value));
    if (!mmay_checker_private_emit_string_constant(
            function, (const unsigned char *)message, strlen(message),
            node->start, node->end, &message_value
        ) || !mmay_function_add_register(function, MMAY_TYPE_VOID, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_TEST_ASSERT;
    instruction.target = target;
    instruction.first = condition.register_id;
    instruction.second = message_value.register_id;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = MMAY_TYPE_VOID;
    result->register_id = target;
    result->valid = true;
    return true;
}

static bool mmay_checker_test_compile_value_assertion(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimaySyntaxToken *member,
    const MarimayAstNode *arguments,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *argument_nodes[2] = {NULL, NULL};
    size_t argument_count = mmay_checker_test_arguments(
        arguments, argument_nodes, 2
    );
    bool binary = mmay_checker_test_member_is(function, member, "equal")
        || mmay_checker_test_member_is(function, member, "notEqual");
    bool negate = mmay_checker_test_member_is(
        function, member, "notEqual"
    ) || mmay_checker_test_member_is(function, member, "false");
    MarimayExpressionValue value;
    MarimayExpressionValue other;
    MarimayExpressionValue condition;
    const char *message = "test expectation failed";
    memset(&value, 0, sizeof(value));
    memset(&other, 0, sizeof(other));
    memset(&condition, 0, sizeof(condition));
    if (function->constant_evaluation
        || mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL || argument_count != (binary ? 2U : 1U)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            function->constant_evaluation
                ? MMAY_DIAGNOSTIC_CONSTANT_EVALUATION
                : MMAY_DIAGNOSTIC_CALL_ARITY,
            binary
                ? "expect.equal and expect.notEqual require exactly two values"
                : "this expectation requires exactly one value",
            node->start, node->end
        );
        return false;
    }
    if (!mmay_compile_expression(
            function, argument_nodes[0],
            (mmay_checker_test_member_is(function, member, "true")
                || mmay_checker_test_member_is(function, member, "false"))
                ? MMAY_TYPE_BOOL : MMAY_TYPE_INVALID,
            &value
        )) return false;
    if (mmay_expression_is_never(function, &value)) {
        *result = value;
        return true;
    }
    if (binary) {
        if (!mmay_compile_expression(
                function, argument_nodes[1], value.type, &other
            )) return false;
        if (mmay_expression_is_never(function, &other)) {
            *result = other;
            return true;
        }
        if (value.type != other.type) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "expect.equal operands must have one identical type",
                node->start, node->end
            );
            return false;
        }
        if (!mmay_checker_private_compile_equality(
                function, node, value, other, negate, &condition
            )) return false;
        message = negate
            ? "expect.notEqual failed: values are equal at root"
            : "expect.equal failed: values differ at root";
    } else if (mmay_checker_test_member_is(function, member, "true")
        || mmay_checker_test_member_is(function, member, "false")) {
        if (value.type != MMAY_TYPE_BOOL) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "expect.true and expect.false require bool",
                argument_nodes[0]->start, argument_nodes[0]->end
            );
            return false;
        }
        if (negate) {
            if (!mmay_function_emit_instruction(
                    function, MMAY_MIR_NOT_BOOL, MMAY_TYPE_BOOL,
                    value.register_id, MMAY_MIR_NO_REGISTER, 0, false,
                    node->start, node->end, &condition
                )) return false;
            message = "expect.false failed: value is true";
        } else {
            condition = value;
            message = "expect.true failed: value is false";
        }
    } else {
        MarimayCheckEnumDeclaration *enum_value =
            mmay_checker_private_checker_find_enum_by_type(
                function->checker, value.type
            );
        MarimayCheckEnumDeclaration concrete;
        MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
        bool option = mmay_checker_test_member_is(function, member, "some")
            || mmay_checker_test_member_is(function, member, "none");
        bool first_variant = mmay_checker_test_member_is(
            function, member, option ? "some" : "ok"
        );
        const char *required = option ? "Option" : "Result";
        if (enum_value == NULL || !enum_value->is_builtin
            || enum_value->owned_name.length != strlen(required)
            || memcmp(
                enum_value->owned_name.data, required, strlen(required)
            ) != 0) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                option
                    ? "expect.some and expect.none require Option<T>"
                    : "expect.ok and expect.err require Result<T, E>",
                argument_nodes[0]->start, argument_nodes[0]->end
            );
            return false;
        }
        concrete = *enum_value;
        concrete.type = value.type;
        if (!mmay_checker_private_function_enum_layout(
                function, &concrete, &enum_id
            ) || !mmay_checker_private_function_emit_enum_access(
                function, MMAY_MIR_ENUM_IS_VARIANT, MMAY_TYPE_BOOL,
                value.register_id, enum_id, first_variant ? 0U : 1U, 0,
                node->start, node->end, &condition
            )) return false;
        if (option) {
            message = first_variant
                ? "expect.some failed: value is None"
                : "expect.none failed: value is Some";
        } else {
            message = first_variant
                ? "expect.ok failed: value is Err"
                : "expect.err failed: value is Ok";
        }
    }
    return mmay_checker_test_emit_assertion(
        function, node, condition, message, result
    );
}

bool mmay_checker_private_compile_test_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *member = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const MarimayAstNode *receiver = NULL;
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    const MarimayAstNode *argument_nodes[2] = {NULL, NULL};
    const MarimayAstNode *closure = NULL;
    const MarimayAstNode *parameters = NULL;
    const MarimayAstNode *return_type = NULL;
    const MarimayAstNode *body = NULL;
    const MarimaySyntaxToken *member_token = NULL;
    const MarimaySyntaxToken *expect_token = NULL;
    MarimayCheckName expect_name;
    MarimayExpressionValue expected_value;
    MarimayTypeId closure_result = MMAY_TYPE_VOID;
    MarimayMirRegister marker = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    (void)expected;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (member == NULL || !mmay_checker_private_member_parts(
            member, &receiver, &member_token
        ) || receiver == NULL
        || receiver->kind != MMAY_SYNTAX_NAME_EXPRESSION) {
        return true;
    }
    expect_token = mmay_checker_private_check_child_token(
        receiver, MMAY_TOKEN_IDENTIFIER, 0
    );
    if (expect_token == NULL || expect_token->synthetic
        || member_token == NULL || member_token->synthetic
        || !mmay_checker_private_check_text_equal(
            function->checker, expect_token->start, expect_token->end,
            "expect"
        ) || (!mmay_checker_test_member_is(function, member_token, "output")
            && !mmay_checker_test_assertion_member(
                function, member_token
            ))) {
        return true;
    }
    expect_name = (MarimayCheckName){
        function->checker->ast->snapshot,
        expect_token->start,
        expect_token->end
    };
    if (mmay_checker_private_function_find_local(function, expect_name) != NULL
        || mmay_checker_private_checker_find_top_level(
            function->checker, expect_name
        ) != NULL) {
        return true;
    }
    *resolved = true;
    if (!function->test_context) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_INVALID_ATTRIBUTE,
            "expect helpers are available only inside @test and @fuzz functions",
            node->start, node->end
        );
        return false;
    }
    if (!mmay_checker_test_member_is(function, member_token, "output")) {
        return mmay_checker_test_compile_value_assertion(
            function, node, member_token, arguments, result
        );
    }
    if (function->constant_evaluation
        || mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL || mmay_checker_test_arguments(
            arguments, argument_nodes, 2
        ) != 2) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            function->constant_evaluation
                ? MMAY_DIAGNOSTIC_CONSTANT_EVALUATION
                : MMAY_DIAGNOSTIC_CALL_ARITY,
            "expect.output requires a string and one zero-parameter void closure",
            node->start, node->end
        );
        return false;
    }
    closure = mmay_checker_private_expression_only_node(argument_nodes[1]);
    parameters = mmay_checker_private_check_child_node(
        closure, MMAY_SYNTAX_PARAMETERS, 0
    );
    return_type = mmay_checker_private_check_child_node(
        closure, MMAY_SYNTAX_TYPE, 0
    );
    body = mmay_checker_private_check_child_node(
        closure, MMAY_SYNTAX_BLOCK, 0
    );
    if (closure == NULL || closure->kind != MMAY_SYNTAX_CLOSURE_EXPRESSION
        || parameters == NULL || body == NULL
        || mmay_checker_test_node_count(
            parameters, MMAY_SYNTAX_PARAMETER
        ) != 0
        || (return_type != NULL
            && (!mmay_checker_private_check_exact_type(
                    function->checker, return_type, &closure_result
                ) || closure_result != MMAY_TYPE_VOID))) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "expect.output closure must have type fn(): void and a block body",
            argument_nodes[1]->start, argument_nodes[1]->end
        );
        return false;
    }
    memset(&expected_value, 0, sizeof(expected_value));
    if (!mmay_compile_expression(
            function, argument_nodes[0], MMAY_TYPE_STRING, &expected_value
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &expected_value)) {
        *result = expected_value;
        return true;
    }
    if (expected_value.type != MMAY_TYPE_STRING
        || !mmay_function_add_register(function, MMAY_TYPE_BOOL, &marker)) {
        if (expected_value.type != MMAY_TYPE_STRING) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "expect.output expected value must have type string",
                argument_nodes[0]->start, argument_nodes[0]->end
            );
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_TEST_OUTPUT_BEGIN;
    instruction.target = marker;
    instruction.first = expected_value.register_id;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_append_instruction(function, &instruction)
        || !mmay_checker_private_compile_scoped_block(function, body)) {
        return false;
    }
    if (function->terminated) {
        mmay_checker_private_expression_set_never(function, result);
        return true;
    }
    if (!mmay_function_add_register(function, MMAY_TYPE_VOID, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_TEST_OUTPUT_END;
    instruction.target = target;
    instruction.first = marker;
    instruction.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = MMAY_TYPE_VOID;
    result->register_id = target;
    result->valid = true;
    return true;
}
