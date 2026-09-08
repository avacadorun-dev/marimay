#include "checker_closure_api.h"
#include "checker_expr_records_api.h"
#include "checker_core_api.h"
#include "checker_imported_nominal_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"
#include "checker_generic_types_api.h"





 bool mmay_checker_private_compile_record_construction(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *name_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_NAME_EXPRESSION,
        0
    );
    const MarimayAstNode *fields_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_RECORD_FIELDS,
        0
    );
    const MarimaySyntaxToken *type_token = mmay_checker_private_check_child_token(
        name_node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    MarimayCheckName type_name;
    MarimayCheckName namespace_name = {0};
    const MarimayCheckedRecord *imported = NULL;
    MarimayTypeId imported_type = MMAY_TYPE_INVALID;
    bool has_namespace = false;
    bool exists_private = false;
    MarimayCheckRecordDeclaration *declaration = NULL;
    MarimayCheckRecordDeclaration concrete;
    MarimayCheckTypeAliasDeclaration *alias = NULL;
    MarimayMirRecordArgument *arguments = NULL;
    unsigned char *seen = NULL;
    size_t argument_bytes = 0;
    size_t seen_bytes = 0;
    size_t new_argument_count = 0;
    size_t child_index = 0;
    size_t supplied_count = 0;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirRecordConstructionId construction =
        MMAY_MIR_NO_RECORD_CONSTRUCTION;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    bool accepted = false;
    if (function->constant_evaluation) {
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_INCOMPLETE_LANGUAGE
        );
        goto cleanup;
    }
    if (name_node == NULL) {
        const MarimayAstNode *member = mmay_checker_private_check_child_node(
            node, MMAY_SYNTAX_MEMBER_EXPRESSION, 0
        );
        const MarimaySyntaxToken *namespace_token = NULL;
        if (member != NULL && mmay_checker_private_member_parts(
                member, &name_node, &type_token
            ) && name_node != NULL
            && name_node->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
            namespace_token = mmay_checker_private_check_child_token(
                name_node, MMAY_TOKEN_IDENTIFIER, 0
            );
        }
        if (namespace_token != NULL && !namespace_token->synthetic) {
            namespace_name.snapshot = function->checker->ast->snapshot;
            namespace_name.start = namespace_token->start;
            namespace_name.end = namespace_token->end;
            has_namespace = true;
        } else {
            name_node = NULL;
        }
    }
    if (name_node == NULL || fields_node == NULL || type_token == NULL
        || type_token->synthetic) {
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_INCOMPLETE_LANGUAGE
        );
        goto cleanup;
    }
    type_name.snapshot = function->checker->ast->snapshot;
    type_name.start = type_token->start;
    type_name.end = type_token->end;
    if (!has_namespace) {
        declaration = mmay_checker_private_checker_find_record(function->checker, type_name);
    }
    if (declaration == NULL && !has_namespace) {
        alias = mmay_checker_private_checker_find_type_alias(function->checker, type_name);
        if (alias != NULL && mmay_checker_private_evaluate_type_alias(
                function->checker,
                alias,
                type_name.start,
                type_name.end
            )) {
            declaration = mmay_checker_private_checker_find_record_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (declaration == NULL) {
        imported = alias != NULL
            ? mmay_check_imported_record_by_type(function->checker, alias->type)
            : mmay_check_imported_record_by_name(function->checker,
                namespace_name, has_namespace, type_name,
                &imported_type, &exists_private);
        if (alias != NULL) imported_type = alias->type;
    }
    if (declaration == NULL && imported == NULL) {
        if (exists_private) {
            (void)mmay_checker_add_diagnostic(function->checker,
                MMAY_DIAGNOSTIC_PRIVATE_IMPORT,
                "record type is package-private in the owning package",
                type_name.start, type_name.end);
        } else if (mmay_checker_private_checker_find_top_level(
                function->checker, type_name) != NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "constructed name is not a nominal record type",
                type_name.start,
                type_name.end
            );
        } else {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_TYPE,
                "unknown record type name",
                type_name.start,
                type_name.end
            );
        }
        goto cleanup;
    }
    if (imported != NULL) {
        memset(&concrete, 0, sizeof(concrete));
        concrete.type = imported_type;
        concrete.symbol = imported->symbol;
        concrete.type_parameters = imported->type_parameters;
        concrete.fields.length = imported->fields.length;
        concrete.is_generic = imported->type_parameters.length != 0
            && imported_type == imported->type;
        declaration = &concrete;
    } else {
        concrete = *declaration;
    }
    if (declaration->is_generic) {
        const MarimayTypeDescriptor *descriptor = expected == MMAY_TYPE_INVALID
            ? NULL : mmay_type_descriptor(
                &function->checker->result->types, expected
            );
        if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
            || !mmay_symbol_id_equal(descriptor->symbol, declaration->symbol)
            || descriptor->argument_count != declaration->type_parameters.length) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "generic record construction requires an expected concrete record type",
                node->start,
                node->end
            );
            goto cleanup;
        }
        concrete.type = expected;
    }
    if (!mmay_size_multiply(
            declaration->fields.length,
            sizeof(MarimayMirRecordArgument),
            &argument_bytes
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    seen_bytes = declaration->fields.length;
    if (argument_bytes != 0) {
        arguments = mmay_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        if (arguments == NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(arguments, 0, argument_bytes);
    }
    if (seen_bytes != 0) {
        seen = mmay_allocate(
            &function->checker->result->allocator,
            seen_bytes
        );
        if (seen == NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen, 0, seen_bytes);
    }
    for (child_index = 0; child_index < fields_node->child_count;
        child_index += 1) {
        const MarimayAstElement *element = mmay_ast_child(
            fields_node,
            child_index
        );
        const MarimayAstNode *field_node = NULL;
        const MarimayAstNode *expression = NULL;
        const MarimaySyntaxToken *field_token = NULL;
        MarimayCheckName field_name;
        MarimayCheckRecordFieldDeclaration *field = NULL;
        MarimayCheckRecordFieldDeclaration imported_field;
        MarimayExpressionValue value;
        MarimayTypeId expected_field_type = MMAY_TYPE_INVALID;
        size_t field_index = 0;
        if (element == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind != MMAY_AST_ELEMENT_NODE) {
            if (element->as.token.synthetic) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            continue;
        }
        field_node = element->as.node;
        if (field_node == NULL || field_node->kind != MMAY_SYNTAX_FIELD_VALUE) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        field_token = mmay_checker_private_check_child_token(
            field_node,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        if (imported == NULL) {
            field = mmay_checker_private_checker_find_record_field(
                function->checker, declaration, field_name, &field_index
            );
        } else {
            const unsigned char *bytes = NULL;
            size_t length = 0;
            if (!mmay_checker_private_check_name_bytes(field_name, &bytes, &length)) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
                goto cleanup;
            }
            for (field_index = 0; field_index < imported->fields.length;
                field_index += 1) {
                const MarimayCheckedRecordField *candidate = mmay_vector_at_const(
                    &imported->fields, field_index
                );
                if (candidate != NULL && candidate->name.length == length
                    && memcmp(candidate->name.data, bytes, length) == 0) {
                    if (!candidate->is_public) {
                        (void)mmay_checker_add_diagnostic(function->checker,
                            MMAY_DIAGNOSTIC_PRIVATE_IMPORT,
                            "record field is package-private in the owning package",
                            field_name.start, field_name.end);
                        goto cleanup;
                    }
                    memset(&imported_field, 0, sizeof(imported_field));
                    imported_field.type = candidate->type;
                    field = &imported_field;
                    break;
                }
            }
        }
        if (field == NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_FIELD,
                "record type has no field with this name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || arguments == NULL
            || field_index >= declaration->fields.length) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[field_index] != 0) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_DUPLICATE_FIELD,
                "record field is supplied more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        expression = mmay_checker_private_check_child_node(
            field_node,
            MMAY_SYNTAX_EXPRESSION,
            0
        );
        memset(&value, 0, sizeof(value));
        value.register_id = MMAY_MIR_NO_REGISTER;
        if (!mmay_checker_private_substitute_nominal_member_type(
                function, concrete.type, &concrete.type_parameters,
                field->type, &expected_field_type
            )) {
            goto cleanup;
        }
        if (expression != NULL) {
            if (!mmay_compile_expression(
                    function,
                    expression,
                    expected_field_type,
                    &value
                )) {
                goto cleanup;
            }
            if (mmay_expression_is_never(function, &value)) {
                *result = value;
                accepted = true;
                goto cleanup;
            }
        } else {
            const MarimayCheckLocal *local = mmay_closure_find_local(
                function,
                field_name
            );
            if (local == NULL) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_UNKNOWN_NAME,
                    "record field shorthand requires a visible local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            if (!local->initialized) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_UNINITIALIZED_LOCAL,
                    "record field shorthand reads an uninitialized local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            if (!mmay_closure_read_local(function, local, field_name.start, field_name.end, &value)) goto cleanup;
        }
        if (value.type != expected_field_type) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
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
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_MISSING_FIELD,
            "record construction omits a required field",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (!(imported != NULL
            ? mmay_checker_private_imported_record_layout(
                function, imported, concrete.type, &record)
            : mmay_checker_private_function_record_layout(function, &concrete, &record))) {
        goto cleanup;
    }
    if (function->mir.record_constructions.length
            >= function->mir.options.max_record_constructions
        || !mmay_size_add(
            function->mir.record_arguments.length,
            concrete.fields.length,
            &new_argument_count
        ) || new_argument_count > function->mir.options.max_record_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!mmay_mir_add_record_construction(
            &function->mir,
            record,
            arguments,
            declaration->fields.length,
            &construction
        )) {
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_ALLOCATION_FAILED
        );
        goto cleanup;
    }
    if (!mmay_function_add_register(function, concrete.type, &target)) {
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_RECORD_NEW;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.record = record;
    instruction.record_construction = construction;
    instruction.span.start = node->start;
    instruction.span.end = node->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        goto cleanup;
    }
    result->type = concrete.type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    mmay_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    mmay_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_member_parts(
    const MarimayAstNode *node,
    const MarimayAstNode **receiver,
    const MarimaySyntaxToken **member
) {
    size_t index = 0;
    *receiver = NULL;
    *member = NULL;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const MarimayAstElement *element = mmay_ast_child(node, index);
        if (element == NULL) {
            return false;
        }
        if (element->kind == MMAY_AST_ELEMENT_NODE) {
            if (*receiver != NULL) {
                return false;
            }
            *receiver = element->as.node;
        } else if (element->as.token.kind == MMAY_TOKEN_IDENTIFIER) {
            if (*member != NULL) {
                return false;
            }
            *member = &element->as.token;
        }
    }
    return *receiver != NULL && *member != NULL && !(*member)->synthetic;
}

 bool mmay_checker_private_resolve_record_member(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue *receiver,
    MarimayCheckRecordDeclaration **record_declaration,
    MarimayCheckRecordFieldDeclaration **field_declaration,
    const MarimayCheckedRecordField **imported_field,
    MarimayMirRecordId *record,
    size_t *field_index
) {
    const MarimayAstNode *receiver_node = NULL;
    const MarimaySyntaxToken *field_token = NULL;
    MarimayCheckName field_name;
    const MarimayCheckedRecord *imported_record = NULL;
    size_t imported_index = 0;
    const unsigned char *field_bytes = NULL;
    size_t field_length = 0;
    MarimayCheckRecordDeclaration concrete;
    *record_declaration = NULL;
    *field_declaration = NULL;
    *imported_field = NULL;
    if (!mmay_checker_private_member_parts(node, &receiver_node, &field_token)
        || !mmay_compile_expression(
            function,
            receiver_node,
            MMAY_TYPE_INVALID,
            receiver
        )) {
        if (!mmay_checker_has_failed(function->checker)
            && function->checker->result->error_count
                == function->diagnostic_start) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INVALID_ARGUMENT
            );
        }
        return false;
    }
    if (mmay_expression_is_never(function, receiver)) {
        return true;
    }
    *record_declaration = mmay_checker_private_checker_find_record_by_type(
        function->checker,
        receiver->type
    );
    if (*record_declaration == NULL) {
        imported_record = mmay_check_imported_record_by_type(
            function->checker, receiver->type
        );
        if (imported_record == NULL) {
            (void)mmay_checker_add_diagnostic(function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "field access requires a record value", receiver_node->start, receiver_node->end);
            return false;
        }
    }
    field_name.snapshot = function->checker->ast->snapshot;
    field_name.start = field_token->start;
    field_name.end = field_token->end;
    if (*record_declaration != NULL) {
        *field_declaration = mmay_checker_private_checker_find_record_field(
            function->checker,
            *record_declaration,
            field_name,
            field_index
        );
    } else if (mmay_checker_private_check_name_bytes(
            field_name, &field_bytes, &field_length
        )) {
        for (imported_index = 0; imported_index < imported_record->fields.length;
            imported_index += 1) {
            const MarimayCheckedRecordField *candidate = mmay_vector_at_const(
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
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_UNKNOWN_FIELD,
            "record type has no field with this name",
            field_name.start,
            field_name.end
        );
        return false;
    }
    if (*imported_field != NULL && !(*imported_field)->is_public) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_PRIVATE_IMPORT,
            "record field is package-private in the owning package",
            field_name.start,
            field_name.end
        );
        return false;
    }
    if (imported_record != NULL) {
        return mmay_checker_private_imported_record_layout(
            function, imported_record, receiver->type, record
        );
    }
    if ((*record_declaration)->is_generic) {
        const MarimayTypeDescriptor *descriptor = mmay_type_descriptor(
            &function->checker->result->types, receiver->type
        );
        if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
            || !mmay_symbol_id_equal(
                descriptor->symbol, (*record_declaration)->symbol
            ) || descriptor->argument_count
                != (*record_declaration)->type_parameters.length) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        concrete = **record_declaration;
        concrete.type = receiver->type;
        return mmay_checker_private_function_record_layout(
            function, &concrete, record
        );
    }
    return mmay_checker_private_function_record_layout(
        function, *record_declaration, record
    );
}

 bool mmay_checker_private_compile_record_member(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayExpressionValue *result
) {
    MarimayExpressionValue receiver;
    MarimayCheckRecordDeclaration *record_declaration = NULL;
    MarimayCheckRecordFieldDeclaration *field_declaration = NULL;
    const MarimayCheckedRecordField *imported_field = NULL;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    size_t field_index = 0;
    memset(&receiver, 0, sizeof(receiver));
    if (!mmay_checker_private_resolve_record_member(
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
    if (mmay_expression_is_never(function, &receiver)) {
        *result = receiver;
        return true;
    }
    {
        MarimayTypeId field_type = field_declaration != NULL
            ? field_declaration->type : imported_field->type;
        if (field_declaration != NULL && record_declaration != NULL
            && record_declaration->is_generic
            && !mmay_checker_private_substitute_nominal_member_type(
                function,
                receiver.type,
                &record_declaration->type_parameters,
                field_type,
                &field_type
            )) {
            return false;
        }
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
        return mmay_checker_private_function_emit_record_access(
            function,
            MMAY_MIR_RECORD_GET,
            field_type,
            receiver.register_id,
            MMAY_MIR_NO_REGISTER,
            record,
            field_index,
            node->start,
            node->end,
            result
        );
    }
}
