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
            } else if (alternative->has_refutable_nested_pattern) {
                continue;
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

static bool ruda_checker_private_compile_nested_pattern_cps(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaTypeId type,
    RudaMirRegister source,
    RudaMirBlockId success,
    RudaMirBlockId failure
);

static bool ruda_checker_private_compile_record_pattern_cps(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaCheckRecordDeclaration *record,
    RudaMirRegister source,
    RudaMirBlockId success,
    RudaMirBlockId failure
) {
    const RudaAstNode *named = ruda_checker_private_check_child_node(
        pattern, RUDA_SYNTAX_NAMED_PATTERN, 0
    );
    const RudaAstNode *fields = ruda_checker_private_check_child_node(
        named, RUDA_SYNTAX_PATTERN_FIELDS, 0
    );
    RudaMirRecordId layout = RUDA_MIR_NO_RECORD;
    size_t ordinal = 0;
    if (named == NULL || fields == NULL
        || !ruda_checker_private_function_record_layout(function, record, &layout)) {
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
        RudaExpressionValue projected;
        RudaMirBlockId next = success;
        size_t field_index = 0;
        if (field == NULL) {
            break;
        }
        ordinal += 1;
        token = ruda_checker_private_check_child_token(
            field, RUDA_TOKEN_IDENTIFIER, 0
        );
        if (token == NULL || token->synthetic) {
            return false;
        }
        name = (RudaCheckName){function->checker->ast->snapshot,
            token->start, token->end};
        declaration = ruda_checker_private_checker_find_record_field(
            function->checker, record, name, &field_index
        );
        child = ruda_checker_private_check_child_node_any(field, 0);
        if (declaration == NULL || (ordinal < record->fields.length
                && !ruda_checker_private_function_add_block(function, &next))) {
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!ruda_checker_private_function_emit_record_access(
                function, RUDA_MIR_RECORD_GET, declaration->type, source,
                RUDA_MIR_NO_REGISTER, layout, field_index, field->start,
                field->end, &projected
            ) || (child == NULL
                ? !ruda_checker_private_assign_match_local(
                    function, name, declaration->type, projected.register_id,
                    name.start, name.end
                ) || !ruda_checker_private_function_set_goto(
                    function, function->block, next, field->end, field->end
                )
                : !ruda_checker_private_compile_nested_pattern_cps(
                    function, child, declaration->type, projected.register_id,
                    next, failure
                ))) {
            return false;
        }
        function->block = next;
        function->terminated = false;
    }
    return ordinal == record->fields.length;
}

static bool ruda_checker_private_compile_enum_payloads_cps(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    RudaMirEnumId enum_id,
    RudaMirRegister receiver,
    RudaMirBlockId success,
    RudaMirBlockId failure
) {
    size_t ordinal = 0;
    const RudaAstNode *shape = NULL;
    if (alternative == NULL || alternative->variant == NULL) {
        return false;
    }
    if (alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_UNIT) {
        return ruda_checker_private_function_set_goto(
            function, function->block, success,
            alternative->pattern->end, alternative->pattern->end
        );
    }
    shape = ruda_checker_private_check_child_node(
        alternative->pattern,
        alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL
            ? RUDA_SYNTAX_POSITIONAL_PATTERN : RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    shape = alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL
        ? ruda_checker_private_check_child_node(shape, RUDA_SYNTAX_PATTERN_LIST, 0)
        : ruda_checker_private_check_child_node(shape, RUDA_SYNTAX_PATTERN_FIELDS, 0);
    if (shape == NULL) {
        return false;
    }
    for (;;) {
        const RudaAstNode *field = alternative->variant->kind
                == RUDA_CHECKED_ENUM_VARIANT_POSITIONAL
            ? ruda_checker_private_check_child_node_any(shape, ordinal)
            : ruda_checker_private_check_child_node(
                shape, RUDA_SYNTAX_PATTERN_FIELD, ordinal
            );
        const RudaAstNode *child = field;
        const RudaCheckEnumPayloadItemDeclaration *payload = NULL;
        RudaCheckName name;
        RudaExpressionValue projected;
        RudaMirBlockId next = success;
        size_t payload_index = ordinal;
        if (field == NULL) {
            break;
        }
        if (alternative->variant->kind == RUDA_CHECKED_ENUM_VARIANT_NAMED) {
            const RudaSyntaxToken *token = ruda_checker_private_check_child_token(
                field, RUDA_TOKEN_IDENTIFIER, 0
            );
            if (token == NULL || token->synthetic) {
                return false;
            }
            name = (RudaCheckName){function->checker->ast->snapshot,
                token->start, token->end};
            payload = ruda_checker_private_checker_find_enum_payload(
                function->checker, alternative->variant, name, &payload_index
            );
            child = ruda_checker_private_check_child_node_any(field, 0);
        } else {
            memset(&name, 0, sizeof(name));
            payload = ruda_vector_at_const(&alternative->variant->payload, ordinal);
        }
        ordinal += 1;
        if (payload == NULL || (ordinal < alternative->variant->payload.length
                && !ruda_checker_private_function_add_block(function, &next))) {
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!ruda_checker_private_function_emit_enum_access(
                function, RUDA_MIR_ENUM_GET_PAYLOAD, payload->type, receiver,
                enum_id, alternative->variant_index, payload_index, field->start,
                field->end, &projected
            ) || (child == NULL
                ? !ruda_checker_private_assign_match_local(
                    function, name, payload->type, projected.register_id,
                    name.start, name.end
                ) || !ruda_checker_private_function_set_goto(
                    function, function->block, next, field->end, field->end
                )
                : !ruda_checker_private_compile_nested_pattern_cps(
                    function, child, payload->type, projected.register_id,
                    next, failure
                ))) {
            return false;
        }
        function->block = next;
        function->terminated = false;
    }
    return ordinal == alternative->variant->payload.length;
}

static bool ruda_checker_private_compile_nested_pattern_cps(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaTypeId type,
    RudaMirRegister source,
    RudaMirBlockId success,
    RudaMirBlockId failure
) {
    RudaCheckName name;
    RudaCheckRecordDeclaration *record = NULL;
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumVariantDeclaration *variant = NULL;
    RudaCheckEnumMatchAlternative alternative;
    RudaMirEnumId enum_id = RUDA_MIR_NO_ENUM;
    RudaExpressionValue matched;
    RudaExpressionValue guarded;
    RudaMirBlockId matched_block = RUDA_MIR_NO_BLOCK;
    size_t variant_index = 0;
    const RudaAstNode *constructor_name = NULL;
    if (pattern == NULL) {
        return false;
    }
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        return ruda_checker_private_function_set_goto(
            function, function->block, success, pattern->end, pattern->end
        );
    }
    if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        return ruda_checker_private_enum_match_name(function, pattern, &name)
            && ruda_checker_private_assign_match_local(
                function, name, type, source, pattern->start, pattern->end
            ) && ruda_checker_private_function_set_goto(
                function, function->block, success, pattern->end, pattern->end
            );
    }
    if (pattern->kind != RUDA_SYNTAX_CONSTRUCTOR_PATTERN) {
        return false;
    }
    record = ruda_checker_private_checker_find_record_by_type(
        function->checker, type
    );
    if (record != NULL) {
        return ruda_checker_private_compile_record_pattern_cps(
            function, pattern, record, source, success, failure
        );
    }
    enum_declaration = ruda_checker_private_checker_find_enum_by_type(
        function->checker, type
    );
    constructor_name = ruda_checker_private_check_child_node(
        pattern, RUDA_SYNTAX_NAME_PATTERN, 0
    );
    if (enum_declaration == NULL
        || !ruda_checker_private_enum_match_name(function, constructor_name, &name)
        || (variant = ruda_checker_private_checker_find_enum_variant(
                function->checker, enum_declaration, name, &variant_index
            )) == NULL
        || !ruda_checker_private_function_enum_layout(
            function, enum_declaration, &enum_id
        ) || !ruda_checker_private_function_add_block(function, &matched_block)) {
        return false;
    }
    memset(&matched, 0, sizeof(matched));
    memset(&guarded, 0, sizeof(guarded));
    if (!ruda_checker_private_function_emit_enum_access(
            function, RUDA_MIR_ENUM_IS_VARIANT, RUDA_TYPE_BOOL, source,
            enum_id, variant_index, 0, pattern->start, pattern->end, &matched
        ) || !ruda_checker_private_function_set_branch(
            function, function->block, matched.register_id, matched_block,
            failure, pattern->start, pattern->end
        )) {
        return false;
    }
    function->block = matched_block;
    function->terminated = false;
    if (!ruda_checker_private_function_emit_enum_access(
            function, RUDA_MIR_ENUM_REQUIRE_VARIANT, type, source, enum_id,
            variant_index, 0, pattern->start, pattern->end, &guarded
        )) {
        return false;
    }
    memset(&alternative, 0, sizeof(alternative));
    alternative.pattern = pattern;
    alternative.variant = variant;
    alternative.variant_index = variant_index;
    return ruda_checker_private_compile_enum_payloads_cps(
        function, &alternative, enum_id, guarded.register_id, success, failure
    );
}

 bool ruda_checker_private_compile_enum_match_bindings_cps(
    RudaFunctionChecker *function,
    const RudaCheckEnumMatchAlternative *alternative,
    RudaMirEnumId enum_id,
    RudaMirRegister receiver,
    RudaMirBlockId success,
    RudaMirBlockId failure
) {
    if (alternative == NULL) {
        return false;
    }
    if (alternative->is_catch_all) {
        const RudaTypeId *type = ruda_vector_at_const(
            &function->mir.register_types, receiver
        );
        return type != NULL && (!alternative->has_catch_binding
            || ruda_checker_private_assign_match_local(
                function, alternative->catch_binding, *type, receiver,
                alternative->pattern->start, alternative->pattern->end
            )) && ruda_checker_private_function_set_goto(
                function, function->block, success,
                alternative->pattern->end, alternative->pattern->end
            );
    }
    return ruda_checker_private_compile_enum_payloads_cps(
        function, alternative, enum_id, receiver, success, failure
    );
}

