#include "checker_match_binding_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"





#include "checker_internal.h"

bool mmay_checker_private_analyze_enum_match_arm(
    MarimayFunctionChecker *function,
    MarimayCheckEnumDeclaration *enum_declaration,
    MarimayTypeId enum_type,
    const MarimayAstNode *arm_node,
    unsigned char *seen_variants,
    bool *covered,
    MarimayCheckEnumMatchArm *arm
) {
    const MarimayAstNode *pattern_wrapper = mmay_checker_private_check_child_node(
        arm_node,
        MMAY_SYNTAX_PATTERN,
        0
    );
    const MarimayAstNode *pattern = mmay_checker_private_check_child_node_any(
        pattern_wrapper,
        0
    );
    MarimayVector pattern_nodes;
    MarimayVector alternative_bindings;
    size_t alternative_index = 0;
    bool pattern_nodes_initialized = false;
    bool alternative_bindings_initialized = false;
    bool accepted = false;
    memset(&pattern_nodes, 0, sizeof(pattern_nodes));
    memset(&alternative_bindings, 0, sizeof(alternative_bindings));
    if (arm == NULL || enum_declaration == NULL || arm_node == NULL
        || pattern_wrapper == NULL || pattern == NULL || covered == NULL
        || (seen_variants == NULL && enum_declaration->variants.length != 0)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!mmay_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(MarimayCheckEnumMatchAlternative)
        ) || !mmay_vector_init(
            &arm->bindings,
            function->checker->result->allocator,
            sizeof(MarimayCheckEnumMatchBinding)
        ) || !mmay_vector_init(
            &pattern_nodes,
            function->checker->result->allocator,
            sizeof(const MarimayAstNode *)
        ) || !mmay_vector_init(
            &alternative_bindings,
            function->checker->result->allocator,
            sizeof(MarimayCheckEnumMatchBinding)
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    pattern_nodes_initialized = true;
    alternative_bindings_initialized = true;
    arm->node = arm_node;
    arm->pattern = pattern;
    if (!mmay_checker_private_match_arm_body_parts(
            function,
            arm_node,
            &arm->guard,
            &arm->body,
            &arm->body_is_block
        )) {
        goto cleanup;
    }
    if (!mmay_checker_private_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (pattern_nodes.length == 0
            && !mmay_checker_has_failed(function->checker)) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        goto cleanup;
    }
    for (alternative_index = 0;
        alternative_index < pattern_nodes.length;
        alternative_index += 1) {
        const MarimayAstNode *const *alternative_pattern = mmay_vector_at_const(
            &pattern_nodes,
            alternative_index
        );
        MarimayCheckEnumMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !mmay_checker_private_analyze_enum_match_alternative(
                function,
                enum_declaration,
                enum_type,
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
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        if (alternative.is_catch_all
            && arm->alternatives.length != 0) {
            const MarimayCheckEnumMatchAlternative *subsumed =
                mmay_vector_at_const(&arm->alternatives, 0);
            if (subsumed == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is subsumed by a catch-all in the same OR pattern",
                subsumed->pattern->start,
                subsumed->pattern->end
            );
            goto cleanup;
        }
        if (*covered || (!alternative.is_catch_all
                && seen_variants[alternative.variant_index] != 0)
            || mmay_checker_private_enum_match_alternative_redundant(
                arm,
                &alternative
            )) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
                "match alternative is unreachable after earlier patterns",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (alternative_index == 0) {
            if (!mmay_vector_append(
                    &arm->bindings,
                    alternative_bindings.data,
                    alternative_bindings.length
                )) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_ALLOCATION_FAILED
                );
                goto cleanup;
            }
        } else if (!mmay_checker_private_enum_match_bindings_equal(
                function,
                &arm->bindings,
                &alternative_bindings
            )) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "OR pattern alternatives must bind the same names with identical types",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (!mmay_vector_push(&arm->alternatives, &alternative)) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (arm->guard == NULL) {
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const MarimayCheckEnumMatchAlternative *alternative =
                mmay_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            if (alternative == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
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
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
        }
    }
    accepted = true;

cleanup:
    if (alternative_bindings_initialized) {
        mmay_vector_dispose(&alternative_bindings);
    }
    if (pattern_nodes_initialized) {
        mmay_vector_dispose(&pattern_nodes);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_declare_match_bindings(
    MarimayFunctionChecker *function,
    const MarimayVector *bindings
) {
    size_t index = 0;
    if (bindings == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < bindings->length; index += 1) {
        const MarimayCheckEnumMatchBinding *binding = mmay_vector_at_const(
            bindings,
            index
        );
        MarimayCheckLocal *local = NULL;
        if (binding == NULL || mmay_checker_private_function_find_local_in_current_scope(
                function,
                binding->name
            ) != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (!mmay_checker_private_function_add_local(
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

 bool mmay_checker_private_assign_match_local(
    MarimayFunctionChecker *function,
    MarimayCheckName name,
    MarimayTypeId type,
    MarimayMirRegister source,
    size_t start,
    size_t end
) {
    const MarimayCheckLocal *local = mmay_checker_private_function_find_local_in_current_scope(
        function,
        name
    );
    if (local == NULL || local->type != type) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    return mmay_checker_private_function_emit_copy(
        function,
        local->register_id,
        source,
        start,
        end
    );
}

static bool mmay_checker_private_compile_nested_pattern_cps(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayTypeId type,
    MarimayMirRegister source,
    MarimayMirBlockId success,
    MarimayMirBlockId failure
);

static bool mmay_checker_private_compile_record_pattern_cps(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayCheckRecordDeclaration *record,
    MarimayMirRegister source,
    MarimayMirBlockId success,
    MarimayMirBlockId failure
) {
    const MarimayAstNode *named = mmay_checker_private_check_child_node(
        pattern, MMAY_SYNTAX_NAMED_PATTERN, 0
    );
    const MarimayAstNode *fields = mmay_checker_private_check_child_node(
        named, MMAY_SYNTAX_PATTERN_FIELDS, 0
    );
    MarimayMirRecordId layout = MMAY_MIR_NO_RECORD;
    size_t ordinal = 0;
    if (named == NULL || fields == NULL
        || !mmay_checker_private_function_record_layout(function, record, &layout)) {
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
        MarimayExpressionValue projected;
        MarimayMirBlockId next = success;
        size_t field_index = 0;
        if (field == NULL) {
            break;
        }
        ordinal += 1;
        token = mmay_checker_private_check_child_token(
            field, MMAY_TOKEN_IDENTIFIER, 0
        );
        if (token == NULL || token->synthetic) {
            return false;
        }
        name = (MarimayCheckName){function->checker->ast->snapshot,
            token->start, token->end};
        declaration = mmay_checker_private_checker_find_record_field(
            function->checker, record, name, &field_index
        );
        child = mmay_checker_private_check_child_node_any(field, 0);
        if (declaration == NULL || (ordinal < record->fields.length
                && !mmay_checker_private_function_add_block(function, &next))) {
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!mmay_checker_private_function_emit_record_access(
                function, MMAY_MIR_RECORD_GET, declaration->type, source,
                MMAY_MIR_NO_REGISTER, layout, field_index, field->start,
                field->end, &projected
            ) || (child == NULL
                ? !mmay_checker_private_assign_match_local(
                    function, name, declaration->type, projected.register_id,
                    name.start, name.end
                ) || !mmay_checker_private_function_set_goto(
                    function, function->block, next, field->end, field->end
                )
                : !mmay_checker_private_compile_nested_pattern_cps(
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

static bool mmay_checker_private_compile_enum_payloads_cps(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    MarimayMirEnumId enum_id,
    MarimayMirRegister receiver,
    MarimayMirBlockId success,
    MarimayMirBlockId failure
) {
    size_t ordinal = 0;
    const MarimayAstNode *shape = NULL;
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    if (alternative == NULL || alternative->variant == NULL) {
        return false;
    }
    enum_declaration = mmay_checker_private_checker_find_enum_by_type(
        function->checker, alternative->enum_type
    );
    if (enum_declaration == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_UNIT) {
        return mmay_checker_private_function_set_goto(
            function, function->block, success,
            alternative->pattern->end, alternative->pattern->end
        );
    }
    shape = mmay_checker_private_check_child_node(
        alternative->pattern,
        alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL
            ? MMAY_SYNTAX_POSITIONAL_PATTERN : MMAY_SYNTAX_NAMED_PATTERN,
        0
    );
    shape = alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL
        ? mmay_checker_private_check_child_node(shape, MMAY_SYNTAX_PATTERN_LIST, 0)
        : mmay_checker_private_check_child_node(shape, MMAY_SYNTAX_PATTERN_FIELDS, 0);
    if (shape == NULL) {
        return false;
    }
    for (;;) {
        const MarimayAstNode *field = alternative->variant->kind
                == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL
            ? mmay_checker_private_check_child_node_any(shape, ordinal)
            : mmay_checker_private_check_child_node(
                shape, MMAY_SYNTAX_PATTERN_FIELD, ordinal
            );
        const MarimayAstNode *child = field;
        const MarimayCheckEnumPayloadItemDeclaration *payload = NULL;
        MarimayTypeId payload_type = MMAY_TYPE_INVALID;
        MarimayCheckName name;
        MarimayExpressionValue projected;
        MarimayMirBlockId next = success;
        size_t payload_index = ordinal;
        if (field == NULL) {
            break;
        }
        if (alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_NAMED) {
            const MarimaySyntaxToken *token = mmay_checker_private_check_child_token(
                field, MMAY_TOKEN_IDENTIFIER, 0
            );
            if (token == NULL || token->synthetic) {
                return false;
            }
            name = (MarimayCheckName){function->checker->ast->snapshot,
                token->start, token->end};
            payload = mmay_checker_private_checker_find_enum_payload(
                function->checker, alternative->variant, name, &payload_index
            );
            child = mmay_checker_private_check_child_node_any(field, 0);
        } else {
            memset(&name, 0, sizeof(name));
            payload = mmay_vector_at_const(&alternative->variant->payload, ordinal);
        }
        ordinal += 1;
        if (payload == NULL || !mmay_checker_private_enum_match_payload_type(
                function,
                alternative->enum_type,
                enum_declaration,
                payload->type,
                &payload_type
            ) || (ordinal < alternative->variant->payload.length
                && !mmay_checker_private_function_add_block(function, &next))) {
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!mmay_checker_private_function_emit_enum_access(
                function, MMAY_MIR_ENUM_GET_PAYLOAD, payload_type, receiver,
                enum_id, alternative->variant_index, payload_index, field->start,
                field->end, &projected
            ) || (child == NULL
                ? (!mmay_checker_private_assign_match_local(
                    function, name, payload_type, projected.register_id,
                    name.start, name.end
                ) || !mmay_checker_private_function_set_goto(
                    function, function->block, next, field->end, field->end
                ))
                : !mmay_checker_private_compile_nested_pattern_cps(
                    function, child, payload_type, projected.register_id,
                    next, failure
                ))) {
            return false;
        }
        function->block = next;
        function->terminated = false;
    }
    return ordinal == alternative->variant->payload.length;
}

static bool mmay_checker_private_compile_nested_pattern_cps(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayTypeId type,
    MarimayMirRegister source,
    MarimayMirBlockId success,
    MarimayMirBlockId failure
) {
    MarimayCheckName name;
    MarimayCheckRecordDeclaration *record = NULL;
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    MarimayCheckEnumDeclaration concrete_enum;
    MarimayCheckEnumVariantDeclaration *variant = NULL;
    MarimayCheckEnumMatchAlternative alternative;
    MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
    MarimayExpressionValue matched;
    MarimayExpressionValue guarded;
    MarimayMirBlockId matched_block = MMAY_MIR_NO_BLOCK;
    size_t variant_index = 0;
    const MarimayAstNode *constructor_name = NULL;
    if (pattern == NULL) {
        return false;
    }
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        if (!mmay_checker_private_match_remainder_name(
                function, pattern, &has_binding, &name
            ) || (has_binding && !mmay_checker_private_assign_match_local(
                function, name, type, source, pattern->start, pattern->end
            ))) {
            return false;
        }
        return mmay_checker_private_function_set_goto(
            function, function->block, success, pattern->end, pattern->end
        );
    }
    if (pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        return mmay_checker_private_enum_match_name(function, pattern, &name)
            && mmay_checker_private_assign_match_local(
                function, name, type, source, pattern->start, pattern->end
            ) && mmay_checker_private_function_set_goto(
                function, function->block, success, pattern->end, pattern->end
            );
    }
    if (pattern->kind != MMAY_SYNTAX_CONSTRUCTOR_PATTERN) {
        return false;
    }
    record = mmay_checker_private_checker_find_record_by_type(
        function->checker, type
    );
    if (record != NULL) {
        return mmay_checker_private_compile_record_pattern_cps(
            function, pattern, record, source, success, failure
        );
    }
    enum_declaration = mmay_checker_private_checker_find_enum_by_type(
        function->checker, type
    );
    constructor_name = mmay_checker_private_check_child_node(
        pattern, MMAY_SYNTAX_NAME_PATTERN, 0
    );
    if (enum_declaration == NULL
        || !mmay_checker_private_enum_match_name(function, constructor_name, &name)
        || (variant = mmay_checker_private_checker_find_enum_variant(
                function->checker, enum_declaration, name, &variant_index
            )) == NULL
        ) {
        return false;
    }
    concrete_enum = *enum_declaration;
    concrete_enum.type = type;
    if (!mmay_checker_private_function_enum_layout(
            function, &concrete_enum, &enum_id
        ) || !mmay_checker_private_function_add_block(function, &matched_block)) {
        return false;
    }
    memset(&matched, 0, sizeof(matched));
    memset(&guarded, 0, sizeof(guarded));
    if (!mmay_checker_private_function_emit_enum_access(
            function, MMAY_MIR_ENUM_IS_VARIANT, MMAY_TYPE_BOOL, source,
            enum_id, variant_index, 0, pattern->start, pattern->end, &matched
        ) || !mmay_checker_private_function_set_branch(
            function, function->block, matched.register_id, matched_block,
            failure, pattern->start, pattern->end
        )) {
        return false;
    }
    function->block = matched_block;
    function->terminated = false;
    if (!mmay_checker_private_function_emit_enum_access(
            function, MMAY_MIR_ENUM_REQUIRE_VARIANT, type, source, enum_id,
            variant_index, 0, pattern->start, pattern->end, &guarded
        )) {
        return false;
    }
    memset(&alternative, 0, sizeof(alternative));
    alternative.pattern = pattern;
    alternative.variant = variant;
    alternative.enum_type = type;
    alternative.variant_index = variant_index;
    return mmay_checker_private_compile_enum_payloads_cps(
        function, &alternative, enum_id, guarded.register_id, success, failure
    );
}

 bool mmay_checker_private_compile_enum_match_bindings_cps(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    MarimayMirEnumId enum_id,
    MarimayMirRegister receiver,
    MarimayMirBlockId success,
    MarimayMirBlockId failure
) {
    if (alternative == NULL) {
        return false;
    }
    if (alternative->is_catch_all) {
        const MarimayTypeId *type = mmay_vector_at_const(
            &function->mir.register_types, receiver
        );
        return type != NULL && (!alternative->has_catch_binding
            || mmay_checker_private_assign_match_local(
                function, alternative->catch_binding, *type, receiver,
                alternative->pattern->start, alternative->pattern->end
            )) && mmay_checker_private_function_set_goto(
                function, function->block, success,
                alternative->pattern->end, alternative->pattern->end
            );
    }
    return mmay_checker_private_compile_enum_payloads_cps(
        function, alternative, enum_id, receiver, success, failure
    );
}

static bool mmay_checker_compile_nested_record_pattern(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayTypeId type,
    MarimayMirRegister source
) {
    MarimayCheckName name;
    MarimayCheckRecordDeclaration *record = NULL;
    MarimayMirRecordId layout = MMAY_MIR_NO_RECORD;
    const MarimayAstNode *constructor_name = NULL;
    const MarimayAstNode *named = NULL;
    const MarimayAstNode *fields = NULL;
    size_t ordinal = 0;
    if (pattern == NULL) {
        return false;
    }
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        return mmay_checker_private_match_remainder_name(
                function, pattern, &has_binding, &name
            ) && (!has_binding || mmay_checker_private_assign_match_local(
                function, name, type, source, pattern->start, pattern->end
            ));
    }
    if (pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        return mmay_checker_private_enum_match_name(function, pattern, &name)
            && mmay_checker_private_assign_match_local(
                function, name, type, source, pattern->start, pattern->end
            );
    }
    if (pattern->kind != MMAY_SYNTAX_CONSTRUCTOR_PATTERN) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "nested pattern does not match the record field type",
            pattern->start, pattern->end
        );
        return false;
    }
    constructor_name = mmay_checker_private_check_child_node(
        pattern, MMAY_SYNTAX_NAME_PATTERN, 0
    );
    if (!mmay_checker_private_enum_match_name(function, constructor_name, &name)
        || (record = mmay_checker_private_checker_find_record_by_type(
                function->checker, type
            )) == NULL
        || mmay_checker_private_checker_find_record(function->checker, name)
            != record
        || (named = mmay_checker_private_check_child_node(
                pattern, MMAY_SYNTAX_NAMED_PATTERN, 0
            )) == NULL
        || (fields = mmay_checker_private_check_child_node(
                named, MMAY_SYNTAX_PATTERN_FIELDS, 0
            )) == NULL
        || !mmay_checker_private_function_record_layout(
            function, record, &layout
        )) {
        if (!mmay_checker_has_failed(function->checker)) {
            (void)mmay_checker_add_diagnostic(
                function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "nested record pattern constructor does not match its exact field type",
                pattern->start, pattern->end
            );
        }
        return false;
    }
    for (;;) {
        const MarimayAstNode *field = mmay_checker_private_check_child_node(
            fields, MMAY_SYNTAX_PATTERN_FIELD, ordinal
        );
        const MarimaySyntaxToken *token = NULL;
        const MarimayAstNode *child = NULL;
        MarimayCheckRecordFieldDeclaration *declaration = NULL;
        MarimayExpressionValue projected;
        size_t field_index = 0;
        if (field == NULL) {
            break;
        }
        ordinal += 1;
        token = mmay_checker_private_check_child_token(
            field, MMAY_TOKEN_IDENTIFIER, 0
        );
        if (token == NULL || token->synthetic) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        name = (MarimayCheckName){function->checker->ast->snapshot,
            token->start, token->end};
        declaration = mmay_checker_private_checker_find_record_field(
            function->checker, record, name, &field_index
        );
        child = mmay_checker_private_check_child_node_any(field, 0);
        if (declaration == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!mmay_checker_private_function_emit_record_access(
                function, MMAY_MIR_RECORD_GET, declaration->type, source,
                MMAY_MIR_NO_REGISTER, layout, field_index,
                child != NULL ? child->start : name.start,
                child != NULL ? child->end : name.end, &projected
            ) || (child == NULL
                ? !mmay_checker_private_assign_match_local(
                    function, name, declaration->type, projected.register_id,
                    name.start, name.end
                ) : !mmay_checker_compile_nested_record_pattern(
                    function, child, declaration->type, projected.register_id
                ))) {
            return false;
        }
    }
    return ordinal == record->fields.length;
}

 bool mmay_checker_private_bind_enum_match_payload(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    MarimayMirEnumId enum_id,
    MarimayMirRegister guarded,
    size_t payload_index,
    const MarimayAstNode *binding,
    MarimayCheckName shorthand
) {
    const MarimayCheckEnumPayloadItemDeclaration *payload = mmay_vector_at_const(
        &alternative->variant->payload,
        payload_index
    );
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    MarimayTypeId payload_type = MMAY_TYPE_INVALID;
    MarimayExpressionValue projected;
    MarimayCheckName name = shorthand;
    if (binding != NULL && binding->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        if (!mmay_checker_private_match_remainder_name(
                function, binding, &has_binding, &name
            )) {
            return false;
        }
        if (!has_binding) {
            return true;
        }
    }
    if (payload == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    enum_declaration = mmay_checker_private_checker_find_enum_by_type(
        function->checker, alternative->enum_type
    );
    if (enum_declaration == NULL
        || !mmay_checker_private_enum_match_payload_type(
            function,
            alternative->enum_type,
            enum_declaration,
            payload->type,
            &payload_type
        )) {
        return false;
    }
    memset(&projected, 0, sizeof(projected));
    if (!mmay_checker_private_function_emit_enum_access(
            function,
            MMAY_MIR_ENUM_GET_PAYLOAD,
            payload_type,
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
        return mmay_checker_private_assign_match_local(
            function, name, payload_type, projected.register_id,
            name.start, name.end
        );
    }
    return mmay_checker_compile_nested_record_pattern(
        function, binding, payload_type, projected.register_id
    );
}

 bool mmay_checker_private_compile_enum_match_bindings(
    MarimayFunctionChecker *function,
    const MarimayCheckEnumMatchAlternative *alternative,
    MarimayMirEnumId enum_id,
    MarimayMirRegister receiver
) {
    if (alternative->is_catch_all) {
        const MarimayTypeId *receiver_type = mmay_vector_at_const(
            &function->mir.register_types,
            receiver
        );
        if (receiver_type == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        return !alternative->has_catch_binding
            || mmay_checker_private_assign_match_local(
                function,
                alternative->catch_binding,
                *receiver_type,
                receiver,
                alternative->pattern->start,
                alternative->pattern->end
            );
    }
    if (alternative->variant == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    if (alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_UNIT) {
        return true;
    }
    if (alternative->variant->kind
        == MMAY_CHECKED_ENUM_VARIANT_POSITIONAL) {
        const MarimayAstNode *positional = mmay_checker_private_check_child_node(
            alternative->pattern,
            MMAY_SYNTAX_POSITIONAL_PATTERN,
            0
        );
        const MarimayAstNode *list = mmay_checker_private_check_child_node(
            positional,
            MMAY_SYNTAX_PATTERN_LIST,
            0
        );
        size_t payload_index = 0;
        const MarimayAstNode *binding = NULL;
        MarimayCheckName no_shorthand;
        memset(&no_shorthand, 0, sizeof(no_shorthand));
        while ((binding = mmay_checker_private_check_child_node_any(
                list,
                payload_index
            )) != NULL) {
            if (!mmay_checker_private_bind_enum_match_payload(
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
    if (alternative->variant->kind == MMAY_CHECKED_ENUM_VARIANT_NAMED) {
        const MarimayAstNode *named = mmay_checker_private_check_child_node(
            alternative->pattern,
            MMAY_SYNTAX_NAMED_PATTERN,
            0
        );
        const MarimayAstNode *fields = mmay_checker_private_check_child_node(
            named,
            MMAY_SYNTAX_PATTERN_FIELDS,
            0
        );
        size_t field_ordinal = 0;
        for (;;) {
            const MarimayAstNode *field = mmay_checker_private_check_child_node(
                fields,
                MMAY_SYNTAX_PATTERN_FIELD,
                field_ordinal
            );
            const MarimaySyntaxToken *field_token = NULL;
            const MarimayAstNode *binding = NULL;
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
                    MMAY_CHECK_INTERNAL_ERROR
                );
                return false;
            }
            field_name.snapshot = function->checker->ast->snapshot;
            field_name.start = field_token->start;
            field_name.end = field_token->end;
            if (mmay_checker_private_checker_find_enum_payload(
                    function->checker,
                    alternative->variant,
                    field_name,
                    &payload_index
                ) == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                return false;
            }
            binding = mmay_checker_private_check_child_node_any(field, 0);
            if (!mmay_checker_private_bind_enum_match_payload(
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
    mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
    return false;
}

 bool mmay_checker_private_compile_record_match_bindings(
    MarimayFunctionChecker *function,
    const MarimayCheckRecordMatchAlternative *alternative,
    MarimayCheckRecordDeclaration *record_declaration,
    MarimayMirRecordId record,
    MarimayMirRegister receiver
) {
    const MarimayAstNode *named = NULL;
    const MarimayAstNode *fields = NULL;
    size_t field_ordinal = 0;
    if (alternative == NULL || record_declaration == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (!alternative->is_record_pattern) {
        return !alternative->has_catch_binding
            || mmay_checker_private_assign_match_local(
                function,
                alternative->catch_binding,
                record_declaration->type,
                receiver,
                alternative->pattern->start,
                alternative->pattern->end
            );
    }
    named = mmay_checker_private_check_child_node(
        alternative->pattern,
        MMAY_SYNTAX_NAMED_PATTERN,
        0
    );
    fields = mmay_checker_private_check_child_node(
        named,
        MMAY_SYNTAX_PATTERN_FIELDS,
        0
    );
    if (named == NULL || fields == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
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
        MarimayExpressionValue projected;
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
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        field_name.snapshot = function->checker->ast->snapshot;
        field_name.start = field_token->start;
        field_name.end = field_token->end;
        binding = mmay_checker_private_check_child_node_any(field, 0);
        if (binding != NULL && binding->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
            bool has_binding = false;
            MarimayCheckName remainder_name;
            if (!mmay_checker_private_match_remainder_name(
                    function, binding, &has_binding, &remainder_name
                )) {
                return false;
            }
            if (!has_binding) {
                continue;
            }
        }
        field_declaration = mmay_checker_private_checker_find_record_field(
            function->checker,
            record_declaration,
            field_name,
            &field_index
        );
        if (field_declaration == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        memset(&projected, 0, sizeof(projected));
        if (!mmay_checker_private_function_emit_record_access(
                function, MMAY_MIR_RECORD_GET, field_declaration->type,
                receiver, MMAY_MIR_NO_REGISTER, record, field_index,
                binding != NULL ? binding->start : field_name.start,
                binding != NULL ? binding->end : field_name.end, &projected
            ) || (binding == NULL
                ? !mmay_checker_private_assign_match_local(
                    function, field_name, field_declaration->type,
                    projected.register_id, field_name.start, field_name.end
                ) : !mmay_checker_compile_nested_record_pattern(
                    function, binding, field_declaration->type,
                    projected.register_id
                ))) {
            return false;
        }
    }
    return field_ordinal == record_declaration->fields.length;
}
