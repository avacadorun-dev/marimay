#include "checker_closure_api.h"
#include "checker_escape_api.h"
#include "checker_enum_bottom_api.h"
#include "checker_match_pattern_api.h"
#include "checker_core_api.h"
#include "checker_enum_pattern_names_api.h"
#include "checker_generic_types_api.h"
#include "checker_lookup_api.h"
#include "package_imports_api.h"

bool mmay_checker_private_analyze_enum_match_alternative(
    MarimayFunctionChecker *function,
    MarimayCheckEnumDeclaration *enum_declaration,
    MarimayTypeId enum_type,
    const MarimayAstNode *pattern,
    MarimayVector *bindings,
    MarimayCheckEnumMatchAlternative *alternative
);

bool mmay_checker_private_enum_match_payload_type(
    MarimayFunctionChecker *function,
    MarimayTypeId enum_type,
    const MarimayCheckEnumDeclaration *enum_declaration,
    MarimayTypeId declared_type,
    MarimayTypeId *result
) {
    if (function == NULL || enum_declaration == NULL || result == NULL
        || enum_type == MMAY_TYPE_INVALID
        || declared_type == MMAY_TYPE_INVALID) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    return mmay_checker_private_substitute_nominal_member_type(
        function,
        enum_type,
        &enum_declaration->type_parameters,
        declared_type,
        result
    );
}
bool mmay_checker_private_record_match_validate_named(
    MarimayFunctionChecker *function,
    const MarimayAstNode *constructor,
    MarimayCheckRecordDeclaration *record,
    MarimayVector *bindings
);






 bool mmay_checker_private_function_add_local(
    MarimayFunctionChecker *function,
    MarimayCheckName name,
    MarimayTypeId type,
    bool mutable_binding,
    bool initialized,
    MarimayCheckLocal **result
) {
    MarimayCheckLocal local = {0};
    MarimayMirRegister register_id = MMAY_MIR_NO_REGISTER;
    if (function->locals.length
        >= function->checker->options.max_locals_per_function) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_function_add_register(function, type, &register_id)) {
        return false;
    }
    local.name = name;
    local.type = type;
    local.register_id = register_id;
    local.scope_depth = function->scope_depth;
    local.mutable_binding = mutable_binding;
    local.initialized = initialized;
    if (!mmay_closure_prepare_local(function, &local)
        || !mmay_escape_local(function, &local)) return false;
    if (!mmay_vector_push(&function->locals, &local)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (result != NULL) {
        *result = mmay_vector_at(&function->locals, function->locals.length - 1);
    }
    return true;
}

 bool mmay_checker_private_function_emit_copy(
    MarimayFunctionChecker *function,
    MarimayMirRegister target,
    MarimayMirRegister source,
    size_t start,
    size_t end
) {
    for (size_t i = 0; i < function->locals.length; i += 1) {
        MarimayCheckLocal *local = mmay_vector_at(&function->locals, i);
        if (local->register_id == target && local->cell_type != MMAY_TYPE_INVALID)
            return mmay_closure_write_local(function, local, source, start, end);
    }
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_COPY;
    instruction.target = target;
    instruction.first = source;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.span.start = start;
    instruction.span.end = end;
    return mmay_function_append_instruction(function, &instruction);
}

 bool mmay_checker_private_enum_match_name(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayCheckName *name
) {
    const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(
        node,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    const MarimaySyntaxToken *qualified = mmay_checker_private_check_child_token(
        node, MMAY_TOKEN_IDENTIFIER, 1
    );
    if (node == NULL || node->kind != MMAY_SYNTAX_NAME_PATTERN
        || name == NULL || token == NULL || token->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (qualified != NULL) {
        const unsigned char *namespace_bytes = NULL;
        size_t namespace_length = 0;
        MarimayCheckName namespace_name;
        if (qualified->synthetic) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (mmay_checker_private_check_child_token(
                node, MMAY_TOKEN_IDENTIFIER, 2
            ) != NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "pattern name does not denote a binding or record type",
                node->start, node->end
            );
            return false;
        }
        namespace_name.snapshot = function->checker->ast->snapshot;
        namespace_name.start = token->start;
        namespace_name.end = token->end;
        if (!mmay_checker_private_check_name_bytes(
                namespace_name, &namespace_bytes, &namespace_length
            ) || !mmay_check_import_namespace_exists(
                function->checker, namespace_bytes, namespace_length
            )) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_IMPORT,
                "qualified enum pattern requires an imported package namespace",
                token->start,
                token->end
            );
            return false;
        }
        token = qualified;
    }
    name->snapshot = function->checker->ast->snapshot;
    name->start = token->start;
    name->end = token->end;
    return true;
}

 bool mmay_checker_private_enum_match_add_binding(
    MarimayFunctionChecker *function,
    MarimayVector *bindings,
    MarimayCheckName name,
    MarimayTypeId type
) {
    size_t index = 0;
    MarimayCheckEnumMatchBinding binding;
    if (bindings == NULL || name.snapshot == NULL
        || type == MMAY_TYPE_INVALID) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (type == MMAY_TYPE_VOID) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "void cannot be stored in a named binding", name.start, name.end
        );
        return false;
    }
    for (index = 0; index < bindings->length; index += 1) {
        const MarimayCheckEnumMatchBinding *existing = mmay_vector_at_const(
            bindings,
            index
        );
        if (existing != NULL && mmay_checker_private_check_names_equal(
                function->checker,
                existing->name,
                name
            )) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_DUPLICATE_NAME,
                "pattern binding name is repeated in the same alternative",
                name.start,
                name.end
            );
            return false;
        }
    }
    binding.name = name;
    binding.type = type;
    if (!mmay_vector_push(bindings, &binding)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool mmay_checker_private_enum_match_collect_simple_binding(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId type,
    MarimayVector *bindings
) {
    MarimayCheckName name;
    if (node != NULL && node->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        return mmay_checker_private_match_remainder_name(
                function, node, &has_binding, &name
            ) && (!has_binding || mmay_checker_private_enum_match_add_binding(
                function, bindings, name, type
            ));
    }
    if (node != NULL && node->kind == MMAY_SYNTAX_NAME_PATTERN) {
        if (mmay_checker_private_check_child_token(
                node, MMAY_TOKEN_IDENTIFIER, 1
            ) != NULL) {
            MarimayCheckEnumDeclaration *enum_declaration =
                mmay_checker_private_checker_find_enum_by_type(function->checker, type);
            MarimayCheckEnumMatchAlternative nested;
            if (enum_declaration != NULL) {
                return mmay_checker_private_analyze_enum_match_alternative(
                    function, enum_declaration, type, node, bindings, &nested
                );
            }
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "qualified nested pattern requires an enum type", node->start, node->end
            );
            return false;
        }
        return mmay_checker_private_enum_match_name(function, node, &name)
            && mmay_checker_private_enum_match_add_binding(
                function,
                bindings,
                name,
                type
            );
    }
    if (node != NULL && node->kind == MMAY_SYNTAX_CONSTRUCTOR_PATTERN) {
        const MarimayAstNode *constructor_name =
            mmay_checker_private_check_child_node(
                node, MMAY_SYNTAX_NAME_PATTERN, 0
            );
        MarimayCheckRecordDeclaration *record = NULL;
        MarimayCheckEnumDeclaration *enum_declaration =
            mmay_checker_private_checker_find_enum_by_type(function->checker, type);
        if (enum_declaration != NULL) {
            MarimayCheckEnumMatchAlternative nested;
            return mmay_checker_private_analyze_enum_match_alternative(
                function, enum_declaration, type, node, bindings, &nested
            );
        }
        record = mmay_checker_private_checker_find_record_by_type(
            function->checker, type
        );
        if (record != NULL) {
            if (!mmay_checker_private_enum_match_name(
                    function, constructor_name, &name
                )) {
                return false;
            }
            MarimayCheckRecordDeclaration *named =
                mmay_checker_private_checker_find_record(
                    function->checker, name
                );
            if (named != record) {
                (void)mmay_checker_add_diagnostic(
                    function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "nested record pattern constructor does not match its exact field type",
                    constructor_name->start, constructor_name->end
                );
                return false;
            }
            return mmay_checker_private_record_match_validate_named(
                function, node, record, bindings
            );
        }
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "nested constructor pattern requires a nominal record or enum field",
            node->start, node->end
        );
        return false;
    }
    (void)mmay_checker_add_diagnostic(
        function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        "pattern does not match the nested field type",
        node != NULL ? node->start : 0, node != NULL ? node->end : 0
    );
    return false;
}

