#include "checker_receiver_calls_api.h"

#include "checker_core_api.h"
#include "checker_array_mutation_api.h"
#include "checker_expr_calls_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

#include <string.h>

static const MarimayAstNode *mmay_receiver_first_argument(
    const MarimayAstNode *arguments
) {
    size_t index = 0;
    for (index = 0; arguments != NULL && index < arguments->child_count;
        index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments, index);
        if (element != NULL && element->kind == MMAY_AST_ELEMENT_NODE) {
            return element->as.node;
        }
    }
    return NULL;
}

static bool mmay_receiver_is_collection_name(
    const MarimayChecker *checker,
    const MarimaySyntaxToken *token,
    bool *is_length
) {
    if (is_length != NULL) {
        *is_length = false;
    }
    if (checker == NULL || token == NULL || token->synthetic) {
        return false;
    }
    if (mmay_checker_private_check_text_equal(checker, token->start, token->end, "push")
        || mmay_checker_private_check_text_equal(checker, token->start, token->end, "pop")
        || mmay_checker_private_check_text_equal(checker, token->start, token->end, "truncate")
        || mmay_checker_private_check_text_equal(checker, token->start, token->end, "clear")
        || mmay_checker_private_check_text_equal(checker, token->start, token->end, "reserve")) {
        return true;
    }
    if (mmay_checker_private_check_text_equal(
            checker, token->start, token->end, "length"
        )) {
        if (is_length != NULL) {
            *is_length = true;
        }
        return true;
    }
    return false;
}

static bool mmay_receiver_is_import_namespace(
    const MarimayFunctionChecker *function,
    const MarimayAstNode *receiver
) {
    const MarimaySyntaxToken *token = NULL;
    if (function == NULL || receiver == NULL
        || receiver->kind != MMAY_SYNTAX_NAME_EXPRESSION) {
        return false;
    }
    token = mmay_checker_private_check_child_token(
        receiver, MMAY_TOKEN_IDENTIFIER, 0
    );
    return token != NULL && !token->synthetic
        && mmay_check_import_namespace_exists(
            function->checker,
            function->checker->source + token->start,
            token->end - token->start
        );
}

bool mmay_checker_private_compile_receiver_collection_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee = NULL;
    const MarimayAstNode *member = NULL;
    const MarimayAstNode *arguments = NULL;
    const MarimayAstNode *receiver_node = NULL;
    const MarimaySyntaxToken *name_token = NULL;
    MarimayCheckName name;
    MarimayCheckCallable candidate;
    MarimayCheckCallResolution resolution = MMAY_CHECK_CALL_UNKNOWN_NAME;
    MarimayExpressionValue receiver;
    MarimayTypeId element = MMAY_TYPE_INVALID;
    bool dot_call = false;
    bool is_length = false;
    bool array_resolved = false;
    if (function == NULL || node == NULL || resolved == NULL || result == NULL) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    callee = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_NAME_EXPRESSION, 0
    );
    member = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0
    );
    arguments = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_ARGUMENTS, 0
    );
    if (callee != NULL) {
        name_token = mmay_checker_private_check_child_token(
            callee, MMAY_TOKEN_IDENTIFIER, 0
        );
    } else if (member != NULL && mmay_checker_private_member_parts(
            member, &receiver_node, &name_token
        )) {
        if (mmay_receiver_is_import_namespace(function, receiver_node)) {
            return true;
        }
        dot_call = true;
    }
    if (!mmay_receiver_is_collection_name(
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
        && mmay_checker_private_function_find_local(function, name) != NULL) {
        return true;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (!dot_call) {
        resolution = mmay_check_resolve_call(
            function->checker,
            name,
            false,
            MMAY_TYPE_INVALID,
            &candidate
        );
        if (resolution == MMAY_CHECK_CALL_FOUND && !candidate.dot_eligible) {
            *resolved = true;
            return mmay_checker_private_compile_ordinary_call(
                function, node, result
            );
        }
        if (resolution == MMAY_CHECK_CALL_NOT_FUNCTION) {
            *resolved = true;
            return mmay_checker_private_compile_ordinary_call(
                function, node, result
            );
        }
        receiver_node = mmay_receiver_first_argument(arguments);
        if (receiver_node == NULL) {
            return true;
        }
    }
    memset(&receiver, 0, sizeof(receiver));
    receiver.register_id = MMAY_MIR_NO_REGISTER;
    if (!mmay_compile_expression(
            function, receiver_node, MMAY_TYPE_INVALID, &receiver
        )) {
        return false;
    }
    *resolved = true;
    if (mmay_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    memset(&candidate, 0, sizeof(candidate));
    resolution = dot_call
        ? mmay_check_resolve_call(
            function->checker, name, true, receiver.type, &candidate
        )
        : mmay_check_resolve_free_call(
            function->checker, name, receiver.type, &candidate
        );
    if (resolution == MMAY_CHECK_CALL_FOUND) {
        return mmay_checker_private_compile_ordinary_call_with_receiver(
            function, node, receiver_node, &receiver, result
        );
    }
    if (!mmay_type_array_element(
            &function->checker->result->types, receiver.type, &element
        )) {
        return mmay_checker_private_compile_ordinary_call_with_receiver(
            function, node, receiver_node, &receiver, result
        );
    }
    if (is_length) {
        return mmay_compile_array_length_call(
            function,
            node,
            true,
            receiver_node,
            &receiver,
            &array_resolved,
            result
        );
    }
    if (!mmay_checker_private_compile_array_mutation_call(
            function, node, receiver_node, &receiver, &array_resolved, result
        )) {
        return false;
    }
    if (array_resolved) {
        return true;
    }
    return mmay_compile_array_builtin_call(
        function,
        node,
        receiver_node,
        &receiver,
        &array_resolved,
        result
    );
}
