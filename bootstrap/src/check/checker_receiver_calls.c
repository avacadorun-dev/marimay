#include "checker_receiver_calls_api.h"

#include "checker_core_api.h"
#include "checker_expr_calls_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include <string.h>

static const RudaAstNode *ruda_receiver_first_argument(
    const RudaAstNode *arguments
) {
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1) {
        const RudaAstElement *element = ruda_ast_child(arguments, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE) {
            return element->as.node;
        }
    }
    return NULL;
}

static bool ruda_receiver_is_collection_name(
    const RudaChecker *checker,
    const RudaSyntaxToken *token,
    bool *is_length
) {
    if (is_length != NULL) {
        *is_length = false;
    }
    if (checker == NULL || token == NULL || token->synthetic) {
        return false;
    }
    if (ruda_checker_private_check_text_equal(
            checker, token->start, token->end, "push"
        )) {
        return true;
    }
    if (ruda_checker_private_check_text_equal(
            checker, token->start, token->end, "length"
        )) {
        if (is_length != NULL) {
            *is_length = true;
        }
        return true;
    }
    return false;
}

static bool ruda_receiver_is_import_namespace(
    const RudaFunctionChecker *function,
    const RudaAstNode *receiver
) {
    const RudaSyntaxToken *token = NULL;
    if (function == NULL || receiver == NULL
        || receiver->kind != RUDA_SYNTAX_NAME_EXPRESSION) {
        return false;
    }
    token = ruda_checker_private_check_child_token(
        receiver, RUDA_TOKEN_IDENTIFIER, 0
    );
    return token != NULL && !token->synthetic
        && ruda_check_import_namespace_exists(
            function->checker,
            function->checker->source + token->start,
            token->end - token->start
        );
}

bool ruda_checker_private_compile_receiver_collection_call(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *callee = NULL;
    const RudaAstNode *member = NULL;
    const RudaAstNode *arguments = NULL;
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *name_token = NULL;
    RudaCheckName name;
    RudaCheckCallable candidate;
    RudaCheckCallResolution resolution = RUDA_CHECK_CALL_UNKNOWN_NAME;
    RudaExpressionValue receiver;
    RudaTypeId element = RUDA_TYPE_INVALID;
    bool dot_call = false;
    bool is_length = false;
    bool array_resolved = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    callee = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_NAME_EXPRESSION, 0
    );
    member = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_MEMBER_EXPRESSION, 0
    );
    arguments = ruda_checker_private_check_child_node(
        node, RUDA_SYNTAX_ARGUMENTS, 0
    );
    if (callee != NULL) {
        name_token = ruda_checker_private_check_child_token(
            callee, RUDA_TOKEN_IDENTIFIER, 0
        );
    } else if (member != NULL && ruda_checker_private_member_parts(
            member, &receiver_node, &name_token
        )) {
        if (ruda_receiver_is_import_namespace(function, receiver_node)) {
            return true;
        }
        dot_call = true;
    }
    if (!ruda_receiver_is_collection_name(
            function->checker, name_token, &is_length
        )) {
        return true;
    }
    if (arguments == NULL || (dot_call && receiver_node == NULL)) {
        return true;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (!dot_call
        && ruda_checker_private_function_find_local(function, name) != NULL) {
        return true;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (!dot_call) {
        resolution = ruda_check_resolve_call(
            function->checker,
            name,
            false,
            RUDA_TYPE_INVALID,
            &candidate
        );
        if (resolution == RUDA_CHECK_CALL_FOUND && !candidate.dot_eligible) {
            *resolved = true;
            return ruda_checker_private_compile_ordinary_call(
                function, node, result
            );
        }
        if (resolution == RUDA_CHECK_CALL_NOT_FUNCTION) {
            *resolved = true;
            return ruda_checker_private_compile_ordinary_call(
                function, node, result
            );
        }
        receiver_node = ruda_receiver_first_argument(arguments);
        if (receiver_node == NULL) {
            return true;
        }
    }
    memset(&receiver, 0, sizeof(receiver));
    receiver.register_id = RUDA_MIR_NO_REGISTER;
    if (!ruda_compile_expression(
            function, receiver_node, RUDA_TYPE_INVALID, &receiver
        )) {
        return false;
    }
    *resolved = true;
    if (ruda_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    memset(&candidate, 0, sizeof(candidate));
    resolution = dot_call
        ? ruda_check_resolve_call(
            function->checker, name, true, receiver.type, &candidate
        )
        : ruda_check_resolve_free_call(
            function->checker, name, receiver.type, &candidate
        );
    if (resolution == RUDA_CHECK_CALL_FOUND) {
        return ruda_checker_private_compile_ordinary_call_with_receiver(
            function, node, receiver_node, &receiver, result
        );
    }
    if (!ruda_type_array_element(
            &function->checker->result->types, receiver.type, &element
        )) {
        return ruda_checker_private_compile_ordinary_call_with_receiver(
            function, node, receiver_node, &receiver, result
        );
    }
    if (is_length) {
        return ruda_compile_array_length_call(
            function,
            node,
            true,
            receiver_node,
            &receiver,
            &array_resolved,
            result
        );
    }
    return ruda_compile_array_builtin_call(
        function,
        node,
        receiver_node,
        &receiver,
        &array_resolved,
        result
    );
}