bool mmay_checker_private_match_remainder_name(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    bool *has_binding,
    MarimayCheckName *name
) {
    const MarimaySyntaxToken *as_token = NULL;
    const MarimaySyntaxToken *name_token = NULL;
    if (function == NULL || pattern == NULL
        || pattern->kind != MMAY_SYNTAX_WILDCARD_PATTERN
        || has_binding == NULL || name == NULL) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    *has_binding = false;
    memset(name, 0, sizeof(*name));
    as_token = mmay_checker_private_check_child_token(
        pattern, MMAY_TOKEN_KW_AS, 0
    );
    name_token = mmay_checker_private_check_child_token(
        pattern, MMAY_TOKEN_IDENTIFIER, 0
    );
    if (as_token == NULL && name_token == NULL) {
        return true;
    }
    if (as_token == NULL || as_token->synthetic || name_token == NULL
        || name_token->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    name->snapshot = function->checker->ast->snapshot;
    name->start = name_token->start;
    name->end = name_token->end;
    *has_binding = true;
    return true;
}

bool mmay_checker_private_enum_match_validate_positional(
    MarimayFunctionChecker *function,
    const MarimayAstNode *constructor,
    const MarimayCheckEnumDeclaration *enum_declaration,
    const MarimayCheckEnumVariantDeclaration *variant,
    MarimayTypeId enum_type,
    MarimayVector *bindings
) {
    const MarimayAstNode *positional = mmay_checker_private_check_child_node(
        constructor,
        MMAY_SYNTAX_POSITIONAL_PATTERN,
        0
    );
    const MarimayAstNode *list = mmay_checker_private_check_child_node(
        positional,
        MMAY_SYNTAX_PATTERN_LIST,
        0
    );
    size_t count = 0;
    const MarimayAstNode *binding = NULL;
    if (positional == NULL || list == NULL || variant == NULL
        || bindings == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    while (mmay_checker_private_check_child_node_any(list, count) != NULL) {
        count += 1;
    }
    if (count != variant->payload.length) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "positional enum pattern arity does not match its variant",
            positional->start,
            positional->end
        );
        return false;
    }
    for (count = 0; count < variant->payload.length; count += 1) {
        const MarimayCheckEnumPayloadItemDeclaration *payload =
            mmay_vector_at_const(&variant->payload, count);
        binding = mmay_checker_private_check_child_node_any(list, count);
        MarimayTypeId payload_type = MMAY_TYPE_INVALID;
        if (payload == NULL || !mmay_checker_private_enum_match_payload_type(
                function,
                enum_type,
                enum_declaration,
                payload->type,
                &payload_type
            ) || !mmay_checker_private_enum_match_collect_simple_binding(
                function,
                binding,
                payload_type,
                bindings
            )) {
            if (payload == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
            }
            return false;
        }
    }
    return true;
}

 bool mmay_checker_private_enum_match_validate_named(
    MarimayFunctionChecker *function,
    const MarimayAstNode *constructor,
    const MarimayCheckEnumDeclaration *enum_declaration,
    MarimayCheckEnumVariantDeclaration *variant,
    MarimayTypeId enum_type,
    MarimayVector *bindings
) {
    const MarimayAstNode *named = mmay_checker_private_check_child_node(
        constructor,
        MMAY_SYNTAX_NAMED_PATTERN,
        0
    );
    const MarimayAstNode *fields = mmay_checker_private_check_child_node(
        named,
        MMAY_SYNTAX_PATTERN_FIELDS,
        0
    );
    unsigned char *seen = NULL;
    size_t seen_bytes = variant != NULL ? variant->payload.length : 0;
    size_t field_ordinal = 0;
    size_t supplied = 0;
    bool accepted = false;
    if (named == NULL || fields == NULL || variant == NULL
        || bindings == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
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
    for (;;) {
        const MarimayAstNode *field = mmay_checker_private_check_child_node(
            fields,
            MMAY_SYNTAX_PATTERN_FIELD,
            field_ordinal
        );
        const MarimaySyntaxToken *field_token = NULL;
        const MarimayAstNode *binding = NULL;
        MarimayCheckEnumPayloadItemDeclaration *payload = NULL;
        MarimayTypeId payload_type = MMAY_TYPE_INVALID;
        MarimayCheckName field_name;
        size_t payload_index = 0;
        if (field == NULL) {
            break;
        }
        field_ordinal += 1;
        field_token = mmay_checker_private_check_child_token(
            field,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INVALID_ARGUMENT
            );
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
        if (seen == NULL || payload_index >= variant->payload.length) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[payload_index] != 0) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_DUPLICATE_FIELD,
                "enum payload pattern names a field more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (!mmay_checker_private_enum_match_payload_type(
                function,
                enum_type,
                enum_declaration,
                payload->type,
                &payload_type
            )) {
            goto cleanup;
        }
        binding = mmay_checker_private_check_child_node_any(field, 0);
        if (binding != NULL) {
            if (!mmay_checker_private_enum_match_collect_simple_binding(
                    function,
                    binding,
                    payload_type,
                    bindings
                )) {
                goto cleanup;
            }
        } else if (!mmay_checker_private_enum_match_add_binding(
                function,
                bindings,
                field_name,
                payload_type
            )) {
            goto cleanup;
        }
        seen[payload_index] = 1;
        supplied += 1;
    }
    if (supplied != variant->payload.length) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_MISSING_FIELD,
            "named enum payload pattern omits a required field",
            named->start,
            named->end
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    mmay_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_record_match_validate_named(
    MarimayFunctionChecker *function,
    const MarimayAstNode *constructor,
    MarimayCheckRecordDeclaration *record,
    MarimayVector *bindings
) {
    const MarimayAstNode *named = mmay_checker_private_check_child_node(
        constructor,
        MMAY_SYNTAX_NAMED_PATTERN,
        0
    );
    const MarimayAstNode *fields = mmay_checker_private_check_child_node(
        named,
        MMAY_SYNTAX_PATTERN_FIELDS,
        0
    );
    unsigned char *seen = NULL;
    size_t seen_bytes = record != NULL ? record->fields.length : 0;
    size_t field_ordinal = 0;
    size_t supplied = 0;
    bool accepted = false;
    if (named == NULL || fields == NULL || record == NULL
        || bindings == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
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
    for (;;) {
        const MarimayAstNode *field = mmay_checker_private_check_child_node(
            fields,
            MMAY_SYNTAX_PATTERN_FIELD,
            field_ordinal
        );
        const MarimaySyntaxToken *field_token = NULL;
        const MarimayAstNode *binding = NULL;
        MarimayCheckRecordFieldDeclaration *field_declaration = NULL;
        MarimayCheckName field_name;
        size_t field_index = 0;
        if (field == NULL) {
            break;
        }
        field_ordinal += 1;
        field_token = mmay_checker_private_check_child_token(
            field,
            MMAY_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INVALID_ARGUMENT
            );
            goto cleanup;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        field_declaration = mmay_checker_private_checker_find_record_field(
            function->checker,
            record,
            field_name,
            &field_index
        );
        if (field_declaration == NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_FIELD,
                "record type has no field with this pattern name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || field_index >= record->fields.length) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[field_index] != 0) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_DUPLICATE_FIELD,
                "record pattern names a field more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        binding = mmay_checker_private_check_child_node_any(field, 0);
        if (binding != NULL) {
            if (!mmay_checker_private_enum_match_collect_simple_binding(
                    function,
                    binding,
                    field_declaration->type,
                    bindings
                )) {
                goto cleanup;
            }
        } else if (!mmay_checker_private_enum_match_add_binding(
            function,
            bindings,
            field_name,
            field_declaration->type
        )) {
            goto cleanup;
        }
        seen[field_index] = 1;
        supplied += 1;
    }
    if (supplied != record->fields.length) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_MISSING_FIELD,
            "record pattern omits a required field",
            named->start,
            named->end
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    mmay_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_match_collect_alternatives(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayVector *patterns
) {
    MarimayVector pending;
    bool initialized = false;
    bool accepted = false;
    memset(&pending, 0, sizeof(pending));
    if (pattern == NULL || patterns == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!mmay_vector_init(
            &pending,
            function->checker->result->allocator,
            sizeof(const MarimayAstNode *)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    if (!mmay_vector_push(&pending, &pattern)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    while (pending.length != 0) {
        const MarimayAstNode *const *slot = mmay_vector_at_const(
            &pending,
            pending.length - 1
        );
        const MarimayAstNode *current = slot != NULL ? *slot : NULL;
        pending.length -= 1;
        if (current == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (current->kind == MMAY_SYNTAX_OR_PATTERN) {
            const MarimayAstNode *left = mmay_checker_private_check_child_node_any(current, 0);
            const MarimayAstNode *right = mmay_checker_private_check_child_node_any(current, 1);
            if (left == NULL || right == NULL
                || mmay_checker_private_check_child_node_any(current, 2) != NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            if (!mmay_vector_push(&pending, &right)
                || !mmay_vector_push(&pending, &left)) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        } else if (!mmay_vector_push(patterns, &current)) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    accepted = true;

cleanup:
    if (initialized) {
        mmay_vector_dispose(&pending);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

bool mmay_checker_private_analyze_enum_match_alternative(
    MarimayFunctionChecker *function,
    MarimayCheckEnumDeclaration *enum_declaration,
    MarimayTypeId enum_type,
    const MarimayAstNode *pattern,
    MarimayVector *bindings,
    MarimayCheckEnumMatchAlternative *alternative
) {
    const MarimayAstNode *constructor_name = NULL;
    MarimayCheckName name;
    size_t variant_index = 0;
    MarimayCheckEnumVariantDeclaration *variant = NULL;
    if (enum_declaration == NULL || pattern == NULL || bindings == NULL
        || alternative == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    alternative->enum_type = enum_type;
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        if (!mmay_checker_private_match_remainder_name(
                function, pattern, &has_binding, &name
            ) || (has_binding
                && !mmay_checker_private_enum_match_add_binding(
                    function, bindings, name, enum_type
                ))) {
            return false;
        }
        alternative->is_catch_all = true;
        alternative->has_catch_binding = has_binding;
        alternative->catch_binding = name;
    } else if (pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        if (!mmay_checker_enum_pattern_name(function, pattern, enum_type, &name)) {
            return false;
        }
        variant = mmay_checker_private_checker_find_enum_variant(
            function->checker,
            enum_declaration,
            name,
            &variant_index
        );
        if (variant == NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_VARIANT,
                "enum type has no variant with this pattern name; use '_ as name' to bind the remainder",
                name.start,
                name.end
            );
            return false;
        } else if (variant->kind != MMAY_CHECKED_ENUM_VARIANT_UNIT) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "enum pattern requires its declared payload shape",
                pattern->start,
                pattern->end
            );
            return false;
        }
    } else if (pattern->kind == MMAY_SYNTAX_CONSTRUCTOR_PATTERN) {
        constructor_name = mmay_checker_private_check_child_node(
            pattern,
            MMAY_SYNTAX_NAME_PATTERN,
            0
        );
        if (!mmay_checker_enum_pattern_name(
                function, constructor_name, enum_type, &name
            )) {
            return false;
        }
        variant = mmay_checker_private_checker_find_enum_variant(
            function->checker,
            enum_declaration,
            name,
            &variant_index
        );
        if (variant == NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_VARIANT,
                "enum type has no variant with this pattern name",
                name.start,
                name.end
            );
            return false;
        }
        if (variant->kind == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL) {
            if (mmay_checker_private_check_child_node(
                    pattern,
                    MMAY_SYNTAX_POSITIONAL_PATTERN,
                    0
                ) == NULL) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "enum pattern payload shape does not match its variant",
                    pattern->start,
                    pattern->end
                );
                return false;
            }
            if (!mmay_checker_private_enum_match_validate_positional(
                    function,
                    pattern,
                    enum_declaration,
                    variant,
                    enum_type,
                    bindings
                )) {
                return false;
            }
        } else if (variant->kind == MMAY_CHECKED_ENUM_VARIANT_NAMED) {
            if (mmay_checker_private_check_child_node(
                    pattern,
                    MMAY_SYNTAX_NAMED_PATTERN,
                    0
                ) == NULL) {
                (void)mmay_checker_add_diagnostic(
                    function->checker,
                    MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                    "enum pattern payload shape does not match its variant",
                    pattern->start,
                    pattern->end
                );
                return false;
            }
            if (!mmay_checker_private_enum_match_validate_named(
                    function,
                    pattern,
                    enum_declaration,
                    variant,
                    enum_type,
                    bindings
                )) {
                return false;
            }
        } else {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "unit enum variant pattern must not have a payload",
                pattern->start,
                pattern->end
            );
            return false;
        }
    } else {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (!alternative->is_catch_all) {
        bool bottom;
        if (!mmay_enum_variant_bottom(function, enum_declaration, enum_type, variant, &bottom)) return false;
        if (bottom) {
            (void)mmay_checker_add_diagnostic(function->checker, MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match variant is unreachable because its payload is never", pattern->start, pattern->end);
            return false;
        }
        alternative->variant = variant;
        alternative->variant_index = variant_index;
        if (!mmay_checker_private_enum_match_payloads_are_refutable(
                function,
                alternative,
                &alternative->has_refutable_nested_pattern
            )) {
            return false;
        }
    }
    return true;
}

static bool mmay_checker_private_pattern_is_refutable(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayTypeId type,
    bool *result
) {
    MarimayCheckRecordDeclaration *record = NULL;
    const MarimayAstNode *named = NULL;
    const MarimayAstNode *fields = NULL;
    size_t ordinal = 0;
    if (function == NULL || pattern == NULL || result == NULL) {
        return false;
    }
    *result = false;
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN
        || pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        *result = pattern->kind == MMAY_SYNTAX_NAME_PATTERN
            && mmay_checker_private_check_child_token(
                pattern, MMAY_TOKEN_IDENTIFIER, 1
            ) != NULL;
        return true;
    }
    if (pattern->kind != MMAY_SYNTAX_CONSTRUCTOR_PATTERN) {
        *result = true;
        return true;
    }
    if (mmay_checker_private_checker_find_enum_by_type(
            function->checker, type
        ) != NULL) {
        *result = true;
        return true;
    }
    record = mmay_checker_private_checker_find_record_by_type(
        function->checker, type
    );
    named = mmay_checker_private_check_child_node(
        pattern, MMAY_SYNTAX_NAMED_PATTERN, 0
    );
    fields = mmay_checker_private_check_child_node(
        named, MMAY_SYNTAX_PATTERN_FIELDS, 0
    );
    if (record == NULL || named == NULL || fields == NULL) {
        return false;
    }
    for (;;) {
        const MarimayAstNode *field = mmay_checker_private_check_child_node(
            fields, MMAY_SYNTAX_PATTERN_FIELD, ordinal
        );
        const MarimaySyntaxToken *token = NULL;
        const MarimayAstNode *child = NULL;
        MarimayCheckName name;
        MarimayCheckRecordFieldDeclaration *declaration = NULL;
        bool child_refutable = false;
        if (field == NULL) {
            break;
        }
        ordinal += 1;
        token = mmay_checker_private_check_child_token(
            field, MMAY_TOKEN_IDENTIFIER, 0
        );
        child = mmay_checker_private_check_child_node_any(field, 0);
        if (token == NULL || token->synthetic || child == NULL) {
            return false;
        }
        name = (MarimayCheckName){function->checker->ast->snapshot,
            token->start, token->end};
        declaration = mmay_checker_private_checker_find_record_field(
            function->checker, record, name, NULL
        );
        if (declaration == NULL
            || !mmay_checker_private_pattern_is_refutable(
                function, child, declaration->type, &child_refutable
            )) {
            return false;
        }
        if (child_refutable) {
            *result = true;
            return true;
        }
    }
    return true;
}

 bool mmay_checker_private_enum_match_payloads_are_refutable(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    bool *result
) {
    const MarimayAstNode *shape = NULL;
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    size_t index = 0;
    if (function == NULL || alternative == NULL || result == NULL
        || alternative->variant == NULL) {
        return false;
    }
    *result = false;
    enum_declaration = mmay_checker_private_checker_find_enum_by_type(
        function->checker, alternative->enum_type
    );
    if (enum_declaration == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_UNIT) {
        return true;
    }
    shape = mmay_checker_private_check_child_node(
        alternative->pattern,
        alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL
            ? MMAY_SYNTAX_POSITIONAL_PATTERN : MMAY_SYNTAX_NAMED_PATTERN,
        0
    );
    if (shape == NULL) {
        return false;
    }
    if (alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL) {
        const MarimayAstNode *list = mmay_checker_private_check_child_node(
            shape, MMAY_SYNTAX_PATTERN_LIST, 0
        );
        if (list == NULL) {
            return false;
        }
        for (index = 0; index < alternative->variant->payload.length; index += 1) {
            const MarimayAstNode *child = mmay_checker_private_check_child_node_any(
                list, index
            );
            const MarimayCheckEnumPayloadItemDeclaration *payload =
                mmay_vector_at_const(&alternative->variant->payload, index);
            MarimayTypeId payload_type = MMAY_TYPE_INVALID;
            bool child_refutable = false;
            if (child == NULL || payload == NULL
                || !mmay_checker_private_enum_match_payload_type(
                    function,
                    alternative->enum_type,
                    enum_declaration,
                    payload->type,
                    &payload_type
                )
                || !mmay_checker_private_pattern_is_refutable(
                    function, child, payload_type, &child_refutable
                )) {
                return false;
            }
            if (child_refutable) {
                *result = true;
                return true;
            }
        }
        return true;
    }
    shape = mmay_checker_private_check_child_node(
        shape, MMAY_SYNTAX_PATTERN_FIELDS, 0
    );
    if (shape == NULL) {
        return false;
    }
    for (index = 0; index < alternative->variant->payload.length; index += 1) {
        const MarimayAstNode *field = mmay_checker_private_check_child_node(
            shape, MMAY_SYNTAX_PATTERN_FIELD, index
        );
        const MarimayAstNode *child = field != NULL
            ? mmay_checker_private_check_child_node_any(field, 0) : NULL;
        if (child != NULL) {
            const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(
                field, MMAY_TOKEN_IDENTIFIER, 0
            );
            MarimayCheckName name;
            size_t payload_index = 0;
            const MarimayCheckEnumPayloadItemDeclaration *payload = NULL;
            MarimayTypeId payload_type = MMAY_TYPE_INVALID;
            bool child_refutable = false;
            if (token == NULL || token->synthetic) {
                return false;
            }
            name = (MarimayCheckName){function->checker->ast->snapshot,
                token->start, token->end};
            payload = mmay_checker_private_checker_find_enum_payload(
                function->checker, alternative->variant, name, &payload_index
            );
            if (payload == NULL
                || !mmay_checker_private_enum_match_payload_type(
                    function,
                    alternative->enum_type,
                    enum_declaration,
                    payload->type,
                    &payload_type
                )
                || !mmay_checker_private_pattern_is_refutable(
                    function, child, payload_type, &child_refutable
                )) {
                return false;
            }
            if (child_refutable) {
                *result = true;
                return true;
            }
        }
    }
    return true;
}

 bool mmay_checker_private_enum_match_bindings_equal(
    const MarimayFunctionChecker *function,
    const MarimayVector *expected,
    const MarimayVector *actual
) {
    size_t expected_index = 0;
    if (expected == NULL || actual == NULL
        || expected->length != actual->length) {
        return false;
    }
    for (expected_index = 0; expected_index < expected->length;
        expected_index += 1) {
        const MarimayCheckEnumMatchBinding *left = mmay_vector_at_const(
            expected,
            expected_index
        );
        size_t actual_index = 0;
        bool found = false;
        if (left == NULL) {
            return false;
        }
        for (actual_index = 0; actual_index < actual->length;
            actual_index += 1) {
            const MarimayCheckEnumMatchBinding *right = mmay_vector_at_const(
                actual,
                actual_index
            );
            if (right != NULL && mmay_checker_private_check_names_equal(
                    function->checker,
                    left->name,
                    right->name
                )) {
                if (left->type != right->type) {
                    return false;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

 bool mmay_checker_private_enum_match_alternative_redundant(
    const MarimayCheckEnumMatchArm *arm,
    const MarimayCheckEnumMatchAlternative *alternative
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const MarimayCheckEnumMatchAlternative *earlier = mmay_vector_at_const(
            &arm->alternatives,
            index
        );
        if (earlier != NULL && (earlier->is_catch_all
                || (!alternative->is_catch_all
                    && earlier->variant_index
                        == alternative->variant_index))) {
            return true;
        }
    }
    return false;
}

 bool mmay_checker_private_match_arm_body_parts(
    MarimayFunctionChecker *function,
    const MarimayAstNode *arm_node,
    const MarimayAstNode **guard,
    const MarimayAstNode **body,
    bool *body_is_block
) {
    const MarimayAstNode *block = NULL;
    bool guarded = false;
    if (arm_node == NULL || guard == NULL || body == NULL
        || body_is_block == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *guard = NULL;
    *body = NULL;
    *body_is_block = false;
    guarded = mmay_checker_private_check_child_token(
        arm_node,
        MMAY_TOKEN_KW_IF,
        0
    ) != NULL;
    block = mmay_checker_private_check_child_node(arm_node, MMAY_SYNTAX_BLOCK, 0);
    if (guarded) {
        *guard = mmay_checker_private_check_child_node(
            arm_node,
            MMAY_SYNTAX_EXPRESSION,
            0
        );
    }
    if (block != NULL) {
        *body = block;
        *body_is_block = true;
    } else {
        *body = mmay_checker_private_check_child_node(
            arm_node,
            MMAY_SYNTAX_EXPRESSION,
            guarded ? 1 : 0
        );
    }
    if (*body == NULL || (guarded && *guard == NULL)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    return true;
}
