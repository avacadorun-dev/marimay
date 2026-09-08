#include "checker_closure_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_core_api.h"
#include "checker_expr_records_api.h"
#include "checker_lookup_api.h"
#include "checker_generic_types_api.h"
#include "package_imports_api.h"






 bool mmay_checker_private_resolve_enum_member(
    MarimayFunctionChecker *function,
    const MarimayAstNode *member_node,
    MarimayTypeId expected,
    MarimayCheckEnumDeclaration **enum_declaration,
    MarimayCheckEnumVariantDeclaration **variant_declaration,
    size_t *variant_index,
    bool *resolved
) {
    const MarimayAstNode *receiver_node = NULL;
    const MarimaySyntaxToken *type_token = NULL;
    const MarimaySyntaxToken *variant_token = NULL;
    MarimayCheckTypeAliasDeclaration *alias = NULL;
    MarimayCheckName type_name;
    MarimayCheckName variant_name;
    MarimayCheckName namespace_name;
    const MarimayCheckedEnum *imported = NULL;
    bool has_namespace = false;
    bool import_resolved = false;
    bool exists_private = false;
    if (function == NULL || enum_declaration == NULL
        || variant_declaration == NULL || variant_index == NULL
        || resolved == NULL) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *enum_declaration = NULL;
    *variant_declaration = NULL;
    *variant_index = 0;
    *resolved = false;
    if (!mmay_checker_private_member_parts(
            member_node, &receiver_node, &variant_token
        ) || receiver_node == NULL) {
        return true;
    }
    memset(&namespace_name, 0, sizeof(namespace_name));
    if (receiver_node->kind == MMAY_SYNTAX_NAME_EXPRESSION) {
        type_token = mmay_checker_private_check_child_token(
            receiver_node, MMAY_TOKEN_IDENTIFIER, 0
        );
    } else if (receiver_node->kind == MMAY_SYNTAX_MEMBER_EXPRESSION) {
        const MarimayAstNode *namespace_node = NULL;
        if (!mmay_checker_private_member_parts(
                receiver_node, &namespace_node, &type_token
            ) || namespace_node == NULL
            || namespace_node->kind != MMAY_SYNTAX_NAME_EXPRESSION) {
            return true;
        }
        {
            const MarimaySyntaxToken *namespace_token =
                mmay_checker_private_check_child_token(
                    namespace_node, MMAY_TOKEN_IDENTIFIER, 0
                );
            if (namespace_token == NULL || namespace_token->synthetic) {
                mmay_checker_fail(
                    function->checker, MMAY_CHECK_INVALID_ARGUMENT
                );
                return false;
            }
            namespace_name.snapshot = function->checker->ast->snapshot;
            namespace_name.start = namespace_token->start;
            namespace_name.end = namespace_token->end;
            has_namespace = true;
        }
    } else {
        return true;
    }
    if (type_token == NULL || type_token->synthetic || variant_token == NULL
        || variant_token->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    type_name.snapshot = function->checker->ast->snapshot;
    type_name.start = type_token->start;
    type_name.end = type_token->end;
    if (!has_namespace
        && mmay_checker_private_function_find_local(
            function, type_name
        ) != NULL) {
        return true;
    }
    if (!has_namespace) {
        *enum_declaration = mmay_checker_private_checker_find_enum(
            function->checker,
            type_name
        );
    }
    if (!has_namespace && *enum_declaration == NULL) {
        alias = mmay_checker_private_checker_find_type_alias(function->checker, type_name);
        if (alias != NULL && mmay_checker_private_evaluate_type_alias(
                function->checker,
                alias,
                type_name.start,
                type_name.end
            )) {
            *enum_declaration = mmay_checker_private_checker_find_enum_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (*enum_declaration == NULL) {
        imported = mmay_check_imported_enum_by_name(
            function->checker,
            namespace_name,
            has_namespace,
            type_name,
            &import_resolved,
            &exists_private,
            NULL
        );
        if (import_resolved && imported == NULL) {
            *resolved = true;
            (void)mmay_checker_add_diagnostic(
                function->checker,
                exists_private ? MMAY_DIAGNOSTIC_PRIVATE_IMPORT
                    : MMAY_DIAGNOSTIC_UNKNOWN_IMPORT,
                exists_private ? "imported enum type is package-private"
                    : "unknown public enum type in imported package",
                type_name.start,
                type_name.end
            );
            return false;
        }
        if (imported == NULL) {
            return !mmay_checker_has_failed(function->checker);
        }
        if (imported->type_parameters.length != 0) {
            const MarimayTypeDescriptor *descriptor = expected == MMAY_TYPE_INVALID
                ? NULL : mmay_type_descriptor(
                    &function->checker->result->types, expected
                );
            if (descriptor == NULL
                || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
                || !mmay_symbol_id_equal(descriptor->symbol, imported->symbol)
                || descriptor->argument_count
                    != imported->type_parameters.length) {
                *resolved = true;
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "generic imported enum constructor requires an expected concrete enum type",
                    type_name.start,
                    type_name.end
                );
                return false;
            }
        } else if (expected != MMAY_TYPE_INVALID
            && expected != imported->type) {
            *resolved = true;
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "imported enum constructor result type does not match the expected type",
                type_name.start,
                type_name.end
            );
            return false;
        }
        *enum_declaration =
            mmay_checker_private_checker_find_enum_by_type(
                function->checker,
                imported->type_parameters.length != 0 ? expected : imported->type
            );
        if (*enum_declaration == NULL) {
            if (!mmay_checker_has_failed(function->checker)) {
                mmay_checker_fail(
                    function->checker, MMAY_CHECK_INTERNAL_ERROR
                );
            }
            return false;
        }
    }
    *resolved = true;
    variant_name.snapshot = function->checker->ast->snapshot;
    variant_name.start = variant_token->start;
    variant_name.end = variant_token->end;
    *variant_declaration = mmay_checker_private_checker_find_enum_variant(
        function->checker,
        *enum_declaration,
        variant_name,
        variant_index
    );
    if (*variant_declaration == NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_UNKNOWN_VARIANT,
            "enum type has no variant with this name",
            variant_name.start,
            variant_name.end
        );
        return false;
    }
    return true;
}

