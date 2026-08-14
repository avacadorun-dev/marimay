#include "checker_expr_nominal_api.h"
#include "checker_core_api.h"
#include "checker_expr_records_api.h"
#include "checker_lookup_api.h"






 bool ruda_checker_private_resolve_enum_member(
    RudaFunctionChecker *function,
    const RudaAstNode *member_node,
    RudaCheckEnumDeclaration **enum_declaration,
    RudaCheckEnumVariantDeclaration **variant_declaration,
    size_t *variant_index,
    bool *resolved
) {
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *type_token = NULL;
    const RudaSyntaxToken *variant_token = NULL;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    RudaCheckName type_name;
    RudaCheckName variant_name;
    if (function == NULL || enum_declaration == NULL
        || variant_declaration == NULL || variant_index == NULL
        || resolved == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *enum_declaration = NULL;
    *variant_declaration = NULL;
    *variant_index = 0;
    *resolved = false;
    if (!ruda_checker_private_member_parts(member_node, &receiver_node, &variant_token)
        || receiver_node == NULL
        || receiver_node->kind != RUDA_SYNTAX_NAME_EXPRESSION) {
        return true;
    }
    type_token = ruda_checker_private_check_child_token(
        receiver_node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    if (type_token == NULL || type_token->synthetic || variant_token == NULL
        || variant_token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    type_name.snapshot = function->checker->ast->snapshot;
    type_name.start = type_token->start;
    type_name.end = type_token->end;
    if (ruda_checker_private_function_find_local(function, type_name) != NULL) {
        return true;
    }
    *enum_declaration = ruda_checker_private_checker_find_enum(
        function->checker,
        type_name
    );
    if (*enum_declaration == NULL) {
        alias = ruda_checker_private_checker_find_type_alias(function->checker, type_name);
        if (alias != NULL && ruda_checker_private_evaluate_type_alias(
                function->checker,
                alias,
                type_name.start,
                type_name.end
            )) {
            *enum_declaration = ruda_checker_private_checker_find_enum_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (*enum_declaration == NULL) {
        return !ruda_checker_has_failed(function->checker);
    }
    *resolved = true;
    variant_name.snapshot = function->checker->ast->snapshot;
    variant_name.start = variant_token->start;
    variant_name.end = variant_token->end;
    *variant_declaration = ruda_checker_private_checker_find_enum_variant(
        function->checker,
        *enum_declaration,
        variant_name,
        variant_index
    );
    if (*variant_declaration == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNKNOWN_VARIANT,
            "enum type has no variant with this name",
            variant_name.start,
            variant_name.end
        );
        return false;
    }
    return true;
}

 bool ruda_checker_private_function_emit_enum_construction(
    RudaFunctionChecker *function,
    const RudaCheckEnumDeclaration *enum_declaration,
    size_t variant_index,
    const RudaMirEnumArgument *arguments,
    size_t argument_count,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirEnumId enum_id = RUDA_MIR_NO_ENUM;
    RudaMirEnumConstructionId construction =
        RUDA_MIR_NO_ENUM_CONSTRUCTION;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    size_t new_argument_count = 0;
    if (function == NULL || enum_declaration == NULL || result == NULL
        || (arguments == NULL && argument_count != 0)) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    if (!ruda_checker_private_function_enum_layout(
            function,
            enum_declaration,
            &enum_id
        )) {
        return false;
    }
    if (function->mir.enum_constructions.length
            >= function->mir.options.max_enum_constructions
        || !ruda_size_add(
            function->mir.enum_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->mir.options.max_enum_arguments) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_enum_construction(
            &function->mir,
            enum_id,
            variant_index,
            arguments,
            argument_count,
            &construction
        )) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    if (!ruda_function_add_register(
            function,
            enum_declaration->type,
            &target
        )) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_ENUM_NEW;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = enum_id;
    instruction.enum_construction = construction;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = enum_declaration->type;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool ruda_checker_private_function_emit_enum_access(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    RudaTypeId result_type,
    RudaMirRegister receiver,
    RudaMirEnumId enum_id,
    size_t variant,
    size_t payload,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    if (result == NULL || (kind != RUDA_MIR_ENUM_IS_VARIANT
            && kind != RUDA_MIR_ENUM_REQUIRE_VARIANT
            && kind != RUDA_MIR_ENUM_GET_PAYLOAD)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!ruda_function_add_register(function, result_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = receiver;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.enum_id = enum_id;
    instruction.variant = variant;
    instruction.field = payload;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool ruda_checker_private_compile_enum_unit(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    size_t variant_index = 0;
    if (!ruda_checker_private_resolve_enum_member(
            function,
            node,
            &enum_declaration,
            &variant,
            &variant_index,
            resolved
        )) {
        return false;
    }
    if (!*resolved) {
        return true;
    }
    if (variant == NULL
        || variant->kind != RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant requires its declared payload constructor syntax",
            node->start,
            node->end
        );
        return false;
    }
    return ruda_checker_private_function_emit_enum_construction(
        function,
        enum_declaration,
        variant_index,
        NULL,
        0,
        node->start,
        node->end,
        result
    );
}

 bool ruda_checker_private_compile_enum_positional(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *member_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const RudaAstNode *arguments_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_ARGUMENTS,
        0
    );
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    RudaMirEnumArgument *arguments = NULL;
    size_t variant_index = 0;
    size_t argument_count = 0;
    size_t argument_index = 0;
    size_t argument_bytes = 0;
    size_t child_index = 0;
    bool accepted = false;
    if (resolved == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    if (member_node == NULL) {
        return true;
    }
    if (!ruda_checker_private_resolve_enum_member(
            function,
            member_node,
            &enum_declaration,
            &variant,
            &variant_index,
            resolved
        )) {
        goto cleanup;
    }
    if (!*resolved) {
        accepted = true;
        goto cleanup;
    }
    if (function->constant_evaluation) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (arguments_node == NULL || variant == NULL
        || variant->kind != RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant is not a positional payload constructor",
            node->start,
            node->end
        );
        goto cleanup;
    }
    for (child_index = 0; child_index < arguments_node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            arguments_node,
            child_index
        );
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (element->as.node == NULL
                || element->as.node->kind != RUDA_SYNTAX_EXPRESSION
                || argument_count == SIZE_MAX) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
                goto cleanup;
            }
            argument_count += 1;
        } else if (element->as.token.synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
    }
    if (argument_count != variant->payload.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_CALL_ARITY,
            "positional enum payload count does not match its declaration",
            arguments_node->start,
            arguments_node->end
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            argument_count,
            sizeof(RudaMirEnumArgument),
            &argument_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (argument_bytes != 0) {
        arguments = ruda_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        if (arguments == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    for (child_index = 0; child_index < arguments_node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            arguments_node,
            child_index
        );
        const RudaCheckEnumPayloadItemDeclaration *payload = NULL;
        RudaTypeId payload_type = RUDA_TYPE_INVALID;
        RudaExpressionValue value;
        if (element == NULL || element->kind != RUDA_AST_ELEMENT_NODE) {
            continue;
        }
        payload = ruda_vector_at_const(&variant->payload, argument_index);
        if (payload == NULL || arguments == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        payload_type = payload->type;
        memset(&value, 0, sizeof(value));
        if (!ruda_compile_expression(
                function,
                element->as.node,
                payload_type,
                &value
            )) {
            goto cleanup;
        }
        if (ruda_expression_is_never(function, &value)) {
            *result = value;
            accepted = true;
            goto cleanup;
        }
        if (value.type != payload_type) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "enum payload value type does not match its declaration",
                element->as.node->start,
                element->as.node->end
            );
            goto cleanup;
        }
        arguments[argument_index].register_id = value.register_id;
        arguments[argument_index].type = value.type;
        argument_index += 1;
    }
    if (argument_index != argument_count) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    accepted = ruda_checker_private_function_emit_enum_construction(
        function,
        enum_declaration,
        variant_index,
        arguments,
        argument_count,
        node->start,
        node->end,
        result
    );

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}


 bool ruda_checker_private_compile_enum_named(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    bool *resolved,
    RudaExpressionValue *result
) {
    const RudaAstNode *member_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const RudaAstNode *fields_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_RECORD_FIELDS,
        0
    );
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    RudaMirEnumArgument *arguments = NULL;
    unsigned char *seen = NULL;
    size_t variant_index = 0;
    size_t argument_bytes = 0;
    size_t seen_bytes = 0;
    size_t child_index = 0;
    size_t supplied_count = 0;
    bool accepted = false;
    if (resolved == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    if (member_node == NULL) {
        return true;
    }
    if (!ruda_checker_private_resolve_enum_member(
            function,
            member_node,
            &enum_declaration,
            &variant,
            &variant_index,
            resolved
        )) {
        goto cleanup;
    }
    if (!*resolved) {
        accepted = true;
        goto cleanup;
    }
    if (function->constant_evaluation) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (fields_node == NULL || variant == NULL
        || variant->kind != RUDA_CHECKED_ENUM_VARIANT_NAMED) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant is not a named payload constructor",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            variant->payload.length,
            sizeof(RudaMirEnumArgument),
            &argument_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    seen_bytes = variant->payload.length;
    if (argument_bytes != 0) {
        arguments = ruda_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        seen = ruda_allocate(
            &function->checker->result->allocator,
            seen_bytes
        );
        if (arguments == NULL || seen == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen, 0, seen_bytes);
    }
    for (child_index = 0; child_index < fields_node->child_count;
        child_index += 1) {
        const RudaAstElement *element = ruda_ast_child(
            fields_node,
            child_index
        );
        const RudaAstNode *field_node = NULL;
        const RudaAstNode *expression = NULL;
        const RudaSyntaxToken *field_token = NULL;
        RudaCheckEnumPayloadItemDeclaration *payload = NULL;
        RudaTypeId payload_type = RUDA_TYPE_INVALID;
        RudaCheckName field_name;
        RudaExpressionValue value;
        size_t payload_index = 0;
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind != RUDA_AST_ELEMENT_NODE) {
            if (element->as.token.synthetic) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            continue;
        }
        field_node = element->as.node;
        if (field_node == NULL || field_node->kind != RUDA_SYNTAX_FIELD_VALUE) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        field_token = ruda_checker_private_check_child_token(
            field_node,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        payload = ruda_checker_private_checker_find_enum_payload(
            function->checker,
            variant,
            field_name,
            &payload_index
        );
        if (payload == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
                "enum variant has no payload field with this name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || arguments == NULL
            || payload_index >= variant->payload.length) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        payload_type = payload->type;
        if (seen[payload_index] != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
                "enum payload field is supplied more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        expression = ruda_checker_private_check_child_node(
            field_node,
            RUDA_SYNTAX_EXPRESSION,
            0
        );
        memset(&value, 0, sizeof(value));
        value.register_id = RUDA_MIR_NO_REGISTER;
        if (expression != NULL) {
            if (!ruda_compile_expression(
                    function,
                    expression,
                    payload_type,
                    &value
                )) {
                goto cleanup;
            }
            if (ruda_expression_is_never(function, &value)) {
                *result = value;
                accepted = true;
                goto cleanup;
            }
        } else {
            const RudaCheckLocal *local = ruda_checker_private_function_find_local(
                function,
                field_name
            );
            if (local == NULL) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_UNKNOWN_NAME,
                    "enum payload shorthand requires a visible local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            if (!local->initialized) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
                    "enum payload shorthand reads an uninitialized local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            value.type = local->type;
            value.register_id = local->register_id;
            value.valid = true;
        }
        if (value.type != payload_type) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "enum payload value type does not match its declaration",
                expression != NULL ? expression->start : field_name.start,
                expression != NULL ? expression->end : field_name.end
            );
            goto cleanup;
        }
        arguments[payload_index].register_id = value.register_id;
        arguments[payload_index].type = value.type;
        seen[payload_index] = 1;
        supplied_count += 1;
    }
    if (supplied_count != variant->payload.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_MISSING_FIELD,
            "named enum payload construction omits a required field",
            node->start,
            node->end
        );
        goto cleanup;
    }
    accepted = ruda_checker_private_function_emit_enum_construction(
        function,
        enum_declaration,
        variant_index,
        arguments,
        variant->payload.length,
        node->start,
        node->end,
        result
    );

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    ruda_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}
