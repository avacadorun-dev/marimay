#include "checker_expr_test_api.h"

#include <string.h>

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_scalar_api.h"
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
        ) || !mmay_checker_private_check_text_equal(
            function->checker, member_token->start, member_token->end,
            "output"
        )) {
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
            "expect.output is available only inside @test and @fuzz functions",
            node->start, node->end
        );
        return false;
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
