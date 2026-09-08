#include "checker_statement_api.h"
#include "checker_closure_api.h"
#include "checker_function_values_internal.h"
#include "checker_core_api.h"
#include "checker_expr_records_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

/* Adapters have an explicit empty managed environment, just like closures.
 * The resolver's selected symbol remains the sole target of the direct call. */
static bool named_adapter(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayCheckCallable callable, MarimayTypeId type, MarimayExpressionValue *result) {
    MarimayFunctionChecker body = {0};
    MarimayVector parameters = {0}, arguments = {0};
    MarimaySymbolId symbol, environment_symbol;
    MarimayTypeId environment_type, signature;
    MarimayMirRecordId layout;
    MarimayExpressionValue environment;
    MarimayMirInstruction call = {0};
    bool ok = false;
    if (!mmay_vector_init(&parameters, function->checker->result->allocator, sizeof(MarimayTypeId))
        || !mmay_vector_init(&arguments, function->checker->result->allocator, sizeof(MarimayMirCallArgument))
        || !mmay_function_value_symbol(function, node->start, 1, &symbol, NULL)
        || !mmay_function_value_symbol(function, node->start, 2, &environment_symbol, &environment_type)
        || !mmay_vector_push(&parameters, &environment_type)) goto allocation;
    for (size_t i = 0; i < callable.parameter_count; i += 1) {
        MarimayMirCallArgument argument = {(MarimayMirRegister)(i + 1), callable.parameter_types[i]};
        if (!mmay_vector_push(&parameters, &argument.type) || !mmay_vector_push(&arguments, &argument)) goto allocation;
    }
    if (!mmay_type_intern_function(&function->checker->result->types, parameters.data,
            parameters.length, mmay_function_value_abi_result(callable.result_type), &signature)) {
        mmay_function_value_intern_failed(function); goto cleanup;
    }
    if (!mmay_function_value_init(&body, function, symbol, parameters.data,
            parameters.length, callable.result_type)
        || !mmay_function_value_record(&body, environment_type, NULL, 0, &layout)) goto cleanup;
    if (body.mir.options.max_calls == 0 || arguments.length > body.mir.options.max_call_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED); goto cleanup;
    }
    if (!mmay_mir_add_call(&body.mir, callable.symbol, callable.result_type,
            arguments.data, arguments.length, &call.call)) goto allocation;
    call.kind = MMAY_MIR_CALL;
    call.span = (MarimayMirSpan){node->start, node->end};
    if (callable.result_type == MMAY_TYPE_NEVER) {
        if (!mmay_checker_private_function_set_never_call(&body, call.call, node->start, node->end)) goto cleanup;
    } else {
        if (!mmay_function_add_register(&body, callable.result_type, &call.target)
            || !mmay_function_append_instruction(&body, &call)
            || !mmay_checker_private_function_set_return(&body,
                callable.result_type == MMAY_TYPE_VOID ? MMAY_MIR_NO_REGISTER : call.target,
                node->start, node->end)) goto cleanup;
    }
    if (!mmay_function_value_publish(&body, signature)
        || !mmay_function_value_record(function, environment_type, NULL, 0, &layout)
        || !mmay_function_value_environment(function, node, layout, NULL, 0, &environment)
        || !mmay_function_value_wrap(function, node, type, environment, symbol, result)) goto cleanup;
    ok = true;
    goto cleanup;
allocation:
    if (!mmay_checker_has_failed(function->checker)) mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
cleanup:
    mmay_function_value_dispose(&body);
    mmay_vector_dispose(&parameters);
    mmay_vector_dispose(&arguments);
    return ok;
}

