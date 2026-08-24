#include "checker_match_analysis_api.h"
#include "checker_core_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"
#include "checker_match_witness_api.h"






 bool mmay_checker_private_analyze_bool_match_alternative(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayVector *bindings,
    MarimayCheckScalarMatchAlternative *alternative
) {
    const MarimaySyntaxToken *literal = NULL;
    MarimayCheckName name;
    if (pattern == NULL || bindings == NULL || alternative == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        if (!mmay_checker_private_match_remainder_name(
                function, pattern, &has_binding, &name
            ) || (has_binding
                && !mmay_checker_private_enum_match_add_binding(
                    function, bindings, name, MMAY_TYPE_BOOL
                ))) {
            return false;
        }
        alternative->is_catch_all = true;
        alternative->has_catch_binding = has_binding;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "catch-all binding must use '_ as name'",
            pattern->start, pattern->end
        );
        return false;
    }
    if (pattern->kind != MMAY_SYNTAX_LITERAL_PATTERN) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    literal = mmay_checker_private_check_first_token(pattern);
    if (literal == NULL || literal->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (literal->kind != MMAY_TOKEN_KW_TRUE
        && literal->kind != MMAY_TOKEN_KW_FALSE) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    alternative->literal = literal->kind == MMAY_TOKEN_KW_TRUE;
    return true;
}

 bool mmay_checker_private_bool_match_alternative_redundant(
    const MarimayCheckScalarMatchArm *arm,
    const MarimayCheckScalarMatchAlternative *alternative
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const MarimayCheckScalarMatchAlternative *earlier = mmay_vector_at_const(
            &arm->alternatives,
            index
        );
        if (earlier != NULL && (earlier->is_catch_all
                || (!alternative->is_catch_all
                    && earlier->literal == alternative->literal))) {
            return true;
        }
    }
    return false;
}

 bool mmay_checker_private_analyze_bool_match_arm(
    MarimayFunctionChecker *function,
    const MarimayAstNode *arm_node,
    unsigned char seen[2],
    bool *covered,
    MarimayCheckScalarMatchArm *arm
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
    if (arm == NULL || arm_node == NULL || pattern_wrapper == NULL
        || pattern == NULL || seen == NULL || covered == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!mmay_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(MarimayCheckScalarMatchAlternative)
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
        ) || !mmay_checker_private_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!mmay_checker_has_failed(function->checker)) {
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
        MarimayCheckScalarMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !mmay_checker_private_analyze_bool_match_alternative(
                function,
                *alternative_pattern,
                &alternative_bindings,
                &alternative
            )) {
            goto cleanup;
        }
        if (alternative.is_catch_all
            && arm->alternatives.length != 0) {
            const MarimayCheckScalarMatchAlternative *subsumed =
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
                && seen[alternative.literal ? 1 : 0] != 0)
            || mmay_checker_private_bool_match_alternative_redundant(
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
            const MarimayCheckScalarMatchAlternative *alternative =
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
            } else {
                seen[alternative->literal ? 1 : 0] = 1;
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

 bool mmay_checker_private_analyze_scalar_match_alternative(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayVector *bindings,
    MarimayTypeId scalar_type,
    MarimayCheckScalarMatchAlternative *alternative
) {
    const MarimaySyntaxToken *literal = NULL;
    MarimayCheckName name;
    uint64_t magnitude = 0;
    MarimayOwnedBytes decoded;
    MarimayStringLiteralStatus decode_status;
    size_t new_string_bytes = 0;
    memset(&decoded, 0, sizeof(decoded));
    if (pattern == NULL || bindings == NULL || alternative == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->string_constant = MMAY_MIR_NO_STRING;
    alternative->pattern = pattern;
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        if (!mmay_checker_private_match_remainder_name(
                function, pattern, &has_binding, &name
            ) || (has_binding
                && !mmay_checker_private_enum_match_add_binding(
                    function, bindings, name, scalar_type
                ))) {
            return false;
        }
        alternative->is_catch_all = true;
        alternative->has_catch_binding = has_binding;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "catch-all binding must use '_ as name'",
            pattern->start, pattern->end
        );
        return false;
    }
    if (pattern->kind != MMAY_SYNTAX_LITERAL_PATTERN) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    literal = mmay_checker_private_check_first_token(pattern);
    if (literal == NULL || literal->synthetic) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (scalar_type == MMAY_TYPE_I64 && literal->kind != MMAY_TOKEN_INTEGER) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "literal pattern type must match the integer scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    if (scalar_type == MMAY_TYPE_I64 && !mmay_checker_private_integer_magnitude(
            function->checker,
            literal,
            (uint64_t)INT64_MAX,
            &magnitude
        )) {
        return false;
    }
    if (scalar_type == MMAY_TYPE_I64) {
        alternative->integer = (int64_t)magnitude;
        return true;
    }
    if (scalar_type != MMAY_TYPE_STRING
        || (literal->kind != MMAY_TOKEN_STRING
            && literal->kind != MMAY_TOKEN_RAW_STRING)) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "literal pattern type must match the string scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    decode_status = mmay_string_literal_decode(
        function->checker->source + literal->start,
        literal->end - literal->start,
        literal->kind == MMAY_TOKEN_RAW_STRING,
        function->checker->result->allocator,
        &decoded
    );
    if (decode_status != MMAY_STRING_LITERAL_OK) {
        mmay_checker_fail(
            function->checker,
            decode_status == MMAY_STRING_LITERAL_ALLOCATION_FAILED
                ? MMAY_CHECK_ALLOCATION_FAILED
                : MMAY_CHECK_INTERNAL_ERROR
        );
        return false;
    }
    if (function->mir.string_constants.length
            >= function->mir.options.max_string_constants
        || !mmay_size_add(
            function->mir.string_bytes,
            decoded.length,
            &new_string_bytes
        ) || new_string_bytes > function->mir.options.max_string_bytes) {
        mmay_owned_bytes_dispose(&decoded);
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!mmay_mir_add_string_constant(
            &function->mir,
            decoded.data,
            decoded.length,
            &alternative->string_constant
        )) {
        mmay_owned_bytes_dispose(&decoded);
        mmay_checker_fail(
            function->checker,
            MMAY_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    mmay_owned_bytes_dispose(&decoded);
    return true;
}

 bool mmay_checker_private_scalar_match_key(
    const MarimayFunctionChecker *function,
    const MarimayCheckScalarMatchAlternative *alternative,
    MarimayTypeId scalar_type,
    const void **key,
    size_t *key_length
) {
    const MarimayMirStringConstant *string_constant = NULL;
    if (alternative == NULL || alternative->is_catch_all
        || key == NULL || key_length == NULL) {
        return false;
    }
    if (scalar_type == MMAY_TYPE_I64) {
        *key = &alternative->integer;
        *key_length = sizeof(alternative->integer);
        return true;
    }
    string_constant = scalar_type == MMAY_TYPE_STRING
        ? mmay_checker_private_check_match_string_constant(
            function,
            alternative->string_constant
        )
        : NULL;
    if (string_constant == NULL) {
        return false;
    }
    *key = string_constant->value.data;
    *key_length = string_constant->value.length;
    return true;
}

 bool mmay_checker_private_scalar_match_alternative_redundant(
    const MarimayFunctionChecker *function,
    const MarimayCheckScalarMatchArm *arm,
    const MarimayCheckScalarMatchAlternative *alternative,
    MarimayTypeId scalar_type
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const MarimayCheckScalarMatchAlternative *earlier = mmay_vector_at_const(
            &arm->alternatives,
            index
        );
        bool equal = scalar_type == MMAY_TYPE_I64
            ? earlier != NULL
                && earlier->integer == alternative->integer
            : earlier != NULL && mmay_checker_private_check_match_string_equal(
                function,
                earlier->string_constant,
                alternative->string_constant
            );
        if (earlier != NULL && (earlier->is_catch_all
                || (!alternative->is_catch_all && equal))) {
            return true;
        }
    }
    return false;
}

 bool mmay_checker_private_analyze_scalar_match_arm(
    MarimayFunctionChecker *function,
    const MarimayAstNode *arm_node,
    MarimayMap *seen_literals,
    bool *covered,
    MarimayTypeId scalar_type,
    MarimayCheckScalarMatchArm *arm
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
    if (arm == NULL || arm_node == NULL || pattern_wrapper == NULL
        || pattern == NULL || seen_literals == NULL || covered == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!mmay_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(MarimayCheckScalarMatchAlternative)
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
        ) || !mmay_checker_private_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!mmay_checker_has_failed(function->checker)) {
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
        MarimayCheckScalarMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !mmay_checker_private_analyze_scalar_match_alternative(
                function,
                *alternative_pattern,
                &alternative_bindings,
                scalar_type,
                &alternative
            )) {
            goto cleanup;
        }
        if (alternative.is_catch_all
            && arm->alternatives.length != 0) {
            const MarimayCheckScalarMatchAlternative *subsumed =
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
        {
            const void *key = NULL;
            size_t key_length = 0;
            bool has_key = alternative.is_catch_all
                || mmay_checker_private_scalar_match_key(
                    function,
                    &alternative,
                    scalar_type,
                    &key,
                    &key_length
                );
            if (!has_key) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (*covered || (!alternative.is_catch_all
                    && mmay_map_get_const(
                        seen_literals,
                        key,
                        key_length
                    ) != NULL)
                || mmay_checker_private_scalar_match_alternative_redundant(
                    function,
                    arm,
                    &alternative,
                    scalar_type
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
            const MarimayCheckScalarMatchAlternative *alternative =
                mmay_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            unsigned char present = 1;
            const void *key = NULL;
            size_t key_length = 0;
            if (alternative == NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (alternative->is_catch_all) {
                *covered = true;
            } else if (!mmay_checker_private_scalar_match_key(
                    function,
                    alternative,
                    scalar_type,
                    &key,
                    &key_length
                )) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            } else if (!mmay_map_put(
                    seen_literals,
                    key,
                    key_length,
                    &present
                )) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_ALLOCATION_FAILED
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

 bool mmay_checker_private_resolve_record_match_pattern(
    MarimayFunctionChecker *function,
    const MarimayAstNode *pattern,
    MarimayCheckRecordDeclaration *scrutinee_record,
    MarimayVector *bindings,
    MarimayCheckRecordMatchAlternative *alternative
) {
    const MarimayAstNode *constructor_name = NULL;
    const MarimayAstNode *named = NULL;
    MarimayCheckRecordDeclaration *pattern_record = NULL;
    MarimayCheckTypeAliasDeclaration *alias = NULL;
    MarimayCheckName name;
    if (pattern == NULL || scrutinee_record == NULL || bindings == NULL
        || alternative == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    if (pattern->kind == MMAY_SYNTAX_WILDCARD_PATTERN) {
        bool has_binding = false;
        if (!mmay_checker_private_match_remainder_name(
                function, pattern, &has_binding, &name
            ) || (has_binding
                && !mmay_checker_private_enum_match_add_binding(
                    function, bindings, name, scrutinee_record->type
                ))) {
            return false;
        }
        alternative->has_catch_binding = has_binding;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind == MMAY_SYNTAX_NAME_PATTERN) {
        (void)mmay_checker_add_diagnostic(
            function->checker, MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "record catch-all binding must use '_ as name'",
            pattern->start, pattern->end
        );
        return false;
    }
    if (pattern->kind != MMAY_SYNTAX_CONSTRUCTOR_PATTERN) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "pattern does not match the nominal record scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    constructor_name = mmay_checker_private_check_child_node(
        pattern,
        MMAY_SYNTAX_NAME_PATTERN,
        0
    );
    named = mmay_checker_private_check_child_node(
        pattern,
        MMAY_SYNTAX_NAMED_PATTERN,
        0
    );
    if (!mmay_checker_private_enum_match_name(function, constructor_name, &name)) {
        return false;
    }
    pattern_record = mmay_checker_private_checker_find_record(function->checker, name);
    if (pattern_record == NULL) {
        alias = mmay_checker_private_checker_find_type_alias(function->checker, name);
        if (alias != NULL && mmay_checker_private_evaluate_type_alias(
                function->checker,
                alias,
                name.start,
                name.end
            )) {
            pattern_record = mmay_checker_private_checker_find_record_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (pattern_record == NULL) {
        if (mmay_checker_private_checker_find_top_level(function->checker, name) != NULL) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "record pattern name does not denote a record type",
                name.start,
                name.end
            );
        } else {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_UNKNOWN_TYPE,
                "unknown record pattern type",
                name.start,
                name.end
            );
        }
        return false;
    }
    if (pattern_record->type != scrutinee_record->type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "record pattern type differs from the scrutinee type",
            name.start,
            name.end
        );
        return false;
    }
    if (named == NULL || mmay_checker_private_check_child_node(
            pattern,
            MMAY_SYNTAX_POSITIONAL_PATTERN,
            0
        ) != NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "record pattern requires its named field shape",
            pattern->start,
            pattern->end
        );
        return false;
    }
    if (!mmay_checker_private_record_match_validate_named(
            function,
            pattern,
            pattern_record,
            bindings
        )) {
        return false;
    }
    alternative->is_record_pattern = true;
    return true;
}

 bool mmay_checker_private_analyze_record_match_arm(
    MarimayFunctionChecker *function,
    MarimayCheckRecordDeclaration *record_declaration,
    const MarimayAstNode *arm_node,
    bool *covered,
    MarimayCheckRecordMatchArm *arm
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
    if (record_declaration == NULL || arm_node == NULL || covered == NULL
        || arm == NULL || pattern_wrapper == NULL || pattern == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!mmay_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(MarimayCheckRecordMatchAlternative)
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
        ) || !mmay_checker_private_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!mmay_checker_has_failed(function->checker)) {
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
        MarimayCheckRecordMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !mmay_checker_private_resolve_record_match_pattern(
                function,
                *alternative_pattern,
                record_declaration,
                &alternative_bindings,
                &alternative
            )) {
            goto cleanup;
        }
        if (*covered || arm->alternatives.length != 0) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_REDUNDANT_PATTERN,
                "record pattern is unreachable after an irrefutable pattern",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (!mmay_vector_append(
                &arm->bindings,
                alternative_bindings.data,
                alternative_bindings.length
            ) || !mmay_vector_push(&arm->alternatives, &alternative)) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
    }
    if (arm->guard == NULL) {
        *covered = true;
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
