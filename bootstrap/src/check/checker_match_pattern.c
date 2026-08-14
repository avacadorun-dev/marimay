#include "checker_match_pattern_api.h"
#include "checker_core_api.h"
#include "checker_lookup_api.h"

bool ruda_checker_private_analyze_enum_match_alternative(
    RudaFunctionChecker *function,
    RudaCheckEnumDeclaration *enum_declaration,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaCheckEnumMatchAlternative *alternative
);
bool ruda_checker_private_record_match_validate_named(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    RudaCheckRecordDeclaration *record,
    RudaVector *bindings
);






 bool ruda_checker_private_function_add_local(
    RudaFunctionChecker *function,
    RudaCheckName name,
    RudaTypeId type,
    bool mutable_binding,
    bool initialized,
    RudaCheckLocal **result
) {
    RudaCheckLocal local;
    RudaMirRegister register_id = RUDA_MIR_NO_REGISTER;
    if (function->locals.length
        >= function->checker->options.max_locals_per_function) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_function_add_register(function, type, &register_id)) {
        return false;
    }
    local.name = name;
    local.type = type;
    local.register_id = register_id;
    local.scope_depth = function->scope_depth;
    local.mutable_binding = mutable_binding;
    local.initialized = initialized;
    if (!ruda_vector_push(&function->locals, &local)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    if (result != NULL) {
        *result = ruda_vector_at(&function->locals, function->locals.length - 1);
    }
    return true;
}

 bool ruda_checker_private_function_emit_copy(
    RudaFunctionChecker *function,
    RudaMirRegister target,
    RudaMirRegister source,
    size_t start,
    size_t end
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_COPY;
    instruction.target = target;
    instruction.first = source;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.span.start = start;
    instruction.span.end = end;
    return ruda_function_append_instruction(function, &instruction);
}

 bool ruda_checker_private_enum_match_name(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaCheckName *name
) {
    const RudaSyntaxToken *token = ruda_checker_private_check_child_token(
        node,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    if (node == NULL || node->kind != RUDA_SYNTAX_NAME_PATTERN
        || name == NULL || token == NULL || token->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (ruda_checker_private_check_child_token(node, RUDA_TOKEN_IDENTIFIER, 1) != NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    name->snapshot = function->checker->ast->snapshot;
    name->start = token->start;
    name->end = token->end;
    return true;
}

 bool ruda_checker_private_enum_match_add_binding(
    RudaFunctionChecker *function,
    RudaVector *bindings,
    RudaCheckName name,
    RudaTypeId type
) {
    size_t index = 0;
    RudaCheckEnumMatchBinding binding;
    if (bindings == NULL || name.snapshot == NULL
        || type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < bindings->length; index += 1) {
        const RudaCheckEnumMatchBinding *existing = ruda_vector_at_const(
            bindings,
            index
        );
        if (existing != NULL && ruda_checker_private_check_names_equal(
                function->checker,
                existing->name,
                name
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_NAME,
                "pattern binding name is repeated in the same alternative",
                name.start,
                name.end
            );
            return false;
        }
    }
    binding.name = name;
    binding.type = type;
    if (!ruda_vector_push(bindings, &binding)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}

 bool ruda_checker_private_enum_match_collect_simple_binding(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId type,
    RudaVector *bindings
) {
    RudaCheckName name;
    if (node != NULL && node->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        return true;
    }
    if (node != NULL && node->kind == RUDA_SYNTAX_NAME_PATTERN) {
        return ruda_checker_private_enum_match_name(function, node, &name)
            && ruda_checker_private_enum_match_add_binding(
                function,
                bindings,
                name,
                type
            );
    }
    if (node != NULL && node->kind == RUDA_SYNTAX_CONSTRUCTOR_PATTERN) {
        const RudaAstNode *constructor_name =
            ruda_checker_private_check_child_node(
                node, RUDA_SYNTAX_NAME_PATTERN, 0
            );
        RudaCheckRecordDeclaration *record = NULL;
        RudaCheckEnumDeclaration *enum_declaration = NULL;
        if (!ruda_checker_private_enum_match_name(
                function, constructor_name, &name
            )) {
            return false;
        }
        record = ruda_checker_private_checker_find_record_by_type(
            function->checker, type
        );
        if (record != NULL) {
            RudaCheckRecordDeclaration *named =
                ruda_checker_private_checker_find_record(
                    function->checker, name
                );
            if (named != record) {
                (void)ruda_checker_add_diagnostic(
                    function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                    "nested record pattern constructor does not match its exact field type",
                    constructor_name->start, constructor_name->end
                );
                return false;
            }
            return ruda_checker_private_record_match_validate_named(
                function, node, record, bindings
            );
        }
        enum_declaration = ruda_checker_private_checker_find_enum_by_type(
            function->checker, type
        );
        if (enum_declaration != NULL) {
            RudaCheckEnumMatchAlternative nested;
            return ruda_checker_private_analyze_enum_match_alternative(
                function, enum_declaration, node, bindings, &nested
            );
        }
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "nested constructor pattern requires a nominal record or enum field",
            node->start, node->end
        );
        return false;
    }
    (void)ruda_checker_add_diagnostic(
        function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        "pattern does not match the nested field type",
        node != NULL ? node->start : 0, node != NULL ? node->end : 0
    );
    return false;
}

 bool ruda_checker_private_enum_match_validate_positional(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    const RudaCheckEnumVariantDeclaration *variant,
    RudaVector *bindings
) {
    const RudaAstNode *positional = ruda_checker_private_check_child_node(
        constructor,
        RUDA_SYNTAX_POSITIONAL_PATTERN,
        0
    );
    const RudaAstNode *list = ruda_checker_private_check_child_node(
        positional,
        RUDA_SYNTAX_PATTERN_LIST,
        0
    );
    size_t count = 0;
    const RudaAstNode *binding = NULL;
    if (positional == NULL || list == NULL || variant == NULL
        || bindings == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    while (ruda_checker_private_check_child_node_any(list, count) != NULL) {
        count += 1;
    }
    if (count != variant->payload.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "positional enum pattern arity does not match its variant",
            positional->start,
            positional->end
        );
        return false;
    }
    for (count = 0; count < variant->payload.length; count += 1) {
        const RudaCheckEnumPayloadItemDeclaration *payload =
            ruda_vector_at_const(&variant->payload, count);
        binding = ruda_checker_private_check_child_node_any(list, count);
        if (payload == NULL || !ruda_checker_private_enum_match_collect_simple_binding(
                function,
                binding,
                payload->type,
                bindings
            )) {
            if (payload == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            return false;
        }
    }
    return true;
}

 bool ruda_checker_private_enum_match_validate_named(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    RudaCheckEnumVariantDeclaration *variant,
    RudaVector *bindings
) {
    const RudaAstNode *named = ruda_checker_private_check_child_node(
        constructor,
        RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    const RudaAstNode *fields = ruda_checker_private_check_child_node(
        named,
        RUDA_SYNTAX_PATTERN_FIELDS,
        0
    );
    unsigned char *seen = NULL;
    size_t seen_bytes = variant != NULL ? variant->payload.length : 0;
    size_t field_ordinal = 0;
    size_t supplied = 0;
    bool accepted = false;
    if (named == NULL || fields == NULL || variant == NULL
        || bindings == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
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
    for (;;) {
        const RudaAstNode *field = ruda_checker_private_check_child_node(
            fields,
            RUDA_SYNTAX_PATTERN_FIELD,
            field_ordinal
        );
        const RudaSyntaxToken *field_token = NULL;
        const RudaAstNode *binding = NULL;
        RudaCheckEnumPayloadItemDeclaration *payload = NULL;
        RudaCheckName field_name;
        size_t payload_index = 0;
        if (field == NULL) {
            break;
        }
        field_ordinal += 1;
        field_token = ruda_checker_private_check_child_token(
            field,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
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
        if (seen == NULL || payload_index >= variant->payload.length) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[payload_index] != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
                "enum payload pattern names a field more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        binding = ruda_checker_private_check_child_node_any(field, 0);
        if (binding != NULL) {
            if (!ruda_checker_private_enum_match_collect_simple_binding(
                    function,
                    binding,
                    payload->type,
                    bindings
                )) {
                goto cleanup;
            }
        } else if (!ruda_checker_private_enum_match_add_binding(
                function,
                bindings,
                field_name,
                payload->type
            )) {
            goto cleanup;
        }
        seen[payload_index] = 1;
        supplied += 1;
    }
    if (supplied != variant->payload.length) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_MISSING_FIELD,
            "named enum payload pattern omits a required field",
            named->start,
            named->end
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_record_match_validate_named(
    RudaFunctionChecker *function,
    const RudaAstNode *constructor,
    RudaCheckRecordDeclaration *record,
    RudaVector *bindings
) {
    const RudaAstNode *named = ruda_checker_private_check_child_node(
        constructor,
        RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    const RudaAstNode *fields = ruda_checker_private_check_child_node(
        named,
        RUDA_SYNTAX_PATTERN_FIELDS,
        0
    );
    unsigned char *seen = NULL;
    size_t seen_bytes = record != NULL ? record->fields.length : 0;
    size_t field_ordinal = 0;
    size_t supplied = 0;
    bool accepted = false;
    if (named == NULL || fields == NULL || record == NULL
        || bindings == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
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
    for (;;) {
        const RudaAstNode *field = ruda_checker_private_check_child_node(
            fields,
            RUDA_SYNTAX_PATTERN_FIELD,
            field_ordinal
        );
        const RudaSyntaxToken *field_token = NULL;
        const RudaAstNode *binding = NULL;
        RudaCheckRecordFieldDeclaration *field_declaration = NULL;
        RudaCheckName field_name;
        size_t field_index = 0;
        if (field == NULL) {
            break;
        }
        field_ordinal += 1;
        field_token = ruda_checker_private_check_child_token(
            field,
            RUDA_TOKEN_IDENTIFIER,
            0
        );
        if (field_token == NULL || field_token->synthetic) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INVALID_ARGUMENT
            );
            goto cleanup;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        field_declaration = ruda_checker_private_checker_find_record_field(
            function->checker,
            record,
            field_name,
            &field_index
        );
        if (field_declaration == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_FIELD,
                "record type has no field with this pattern name",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        if (seen == NULL || field_index >= record->fields.length) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (seen[field_index] != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_DUPLICATE_FIELD,
                "record pattern names a field more than once",
                field_name.start,
                field_name.end
            );
            goto cleanup;
        }
        binding = ruda_checker_private_check_child_node_any(field, 0);
        if (binding != NULL) {
            if (!ruda_checker_private_enum_match_collect_simple_binding(
                    function,
                    binding,
                    field_declaration->type,
                    bindings
                )) {
                goto cleanup;
            }
        } else if (!ruda_checker_private_enum_match_add_binding(
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
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_MISSING_FIELD,
            "record pattern omits a required field",
            named->start,
            named->end
        );
        goto cleanup;
    }
    accepted = true;

cleanup:
    ruda_deallocate(
        &function->checker->result->allocator,
        seen,
        seen_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_match_collect_alternatives(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *patterns
) {
    RudaVector pending;
    bool initialized = false;
    bool accepted = false;
    memset(&pending, 0, sizeof(pending));
    if (pattern == NULL || patterns == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_vector_init(
            &pending,
            function->checker->result->allocator,
            sizeof(const RudaAstNode *)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    initialized = true;
    if (!ruda_vector_push(&pending, &pattern)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    while (pending.length != 0) {
        const RudaAstNode *const *slot = ruda_vector_at_const(
            &pending,
            pending.length - 1
        );
        const RudaAstNode *current = slot != NULL ? *slot : NULL;
        pending.length -= 1;
        if (current == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (current->kind == RUDA_SYNTAX_OR_PATTERN) {
            const RudaAstNode *left = ruda_checker_private_check_child_node_any(current, 0);
            const RudaAstNode *right = ruda_checker_private_check_child_node_any(current, 1);
            if (left == NULL || right == NULL
                || ruda_checker_private_check_child_node_any(current, 2) != NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                goto cleanup;
            }
            if (!ruda_vector_push(&pending, &right)
                || !ruda_vector_push(&pending, &left)) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        } else if (!ruda_vector_push(patterns, &current)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    accepted = true;

cleanup:
    if (initialized) {
        ruda_vector_dispose(&pending);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_analyze_enum_match_alternative(
    RudaFunctionChecker *function,
    RudaCheckEnumDeclaration *enum_declaration,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaCheckEnumMatchAlternative *alternative
) {
    const RudaAstNode *constructor_name = NULL;
    RudaCheckName name;
    size_t variant_index = 0;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    if (enum_declaration == NULL || pattern == NULL || bindings == NULL
        || alternative == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        alternative->is_catch_all = true;
    } else if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        if (!ruda_checker_private_enum_match_name(function, pattern, &name)) {
            return false;
        }
        variant = ruda_checker_private_checker_find_enum_variant(
            function->checker,
            enum_declaration,
            name,
            &variant_index
        );
        if (variant == NULL) {
            alternative->is_catch_all = true;
            alternative->has_catch_binding = true;
            alternative->catch_binding = name;
            if (!ruda_checker_private_enum_match_add_binding(
                    function,
                    bindings,
                    name,
                    enum_declaration->type
                )) {
                return false;
            }
        } else if (variant->kind != RUDA_CHECKED_ENUM_VARIANT_UNIT) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "enum pattern requires its declared payload shape",
                pattern->start,
                pattern->end
            );
            return false;
        }
    } else if (pattern->kind == RUDA_SYNTAX_CONSTRUCTOR_PATTERN) {
        constructor_name = ruda_checker_private_check_child_node(
            pattern,
            RUDA_SYNTAX_NAME_PATTERN,
            0
        );
        if (!ruda_checker_private_enum_match_name(function, constructor_name, &name)) {
            return false;
        }
        variant = ruda_checker_private_checker_find_enum_variant(
            function->checker,
            enum_declaration,
            name,
            &variant_index
        );
        if (variant == NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_VARIANT,
                "enum type has no variant with this pattern name",
                name.start,
                name.end
            );
            return false;
        }
        if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
            if (ruda_checker_private_check_child_node(
                    pattern,
                    RUDA_SYNTAX_POSITIONAL_PATTERN,
                    0
                ) == NULL) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                    "enum pattern payload shape does not match its variant",
                    pattern->start,
                    pattern->end
                );
                return false;
            }
            if (!ruda_checker_private_enum_match_validate_positional(
                    function,
                    pattern,
                    variant,
                    bindings
                )) {
                return false;
            }
        } else if (variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED) {
            if (ruda_checker_private_check_child_node(
                    pattern,
                    RUDA_SYNTAX_NAMED_PATTERN,
                    0
                ) == NULL) {
                (void)ruda_checker_add_diagnostic(
                    function->checker,
                    RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                    "enum pattern payload shape does not match its variant",
                    pattern->start,
                    pattern->end
                );
                return false;
            }
            if (!ruda_checker_private_enum_match_validate_named(
                    function,
                    pattern,
                    variant,
                    bindings
                )) {
                return false;
            }
        } else {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "unit enum variant pattern must not have a payload",
                pattern->start,
                pattern->end
            );
            return false;
        }
    } else {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (!alternative->is_catch_all) {
        alternative->variant = variant;
        alternative->variant_index = variant_index;
        if (!ruda_checker_private_enum_match_payloads_are_refutable(
                function,
                alternative,
                &alternative->has_refutable_nested_pattern
            )) {
            return false;
        }
    }
    return true;
}

static bool ruda_checker_private_pattern_is_refutable(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaTypeId type,
    bool *result
) {
    RudaCheckRecordDeclaration *record = NULL;
    const RudaAstNode *named = NULL;
    const RudaAstNode *fields = NULL;
    size_t ordinal = 0;
    if (function == NULL || pattern == NULL || result == NULL) {
        return false;
    }
    *result = false;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN
        || pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        return true;
    }
    if (pattern->kind != RUDA_SYNTAX_CONSTRUCTOR_PATTERN) {
        *result = true;
        return true;
    }
    if (ruda_checker_private_checker_find_enum_by_type(
            function->checker, type
        ) != NULL) {
        *result = true;
        return true;
    }
    record = ruda_checker_private_checker_find_record_by_type(
        function->checker, type
    );
    named = ruda_checker_private_check_child_node(
        pattern, RUDA_SYNTAX_NAMED_PATTERN, 0
    );
    fields = ruda_checker_private_check_child_node(
        named, RUDA_SYNTAX_PATTERN_FIELDS, 0
    );
    if (record == NULL || named == NULL || fields == NULL) {
        return false;
    }
    for (;;) {
        const RudaAstNode *field = ruda_checker_private_check_child_node(
            fields, RUDA_SYNTAX_PATTERN_FIELD, ordinal
        );
        const RudaSyntaxToken *token = NULL;
        const RudaAstNode *child = NULL;
        RudaCheckName name;
        RudaCheckRecordFieldDeclaration *declaration = NULL;
        bool child_refutable = false;
        if (field == NULL) {
            break;
        }
        ordinal += 1;
        token = ruda_checker_private_check_child_token(
            field, RUDA_TOKEN_IDENTIFIER, 0
        );
        child = ruda_checker_private_check_child_node_any(field, 0);
        if (token == NULL || token->synthetic || child == NULL) {
            return false;
        }
        name = (RudaCheckName){function->checker->ast->snapshot,
            token->start, token->end};
        declaration = ruda_checker_private_checker_find_record_field(
            function->checker, record, name, NULL
        );
        if (declaration == NULL
            || !ruda_checker_private_pattern_is_refutable(
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

 bool ruda_checker_private_enum_match_payloads_are_refutable(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    bool *result
) {
    const RudaAstNode *shape = NULL;
    size_t index = 0;
    if (function == NULL || alternative == NULL || result == NULL
        || alternative->variant == NULL) {
        return false;
    }
    *result = false;
    if (alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        return true;
    }
    shape = ruda_checker_private_check_child_node(
        alternative->pattern,
        alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL
            ? RUDA_SYNTAX_POSITIONAL_PATTERN : RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    if (shape == NULL) {
        return false;
    }
    if (alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
        const RudaAstNode *list = ruda_checker_private_check_child_node(
            shape, RUDA_SYNTAX_PATTERN_LIST, 0
        );
        if (list == NULL) {
            return false;
        }
        for (index = 0; index < alternative->variant->payload.length; index += 1) {
            const RudaAstNode *child = ruda_checker_private_check_child_node_any(
                list, index
            );
            const RudaCheckEnumPayloadItemDeclaration *payload =
                ruda_vector_at_const(&alternative->variant->payload, index);
            bool child_refutable = false;
            if (child == NULL || payload == NULL
                || !ruda_checker_private_pattern_is_refutable(
                    function, child, payload->type, &child_refutable
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
    shape = ruda_checker_private_check_child_node(
        shape, RUDA_SYNTAX_PATTERN_FIELDS, 0
    );
    if (shape == NULL) {
        return false;
    }
    for (index = 0; index < alternative->variant->payload.length; index += 1) {
        const RudaAstNode *field = ruda_checker_private_check_child_node(
            shape, RUDA_SYNTAX_PATTERN_FIELD, index
        );
        const RudaAstNode *child = field != NULL
            ? ruda_checker_private_check_child_node_any(field, 0) : NULL;
        if (child != NULL) {
            const RudaSyntaxToken *token = ruda_checker_private_check_child_token(
                field, RUDA_TOKEN_IDENTIFIER, 0
            );
            RudaCheckName name;
            size_t payload_index = 0;
            const RudaCheckEnumPayloadItemDeclaration *payload = NULL;
            bool child_refutable = false;
            if (token == NULL || token->synthetic) {
                return false;
            }
            name = (RudaCheckName){function->checker->ast->snapshot,
                token->start, token->end};
            payload = ruda_checker_private_checker_find_enum_payload(
                function->checker, alternative->variant, name, &payload_index
            );
            if (payload == NULL
                || !ruda_checker_private_pattern_is_refutable(
                    function, child, payload->type, &child_refutable
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

 bool ruda_checker_private_enum_match_bindings_equal(
    const RudaFunctionChecker *function,
    const RudaVector *expected,
    const RudaVector *actual
) {
    size_t expected_index = 0;
    if (expected == NULL || actual == NULL
        || expected->length != actual->length) {
        return false;
    }
    for (expected_index = 0; expected_index < expected->length;
        expected_index += 1) {
        const RudaCheckEnumMatchBinding *left = ruda_vector_at_const(
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
            const RudaCheckEnumMatchBinding *right = ruda_vector_at_const(
                actual,
                actual_index
            );
            if (right != NULL && ruda_checker_private_check_names_equal(
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

 bool ruda_checker_private_enum_match_alternative_redundant(
    const RudaCheckEnumMatchArm *arm,
    const RudaCheckEnumMatchAlternative *alternative
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const RudaCheckEnumMatchAlternative *earlier = ruda_vector_at_const(
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

 bool ruda_checker_private_match_arm_body_parts(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    const RudaAstNode **guard,
    const RudaAstNode **body,
    bool *body_is_block
) {
    const RudaAstNode *block = NULL;
    bool guarded = false;
    if (arm_node == NULL || guard == NULL || body == NULL
        || body_is_block == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *guard = NULL;
    *body = NULL;
    *body_is_block = false;
    guarded = ruda_checker_private_check_child_token(
        arm_node,
        RUDA_TOKEN_KW_IF,
        0
    ) != NULL;
    block = ruda_checker_private_check_child_node(arm_node, RUDA_SYNTAX_BLOCK, 0);
    if (guarded) {
        *guard = ruda_checker_private_check_child_node(
            arm_node,
            RUDA_SYNTAX_EXPRESSION,
            0
        );
    }
    if (block != NULL) {
        *body = block;
        *body_is_block = true;
    } else {
        *body = ruda_checker_private_check_child_node(
            arm_node,
            RUDA_SYNTAX_EXPRESSION,
            guarded ? 1 : 0
        );
    }
    if (*body == NULL || (guarded && *guard == NULL)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    return true;
}