static bool named_value(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId expected, MarimayTypeId first_parameter, bool *resolved, MarimayExpressionValue *result) {
    const MarimaySyntaxToken *name_token = NULL, *namespace_token = NULL;
    const MarimayAstNode *receiver = NULL;
    MarimayCheckName name, namespace_name = {0};
    MarimayCheckCallable callable;
    MarimayCheckCallResolution resolution;
    const MarimayTypeDescriptor *context = mmay_type_descriptor(&function->checker->result->types, expected);
    MarimayTypeId type;
    bool qualified = node->kind == MMAY_SYNTAX_MEMBER_EXPRESSION;
    *resolved = false;
    if (qualified) {
        if (!mmay_checker_private_member_parts(node, &receiver, &name_token)
            || receiver == NULL || receiver->kind != MMAY_SYNTAX_NAME_EXPRESSION) return true;
        namespace_token = mmay_checker_private_check_child_token(receiver, MMAY_TOKEN_IDENTIFIER, 0);
        if (namespace_token == NULL) return true;
        namespace_name = (MarimayCheckName){function->checker->ast->snapshot, namespace_token->start, namespace_token->end};
        if (mmay_closure_find_local(function, namespace_name) != NULL
            || !mmay_check_import_namespace_exists(function->checker,
                mmay_source_bytes(function->checker->ast->snapshot) + namespace_name.start,
                namespace_name.end - namespace_name.start)) return true;
    } else if (node->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        name_token = mmay_checker_private_check_child_token(node, MMAY_TOKEN_IDENTIFIER, 0);
    }
    if (name_token == NULL) return true;
    name = (MarimayCheckName){function->checker->ast->snapshot, name_token->start, name_token->end};
    if (!qualified && mmay_checker_private_function_find_local(function, name) != NULL) return true;
    if (context != NULL && context->kind == MMAY_TYPE_KIND_FUNCTION && context->parameter_count != 0)
        first_parameter = context->arguments[0];
    resolution = qualified ? mmay_check_resolve_qualified_call(function->checker, namespace_name,
        name, false, MMAY_TYPE_INVALID, &callable)
        : mmay_check_resolve_call(function->checker, name, false, MMAY_TYPE_INVALID, &callable);
    if (resolution == MMAY_CHECK_CALL_RECEIVER_REQUIRED && first_parameter != MMAY_TYPE_INVALID) {
        resolution = qualified ? mmay_check_resolve_qualified_call(function->checker, namespace_name,
            name, true, first_parameter, &callable)
            : mmay_check_resolve_free_call(function->checker, name, first_parameter, &callable);
    }
    if (resolution == MMAY_CHECK_CALL_UNKNOWN_NAME || resolution == MMAY_CHECK_CALL_NOT_FUNCTION) return true;
    *resolved = true;
    if (resolution != MMAY_CHECK_CALL_FOUND || callable.is_external) {
        (void)mmay_checker_add_diagnostic(function->checker,
            resolution == MMAY_CHECK_CALL_PRIVATE_IMPORT ? MMAY_DIAGNOSTIC_PRIVATE_IMPORT
                : resolution == MMAY_CHECK_CALL_UNKNOWN_IMPORT ? MMAY_DIAGNOSTIC_UNKNOWN_IMPORT
                : MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "function value requires an unambiguous visible ordinary function",
            node->start, node->end); return false;
    }
    for (size_t i = 0; i < function->checker->declarations.length; i += 1) {
        const MarimayCheckFunctionDeclaration *declaration = mmay_vector_at_const(&function->checker->declarations, i);
        if (mmay_symbol_id_equal(declaration->symbol, callable.symbol)
            && ((declaration->is_generic && !declaration->is_instance) || declaration->is_unsafe)) {
            mmay_checker_fail_incomplete(function->checker, node,
                "generic or unsafe function reference"); return false;
        }
    }
    for (size_t i = 0; i < function->checker->result->functions.length; i += 1) {
        const MarimayCheckedFunction *published = mmay_checked_function(function->checker->result, i);
        if (mmay_symbol_id_equal(published->mir.symbol, callable.symbol) && published->is_unsafe) {
            mmay_checker_fail_incomplete(function->checker, node, "unsafe function reference"); return false;
        }
    }
    if (!mmay_type_intern_function(&function->checker->result->types, callable.parameter_types,
            callable.parameter_count, callable.result_type, &type)) {
        mmay_function_value_intern_failed(function); return false;
    }
    if (expected != MMAY_TYPE_INVALID && type != expected) {
        (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "function value does not have the expected exact signature", node->start, node->end); return false;
    }
    return mmay_check_record_occurrence(function->checker, name, callable.symbol)
        && named_adapter(function, node, callable, type, result);
}

bool mmay_function_value_named(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId expected, bool *resolved, MarimayExpressionValue *result) {
    return named_value(function, node, expected, MMAY_TYPE_INVALID, resolved, result);
}

bool mmay_function_value_named_input(MarimayFunctionChecker *function, const MarimayAstNode *node,
    MarimayTypeId first_parameter, bool *resolved, MarimayExpressionValue *result) {
    return named_value(function, node, MMAY_TYPE_INVALID, first_parameter, resolved, result);
}
