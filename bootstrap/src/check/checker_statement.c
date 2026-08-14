#include "checker_statement_api.h"
#include "checker_core_api.h"
#include "checker_expr_records_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"






 bool ruda_checker_private_compile_binding(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaSyntaxToken *discard = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_UNDERSCORE,
        0
    );
    const RudaAstNode *type_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_TYPE,
        0
    );
    const RudaAstNode *expression = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaCheckName name;
    RudaTypeId declared_type = RUDA_TYPE_INVALID;
    RudaTypeId local_type = RUDA_TYPE_INVALID;
    RudaExpressionValue value;
    RudaCheckLocal *local = NULL;
    size_t diagnostics_before = function->checker->result->diagnostics.length;
    bool has_value = false;
    memset(&value, 0, sizeof(value));
    if (discard != NULL) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (ruda_checker_private_function_find_local_in_current_scope(function, name) != NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            name.start,
            name.end
        );
        return false;
    }
    if (type_node != NULL
        && !ruda_checker_private_check_exact_type(
            function->checker,
            type_node,
            &declared_type
        ) && ruda_checker_has_failed(function->checker)) {
        return false;
    }
    if (expression != NULL) {
        has_value = ruda_compile_expression(
            function,
            expression,
            declared_type,
            &value
        );
        if (!has_value && ruda_checker_has_failed(function->checker)) {
            return false;
        }
        if (has_value && ruda_expression_is_never(function, &value)) {
            return true;
        }
    }
    if (declared_type == RUDA_TYPE_INVALID && expression == NULL) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    local_type = declared_type != RUDA_TYPE_INVALID
        ? declared_type
        : value.type;
    if (has_value && declared_type != RUDA_TYPE_INVALID
        && value.type != declared_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "initializer type does not match the binding type",
            expression->start,
            expression->end
        );
        has_value = false;
    }
    if (local_type == RUDA_TYPE_INVALID) {
        return false;
    }
    if (!ruda_checker_private_function_add_local(
            function,
            name,
            local_type,
            node->kind == RUDA_SYNTAX_LET_DECL,
            expression != NULL,
            &local
        )) {
        return false;
    }
    if (has_value && local != NULL) {
        return ruda_checker_private_function_emit_copy(
            function,
            local->register_id,
            value.register_id,
            expression->start,
            expression->end
        );
    }
    return function->checker->result->diagnostics.length > diagnostics_before
        || expression == NULL;
}

