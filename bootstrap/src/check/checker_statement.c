#include "checker_statement_api.h"
#include "checker_core_api.h"
#include "checker_defer_api.h"
#include "checker_expr_records_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"
#include "package_imports_api.h"






 bool mmay_checker_private_compile_binding(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
) {
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    const MarimaySyntaxToken *discard = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_UNDERSCORE,
        0
    );
    const MarimayAstNode *type_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_TYPE,
        0
    );
    const MarimayAstNode *expression = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    MarimayCheckName name;
    MarimayTypeId declared_type = MMAY_TYPE_INVALID;
    MarimayTypeId local_type = MMAY_TYPE_INVALID;
    MarimayExpressionValue value;
    MarimayCheckLocal *local = NULL;
    size_t diagnostics_before = function->checker->result->diagnostics.length;
    bool has_value = false;
    memset(&value, 0, sizeof(value));
    if (discard != NULL) {
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    if (name_token == NULL || name_token->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (mmay_checker_private_function_find_local_in_current_scope(function, name) != NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_DUPLICATE_NAME,
            "name is already declared in this scope",
            name.start,
            name.end
        );
        return false;
    }
    if (type_node != NULL
        && !mmay_checker_private_check_exact_type(
            function->checker,
            type_node,
            &declared_type
        ) && mmay_checker_has_failed(function->checker)) {
        return false;
    }
    if (expression != NULL) {
        has_value = mmay_compile_expression(
            function,
            expression,
            declared_type,
            &value
        );
        if (!has_value && mmay_checker_has_failed(function->checker)) {
            return false;
        }
        if (has_value && mmay_expression_is_never(function, &value)) {
            return true;
        }
    }
    if (declared_type == MMAY_TYPE_INVALID && expression == NULL) {
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    local_type = declared_type != MMAY_TYPE_INVALID
        ? declared_type
        : value.type;
    if (has_value && declared_type != MMAY_TYPE_INVALID
        && value.type != declared_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "initializer type does not match the binding type",
            expression->start,
            expression->end
        );
        has_value = false;
    }
    if (local_type == MMAY_TYPE_INVALID) {
        return false;
    }
    if (!mmay_checker_private_function_add_local(
            function,
            name,
            local_type,
            node->kind == MMAY_SYNTAX_LET_DECL,
            expression != NULL,
            &local
        )) {
        return false;
    }
    if (has_value && local != NULL) {
        return mmay_checker_private_function_emit_copy(
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

bool mmay_compound_assignment_kind(
    MarimayTypeId type,
    MarimayTokenKind operator_kind,
    MarimayMirInstructionKind *kind
) {
    if (kind == NULL) {
        return false;
    }
    if (type >= MMAY_TYPE_I8 && type <= MMAY_TYPE_U64) {
        switch (operator_kind) {
            case MMAY_TOKEN_PLUS_EQUAL:
                *kind = MMAY_MIR_ADD_CHECKED_I64;
                return true;
            case MMAY_TOKEN_MINUS_EQUAL:
                *kind = MMAY_MIR_SUBTRACT_CHECKED_I64;
                return true;
            case MMAY_TOKEN_STAR_EQUAL:
                *kind = MMAY_MIR_MULTIPLY_CHECKED_I64;
                return true;
            case MMAY_TOKEN_SLASH_EQUAL:
                *kind = MMAY_MIR_DIVIDE_CHECKED_I64;
                return true;
            case MMAY_TOKEN_PERCENT_EQUAL:
                *kind = MMAY_MIR_REMAINDER_CHECKED_I64;
                return true;
            case MMAY_TOKEN_AMPERSAND_EQUAL:
                *kind = MMAY_MIR_BITWISE_AND_I64;
                return true;
            case MMAY_TOKEN_CARET_EQUAL:
                *kind = MMAY_MIR_BITWISE_XOR_I64;
                return true;
            case MMAY_TOKEN_PIPE_EQUAL:
                *kind = MMAY_MIR_BITWISE_OR_I64;
                return true;
            case MMAY_TOKEN_SHIFT_LEFT_EQUAL:
                *kind = MMAY_MIR_SHIFT_LEFT_CHECKED_I64;
                return true;
            case MMAY_TOKEN_SHIFT_RIGHT_EQUAL:
                *kind = MMAY_MIR_SHIFT_RIGHT_I64;
                return true;
            default:
                return false;
        }
    }
    if (type == MMAY_TYPE_F64) {
        switch (operator_kind) {
            case MMAY_TOKEN_PLUS_EQUAL:
                *kind = MMAY_MIR_ADD_F64;
                return true;
            case MMAY_TOKEN_MINUS_EQUAL:
                *kind = MMAY_MIR_SUBTRACT_F64;
                return true;
            case MMAY_TOKEN_STAR_EQUAL:
                *kind = MMAY_MIR_MULTIPLY_F64;
                return true;
            case MMAY_TOKEN_SLASH_EQUAL:
                *kind = MMAY_MIR_DIVIDE_F64;
                return true;
            default:
                return false;
        }
    }
    return false;
}

 bool mmay_checker_private_compile_record_assignment(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    const MarimayAstNode *left,
    const MarimayAstNode *right,
    const MarimaySyntaxToken *operator_token,
    MarimayMirInstructionKind compound_kind,
    bool compound
) {
    const MarimayAstNode *receiver_node = NULL;
    const MarimaySyntaxToken *field_token = NULL;
    MarimayExpressionValue receiver;
    MarimayExpressionValue previous;
    MarimayExpressionValue value;
    MarimayExpressionValue combined;
    MarimayExpressionValue stored;
    MarimayCheckRecordDeclaration *record_declaration = NULL;
    MarimayCheckRecordFieldDeclaration *field_declaration = NULL;
    const MarimayCheckedRecordField *imported_field = NULL;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayTypeId field_type = MMAY_TYPE_INVALID;
    size_t field_index = 0;
    memset(&receiver, 0, sizeof(receiver));
    memset(&previous, 0, sizeof(previous));
    memset(&value, 0, sizeof(value));
    memset(&combined, 0, sizeof(combined));
    memset(&stored, 0, sizeof(stored));
    if (!mmay_checker_private_member_parts(left, &receiver_node, &field_token)
        || !mmay_checker_private_resolve_record_member(
            function,
            left,
            &receiver,
            &record_declaration,
            &field_declaration,
            &imported_field,
            &record,
            &field_index
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &receiver)) {
        return true;
    }
    (void)receiver_node;
    field_type = field_declaration != NULL ? field_declaration->type
        : imported_field->type;
    if (record_declaration != NULL && record_declaration->is_generic
        && !mmay_checker_private_substitute_nominal_member_type(
            function,
            receiver.type,
            &record_declaration->type_parameters,
            field_type,
            &field_type
        )) return false;
    if (imported_field != NULL) {
        const MarimayCheckedRecord *imported_record =
            mmay_check_imported_record_by_type(
                function->checker, receiver.type
            );
        if (imported_record == NULL
            || !mmay_checker_private_substitute_nominal_member_type(
                function,
                receiver.type,
                &imported_record->type_parameters,
                field_type,
                &field_type
            )) return false;
    }
    if ((field_declaration != NULL && !field_declaration->is_mutable)
        || (imported_field != NULL && !imported_field->is_mutable)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_ASSIGN_IMMUTABLE,
            "cannot assign to an immutable record field",
            field_token->start,
            field_token->end
        );
        return false;
    }
    if (compound && !mmay_compound_assignment_kind(
            field_type,
            operator_token->kind,
            &compound_kind
        )) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the record field type",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    if (compound && !mmay_checker_private_function_emit_record_access(
            function,
            MMAY_MIR_RECORD_GET,
            field_type,
            receiver.register_id,
            MMAY_MIR_NO_REGISTER,
            record,
            field_index,
            left->start,
            left->end,
            &previous
        )) {
        return false;
    }
    if (!mmay_compile_expression(
            function,
            right,
            field_type,
            &value
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type != field_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "assigned value type does not match the record field type",
            right->start,
            right->end
        );
        return false;
    }
    if (compound && !mmay_function_emit_instruction(
            function,
            compound_kind,
            field_type,
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
    return mmay_checker_private_function_emit_record_access(
        function,
        MMAY_MIR_RECORD_SET,
        field_type,
        receiver.register_id,
        compound ? combined.register_id : value.register_id,
        record,
        field_index,
        node->start,
        node->end,
        &stored
    );
}

 bool mmay_checker_private_compile_assignment(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
) {
    const MarimayAstNode *left = NULL;
    const MarimayAstNode *right = NULL;
    const MarimaySyntaxToken *operator_token = NULL;
    const MarimaySyntaxToken *name_token = NULL;
    MarimayCheckName name;
    MarimayCheckLocal *local = NULL;
    MarimayExpressionValue value;
    MarimayExpressionValue combined;
    MarimayMirInstructionKind compound_kind = MMAY_MIR_COPY;
    bool compound = false;
    if (!mmay_checker_private_binary_parts(node, &left, &operator_token, &right)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    switch (operator_token->kind) {
        case MMAY_TOKEN_EQUAL:
            break;
        case MMAY_TOKEN_PLUS_EQUAL:
            compound_kind = MMAY_MIR_ADD_CHECKED_I64;
            compound = true;
            break;
        case MMAY_TOKEN_MINUS_EQUAL:
            compound_kind = MMAY_MIR_SUBTRACT_CHECKED_I64;
            compound = true;
            break;
        case MMAY_TOKEN_STAR_EQUAL:
            compound_kind = MMAY_MIR_MULTIPLY_CHECKED_I64;
            compound = true;
            break;
        case MMAY_TOKEN_SLASH_EQUAL:
            compound_kind = MMAY_MIR_DIVIDE_CHECKED_I64;
            compound = true;
            break;
        case MMAY_TOKEN_PERCENT_EQUAL:
            compound_kind = MMAY_MIR_REMAINDER_CHECKED_I64;
            compound = true;
            break;
        case MMAY_TOKEN_AMPERSAND_EQUAL:
            compound_kind = MMAY_MIR_BITWISE_AND_I64;
            compound = true;
            break;
        case MMAY_TOKEN_CARET_EQUAL:
            compound_kind = MMAY_MIR_BITWISE_XOR_I64;
            compound = true;
            break;
        case MMAY_TOKEN_PIPE_EQUAL:
            compound_kind = MMAY_MIR_BITWISE_OR_I64;
            compound = true;
            break;
        case MMAY_TOKEN_SHIFT_LEFT_EQUAL:
            compound_kind = MMAY_MIR_SHIFT_LEFT_CHECKED_I64;
            compound = true;
            break;
        case MMAY_TOKEN_SHIFT_RIGHT_EQUAL:
            compound_kind = MMAY_MIR_SHIFT_RIGHT_I64;
            compound = true;
            break;
        default:
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
    }
    if (left->kind == MMAY_SYNTAX_MEMBER_EXPRESSION) {
        return mmay_checker_private_compile_record_assignment(
            function,
            node,
            left,
            right,
            operator_token,
            compound_kind,
            compound
        );
    }
    if (left->kind == MMAY_SYNTAX_INDEX_EXPRESSION) {
        return mmay_compile_array_assignment(
            function,
            node,
            left,
            right,
            operator_token,
            compound
        );
    }
    if (left->kind != MMAY_SYNTAX_NAME_EXPRESSION) {
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    name_token = mmay_checker_private_check_child_token(left, MMAY_TOKEN_IDENTIFIER, 0);
    if (name_token == NULL || name_token->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    local = mmay_checker_private_function_find_local_mutable(function, name);
    if (local == NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_UNKNOWN_NAME,
            "unknown name",
            name.start,
            name.end
        );
        return false;
    }
    if (!local->mutable_binding) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_ASSIGN_IMMUTABLE,
            "cannot assign to a const binding",
            name.start,
            name.end
        );
        return false;
    }
    if (compound && !local->initialized) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_UNINITIALIZED_LOCAL,
            "local is used before it is initialized",
            name.start,
            name.end
        );
        return false;
    }
    if (compound && !mmay_compound_assignment_kind(
            local->type,
            operator_token->kind,
            &compound_kind
        )) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_INVALID_OPERATOR,
            "operator is not defined for the operand types",
            operator_token->start,
            operator_token->end
        );
        return false;
    }
    memset(&value, 0, sizeof(value));
    memset(&combined, 0, sizeof(combined));
    if (!mmay_compile_expression(function, right, local->type, &value)) {
        return false;
    }
    if (mmay_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type != local->type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "assigned value type does not match the binding type",
            right->start,
            right->end
        );
        return false;
    }
    if (compound && !mmay_function_emit_instruction(
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
    if (!mmay_checker_private_function_emit_copy(
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

 bool mmay_checker_private_compile_expression_statement(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
) {
    const MarimayAstNode *expression = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    const MarimayAstNode *inner = expression != NULL
        ? mmay_checker_private_expression_only_node(expression)
        : NULL;
    MarimayExpressionValue value;
    if (inner == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (inner->kind == MMAY_SYNTAX_CALL_EXPRESSION
        || inner->kind == MMAY_SYNTAX_TRY_EXPRESSION) {
        memset(&value, 0, sizeof(value));
        if (!mmay_compile_expression(
                function, inner, MMAY_TYPE_INVALID, &value
            )) return false;
        if (inner->kind == MMAY_SYNTAX_TRY_EXPRESSION
            && value.type != MMAY_TYPE_VOID
            && !mmay_expression_is_never(function, &value)) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "a standalone postfix '?' expression must produce void",
                inner->start, inner->end
            );
            return false;
        }
        return true;
    }
    if (inner->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(
            inner,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
        memset(&value, 0, sizeof(value));
        if (token != NULL && mmay_checker_private_check_text_equal(
                function->checker,
                token->start,
                token->end,
                "panic"
            )) {
            return mmay_compile_expression(
                function,
                inner,
                MMAY_TYPE_INVALID,
                &value
            );
        }
    }
    if (inner->kind != MMAY_SYNTAX_ASSIGNMENT_EXPRESSION) {
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_INCOMPLETE_LANGUAGE
        );
        return false;
    }
    return mmay_checker_private_compile_assignment(function, inner);
}

 bool mmay_checker_private_function_set_goto(
    MarimayFunctionChecker *function,
    MarimayMirBlockId block,
    MarimayMirBlockId target,
    size_t start,
    size_t end
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_GOTO;
    terminator.value = MMAY_MIR_NO_REGISTER;
    terminator.target = target;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!mmay_mir_set_terminator(&function->mir, block, terminator)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    return true;
}

 bool mmay_checker_private_function_set_branch(
    MarimayFunctionChecker *function,
    MarimayMirBlockId block,
    MarimayMirRegister condition,
    MarimayMirBlockId when_true,
    MarimayMirBlockId when_false,
    size_t start,
    size_t end
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_BRANCH;
    terminator.value = condition;
    terminator.target = when_true;
    terminator.otherwise = when_false;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!mmay_mir_set_terminator(&function->mir, block, terminator)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    return true;
}

 bool mmay_checker_private_function_set_return(
    MarimayFunctionChecker *function,
    MarimayMirRegister value,
    size_t start,
    size_t end
) {
    MarimayMirTerminator terminator;
    if (!mmay_checker_private_emit_defers_from(function, 0)) {
        return false;
    }
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_RETURN;
    terminator.value = value;
    terminator.target = MMAY_MIR_NO_BLOCK;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!mmay_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}

 bool mmay_checker_private_function_set_panic(
    MarimayFunctionChecker *function,
    MarimayMirRegister message,
    size_t start,
    size_t end
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_PANIC;
    terminator.value = message;
    terminator.target = MMAY_MIR_NO_BLOCK;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!mmay_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}

 bool mmay_checker_private_function_set_never_call(
    MarimayFunctionChecker *function,
    MarimayMirCallId call,
    size_t start,
    size_t end
) {
    MarimayMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = MMAY_MIR_CALL_NEVER;
    terminator.value = MMAY_MIR_NO_REGISTER;
    terminator.call = call;
    terminator.target = MMAY_MIR_NO_BLOCK;
    terminator.otherwise = MMAY_MIR_NO_BLOCK;
    terminator.span.start = start;
    terminator.span.end = end;
    if (!mmay_mir_set_terminator(
            &function->mir,
            function->block,
            terminator
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    function->terminated = true;
    return true;
}
