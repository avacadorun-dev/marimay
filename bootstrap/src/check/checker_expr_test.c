#include "checker_expr_test_api.h"

#include <string.h>

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_lookup_api.h"

static size_t ruda_checker_test_node_count(
    const RudaAstNode *node,
    RudaSyntaxKind kind
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL
            && element->as.node->kind == kind) {
            count += 1;
        }
    }
    return count;
}

static size_t ruda_checker_test_arguments(
    const RudaAstNode *arguments,
    const RudaAstNode **nodes,
    size_t capacity
) {
    size_t count = 0;
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            if (count < capacity) nodes[count] = element->as.node;
            count += 1;
        }
    }
    return count;
}

bool ruda_checker_private_compile_test_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *member = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_MEMBER_EXPRESSION, 0
    );
    const RudaAstNode *receiver = NULL;
    const RudaAstNode *arguments = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_ARGUMENTS, 0
    );
    const RudaAstNode *argument_nodes[2] = {NULL, NULL};
    const RudaAstNode *closure = NULL;
    const RudaAstNode *parameters = NULL;
    const RudaAstNode *return_type = NULL;
    const RudaAstNode *body = NULL;
    const RudaSyntaxToken *member_token = NULL;
    const RudaSyntaxToken *expect_token = NULL;
    RudaCheckName expect_name;
    RudaExpressionValue expected_value;
    RudaTypeId closure_result = RUDA_TYPE_VOID;
    RudaMirRegister marker = RUDA_MIR_NO_REGISTER;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    (void)expected;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        return false;
    }
    *resolved = false;
    if (member == NULL || !ruda_checker_private_member_parts(
            member, &receiver, &member_token
        ) || receiver == NULL
        || receiver->kind != RUDA_SYNTAX_NAME_EXPRESSION) {
        return true;
    }
    expect_token = ruda_checker_private_check_child_token(
        receiver, RUDA_TOKEN_IDENTIFIER, 0
    );
    if (expect_token == NULL || expect_token->synthetic
        || member_token == NULL || member_token->synthetic
        || !ruda_checker_private_check_text_equal(
            function->checker, expect_token->start, expect_token->end,
            "expect"
        ) || !ruda_checker_private_check_text_equal(
            function->checker, member_token->start, member_token->end,
            "output"
        )) {
        return true;
    }
    expect_name = (RudaCheckName){
        function->checker->ast->snapshot,
        expect_token->start,
        expect_token->end
    };
    if (ruda_checker_private_function_find_local(function, expect_name) != NULL
        || ruda_checker_private_checker_find_top_level(
            function->checker, expect_name
        ) != NULL) {
        return true;
    }
    *resolved = true;
    if (!function->test_context) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_INVALID_ATTRIBUTE,
            "expect.output is available only inside @test and @fuzz functions",
            node->start, node->end
        );
        return false;
    }
    if (function->constant_evaluation
        || ruda_checker_private_check_child_node(
            node, RUDA_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL || ruda_checker_test_arguments(
            arguments, argument_nodes, 2
        ) != 2) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            function->constant_evaluation
                ? RUDA_DIAGNOSTIC_CONSTANT_EVALUATION
                : RUDA_DIAGNOSTIC_CALL_ARITY,
            "expect.output requires a string and one zero-parameter void closure",
            node->start, node->end
        );
        return false;
    }
    closure = ruda_checker_private_expression_only_node(argument_nodes[1]);
    parameters = ruda_checker_private_check_child_node(
        closure, RUDA_SYNTAX_PARAMETERS, 0
    );
    return_type = ruda_checker_private_check_child_node(
        closure, RUDA_SYNTAX_TYPE, 0
    );
    body = ruda_checker_private_check_child_node(
        closure, RUDA_SYNTAX_BLOCK, 0
    );
    if (closure == NULL || closure->kind != RUDA_SYNTAX_CLOSURE_EXPRESSION
        || parameters == NULL || body == NULL
        || ruda_checker_test_node_count(
            parameters, RUDA_SYNTAX_PARAMETER
        ) != 0
        || (return_type != NULL
            && (!ruda_checker_private_check_exact_type(
                    function->checker, return_type, &closure_result
                ) || closure_result != RUDA_TYPE_VOID))) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "expect.output closure must have type fn(): void and a block body",
            argument_nodes[1]->start, argument_nodes[1]->end
        );
        return false;
    }
    memset(&expected_value, 0, sizeof(expected_value));
    if (!ruda_compile_expression(
            function, argument_nodes[0], RUDA_TYPE_STRING, &expected_value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &expected_value)) {
        *result = expected_value;
        return true;
    }
    if (expected_value.type != RUDA_TYPE_STRING
        || !ruda_function_add_register(function, RUDA_TYPE_BOOL, &marker)) {
        if (expected_value.type != RUDA_TYPE_STRING) {
            (void)ruda_checker_add_diagnostic(
                function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "expect.output expected value must have type string",
                argument_nodes[0]->start, argument_nodes[0]->end
            );
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_TEST_OUTPUT_BEGIN;
    instruction.target = marker;
    instruction.first = expected_value.register_id;
    instruction.span = (RudaMirSpan){node->start, node->end};
    if (!ruda_function_append_instruction(function, &instruction)
        || !ruda_checker_private_compile_scoped_block(function, body)) {
        return false;
    }
    if (function->terminated) {
        ruda_checker_private_expression_set_never(function, result);
        return true;
    }
    if (!ruda_function_add_register(function, RUDA_TYPE_VOID, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_TEST_OUTPUT_END;
    instruction.target = target;
    instruction.first = marker;
    instruction.span = (RudaMirSpan){node->start, node->end};
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = RUDA_TYPE_VOID;
    result->register_id = target;
    result->valid = true;
    return true;
}
