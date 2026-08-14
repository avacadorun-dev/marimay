#include "checker_match_binding_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"





#include "checker_internal.h"

 bool ruda_checker_private_analyze_enum_match_arm(
    RudaFunctionChecker *function,
    RudaCheckEnumDeclaration *enum_declaration,
    const RudaAstNode *arm_node,
    unsigned char *seen_variants,
    bool *covered,
    RudaCheckEnumMatchArm *arm
) {
    const RudaAstNode *pattern_wrapper = ruda_checker_private_check_child_node(
        arm_node,
        RUDA_SYNTAX_PATTERN,
        0
    );
    const RudaAstNode *pattern = ruda_checker_private_check_child_node_any(
        pattern_wrapper,
        0
    );
    RudaVector pattern_nodes;
    RudaVector alternative_bindings;
    size_t alternative_index = 0;
    bool pattern_nodes_initialized = false;
    bool alternative_bindings_initialized = false;
    bool accepted = false;
    memset(&pattern_nodes, 0, sizeof(pattern_nodes));
    memset(&alternative_bindings, 0, sizeof(alternative_bindings));
    if (arm == NULL || enum_declaration == NULL || arm_node == NULL
        || pattern_wrapper == NULL || pattern == NULL || covered == NULL
        || (seen_variants == NULL && enum_declaration->variants.length != 0)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!ruda_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchAlternative)
        ) || !ruda_vector_init(
            &arm->bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        ) || !ruda_vector_init(
            &pattern_nodes,
            function->checker->result->allocator,
            sizeof(const RudaAstNode *)
        ) || !ruda_vector_init(
            &alternative_bindings,
            function->checker->result->allocator,
            sizeof(RudaCheckEnumMatchBinding)
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    pattern_nodes_initialized = true;
    alternative_bindings_initialized = true;
    arm->node = arm_node;
    arm->pattern = pattern;
    if (!ruda_checker_private_match_arm_body_parts(
            function,
            arm_node,
            &arm->guard,
            &arm->body,
            &arm->body_is_block
        )) {
        goto cleanup;
    }
    if (!ruda_checker_private_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (pattern_nodes.length == 0
            && !ruda_checker_has_failed(function->checker)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        goto cleanup;
    }
    for (alternative_index = 0;
        alternative_index < pattern_nodes.length;
        alternative_index += 1) {
        const RudaAstNode *const *alternative_pattern = ruda_vector_at_const(
            &pattern_nodes,
            alternative_index
        );
        RudaCheckEnumMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !ruda_checker_private_analyze_enum_match_alternative(
                function,
                enum_declaration,
                *alternative_pattern,
                &alternative_bindings,
                &alternative
            )) {
            goto cleanup;
        }
        if (!alternative.is_catch_all
            && (seen_variants == NULL
                || alternative.variant_index
                    >= enum_declaration->variants.length)) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (alternative.is_catch_all
            && arm->alternatives.length != 0) {
            const RudaCheckEnumMatchAlternative *subsumed =
                ruda_vector_at_const(&arm->alternatives, 0);
            if (subsumed == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is subsumed by a catch-all in the same OR pattern",
                subsumed->pattern->start,
                subsumed->pattern->end
            );
            goto cleanup;
        }
        if (*covered || (!alternative.is_catch_all
                && seen_variants[alternative.variant_index] != 0)
            || ruda_checker_private_enum_match_alternative_redundant(
                arm,
                &alternative
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is unreachable after earlier patterns",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (alternative_index == 0) {
            if (!ruda_vector_append(
                    &arm->bindings,
                    alternative_bindings.data,
                    alternative_bindings.length
                )) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        } else if (!ruda_checker_private_enum_match_bindings_equal(
                function,
                &arm->bindings,
                &alternative_bindings
            )) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "OR pattern alternatives must bind the same names with identical types",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (!ruda_vector_push(&arm->alternatives, &alternative)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (arm->guard == NULL) {
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckEnumMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (alternative->is_catch_all) {
                *covered = true;
            } else if (seen_variants != NULL
                && alternative->variant_index
                    < enum_declaration->variants.length) {
                seen_variants[alternative->variant_index] = 1;
            } else {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
        }
    }
    accepted = true;

cleanup:
    if (alternative_bindings_initialized) {
        ruda_vector_dispose(&alternative_bindings);
    }
    if (pattern_nodes_initialized) {
        ruda_vector_dispose(&pattern_nodes);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_declare_match_bindings(
    RudaFunctionChecker *function,
    const RudaVector *bindings
) {
    size_t index = 0;
    if (bindings == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < bindings->length; index += 1) {
        const RudaCheckEnumMatchBinding *binding = ruda_vector_at_const(
            bindings,
            index
        );
        RudaCheckLocal *local = NULL;
        if (binding == NULL || ruda_checker_private_function_find_local_in_current_scope(
                function,
                binding->name
            ) != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!ruda_checker_private_function_add_local(
                function,
                binding->name,
                binding->type,
                false,
                true,
                &local
            ) || local == NULL) {
            return false;
        }
    }
    return true;
}

 bool ruda_checker_private_assign_match_local(
    RudaFunctionChecker *function,
    RudaCheckName name,
    RudaTypeId type,
    RudaMirRegister source,
    size_t start,
    size_t end
) {
    const RudaCheckLocal *local = ruda_checker_private_function_find_local_in_current_scope(
        function,
        name
    );
    if (local == NULL || local->type != type) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    return ruda_checker_private_function_emit_copy(
        function,
        local->register_id,
        source,
        start,
        end
    );
}

 bool ruda_checker_private_bind_enum_match_payload(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    RudaMirEnumId enum_id,
    RudaMirRegister guarded,
    size_t payload_index,
    const RudaAstNode *binding,
    RudaCheckName shorthand
) {
    const RudaCheckEnumPayloadItemDeclaration *payload = ruda_vector_at_const(
        &alternative->variant->payload,
        payload_index
    );
    RudaExpressionValue projected;
    RudaCheckName name = shorthand;
    if (binding != NULL && binding->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        return true;
    }
    if (binding != NULL && !ruda_checker_private_enum_match_name(
            function,
            binding,
            &name
        )) {
        return false;
    }
    if (payload == NULL || name.snapshot == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&projected, 0, sizeof(projected));
    if (!ruda_checker_private_function_emit_enum_access(
            function,
            RUDA_MIR_ENUM_GET_PAYLOAD,
            payload->type,
            guarded,
            enum_id,
            alternative->variant_index,
            payload_index,
            binding != NULL ? binding->start : name.start,
            binding != NULL ? binding->end : name.end,
            &projected
        )) {
        return false;
    }
    return ruda_checker_private_assign_match_local(
        function,
        name,
        payload->type,
        projected.register_id,
        binding != NULL ? binding->start : name.start,
        binding != NULL ? binding->end : name.end
    );
}

 bool ruda_checker_private_compile_enum_match_bindings(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    RudaMirEnumId enum_id,
    RudaMirRegister receiver
) {
    if (alternative->is_catch_all) {
        const RudaTypeId *receiver_type = ruda_vector_at_const(
            &function->mir.register_types,
            receiver
        );
        if (receiver_type == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        return !alternative->has_catch_binding
            || ruda_checker_private_assign_match_local(
                function,
                alternative->catch_binding,
                *receiver_type,
                receiver,
                alternative->pattern->start,
                alternative->pattern->end
            );
    }
    if (alternative->variant == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        return true;
    }
    if (alternative->variant->kind
        == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL) {
        const RudaAstNode *positional = ruda_checker_private_check_child_node(
            alternative->pattern,
            RUDA_SYNTAX_POSITIONAL_PATTERN,
            0
        );
        const RudaAstNode *list = ruda_checker_private_check_child_node(
            positional,
            RUDA_SYNTAX_PATTERN_LIST,
            0
        );
        size_t payload_index = 0;
        const RudaAstNode *binding = NULL;
        RudaCheckName no_shorthand;
        memset(&no_shorthand, 0, sizeof(no_shorthand));
        while ((binding = ruda_checker_private_check_child_node_any(
                list,
                payload_index
            )) != NULL) {
            if (!ruda_checker_private_bind_enum_match_payload(
                    function,
                    alternative,
                    enum_id,
                    receiver,
                    payload_index,
                    binding,
                    no_shorthand
                )) {
                return false;
            }
            payload_index += 1;
        }
        return payload_index == alternative->variant->payload.length;
    }
    if (alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED) {
        const RudaAstNode *named = ruda_checker_private_check_child_node(
            alternative->pattern,
            RUDA_SYNTAX_NAMED_PATTERN,
            0
        );
        const RudaAstNode *fields = ruda_checker_private_check_child_node(
            named,
            RUDA_SYNTAX_PATTERN_FIELDS,
            0
        );
        size_t field_ordinal = 0;
        for (;;) {
            const RudaAstNode *field = ruda_checker_private_check_child_node(
                fields,
                RUDA_SYNTAX_PATTERN_FIELD,
                field_ordinal
            );
            const RudaSyntaxToken *field_token = NULL;
            const RudaAstNode *binding = NULL;
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
                    RUDA_CHECK_INTERNAL_ERROR
                );
                return false;
            }
            field_name.snapshot = function->checker->ast->snapshot;
            field_name.start = field_token->start;
            field_name.end = field_token->end;
            if (ruda_checker_private_checker_find_enum_payload(
                    function->checker,
                    alternative->variant,
                    field_name,
                    &payload_index
                ) == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                return false;
            }
            binding = ruda_checker_private_check_child_node_any(field, 0);
            if (!ruda_checker_private_bind_enum_match_payload(
                    function,
                    alternative,
                    enum_id,
                    receiver,
                    payload_index,
                    binding,
                    field_name
                )) {
                return false;
            }
        }
        return field_ordinal == alternative->variant->payload.length;
    }
    ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
    return false;
}

 bool ruda_checker_private_compile_record_match_bindings(
    RudaFunctionChecker *function,
    const RudaCheckRecordMatchAlternative *alternative,
    RudaCheckRecordDeclaration *record_declaration,
    RudaMirRecordId record,
    RudaMirRegister receiver
) {
    const RudaAstNode *named = NULL;
    const RudaAstNode *fields = NULL;
    size_t field_ordinal = 0;
    if (alternative == NULL || record_declaration == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (!alternative->is_record_pattern) {
        return !alternative->has_catch_binding
            || ruda_checker_private_assign_match_local(
                function,
                alternative->catch_binding,
                record_declaration->type,
                receiver,
                alternative->pattern->start,
                alternative->pattern->end
            );
    }
    named = ruda_checker_private_check_child_node(
        alternative->pattern,
        RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    fields = ruda_checker_private_check_child_node(
        named,
        RUDA_SYNTAX_PATTERN_FIELDS,
        0
    );
    if (named == NULL || fields == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
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
        RudaCheckName binding_name;
        RudaExpressionValue projected;
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
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        binding = ruda_checker_private_check_child_node_any(field, 0);
        if (binding != NULL && binding->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
            continue;
        }
        binding_name = field_name;
        if (binding != NULL
            && !ruda_checker_private_enum_match_name(function, binding, &binding_name)) {
            return false;
        }
        field_declaration = ruda_checker_private_checker_find_record_field(
            function->checker,
            record_declaration,
            field_name,
            &field_index
        );
        if (field_declaration == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!ruda_checker_private_function_emit_record_access(
                function,
                RUDA_MIR_RECORD_GET,
                field_declaration->type,
                receiver,
                RUDA_MIR_NO_REGISTER,
                record,
                field_index,
                binding != NULL ? binding->start : field_name.start,
                binding != NULL ? binding->end : field_name.end,
                &projected
            ) || !ruda_checker_private_assign_match_local(
                function,
                binding_name,
                field_declaration->type,
                projected.register_id,
                binding != NULL ? binding->start : field_name.start,
                binding != NULL ? binding->end : field_name.end
            )) {
            return false;
        }
    }
    return field_ordinal == record_declaration->fields.length;
}
