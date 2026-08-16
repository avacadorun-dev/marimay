#include "checker_expr_records_api.h"
#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"





 bool ruda_checker_private_compile_record_construction(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    const RudaAstNode *name_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_NAME_EXPRESSION,
        0
    );
    const RudaAstNode *fields_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_RECORD_FIELDS,
        0
    );
    const RudaSyntaxToken *type_token = ruda_checker_private_check_child_token(
        name_node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    RudaCheckName type_name;
    RudaCheckRecordDeclaration *declaration = NULL;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    RudaMirRecordArgument *arguments = NULL;
    unsigned char *seen = NULL;
    size_t argument_bytes = 0;
    size_t seen_bytes = 0;
    size_t new_argument_count = 0;
    size_t child_index = 0;
    size_t supplied_count = 0;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    RudaMirRecordConstructionId construction =
        RUDA_MIR_NO_RECORD_CONSTRUCTION;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaMirInstruction instruction;
    bool accepted = false;
    if (function->constant_evaluation) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        goto cleanup;
    }
    if (name_node == NULL || fields_node == NULL || type_token == NULL
        || type_token->synthetic) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_INCOMPLETE_LANGUAGE
        );
        goto cleanup;
    }
    type_name.snapshot = function->checker->ast->snapshot;
    type_name.start = type_token->start;
    type_name.end = type_token->end;
    declaration = ruda_checker_private_checker_find_record(function->checker, type_name);
    if (declaration == NULL) {
        alias = ruda_checker_private_checker_find_type_alias(function->checker, type_name);
        if (alias != NULL && ruda_checker_private_evaluate_type_alias(
                function->checker,
                alias,
                type_name.start,
                type_name.end
            )) {
            declaration = ruda_checker_private_checker_find_record_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (declaration == NULL) {
        if (ruda_checker_private_checker_find_top_level(function->checker, type_name) != NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "constructed name is not a nominal record type",
                type_name.start,
                type_name.end
            );
        } else {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
                "unknown record type name",
                type_name.start,
                type_name.end
            );
        }
        goto cleanup;
    }
    if (!ruda_size_multiply(
            declaration->fields.length,
            sizeof(RudaMirRecordArgument),
            &argument_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    seen_bytes = declaration->fields.length;
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
        memset(arguments, 0, argument_bytes);
    }
    if (seen_bytes != 0) {
        seen = ruda_allocate(
            &function->checker->result->allocator,
            seen_bytes
        );
        if (seen == NULL) {
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
        RudaCheckName field_name;
        RudaCheckRecordFieldDeclaration *field = NULL;
        RudaExpressionValue value;
        size_t field_index = 0;
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
        field = ruda_checker_private_checker_find_record_field(
            function->checker,
            declaration,
            field_name,
            &field_index
        );
        if (field == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
                "record type has no field with this name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || arguments == NULL
            || field_index >= declaration->fields.length) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[field_index] != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
                "record field is supplied more than once",
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
                    field->type,
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
                    "record field shorthand requires a visible local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            if (!local->initialized) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_UNINITIALIZED_LOCAL,
                    "record field shorthand reads an uninitialized local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            value.type = local->type;
            value.register_id = local->register_id;
            value.valid = true;
        }
        if (value.type != field->type) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "record field value type does not match its declaration",
                expression != NULL ? expression->start : field_name.start,
                expression != NULL ? expression->end : field_name.end
            );
            goto cleanup;
        }
        arguments[field_index].register_id = value.register_id;
        arguments[field_index].type = value.type;
        seen[field_index] = 1;
        supplied_count += 1;
    }
    if (supplied_count != declaration->fields.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_MISSING_FIELD,
            "record construction omits a required field",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (!ruda_checker_private_function_record_layout(function, declaration, &record)) {
        goto cleanup;
    }
    if (function->mir.record_constructions.length
            >= function->mir.options.max_record_constructions
        || !ruda_size_add(
            function->mir.record_arguments.length,
            declaration->fields.length,
            &new_argument_count
        ) || new_argument_count > function->mir.options.max_record_arguments) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_mir_add_record_construction(
            &function->mir,
            record,
            arguments,
            declaration->fields.length,
            &construction
        )) {
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    if (!ruda_function_add_register(function, declaration->type, &target)) {
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_RECORD_NEW;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.record = record;
    instruction.record_construction = construction;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        goto cleanup;
    }
    result->type = declaration->type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

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

 bool ruda_checker_private_member_parts(
    const RudaAstNode *node,
    const RudaAstNode **receiver,
    const RudaSyntaxToken **member
) {
    size_t index = 0;
    *receiver = NULL;
    *member = NULL;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_NODE) {
            if (*receiver != NULL) {
                return false;
            }
            *receiver = element->as.node;
        } else if (element->as.token.kind == RUDA_TOKEN_IDENTIFIER) {
            if (*member != NULL) {
                return false;
            }
            *member = &element->as.token;
        }
    }
    return *receiver != NULL && *member != NULL && !(*member)->synthetic;
}

 bool ruda_checker_private_resolve_record_member(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *receiver,
    RudaCheckRecordDeclaration **record_declaration,
    RudaCheckRecordFieldDeclaration **field_declaration,
    const RudaCheckedRecordField **imported_field,
    RudaMirRecordId *record,
    size_t *field_index
) {
    const RudaAstNode *receiver_node = NULL;
    const RudaSyntaxToken *field_token = NULL;
    RudaCheckName field_name;
    const RudaCheckedRecord *imported_record = NULL;
    size_t imported_index = 0;
    const unsigned char *field_bytes = NULL;
    size_t field_length = 0;
    *record_declaration = NULL;
    *field_declaration = NULL;
    *imported_field = NULL;
    if (!ruda_checker_private_member_parts(node, &receiver_node, &field_token)
        || !ruda_compile_expression(
            function,
            receiver_node,
            RUDA_TYPE_INVALID,
            receiver
        )) {
        if (!ruda_checker_has_failed(function->checker)
            && function->checker->result->diagnostics.length
                == function->diagnostic_start) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
        }
        return false;
    }
    if (ruda_expression_is_never(function, receiver)) {
        return true;
    }
    *record_declaration = ruda_checker_private_checker_find_record_by_type(
        function->checker,
        receiver->type
    );
    if (*record_declaration == NULL) {
        imported_record = ruda_check_imported_record_by_type(
            function->checker, receiver->type
        );
        if (imported_record == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
    }
    field_name.snapshot = function->checker->ast->snapshot;
    field_name.start = field_token->start;
    field_name.end = field_token->end;
    if (*record_declaration != NULL) {
        *field_declaration = ruda_checker_private_checker_find_record_field(
            function->checker,
            *record_declaration,
            field_name,
            field_index
        );
    } else if (ruda_checker_private_check_name_bytes(
            field_name, &field_bytes, &field_length
        )) {
        for (imported_index = 0; imported_index < imported_record->fields.length;
            imported_index += 1) {
            const RudaCheckedRecordField *candidate = ruda_vector_at_const(
                &imported_record->fields, imported_index
            );
            if (candidate != NULL && candidate->name.length == field_length
                && memcmp(candidate->name.data, field_bytes, field_length) == 0) {
                *imported_field = candidate;
                *field_index = imported_index;
                break;
            }
        }
    }
    if (*field_declaration == NULL && *imported_field == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
            "record type has no field with this name",
            field_name.start,
            field_name.end
        );
        return false;
    }
    if (*imported_field != NULL && !(*imported_field)->is_public) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_PRIVATE_IMPORT,
            "record field is package-private in the owning package",
            field_name.start,
            field_name.end
        );
        return false;
    }
    if (imported_record != NULL) {
        return ruda_checker_private_imported_record_layout(
            function, imported_record, record
        );
    }
    return ruda_checker_private_function_record_layout(
        function,
        *record_declaration,
        record
    );
}

 bool ruda_checker_private_compile_record_member(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaExpressionValue *result
) {
    RudaExpressionValue receiver;
    RudaCheckRecordDeclaration *record_declaration = NULL;
    RudaCheckRecordFieldDeclaration *field_declaration = NULL;
    const RudaCheckedRecordField *imported_field = NULL;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    size_t field_index = 0;
    memset(&receiver, 0, sizeof(receiver));
    if (!ruda_checker_private_resolve_record_member(
            function,
            node,
            &receiver,
            &record_declaration,
            &field_declaration,
            &imported_field,
            &record,
            &field_index
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    (void)record_declaration;
    return ruda_checker_private_function_emit_record_access(
        function,
        RUDA_MIR_RECORD_GET,
        field_declaration != NULL ? field_declaration->type
            : imported_field->type,
        receiver.register_id,
        RUDA_MIR_NO_REGISTER,
        record,
        field_index,
        node->start,
        node->end,
        result
    );
}