static bool ruda_checker_compile_nested_record_pattern(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaTypeId type,
    RudaMirRegister source
) {
    RudaCheckName name;
    RudaCheckRecordDeclaration *record = NULL;
    RudaMirRecordId layout = RUDA_MIR_NO_RECORD;
    const RudaAstNode *constructor_name = NULL;
    const RudaAstNode *named = NULL;
    const RudaAstNode *fields = NULL;
    size_t ordinal = 0;
    if (pattern == NULL) {
        return false;
    }
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        return true;
    }
    if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        return ruda_checker_private_enum_match_name(function, pattern, &name)
            && ruda_checker_private_assign_match_local(
                function, name, type, source, pattern->start, pattern->end
            );
    }
    if (pattern->kind != RUDA_SYNTAX_CONSTRUCTOR_PATTERN) {
        (void)ruda_checker_add_diagnostic(
            function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "nested pattern does not match the record field type",
            pattern->start, pattern->end
        );
        return false;
    }
    constructor_name = ruda_checker_private_check_child_node(
        pattern, RUDA_SYNTAX_NAME_PATTERN, 0
    );
    if (!ruda_checker_private_enum_match_name(function, constructor_name, &name)
        || (record = ruda_checker_private_checker_find_record_by_type(
                function->checker, type
            )) == NULL
        || ruda_checker_private_checker_find_record(function->checker, name)
            != record
        || (named = ruda_checker_private_check_child_node(
                pattern, RUDA_SYNTAX_NAMED_PATTERN, 0
            )) == NULL
        || (fields = ruda_checker_private_check_child_node(
                named, RUDA_SYNTAX_PATTERN_FIELDS, 0
            )) == NULL
        || !ruda_checker_private_function_record_layout(
            function, record, &layout
        )) {
        if (!ruda_checker_has_failed(function->checker)) {
            (void)ruda_checker_add_diagnostic(
                function->checker, RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "nested record pattern constructor does not match its exact field type",
                pattern->start, pattern->end
            );
        }
        return false;
    }
    for (;;) {
        const RudaAstNode *field = ruda_checker_private_check_child_node(
            fields, RUDA_SYNTAX_PATTERN_FIELD, ordinal
        );
        const RudaSyntaxToken *token = NULL;
        const RudaAstNode *child = NULL;
        RudaCheckRecordFieldDeclaration *declaration = NULL;
        RudaExpressionValue projected;
        size_t field_index = 0;
        if (field == NULL) {
            break;
        }
        ordinal += 1;
        token = ruda_checker_private_check_child_token(
            field, RUDA_TOKEN_IDENTIFIER, 0
        );
        if (token == NULL || token->synthetic) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        name = (RudaCheckName){function->checker->ast->snapshot,
            token->start, token->end};
        declaration = ruda_checker_private_checker_find_record_field(
            function->checker, record, name, &field_index
        );
        child = ruda_checker_private_check_child_node_any(field, 0);
        if (declaration == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!ruda_checker_private_function_emit_record_access(
                function, RUDA_MIR_RECORD_GET, declaration->type, source,
                RUDA_MIR_NO_REGISTER, layout, field_index,
                child != NULL ? child->start : name.start,
                child != NULL ? child->end : name.end, &projected
            ) || (child == NULL
                ? !ruda_checker_private_assign_match_local(
                    function, name, declaration->type, projected.register_id,
                    name.start, name.end
                ) : !ruda_checker_compile_nested_record_pattern(
                    function, child, declaration->type, projected.register_id
                ))) {
            return false;
        }
    }
    return ordinal == record->fields.length;
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
    if (payload == NULL) {
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
            binding != NULL ? binding->start : shorthand.start,
            binding != NULL ? binding->end : shorthand.end,
            &projected
        )) {
        return false;
    }
    if (binding == NULL) {
        return ruda_checker_private_assign_match_local(
            function, name, payload->type, projected.register_id,
            name.start, name.end
        );
    }
    return ruda_checker_compile_nested_record_pattern(
        function, binding, payload->type, projected.register_id
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
                function, RUDA_MIR_RECORD_GET, field_declaration->type,
                receiver, RUDA_MIR_NO_REGISTER, record, field_index,
                binding != NULL ? binding->start : field_name.start,
                binding != NULL ? binding->end : field_name.end, &projected
            ) || (binding == NULL
                ? !ruda_checker_private_assign_match_local(
                    function, field_name, field_declaration->type,
                    projected.register_id, field_name.start, field_name.end
                ) : !ruda_checker_compile_nested_record_pattern(
                    function, binding, field_declaration->type,
                    projected.register_id
                ))) {
            return false;
        }
    }
    return field_ordinal == record_declaration->fields.length;
}
