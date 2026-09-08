#include "checker_expr_string_api.h"
#include "checker_closure_api.h"
#include "checker_function_values_api.h"
#include "checker_core_api.h"
#include "checker_expression_api.h"
#include "checker_lookup_api.h"
#include "checker_expr_records_api.h"

bool mmay_function_value_invoke(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayExpressionValue callee, const MarimayAstNode *arguments, MarimayExpressionValue *result) {
    const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(&function->checker->result->types, callee.type);
    MarimayVector types = {0}, values = {0};
    MarimayMirCallArgument self = {callee.register_id, callee.type};
    MarimayMirInstruction call = {0};
    MarimayTypeId result_type;
    size_t count = 0, index = 0;
    bool ok = false;
    if (mmay_expression_is_never(function, &callee)) { *result = callee; return true; }
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_FUNCTION) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "called value is not a function", node->start, node->end); return false;
    }
    for (size_t i = 0; arguments != NULL && i < arguments->child_count; i += 1)
        if (mmay_ast_child(arguments, i)->kind == MMAY_AST_ELEMENT_NODE) count += 1;
    if (count != descriptor->parameter_count) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_CALL_ARITY,
            "argument count does not match the function value", node->start, node->end); return false;
    }
    result_type = descriptor->arguments[count];
    size_t added_arguments;
    if (function->mir.calls.length >= function->mir.options.max_calls
        || !mmay_size_add(count, 1, &added_arguments)
        || !mmay_size_add(function->mir.call_arguments.length, added_arguments, &added_arguments)
        || added_arguments > function->mir.options.max_call_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); return false;
    }
    if (!mmay_vector_init(&types, function->checker->result->allocator, sizeof(MarimayTypeId))
        || !mmay_vector_init(&values, function->checker->result->allocator, sizeof(MarimayMirCallArgument))) goto allocation;
    for (size_t i = 0; i < count; i += 1)
        if (!mmay_vector_push(&types, &descriptor->arguments[i])) goto allocation;
    if (!mmay_vector_push(&values, &self)) goto allocation;
    for (size_t i = 0; arguments != NULL && i < arguments->child_count; i += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, i);
        MarimayExpressionValue value;
        if (element->kind != MMAY_AST_ELEMENT_NODE) continue;
        MarimayTypeId expected = *(const MarimayTypeId *)mmay_vector_at_const(&types, index++);
        if (!mmay_compile_expression(function, element->as.node, expected, &value)) goto cleanup;
        if (mmay_expression_is_never(function, &value)) { *result = value; ok = true; goto cleanup; }
        if (value.type != expected) {
            (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "argument type does not match the function value", element->as.node->start, element->as.node->end);
            goto cleanup;
        }
        MarimayMirCallArgument argument = {value.register_id, value.type};
        if (!mmay_vector_push(&values, &argument)) goto allocation;
    }
    if (!mmay_mir_add_call(&function->mir, (MarimaySymbolId){{0}}, mmay_function_value_abi_result(result_type),
            values.data, values.length, &call.call)
        || !mmay_mir_set_call_interface_slot(&function->mir, call.call, 0)) goto allocation;
    call.kind = MMAY_MIR_CALL_INTERFACE;
    call.span = (MarimayMirSpan){node->start, node->end};
    if (!mmay_function_add_register(function, mmay_function_value_abi_result(result_type), &call.target)
        || !mmay_function_append_instruction(function, &call)) goto cleanup;
    if (result_type == MMAY_TYPE_NEVER) {
        static const unsigned char message[] = "unreachable return from never function value";
        MarimayExpressionValue guard;
        if (!mmay_checker_private_emit_string_constant(function, message, sizeof(message) - 1,
                node->start, node->end, &guard)
            || !mmay_checker_private_function_set_panic(function, guard.register_id,
                node->start, node->end)) goto cleanup;
        mmay_checker_private_expression_set_never(function, result);
    } else *result = (MarimayExpressionValue){result_type, call.target, true};
    ok = true;
    goto cleanup;
allocation:
    mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
cleanup:
    mmay_vector_dispose(&types);
    mmay_vector_dispose(&values);
    return ok;
}

bool mmay_function_value_call(MarimayFunctionChecker *function, const MarimayAstNode *node,
    bool *resolved, MarimayExpressionValue *result) {
    const MarimayAstNode *callee = NULL;
    const MarimayAstNode *arguments = mmay_checker_private_check_child_node(node, MMAY_SYNTAX_ARGUMENTS, 0);
    *resolved = false;
    for (size_t i = 0; i < node->child_count; i += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, i);
        if (element->kind == MMAY_AST_ELEMENT_NODE && element->as.node != arguments) { callee = element->as.node; break; }
    }
    if (callee == NULL) return true;
    if (callee->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(callee, MMAY_TOKEN_IDENTIFIER, 0);
        if (token == NULL) return true;
        const MarimayCheckLocal *local = mmay_closure_find_local(function,
            (MarimayCheckName){function->checker->ast->snapshot, token->start, token->end});
        if (local == NULL) return true;
        if (mmay_type_kind(&function->checker->result->types, local->type) != MMAY_TYPE_KIND_FUNCTION) {
            *resolved = true;
            (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "called name is not a function", token->start, token->end); return false;
        }
    } else if (callee->kind == MMAY_SYNTAX_MEMBER_EXPRESSION) {
        const MarimayAstNode *receiver = NULL;
        const MarimaySyntaxToken *member = NULL;
        if (mmay_checker_private_member_parts(callee, &receiver, &member)
            && receiver != NULL && receiver->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
            const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(receiver, MMAY_TOKEN_IDENTIFIER, 0);
            if (token != NULL) (void)mmay_closure_find_local(function,
                (MarimayCheckName){function->checker->ast->snapshot, token->start, token->end});
        }
        return !mmay_checker_has_failed(function->checker);
    }
    *resolved = true;
    if (mmay_checker_private_check_child_node(node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "function value invocation does not accept type arguments", node->start, node->end); return false;
    }
    MarimayExpressionValue value;
    return mmay_compile_expression(function, callee, MMAY_TYPE_INVALID, &value)
        && mmay_function_value_invoke(function, node, value, arguments, result);
}