bool ruda_compound_assignment_kind(
    RudaTypeId type,
    RudaTokenKind operator_kind,
    RudaMirInstructionKind *kind
) {
    if (kind == NULL) {
        return false;
    }
    if (type == RUDA_TYPE_I64) {
        switch (operator_kind) {
            case RUDA_TOKEN_PLUS_EQUAL:
                *kind = RUDA_MIR_ADD_CHECKED_I64;
                return true;
            case RUDA_TOKEN_MINUS_EQUAL:
                *kind = RUDA_MIR_SUBTRACT_CHECKED_I64;
                return true;
            case RUDA_TOKEN_STAR_EQUAL:
                *kind = RUDA_MIR_MULTIPLY_CHECKED_I64;
                return true;
            case RUDA_TOKEN_SLASH_EQUAL:
                *kind = RUDA_MIR_DIVIDE_CHECKED_I64;
                return true;
            case RUDA_TOKEN_PERCENT_EQUAL:
                *kind = RUDA_MIR_REMAINDER_CHECKED_I64;
                return true;
            case RUDA_TOKEN_AMPERSAND_EQUAL:
                *kind = RUDA_MIR_BITWISE_AND_I64;
                return true;
            case RUDA_TOKEN_CARET_EQUAL:
                *kind = RUDA_MIR_BITWISE_XOR_I64;
                return true;
            case RUDA_TOKEN_PIPE_EQUAL:
                *kind = RUDA_MIR_BITWISE_OR_I64;
                return true;
            case RUDA_TOKEN_SHIFT_LEFT_EQUAL:
                *kind = RUDA_MIR_SHIFT_LEFT_CHECKED_I64;
                return true;
            case RUDA_TOKEN_SHIFT_RIGHT_EQUAL:
                *kind = RUDA_MIR_SHIFT_RIGHT_I64;
                return true;
            default:
                return false;
        }
    }
    if (type == RUDA_TYPE_F64) {
        switch (operator_kind) {
            case RUDA_TOKEN_PLUS_EQUAL:
                *kind = RUDA_MIR_ADD_F64;
                return true;
            case RUDA_TOKEN_MINUS_EQUAL:
                *kind = RUDA_MIR_SUBTRACT_F64;
                return true;
            case RUDA_TOKEN_STAR_EQUAL:
                *kind = RUDA_MIR_MULTIPLY_F64;
                return true;
            case RUDA_TOKEN_SLASH_EQUAL:
                *kind = RUDA_MIR_DIVIDE_F64;
                return true;
            default:
                return false;
        }
    }
    return false;
}

 bool ruda_checker_private_compile_record_assignment(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    const RudaAstNode *left,
    const RudaAstNode *right,
    const RudaSyntaxToken *operator_token,
    RudaMirInstructionKind compound_kind,
    bool compound
) {
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *field_token = NULL;
    RudaExpressionValue receiver;
    RudaExpressionValue previous;
    RudaExpressionValue value;
    RudaExpressionValue combined;
    RudaExpressionValue stored;
    RudaCheckRecordDeclaration *record_declaration = NULL;
    RudaCheckRecordFieldDeclaration *field_declaration = NULL;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    size_t field_index = 0;
    memset(&receiver, 0, sizeof(receiver));
    memset(&previous, 0, sizeof(previous));
    memset(&value, 0, sizeof(value));
    memset(&combined, 0, sizeof(combined));
    memset(&stored, 0, sizeof(stored));
    if (!ruda_checker_private_member_parts(left, &receiver_node, &field_token)
        || !ruda_checker_private_resolve_record_member(
            function,
            left,
            &receiver,
            &record_declaration,
            &field_declaration,
            &record,
            &field_index
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &receiver)) {
        return true;
    }
    (void)receiver_node;
    (void)record_declaration;
    if (!field_declaration->is_mutable) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_ASSIGN_IMMUTABLE,
            "cannot assign to an immutable record field",
            field_token->start,
            field_token->end
        );
        return false;
    }
    if (compound && !ruda_compound_assignment_kind(
            field_declaration->type,
            operator_token->kind,
            &compound_kind
        )) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the record field type",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (compound && !ruda_checker_private_function_emit_record_access(
            function,
            RUDA_MIR_RECORD_GET,
            field_declaration->type,
            receiver.register_id,
            RUDA_MIR_NO_REGISTER,
            record,
            field_index,
            left->start,
            left->end,
            &previous
        )) {
        return false;
    }
    if (!ruda_compile_expression(
            function,
            right,
            field_declaration->type,
            &value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type != field_declaration->type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "assigned value type does not match the record field type",
            right->start,
            right->end
        );
        return false;
    }
    if (compound && !ruda_function_emit_instruction(
            function,
            compound_kind,
            field_declaration->type,
            previous.register_id,
            value.register_id,
            0,
            false,
            node->start,
            node->end,
            &combined
        )) {
        return false;
    }
    return ruda_checker_private_function_emit_record_access(
        function,
        RUDA_MIR_RECORD_SET,
        field_declaration->type,
        receiver.register_id,
        compound ? combined.register_id : value.register_id,
        record,
        field_index,
        node->start,
        node->end,
        &stored
    );
}

 bool ruda_checker_private_compile_assignment(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *left = NULL;
    const RudaAstNode *right = NULL;
    const RudaSyntaxToken *operator_token = NULL;
    const RudaSyntaxToken *name_token = NULL;
    RudaCheckName name;
    RudaCheckLocal *local = NULL;
    RudaExpressionValue value;
    RudaExpressionValue combined;
    RudaMirInstructionKind compound_kind = RUDA_MIR_COPY;
    bool compound = false;
    if (!ruda_checker_private_binary_parts(node, &left, &operator_token, &right)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    switch (operator_token->kind) {
        case RUDA_TOKEN_EQUAL:
            break;
        case RUDA_TOKEN_PLUS_EQUAL:
            compound_kind = RUDA_MIR_ADD_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_MINUS_EQUAL:
            compound_kind = RUDA_MIR_SUBTRACT_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_STAR_EQUAL:
            compound_kind = RUDA_MIR_MULTIPLY_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_SLASH_EQUAL:
            compound_kind = RUDA_MIR_DIVIDE_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_PERCENT_EQUAL:
            compound_kind = RUDA_MIR_REMAINDER_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_AMPERSAND_EQUAL:
            compound_kind = RUDA_MIR_BITWISE_AND_I64;
            compound = true;
            break;
        case RUDA_TOKEN_CARET_EQUAL:
            compound_kind = RUDA_MIR_BITWISE_XOR_I64;
            compound = true;
            break;
        case RUDA_TOKEN_PIPE_EQUAL:
            compound_kind = RUDA_MIR_BITWISE_OR_I64;
            compound = true;
            break;
        case RUDA_TOKEN_SHIFT_LEFT_EQUAL:
            compound_kind = RUDA_MIR_SHIFT_LEFT_CHECKED_I64;
            compound = true;
            break;
        case RUDA_TOKEN_SHIFT_RIGHT_EQUAL:
            compound_kind = RUDA_MIR_SHIFT_RIGHT_I64;
            compound = true;
            break;
        default:
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
    }
    if (left->kind == RUDA_SYNTAX_MEMBER_EXPRESSION) {
        return ruda_checker_private_compile_record_assignment(
            function,
            node,
            left,
            right,
            operator_token,
            compound_kind,
            compound
        );
    }
    if (left->kind == RUDA_SYNTAX_INDEX_EXPRESSION) {
        return ruda_compile_array_assignment(
            function,
            node,
            left,
            right,
            operator_token,
            compound
        );
    }
    if (left->kind != RUDA_SYNTAX_NAME_EXPRESSION) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    name_token = ruda_checker_private_check_child_token(left, RUDA_TOKEN_IDENTIFIER, 0);
    if (name_token == NULL || name_token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    local = ruda_checker_private_function_find_local_mutable(function, name);
    if (local == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            name.start,
            name.end
        );
        return false;
    }
    if (!local->mutable_binding) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_ASSIGN_IMMUTABLE,
            "cannot assign to a const binding",
            name.start,
            name.end
        );
        return false;
    }
    if (compound && !local->initialized) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
            "local is used before it is initialized",
            name.start,
            name.end
        );
        return false;
    }
    if (compound && !ruda_compound_assignment_kind(
            local->type,
            operator_token->kind,
            &compound_kind
        )) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    memset(&value, 0, sizeof(value));
    memset(&combined, 0, sizeof(combined));
    if (!ruda_compile_expression(function, right, local->type, &value)) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type != local->type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "assigned value type does not match the binding type",
            right->start,
            right->end
        );
        return false;
    }
    if (compound && !ruda_function_emit_instruction(
            function,
            compound_kind,
            local->type,
            local->register_id,
            value.register_id,
            0,
            false,
            node->start,
            node->end,
            &combined
        )) {
        return false;
    }
    if (!ruda_checker_private_function_emit_copy(
            function,
            local->register_id,
            compound ? combined.register_id : value.register_id,
            right->start,
            right->end
        )) {
        return false;
    }
    local->initialized = true;
    return true;
}

 bool ruda_checker_private_compile_expression_statement(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *expression = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    const RudaAstNode *inner = expression != NULL
        ? ruda_checker_private_expression_only_node(expression)
        : NULL;
    RudaExpressionValue value;
    if (inner == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (inner->kind == RUDA_SYNTAX_CALL_EXPRESSION) {
        memset(&value, 0, sizeof(value));
        return ruda_compile_expression(
            function,
            inner,
            RUDA_TYPE_INVALID,
            &value
        );
    }
    if (inner->kind == RUDA_SYNTAX_NAME_EXPRESSION) {
        const RudaSyntaxToken *token = ruda_checker_private_check_child_token(
            inner,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        memset(&value, 0, sizeof(value));
        if (token != NULL && ruda_checker_private_check_text_equal(
                function->checker,
                token->start,
                token->end,
                "panic"
            )) {
            return ruda_compile_expression(
                function,
                inner,
                RUDA_TYPE_INVALID,
                &value
            );
        }
    }
    if (inner->kind != RUDA_SYNTAX_ASSIGNMENT_EXPRESSION) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    return ruda_checker_private_compile_assignment(function, inner);
}

 bool ruda_checker_private_function_set_goto(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirBlockId target,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_GOTO;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.target = target;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(&function->mir, block, terminator)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return true;
}

 bool ruda_checker_private_function_set_branch(
    RudaFunctionChecker *function,
    RudaMirBlockId block,
    RudaMirRegister condition,
    RudaMirBlockId when_true,
    RudaMirBlockId when_false,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_BRANCH;
    terminator.value = condition;
    terminator.target = when_true;
    terminator.otherwise = when_false;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(&function->mir, block, terminator)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return true;
}

 bool ruda_checker_private_function_set_return(
    RudaFunctionChecker *function,
    RudaMirRegister value,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_RETURN;
    terminator.value = value;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}

 bool ruda_checker_private_function_set_panic(
    RudaFunctionChecker *function,
    RudaMirRegister message,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_PANIC;
    terminator.value = message;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}

 bool ruda_checker_private_function_set_never_call(
    RudaFunctionChecker *function,
    RudaMirCallId call,
    size_t start,
    size_t end
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = RUDA_MIR_CALL_NEVER;
    terminator.value = RUDA_MIR_NO_REGISTER;
    terminator.call = call;
    terminator.target = RUDA_MIR_NO_BLOCK;
    terminator.otherwise = RUDA_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!ruda_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}