static bool mmay_checker_private_concrete_enum(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *base,
    MarimayTypeId expected,
    MarimayCheckEnumDeclaration *result
) {
    const MarimayTypeDescriptor *descriptor = NULL;
    if (function == NULL || base == NULL || result == NULL) {
        return false;
    }
    *result = *base;
    if (!base->is_generic) {
        if (expected != MMAY_TYPE_INVALID && expected != base->type) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "enum constructor result type does not match the expected type",
                base->name.start,
                base->name.end
            );
            return false;
        }
        return true;
    }
    descriptor = expected == MMAY_TYPE_INVALID ? NULL
        : mmay_type_descriptor(&function->checker->result->types, expected);
    if (descriptor == NULL || descriptor->kind != MMAY_TYPE_KIND_NOMINAL
        || !mmay_symbol_id_equal(descriptor->symbol, base->symbol)
        || descriptor->argument_count != base->type_parameters.length) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "generic enum constructor requires an expected concrete enum type",
            base->name.start,
            base->name.end
        );
        return false;
    }
    result->type = expected;
    return true;
}

 bool mmay_checker_private_function_emit_enum_construction(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumDeclaration *enum_declaration,
    size_t variant_index,
    const MarimayMirEnumArgument *arguments,
    size_t argument_count,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
    MarimayMirEnumConstructionId construction =
        MMAY_MIR_NO_ENUM_CONSTRUCTION;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    size_t new_argument_count = 0;
    if (function == NULL || enum_declaration == NULL || result == NULL
        || (arguments == NULL && argument_count != 0)) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    if (!mmay_checker_private_function_enum_layout(
            function,
            enum_declaration,
            &enum_id
        )) {
        return false;
    }
    if (function->mir.enum_constructions.length
            >= function->mir.options.max_enum_constructions
        || !mmay_size_add(
            function->mir.enum_arguments.length,
            argument_count,
            &new_argument_count
        ) || new_argument_count > function->mir.options.max_enum_arguments) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_mir_add_enum_construction(
            &function->mir,
            enum_id,
            variant_index,
            arguments,
            argument_count,
            &construction
        )) {
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    if (!mmay_function_add_register(
            function,
            enum_declaration->type,
            &target
        )) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ENUM_NEW;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.enum_id = enum_id;
    instruction.enum_construction = construction;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = enum_declaration->type;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool mmay_checker_private_function_emit_enum_access(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    MarimayTypeId result_type,
    MarimayMirRegister receiver,
    MarimayMirEnumId enum_id,
    size_t variant,
    size_t payload,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayMirInstruction instruction;
    if (result == NULL || (kind != MMAY_MIR_ENUM_IS_VARIANT
            && kind != MMAY_MIR_ENUM_REQUIRE_VARIANT
            && kind != MMAY_MIR_ENUM_GET_PAYLOAD)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (!mmay_function_add_register(function, result_type, &target)) {
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = receiver;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.enum_id = enum_id;
    instruction.variant = variant;
    instruction.field = payload;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = result_type;
    result->register_id = target;
    result->valid = true;
    return true;
}

 bool mmay_checker_private_compile_enum_unit(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    MarimayCheckEnumVariantDeclaration *variant = NULL;
    MarimayCheckEnumDeclaration concrete;
    size_t variant_index = 0;
    if (!mmay_checker_private_resolve_enum_member(
            function,
            node,
            expected,
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
    if (!mmay_checker_private_concrete_enum(
            function, enum_declaration, expected, &concrete
        )) {
        return false;
    }
    if (variant == NULL
        || variant->kind != MMAY_CHECKED_ENUM_VARIANT_UNIT) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant requires its declared payload constructor syntax",
            node->start,
            node->end
        );
        return false;
    }
    return mmay_checker_private_function_emit_enum_construction(
        function,
        &concrete,
        variant_index,
        NULL,
        0,
        node->start,
        node->end,
        result
    );
}

 bool mmay_checker_private_compile_enum_positional(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
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
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    MarimayCheckEnumVariantDeclaration *variant = NULL;
    MarimayCheckEnumDeclaration concrete;
    MarimayMirEnumArgument *arguments = NULL;
    size_t variant_index = 0;
    size_t argument_count = 0;
    size_t argument_index = 0;
    size_t argument_bytes = 0;
    size_t child_index = 0;
    bool accepted = false;
    if (resolved == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    if (member_node == NULL) {
        return true;
    }
    if (!mmay_checker_private_resolve_enum_member(
            function,
            member_node,
            expected,
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
    if (!mmay_checker_private_concrete_enum(
            function, enum_declaration, expected, &concrete
        )) {
        goto cleanup;
    }
    if (function->constant_evaluation) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (arguments_node == NULL || variant == NULL
        || variant->kind != MMAY_CHECKED_ENUM_VARIANT_POSITIONAL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant is not a positional payload constructor",
            node->start,
            node->end
        );
        goto cleanup;
    }
    for (child_index = 0; child_index < arguments_node->child_count;
        child_index += 1) {
        const MarimayAstElement *element = mmay_ast_child(
            arguments_node,
            child_index
        );
        if (element == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
        if (element->kind == MMAY_AST_ELEMENT_NODE) {
            if (element->as.node == NULL
                || element->as.node->kind != MMAY_SYNTAX_EXPRESSION
                || argument_count == SIZE_MAX) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
                goto cleanup;
            }
            argument_count += 1;
        } else if (element->as.token.synthetic) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            goto cleanup;
        }
    }
    if (argument_count != variant->payload.length) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_CALL_ARITY,
            "positional enum payload count does not match its declaration",
            arguments_node->start,
            arguments_node->end
        );
        goto cleanup;
    }
    if (!mmay_size_multiply(
            argument_count,
            sizeof(MarimayMirEnumArgument),
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
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    for (child_index = 0; child_index < arguments_node->child_count;
        child_index += 1) {
        const MarimayAstElement *element = mmay_ast_child(
            arguments_node,
            child_index
        );
        const MarimayCheckEnumPayloadItemDeclaration *payload = NULL;
        MarimayTypeId payload_type = MMAY_TYPE_INVALID;
        MarimayExpressionValue value;
        if (element == NULL || element->kind != MMAY_AST_ELEMENT_NODE) {
            continue;
        }
        payload = mmay_vector_at_const(&variant->payload, argument_index);
        if (payload == NULL || arguments == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_checker_private_substitute_nominal_member_type(
                function,
                concrete.type,
                &concrete.type_parameters,
                payload->type,
                &payload_type
            )) {
            goto cleanup;
        }
        memset(&value, 0, sizeof(value));
        if (!mmay_compile_expression(
                function,
                element->as.node,
                payload_type,
                &value
            )) {
            goto cleanup;
        }
        if (mmay_expression_is_never(function, &value)) {
            *result = value;
            accepted = true;
            goto cleanup;
        }
        if (value.type != payload_type) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
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
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    accepted = mmay_checker_private_function_emit_enum_construction(
        function,
        &concrete,
        variant_index,
        arguments,
        argument_count,
        node->start,
        node->end,
        result
    );

cleanup:
    mmay_deallocate(
        &function->checker->result->allocator,
        arguments,
        argument_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}


 bool mmay_checker_private_compile_enum_named(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    bool *resolved,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *member_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_MEMBER_EXPRESSION,
        0
    );
    const MarimayAstNode *fields_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_RECORD_FIELDS,
        0
    );
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    MarimayCheckEnumVariantDeclaration *variant = NULL;
    MarimayCheckEnumDeclaration concrete;
    MarimayMirEnumArgument *arguments = NULL;
    unsigned char *seen = NULL;
    size_t variant_index = 0;
    size_t argument_bytes = 0;
    size_t seen_bytes = 0;
    size_t child_index = 0;
    size_t supplied_count = 0;
    bool accepted = false;
    if (resolved == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *resolved = false;
    if (member_node == NULL) {
        return true;
    }
    if (!mmay_checker_private_resolve_enum_member(
            function,
            member_node,
            expected,
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
    if (!mmay_checker_private_concrete_enum(
            function, enum_declaration, expected, &concrete
        )) {
        goto cleanup;
    }
    if (function->constant_evaluation) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (fields_node == NULL || variant == NULL
        || variant->kind != MMAY_CHECKED_ENUM_VARIANT_NAMED) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "enum variant is not a named payload constructor",
            node->start,
            node->end
        );
        goto cleanup;
    }
    if (!mmay_size_multiply(
            variant->payload.length,
            sizeof(MarimayMirEnumArgument),
            &argument_bytes
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    seen_bytes = variant->payload.length;
    if (argument_bytes != 0) {
        arguments = mmay_allocate(
            &function->checker->result->allocator,
            argument_bytes
        );
        seen = mmay_allocate(
            &function->checker->result->allocator,
            seen_bytes
        );
        if (arguments == NULL || seen == NULL) {
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
        MarimayCheckEnumPayloadItemDeclaration *payload = NULL;
        MarimayTypeId payload_type = MMAY_TYPE_INVALID;
        MarimayCheckName field_name;
        MarimayExpressionValue value;
        size_t payload_index = 0;
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
        payload = mmay_checker_private_checker_find_enum_payload(
            function->checker,
            variant,
            field_name,
            &payload_index
        );
        if (payload == NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_FIELD,
                "enum variant has no payload field with this name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || arguments == NULL
            || payload_index >= variant->payload.length) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (!mmay_checker_private_substitute_nominal_member_type(
                function,
                concrete.type,
                &concrete.type_parameters,
                payload->type,
                &payload_type
            )) {
            goto cleanup;
        }
        if (seen[payload_index] != 0) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_DUPLICATE_FIELD,
                "enum payload field is supplied more than once",
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
        if (expression != NULL) {
            if (!mmay_compile_expression(
                    function,
                    expression,
                    payload_type,
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
                    "enum payload shorthand requires a visible local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            if (!local->initialized) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_UNINITIALIZED_LOCAL,
                    "enum payload shorthand reads an uninitialized local",
                    field_name.start,
                    field_name.end
                );
                goto cleanup;
            }
            if (!mmay_closure_read_local(function, local, field_name.start, field_name.end, &value)) goto cleanup;
        }
        if (value.type != payload_type) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
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
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_MISSING_FIELD,
            "named enum payload construction omits a required field",
            node->start,
            node->end
        );
        goto cleanup;
    }
    accepted = mmay_checker_private_function_emit_enum_construction(
        function,
        &concrete,
        variant_index,
        arguments,
        variant->payload.length,
        node->start,
        node->end,
        result
    );

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
