#include "checker_closure_internal.h"
#include "checker_core_api.h"
#include "checker_cfg_api.h"
#include "checker_expression_api.h"
#include "checker_lookup_api.h"
#include "checker_statement_api.h"

bool mmay_function_value_closure(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId expected, MarimayExpressionValue *result) {
    MarimayFunctionChecker body = {0};
    MarimayClosureContext context = {0};
    MarimayVector parameters = {0}, names = {0};
    MarimaySymbolId symbol, environment_symbol;
    MarimayTypeId type, signature, result_type;
    MarimayExpressionValue environment, value;
    const MarimayAstNode *parameter_nodes = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_PARAMETERS, 0);
    const MarimayAstNode *result_node = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_TYPE, 0);
    const MarimayAstNode *block = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_BLOCK, 0);
    const MarimayAstNode *expression = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_EXPRESSION, 0);
    bool ok = false;
    context.parent = function; context.node = node;
    if (result_node == NULL) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "ordinary closure requires an explicit result type", node->start, node->end); return false;
    }
    if (!mmay_checker_private_check_exact_type(function->checker, result_node, &result_type)) return false;
    if (!mmay_vector_init(&parameters, function->checker->result->allocator, sizeof(MarimayTypeId))
        || !mmay_vector_init(&names, function->checker->result->allocator, sizeof(MarimayCheckName))
        || !mmay_vector_init(&context.captures, function->checker->result->allocator, sizeof(MarimayClosureCapture))
        || !mmay_function_value_symbol(function, node->start, 3, &symbol, NULL)
        || !mmay_function_value_symbol(function, node->start, 4, &environment_symbol, &context.environment_type)
        || !mmay_vector_push(&parameters, &context.environment_type)) goto allocation;
    for (size_t i = 0; parameter_nodes != NULL && i < parameter_nodes->child_count; i += 1) {
        const MarimayAstElement *element = mmay_ast_child(parameter_nodes, i);
        if (element->kind != MMAY_AST_ELEMENT_NODE) continue;
        const MarimayAstNode *parameter = element->as.node;
        const MarimayAstNode *annotation = mmay_checker_private_check_child_node(parameter, MMAY_SYNTAX_TYPE, 0);
        const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(parameter, MMAY_TOKEN_IDENTIFIER, 0);
        MarimayTypeId parameter_type;
        if (annotation == NULL || token == NULL) {
            (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "ordinary closure requires explicit parameter types", parameter->start, parameter->end); goto cleanup;
        }
        if (!mmay_checker_private_check_exact_type(function->checker, annotation, &parameter_type)) goto cleanup;
        if (parameter_type == MMAY_TYPE_VOID || parameter_type == MMAY_TYPE_NEVER) {
            (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "closure parameter requires an inhabited value type", annotation->start, annotation->end); goto cleanup;
        }
        if (names.length >= function->checker->options.max_locals_per_function
            || parameters.length >= function->checker->options.mir_options.max_registers) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); goto cleanup;
        }
        MarimayCheckName name = {function->checker->ast->snapshot, token->start, token->end};
        for (size_t n = 0; n < names.length; n += 1) {
            if (mmay_checker_private_check_names_equal(function->checker, name,
                    *(const MarimayCheckName *)mmay_vector_at_const(&names, n))) {
                (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_DUPLICATE_NAME,
                    "duplicate closure parameter", token->start, token->end); goto cleanup;
            }
        }
        if (!mmay_vector_push(&parameters, &parameter_type) || !mmay_vector_push(&names, &name)) goto allocation;
    }
    if (!mmay_type_intern_function(&function->checker->result->types,
            (const MarimayTypeId *)parameters.data + 1, names.length, result_type, &type)
        || !mmay_type_intern_function(&function->checker->result->types,
            parameters.data, parameters.length, mmay_function_value_abi_result(result_type), &signature)) {
        mmay_function_value_intern_failed(function); goto cleanup;
    }
    if (expected != MMAY_TYPE_INVALID && expected != type) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "closure does not have the expected exact signature", node->start, node->end); goto cleanup;
    }
    if (!mmay_function_value_init(&body, function, symbol, parameters.data, parameters.length, result_type)) goto cleanup;
    body.closure = &context;
    body.body_node = block != NULL ? block : expression;
    for (size_t i = 0; i < names.length; i += 1) {
        MarimayCheckLocal local = {0};
        local.name = *(const MarimayCheckName *)mmay_vector_at_const(&names, i);
        local.type = *(const MarimayTypeId *)mmay_vector_at_const(&parameters, i + 1);
        local.register_id = (MarimayMirRegister)(i + 1); local.initialized = true;
        if (!mmay_vector_push(&body.locals, &local)) goto allocation;
    }
    if (block != NULL) {
        if (!mmay_checker_private_compile_block(&body, block)) goto cleanup;
        if (!body.terminated) {
            if (result_type != MMAY_TYPE_VOID) {
                (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_MISSING_RETURN,
                    "not every closure path returns a value", node->start, node->end); goto cleanup;
            }
            if (!mmay_checker_private_function_set_return(&body, MMAY_MIR_NO_REGISTER, node->start, node->end)) goto cleanup;
        }
    } else {
        if (expression == NULL || !mmay_compile_expression(&body, expression, result_type, &value)) goto cleanup;
        if (!mmay_expression_is_never(&body, &value)) {
            if (value.type != result_type) {
                (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "closure result does not match its annotation", expression->start, expression->end); goto cleanup;
            }
            if (!mmay_checker_private_function_set_return(&body,
                    result_type == MMAY_TYPE_VOID ? MMAY_MIR_NO_REGISTER : value.register_id, node->start, node->end)) goto cleanup;
        }
    }
    if (function->checker->result->diagnostics.length != body.diagnostic_start) goto cleanup;
    if (!mmay_closure_finish_captures(&body, &environment)
        || !mmay_function_value_publish(&body, signature)
        || !mmay_function_value_wrap(function, node, type, environment, symbol, result)) goto cleanup;
    ok = true;
    goto cleanup;
allocation:
    if (!mmay_checker_has_failed(function->checker)) mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
cleanup:
    mmay_function_value_dispose(&body);
    mmay_vector_dispose(&parameters);
    mmay_vector_dispose(&names);
    mmay_vector_dispose(&context.captures);
    return ok;
}
