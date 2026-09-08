#include "checker_function_values_api.h"
#include "checker_expr_calls_api.h"
#include "checker_receiver_calls_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_expr_interfaces_api.h"
#include "checker_expr_records_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"
#include "checker_generic_specialization_api.h"
#include "checker_generic_types_api.h"
#include "checker_expr_prelude_api.h"
#include "checker_expr_map_error_api.h"
#include "checker_expr_numeric_api.h"
#include "checker_expr_io_api.h"
#include "checker_expr_test_api.h"
#include "checker_expr_string_api.h"
#include "checker_expr_text_api.h"
#include "checker_expr_fs_api.h"
#include "checker_expr_context_api.h"
#include "checker_expr_collections_api.h"
#include "checker_json_api.h"






 bool mmay_checker_private_compile_identity_same(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *member_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const MarimayAstNode *arguments_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_ARGUMENTS,
        0
    );
    const MarimayAstNode *receiver_node = NULL;
    const MarimayAstNode *argument_nodes[2] = {NULL, NULL};
    const MarimaySyntaxToken *identity_token = NULL;
    const MarimaySyntaxToken *member_token = NULL;
    MarimayCheckName identity_name;
    MarimayExpressionValue left;
    MarimayExpressionValue right;
    MarimayCheckRecordDeclaration *record_declaration = NULL;
    const MarimayCheckedRecord *imported_record = NULL;
    MarimayCheckRecordDeclaration concrete_record;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    size_t argument_count = 0;
    size_t index = 0;
    if (function == NULL || resolved == NULL || result == NULL) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *resolved = false;
    if (member_node == NULL || !mmay_checker_private_member_parts(
            member_node,
            &receiver_node,
            &member_token
        ) || receiver_node == NULL
        || receiver_node->kind != MMAY_SYNTAX_NAME_EXPRESSION) {
        return true;
    }
    identity_token = mmay_checker_private_check_child_token(
        receiver_node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    if (identity_token == NULL || identity_token->synthetic
        || member_token == NULL || member_token->synthetic
        || !mmay_checker_private_check_text_equal(
            function->checker,
            identity_token->start,
            identity_token->end,
            "identity"
        ) || !mmay_checker_private_check_text_equal(
            function->checker,
            member_token->start,
            member_token->end,
            "same"
        )) {
        return true;
    }
    identity_name.snapshot = function->checker->ast->snapshot;
    identity_name.start = identity_token->start;
    identity_name.end = identity_token->end;
    if (mmay_checker_private_function_find_local(function, identity_name) != NULL
        || mmay_checker_private_checker_find_top_level(
            function->checker,
            identity_name
        ) != NULL) {
        return true;
    }
    *resolved = true;
    if (mmay_checker_private_check_child_node(node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0) != NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "identity.same does not accept type arguments",
            node->start,
            node->end
        );
        return false;
    }
    if (function->constant_evaluation) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "record identity is not available in constant expressions",
            node->start,
            node->end
        );
        return false;
    }
    if (arguments_node == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments_node, index);
        if (element == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind == MMAY_AST_ELEMENT_NODE) {
            if (element->as.node == NULL
                || element->as.node->kind != MMAY_SYNTAX_EXPRESSION
                || argument_count >= 2) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_CALL_ARITY,
                    "identity.same requires exactly two arguments",
                    arguments_node->start,
                    arguments_node->end
                );
                return false;
            }
            argument_nodes[argument_count] = element->as.node;
            argument_count += 1;
        } else if (element->as.token.synthetic) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
    }
    if (argument_count != 2) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CALL_ARITY,
            "identity.same requires exactly two arguments",
            arguments_node->start,
            arguments_node->end
        );
        return false;
    }
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    if (!mmay_compile_expression(
            function,
            argument_nodes[0],
            MMAY_TYPE_INVALID,
            &left
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &left)) {
        *result = left;
        return true;
    }
    record_declaration = mmay_checker_private_checker_find_record_by_type(
        function->checker,
        left.type
    );
    if (record_declaration == NULL) {
        imported_record = mmay_check_imported_record_by_type(function->checker, left.type);
    }
    if (record_declaration == NULL && imported_record == NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "identity.same requires record arguments",
            argument_nodes[0]->start,
            argument_nodes[0]->end
        );
        return false;
    }
    if (!mmay_compile_expression(
            function,
            argument_nodes[1],
            MMAY_TYPE_INVALID,
            &right
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &right)) {
        *result = right;
        return true;
    }
    if (right.type != left.type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "identity.same arguments must have the same nominal record type",
            argument_nodes[1]->start,
            argument_nodes[1]->end
        );
        return false;
    }
    if (record_declaration != NULL) {
        concrete_record = *record_declaration;
        concrete_record.type = left.type;
    }
    if (!(imported_record != NULL
            ? mmay_checker_private_imported_record_layout(function, imported_record, left.type, &record)
            : mmay_checker_private_function_record_layout(function, &concrete_record, &record))
        || !mmay_function_add_register(function, MMAY_TYPE_BOOL, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_IDENTITY_SAME;
    instruction.target = target;
    instruction.first = left.register_id;
    instruction.second = right.register_id;
    instruction.record = record;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = MMAY_TYPE_BOOL;
    result->register_id = target;
    result->valid = true;
    return true;
}

bool mmay_checker_private_compile_ordinary_call_with_receiver(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *precompiled_receiver_node,
    const MarimayExpressionValue *precompiled_receiver,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_NAME_EXPRESSION,
        0
    );
    const MarimayAstNode *arguments_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_ARGUMENTS,
        0
    );
    const MarimayAstNode *member_callee = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const MarimayAstNode *receiver_node = NULL;
    const MarimayAstNode *resolution_receiver_node = NULL;
    const MarimaySyntaxToken *name_token = NULL;
    MarimayCheckCallable declaration;
    MarimayCheckName name;
    MarimayCheckName namespace_name;
    MarimayMirCallArgument *arguments = NULL;
    MarimayExpressionValue receiver;
    MarimayCheckCallResolution resolution = MMAY_CHECK_CALL_INTERNAL_ERROR;
    size_t explicit_argument_count = 0;
    size_t explicit_argument_index = 0;
    size_t argument_count = 0;
    size_t argument_bytes = 0;
    size_t argument_index = 0;
    size_t preflight_call_argument_count = 0;
    size_t new_call_argument_count = 0;
    size_t index = 0;
    MarimayMirCallId call = MMAY_MIR_NO_CALL;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    bool dot_call = false;
    bool qualified_call = false;
    bool first_argument_compiled = false;
    bool accepted = false;
    const MarimayCheckFunctionDeclaration *generic_base = NULL;
    MarimayCheckFunctionDeclaration *generic_instance = NULL;
    size_t generic_base_index = SIZE_MAX;
    MarimayVector explicit_generic_arguments;
    bool explicit_generic_arguments_initialized = false;
    MarimayExpressionValue *generic_values = NULL;
    MarimayTypeId *generic_actual_types = NULL;
    size_t generic_value_bytes = 0;
    size_t generic_type_bytes = 0;
    size_t generic_cursor = 0;
    bool generic_call = false;
    memset(&explicit_generic_arguments, 0, sizeof(explicit_generic_arguments));
    memset(&receiver, 0, sizeof(receiver));
    memset(&declaration, 0, sizeof(declaration));
    memset(&namespace_name, 0, sizeof(namespace_name));
    receiver.register_id = MMAY_MIR_NO_REGISTER;
    if ((precompiled_receiver_node == NULL) != (precompiled_receiver == NULL)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (callee_node != NULL) {
        name_token = mmay_checker_private_check_child_token(
            callee_node,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
    } else if (member_callee != NULL && mmay_checker_private_member_parts(
            member_callee,
            &receiver_node,
            &name_token
        )) {
        dot_call = true;
        if (receiver_node != NULL
            && receiver_node->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
            const MarimaySyntaxToken *namespace_token =
                mmay_checker_private_check_child_token(
                    receiver_node, MMAY_TOKEN_IDENTIFIER, 0
                );
            const unsigned char *source = function->checker->source;
            MarimayCheckName candidate = {
                function->checker->ast->snapshot,
                namespace_token == NULL ? 0 : namespace_token->start,
                namespace_token == NULL ? 0 : namespace_token->end
            };
            if (namespace_token != NULL && !namespace_token->synthetic
                && mmay_checker_private_function_find_local(function, candidate) == NULL
                && mmay_checker_private_checker_find_top_level(function->checker, candidate) == NULL
                && mmay_check_import_namespace_exists(
                    function->checker,
                    source + namespace_token->start,
                    namespace_token->end - namespace_token->start
                )) {
                namespace_name.snapshot = function->checker->ast->snapshot;
                namespace_name.start = namespace_token->start;
                namespace_name.end = namespace_token->end;
                qualified_call = true;
                dot_call = false;
            }
        }
    }
    if (arguments_node == NULL || name_token == NULL || name_token->synthetic
        || (!dot_call && !qualified_call && callee_node == NULL)
        || (dot_call && receiver_node == NULL)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (!dot_call && !qualified_call) {
        const MarimayCheckLocal *local = mmay_checker_private_function_find_local(function, name);
        if (local != NULL) {
            MarimayTypeKind kind = mmay_type_kind(
                &function->checker->result->types, local->type
            );
            if (kind == MMAY_TYPE_KIND_INVALID || kind == MMAY_TYPE_KIND_FUNCTION) {
                mmay_checker_fail(function->checker, kind == MMAY_TYPE_KIND_INVALID
                    ? MMAY_CHECK_INVALID_ARGUMENT : MMAY_CHECK_INCOMPLETE_LANGUAGE);
            } else {
                (void)mmay_checker_add_diagnostic(function->checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH, "called name is not a function",
                    name.start, name.end);
            }
            goto cleanup;
        }
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments_node, index);
        if (element == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == MMAY_AST_ELEMENT_NODE) {
            if (element->as.node == NULL
                || element->as.node->kind != MMAY_SYNTAX_EXPRESSION
                || explicit_argument_count == SIZE_MAX) {
                mmay_checker_fail(
                    function->checker,
                    element->as.node == NULL
                            || element->as.node->kind
                                != MMAY_SYNTAX_EXPRESSION
                        ? MMAY_CHECK_INCOMPLETE_LANGUAGE
                        : MMAY_CHECK_LIMIT_REACHED
                );
                goto cleanup;
            }
            explicit_argument_count += 1;
        } else if (element->as.token.synthetic) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
    }
    if (dot_call && explicit_argument_count == SIZE_MAX) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    argument_count = explicit_argument_count + (dot_call ? 1 : 0);
    if (function->mir.calls.length >= function->mir.options.max_calls
        || !mmay_size_add(
            function->mir.call_arguments.length,
            argument_count,
            &preflight_call_argument_count
        ) || preflight_call_argument_count
            > function->mir.options.max_call_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (qualified_call) {
        resolution = mmay_check_resolve_qualified_call(
            function->checker,
            namespace_name,
            name,
            false,
            MMAY_TYPE_INVALID,
            &declaration
        );
        if (resolution == MMAY_CHECK_CALL_RECEIVER_REQUIRED) {
            const MarimayAstNode *first_argument = NULL;
            for (index = 0; index < arguments_node->child_count; index += 1) {
                const MarimayAstElement *element = mmay_ast_child(
                    arguments_node, index
                );
                if (element != NULL
                    && element->kind == MMAY_AST_ELEMENT_NODE) {
                    first_argument = element->as.node;
                    break;
                }
            }
            if (first_argument == NULL) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_CALL_ARITY,
                    "qualified overloaded call requires a first argument",
                    arguments_node->start,
                    arguments_node->end
                );
                goto cleanup;
            }
            resolution_receiver_node = first_argument;
            if (!mmay_compile_expression(function, first_argument,
                    MMAY_TYPE_INVALID, &receiver)) {
                goto cleanup;
            }
            if (mmay_expression_is_never(function, &receiver)) {
                *result = receiver;
                accepted = true;
                goto cleanup;
            }
            first_argument_compiled = true;
            resolution = mmay_check_resolve_qualified_call(
                function->checker,
                namespace_name,
                name,
                true,
                receiver.type,
                &declaration
            );
        }
    } else if (dot_call) {
        resolution_receiver_node = receiver_node;
        if (precompiled_receiver != NULL) {
            if (precompiled_receiver_node != receiver_node
                || !precompiled_receiver->valid) {
                mmay_checker_fail(
                    function->checker, MMAY_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            receiver = *precompiled_receiver;
        } else {
            if (!mmay_compile_expression(
                    function,
                    receiver_node,
                    MMAY_TYPE_INVALID,
                    &receiver
                )) {
                goto cleanup;
            }
        }
        if (mmay_expression_is_never(function, &receiver)) {
            *result = receiver;
            accepted = true;
            goto cleanup;
        }
        resolution = mmay_check_resolve_call(
            function->checker,
            name,
            true,
            receiver.type,
            &declaration
        );
    } else {
        if (precompiled_receiver != NULL) {
            const MarimayAstNode *first_argument = NULL;
            for (index = 0; index < arguments_node->child_count; index += 1) {
                const MarimayAstElement *element = mmay_ast_child(
                    arguments_node,
                    index
                );
                if (element != NULL
                    && element->kind == MMAY_AST_ELEMENT_NODE) {
                    first_argument = element->as.node;
                    break;
                }
            }
            if (first_argument == NULL
                || first_argument != precompiled_receiver_node
                || !precompiled_receiver->valid) {
                mmay_checker_fail(
                    function->checker, MMAY_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            resolution_receiver_node = first_argument;
            receiver = *precompiled_receiver;
            first_argument_compiled = true;
            resolution = mmay_check_resolve_free_call(
                function->checker,
                name,
                receiver.type,
                &declaration
            );
        } else {
            resolution = mmay_check_resolve_call(
                function->checker,
                name,
                false,
                MMAY_TYPE_INVALID,
                &declaration
            );
        }
        if (precompiled_receiver == NULL
            && resolution == MMAY_CHECK_CALL_RECEIVER_REQUIRED) {
            const MarimayAstNode *first_argument = NULL;
            for (index = 0; index < arguments_node->child_count; index += 1) {
                const MarimayAstElement *element = mmay_ast_child(
                    arguments_node,
                    index
                );
                if (element != NULL
                    && element->kind == MMAY_AST_ELEMENT_NODE) {
                    first_argument = element->as.node;
                    break;
                }
            }
            if (first_argument == NULL) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_CALL_ARITY,
                    "overloaded free call requires a first argument of a locally owned nominal type; value.name() and name(value) resolve to one symbol",
                    arguments_node->start,
                    arguments_node->end
                );
                goto cleanup;
            }
            resolution_receiver_node = first_argument;
            if (!mmay_compile_expression(
                    function,
                    first_argument,
                    MMAY_TYPE_INVALID,
                    &receiver
                )) {
                goto cleanup;
            }
            if (mmay_expression_is_never(function, &receiver)) {
                *result = receiver;
                accepted = true;
                goto cleanup;
            }
            first_argument_compiled = true;
            resolution = mmay_check_resolve_free_call(
                function->checker,
                name,
                receiver.type,
                &declaration
            );
        }
    }
    if (resolution != MMAY_CHECK_CALL_FOUND) {
        size_t diagnostic_start = resolution_receiver_node == NULL
            ? name.start
            : resolution_receiver_node->start;
        size_t diagnostic_end = resolution_receiver_node == NULL
            ? name.end
            : resolution_receiver_node->end;
        if (resolution == MMAY_CHECK_CALL_UNKNOWN_NAME) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_NAME,
                "unknown function name",
                name.start,
                name.end
            );
        } else if (resolution == MMAY_CHECK_CALL_UNKNOWN_IMPORT) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_IMPORT,
                "unknown public function in imported package",
                name.start,
                name.end
            );
        } else if (resolution == MMAY_CHECK_CALL_PRIVATE_IMPORT) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_PRIVATE_IMPORT,
                "imported function is package-private",
                name.start,
                name.end
            );
        } else if (resolution == MMAY_CHECK_CALL_NOT_FUNCTION) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "called name is not a function",
                name.start,
                name.end
            );
        } else if (resolution == MMAY_CHECK_CALL_NOT_DOT_ELIGIBLE) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "function is not dot-call eligible because its first parameter is not an exact locally owned nominal type",
                name.start,
                name.end
            );
        } else if (resolution == MMAY_CHECK_CALL_RECEIVER_NOT_OWNED) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                first_argument_compiled
                    ? "overloaded free call first argument is not a locally owned nominal type; value.name() and name(value) resolve to one symbol"
                    : "dot-call receiver is not a locally owned nominal type",
                diagnostic_start,
                diagnostic_end
            );
        } else if (resolution == MMAY_CHECK_CALL_OTHER_NOMINAL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                first_argument_compiled
                    ? "overloaded free call has no declaration for this first-argument nominal type; value.name() and name(value) resolve to one symbol"
                    : "an eligible function with this name exists for a different locally owned nominal type",
                diagnostic_start,
                diagnostic_end
            );
        } else {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        goto cleanup;
    }
    generic_base = mmay_checker_private_find_function_by_symbol(
        function->checker, declaration.symbol, &generic_base_index
    );
    if (generic_base != NULL && generic_base->is_generic) {
        const MarimayAstNode *type_arguments =
            mmay_checker_private_check_child_node(
                node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
            );
        generic_call = true;
        if (type_arguments != NULL) {
            if (!mmay_vector_init(
                    &explicit_generic_arguments,
                    function->checker->result->allocator,
                    sizeof(MarimayTypeId)
                ) || !mmay_checker_private_parse_type_arguments(
                    function->checker,
                    type_arguments,
                    &explicit_generic_arguments
                )) {
                goto cleanup;
            }
            explicit_generic_arguments_initialized = true;
        }
        if (!mmay_size_multiply(
                argument_count,
                sizeof(MarimayExpressionValue),
                &generic_value_bytes
            ) || !mmay_size_multiply(
                argument_count,
                sizeof(MarimayTypeId),
                &generic_type_bytes
            )) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        if (generic_value_bytes != 0) {
            generic_values = mmay_allocate(
                &function->checker->result->allocator,
                generic_value_bytes
            );
            generic_actual_types = mmay_allocate(
                &function->checker->result->allocator,
                generic_type_bytes
            );
            if (generic_values == NULL || generic_actual_types == NULL) {
                mmay_checker_fail(
                    function->checker, MMAY_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
            memset(generic_values, 0, generic_value_bytes);
            memset(generic_actual_types, 0, generic_type_bytes);
        }
        if (dot_call || first_argument_compiled) {
            if (argument_count == 0) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
                goto cleanup;
            }
            generic_values[0] = receiver;
            generic_actual_types[0] = receiver.type;
            generic_cursor = 1;
        }
        for (index = 0; index < arguments_node->child_count; index += 1) {
            const MarimayAstElement *element = mmay_ast_child(
                arguments_node, index
            );
            MarimayExpressionValue value;
            if (element == NULL || element->kind != MMAY_AST_ELEMENT_NODE) {
                continue;
            }
            if (first_argument_compiled && explicit_argument_index == 0) {
                explicit_argument_index += 1;
                continue;
            }
            memset(&value, 0, sizeof(value));
            if (!mmay_compile_expression(
                    function,
                    element->as.node,
                    MMAY_TYPE_INVALID,
                    &value
                )) {
                goto cleanup;
            }
            if (mmay_expression_is_never(function, &value)) {
                *result = value;
                accepted = true;
                goto cleanup;
            }
            if (generic_cursor >= argument_count) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
                goto cleanup;
            }
            generic_values[generic_cursor] = value;
            generic_actual_types[generic_cursor] = value.type;
            generic_cursor += 1;
            explicit_argument_index += 1;
        }
        if (generic_cursor != argument_count) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        {
            MarimayVector inferred_arguments;
            /* Argument expressions can specialize calls and relocate the
             * declaration table. Only the saved base index survives them. */
            generic_base = mmay_vector_at_const(
                &function->checker->declarations, generic_base_index
            );
            memset(&inferred_arguments, 0, sizeof(inferred_arguments));
            if (!mmay_vector_init(
                    &inferred_arguments,
                    function->checker->result->allocator,
                    sizeof(MarimayTypeId)
                ) || !mmay_checker_private_prepare_generic_arguments(
                    function->checker,
                    generic_base,
                    generic_actual_types,
                    argument_count,
                    explicit_generic_arguments_initialized
                        ? &explicit_generic_arguments : NULL,
                    &inferred_arguments
                ) || !mmay_checker_private_instantiate_function(
                    function->checker,
                    generic_base_index,
                    &inferred_arguments,
                    &generic_instance
                )) {
                mmay_vector_dispose(&inferred_arguments);
                goto cleanup;
            }
            declaration.parameter_types = generic_instance->parameter_types.data;
            declaration.parameter_count = generic_instance->parameter_types.length;
            declaration.symbol = generic_instance->symbol;
            declaration.result_type = generic_instance->result_type;
            declaration.receiver_symbol = generic_instance->receiver_symbol;
            declaration.receiver_type = generic_instance->receiver_type;
            declaration.dot_eligible = generic_instance->dot_eligible;
            mmay_vector_dispose(&inferred_arguments);
        }
    } else if (mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_TYPE_ARGUMENTS, 0
        ) != NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "call supplies type arguments to a non-generic function",
            node->start,
            node->end
        );
        goto cleanup;
    }
    /* Generic preflight caches argument values; the ordinary lowering pass
     * starts its source-argument cursor afresh. */
    if (generic_call) {
        explicit_argument_index = 0;
        argument_index = 0;
    }
    if (argument_count != declaration.parameter_count) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CALL_ARITY,
            "call argument count does not match the function signature",
            arguments_node->start,
            arguments_node->end
        );
        goto cleanup;
    }
    if (!mmay_size_multiply(
            argument_count,
            sizeof(MarimayMirCallArgument),
            &argument_bytes
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (argument_bytes != 0) {
        arguments = mmay_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        if (arguments == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
            goto cleanup;
        }
    }
    if (dot_call || first_argument_compiled) {
        const MarimayTypeId *first_parameter = declaration.parameter_count == 0
            ? NULL : &declaration.parameter_types[0];
        if (first_parameter == NULL || receiver.type != *first_parameter
            || arguments == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        arguments[0].register_id = receiver.register_id;
        arguments[0].type = receiver.type;
        argument_index = 1;
    }
    for (index = 0; index < arguments_node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(arguments_node, index);
        const MarimayTypeId *parameter_type = NULL;
        MarimayExpressionValue value;
        if (element == NULL || element->kind != MMAY_AST_ELEMENT_NODE) {
            continue;
        }
        if (first_argument_compiled && explicit_argument_index == 0) {
            explicit_argument_index += 1;
            continue;
        }
        if ((dot_call ? explicit_argument_index + 1
                : explicit_argument_index) < declaration.parameter_count) {
            parameter_type = &declaration.parameter_types[
                dot_call ? explicit_argument_index + 1
                    : explicit_argument_index
            ];
        }
        if (parameter_type == NULL || arguments == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        memset(&value, 0, sizeof(value));
        if (generic_call) {
            size_t cached_index = argument_index;
            if (cached_index >= argument_count || generic_values == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
                goto cleanup;
            }
            value = generic_values[cached_index];
        } else if (!mmay_compile_expression(
                function, element->as.node, *parameter_type, &value
            )) {
            goto cleanup;
        }
        if (mmay_expression_is_never(function, &value)) {
            *result = value;
            accepted = true;
            goto cleanup;
        }
        if (value.type != *parameter_type) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "argument type does not match the function parameter type",
                element->as.node->start,
                element->as.node->end
            );
            goto cleanup;
        }
        arguments[argument_index].register_id = value.register_id;
        arguments[argument_index].type = value.type;
        argument_index += 1;
        explicit_argument_index += 1;
    }
    if (argument_index != argument_count
        || function->mir.calls.length >= function->mir.options.max_calls
        || !mmay_size_add(
            function->mir.call_arguments.length,
            argument_count,
            &new_call_argument_count
        ) || new_call_argument_count
            > function->mir.options.max_call_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!mmay_check_record_occurrence(
            function->checker,
            name,
            declaration.symbol
        )) {
        goto cleanup;
    }
    if (!mmay_mir_add_call(
            &function->mir,
            declaration.symbol,
            declaration.result_type,
            arguments,
            argument_count,
            &call
        )) {
        if (!mmay_checker_has_failed(function->checker)) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
        }
        goto cleanup;
    }
    if (declaration.result_type == MMAY_TYPE_NEVER) {
        if (!mmay_checker_private_function_set_never_call(
                function,
                call,
                node->start,
                node->end
            )) {
            goto cleanup;
        }
        mmay_checker_private_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (!mmay_function_add_register(
            function,
            declaration.result_type,
            &target
        )) {
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CALL;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.call = call;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        goto cleanup;
    }
    result->type = declaration.result_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    mmay_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    mmay_deallocate(
        &function->checker->result->allocator,
        generic_values,
        generic_value_bytes
    );
    mmay_deallocate(
        &function->checker->result->allocator,
        generic_actual_types,
        generic_type_bytes
    );
    if (explicit_generic_arguments_initialized) {
        mmay_vector_dispose(&explicit_generic_arguments);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

bool mmay_checker_private_compile_ordinary_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue *result
) {
    return mmay_checker_private_compile_ordinary_call_with_receiver(
        function, node, NULL, NULL, result
    );
}

 bool mmay_checker_private_compile_call(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *callee_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_NAME_EXPRESSION,
        0
    );
    const MarimayAstNode *arguments_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_ARGUMENTS,
        0
    );
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        callee_node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    size_t index = 0;
    bool accepted = false;
    bool enum_resolved = false;
    bool identity_resolved = false;
    bool length_resolved = false;
    bool length_available = false;
    bool array_builtin_resolved = false;
    bool receiver_collection_resolved = false;
    bool interface_resolved = false;
    bool prelude_resolved = false;
    bool map_error_resolved = false;
    bool numeric_resolved = false;
    bool convert_format_resolved = false;
    bool convert_string_resolved = false;
    bool convert_parse_resolved = false;
    bool io_resolved = false;
    bool text_primitive_resolved = false;
    bool fs_primitive_resolved = false;
    bool context_primitive_resolved = false;
    bool test_resolved = false;
    bool collection_constructor_resolved = false;
    bool json_resolved = false;
    const MarimayAstNode *member_callee = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const MarimaySyntaxToken *length_token = name_token != NULL
        ? name_token
        : mmay_checker_private_check_child_token(
            member_callee,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
    if (!mmay_function_value_call(function, node, &json_resolved, result)) return false;
    if (json_resolved) return true;
    if (!mmay_checker_compile_json(function, node, &json_resolved, result)) {
        goto cleanup;
    }
    if (json_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_numeric_call(
            function, node, expected, &numeric_resolved, result
        )) {
        goto cleanup;
    }
    if (numeric_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_convert_format_call(
            function, node, expected, &convert_format_resolved, result
        )) {
        goto cleanup;
    }
    if (convert_format_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_convert_string_call(
            function, node, expected, &convert_string_resolved, result
        )) {
        goto cleanup;
    }
    if (convert_string_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_convert_parse_call(
            function, node, expected, &convert_parse_resolved, result
        )) {
        goto cleanup;
    }
    if (convert_parse_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_text_primitive_call(
            function, node, expected, &text_primitive_resolved, result
        )) {
        goto cleanup;
    }
    if (text_primitive_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_fs_primitive_call(
            function, node, expected, &fs_primitive_resolved, result
        )) {
        goto cleanup;
    }
    if (fs_primitive_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_context_primitive_call(
            function, node, expected, &context_primitive_resolved, result
        )) {
        goto cleanup;
    }
    if (context_primitive_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_io_call(
            function, node, expected, &io_resolved, result
        )) {
        goto cleanup;
    }
    if (io_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_test_call(
            function, node, expected, &test_resolved, result
        )) {
        goto cleanup;
    }
    if (test_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (length_token != NULL && !length_token->synthetic
        && mmay_checker_private_check_text_equal(
            function->checker,
            length_token->start,
            length_token->end,
            "length"
        )) {
        MarimayCheckName length_name;
        length_name.snapshot = function->checker->ast->snapshot;
        length_name.start = length_token->start;
        length_name.end = length_token->end;
        length_available = mmay_checker_private_function_find_local(function, length_name) == NULL
            && mmay_checker_private_checker_find_top_level(
                function->checker,
                length_name
            ) == NULL;
    }
    if (!mmay_checker_private_compile_identity_same(
            function,
            node,
            &identity_resolved,
            result
        )) {
        goto cleanup;
    }
    if (identity_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_prelude_constructor(
            function, node, expected, &prelude_resolved, result
        )) {
        goto cleanup;
    }
    if (prelude_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_collection_constructor(
            function, node, expected, &collection_constructor_resolved, result
        )) {
        goto cleanup;
    }
    if (collection_constructor_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_map_error(
            function, node, expected, &map_error_resolved, result
        )) {
        goto cleanup;
    }
    if (map_error_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_interface_call(
            function, node, &interface_resolved, result
        )) {
        goto cleanup;
    }
    if (interface_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_receiver_collection_call(
            function,
            node,
            &receiver_collection_resolved,
            result
        )) {
        goto cleanup;
    }
    if (receiver_collection_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_compile_array_length_call(
            function,
            node,
            length_available,
            NULL,
            NULL,
            &length_resolved,
            result
        )) {
        goto cleanup;
    }
    if (length_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_compile_array_builtin_call(
            function, node, NULL, NULL, &array_builtin_resolved, result
        )) {
        goto cleanup;
    }
    if (array_builtin_resolved) {
        accepted = true;
        goto cleanup;
    }
    {
        const MarimaySyntaxToken *array_builtin_token = name_token != NULL
            ? name_token
            : mmay_checker_private_check_child_token(
                member_callee,
                MMAY_TOKEN_IDENTIFIER,
                0
            );
        bool is_deferred_array_builtin = name_token != NULL
            && array_builtin_token != NULL
            && !array_builtin_token->synthetic
            && mmay_checker_private_check_text_equal(
                    function->checker,
                    array_builtin_token->start,
                    array_builtin_token->end,
                    "arrayOf"
                );
        if (is_deferred_array_builtin) {
            MarimayCheckName builtin_name;
            builtin_name.snapshot = function->checker->ast->snapshot;
            builtin_name.start = array_builtin_token->start;
            builtin_name.end = array_builtin_token->end;
            if (mmay_checker_private_function_find_local(function, builtin_name) == NULL
                && mmay_checker_private_checker_find_top_level(
                    function->checker,
                    builtin_name
                ) == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INCOMPLETE_LANGUAGE
                );
                goto cleanup;
            }
        }
    }
    if (name_token != NULL && !name_token->synthetic
        && mmay_checker_private_check_text_equal(
            function->checker,
            name_token->start,
            name_token->end,
            "panic"
        ) && mmay_checker_private_function_find_local(function, (MarimayCheckName){
            function->checker->ast->snapshot,
            name_token->start,
            name_token->end
        }) == NULL && mmay_checker_private_checker_find_top_level(
            function->checker,
            (MarimayCheckName){
                function->checker->ast->snapshot,
                name_token->start,
                name_token->end
            }
        ) == NULL) {
        const MarimayAstNode *message_node = NULL;
        MarimayExpressionValue message;
        memset(&message, 0, sizeof(message));
        if (mmay_checker_private_check_child_node(
                node,
                MMAY_SYNTAX_TYPE_ARGUMENTS,
                0
            ) != NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "panic does not accept type arguments",
                node->start,
                node->end
            );
            goto cleanup;
        }
        if (function->constant_evaluation) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
                "constant expression can panic",
                node->start,
                node->end
            );
            goto cleanup;
        }
        if (arguments_node == NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INVALID_ARGUMENT
            );
            goto cleanup;
        }
        for (index = 0; index < arguments_node->child_count; index += 1) {
            const MarimayAstElement *element = mmay_ast_child(
                arguments_node,
                index
            );
            if (element == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            if (element->kind == MMAY_AST_ELEMENT_NODE) {
                if (element->as.node->kind != MMAY_SYNTAX_EXPRESSION
                    || message_node != NULL) {
                    (void)mmay_checker_add_diagnostic(
                        function->checker,
                        MMAY_DIAGNOSTIC_CALL_ARITY,
                        "panic requires exactly one message argument",
                        arguments_node->start,
                        arguments_node->end
                    );
                    goto cleanup;
                }
                message_node = element->as.node;
            } else if (element->as.token.synthetic) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
        }
        if (message_node == NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_CALL_ARITY,
                "panic requires exactly one message argument",
                arguments_node->start,
                arguments_node->end
            );
            goto cleanup;
        }
        if (!mmay_compile_expression(
                function,
                message_node,
                MMAY_TYPE_STRING,
                &message
            )) {
            goto cleanup;
        }
        if (mmay_expression_is_never(function, &message)) {
            *result = message;
            accepted = true;
            goto cleanup;
        }
        if (message.type != MMAY_TYPE_STRING) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "panic message must have type string",
                message_node->start,
                message_node->end
            );
            goto cleanup;
        }
        if (!mmay_checker_private_function_set_panic(
                function,
                message.register_id,
                node->start,
                node->end
            )) {
            goto cleanup;
        }
        mmay_checker_private_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_compile_enum_positional(
            function,
            node,
            expected,
            &enum_resolved,
            result
        )) {
        goto cleanup;
    }
    if (enum_resolved) {
        accepted = true;
        goto cleanup;
    }
    if (function->constant_evaluation) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CONSTANT_EVALUATION,
            "function calls are not permitted in this constant subset",
            node->start,
            node->end
        );
        goto cleanup;
    }
    accepted = mmay_checker_private_compile_ordinary_call(function, node, result);

cleanup:
    return accepted && !mmay_checker_has_failed(function->checker);
}
