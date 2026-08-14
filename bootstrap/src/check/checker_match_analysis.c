#include "checker_match_analysis_api.h"
#include "checker_core_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"
#include "checker_match_witness_api.h"






 bool ruda_checker_private_analyze_bool_match_alternative(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaCheckScalarMatchAlternative *alternative
) {
    const RudaSyntaxToken *literal = NULL;
    RudaCheckName name;
    if (pattern == NULL || bindings == NULL || alternative == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        alternative->is_catch_all = true;
        return true;
    }
    if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        if (!ruda_checker_private_enum_match_name(function, pattern, &name)
            || !ruda_checker_private_enum_match_add_binding(
                function,
                bindings,
                name,
                RUDA_TYPE_BOOL
            )) {
            return false;
        }
        alternative->is_catch_all = true;
        alternative->has_catch_binding = true;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind != RUDA_SYNTAX_LITERAL_PATTERN) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    literal = ruda_checker_private_check_first_token(pattern);
    if (literal == NULL || literal->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (literal->kind != RUDA_TOKEN_KW_TRUE
        && literal->kind != RUDA_TOKEN_KW_FALSE) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    alternative->literal = literal->kind == RUDA_TOKEN_KW_TRUE;
    return true;
}

 bool ruda_checker_private_bool_match_alternative_redundant(
    const RudaCheckScalarMatchArm *arm,
    const RudaCheckScalarMatchAlternative *alternative
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const RudaCheckScalarMatchAlternative *earlier = ruda_vector_at_const(
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

 bool ruda_checker_private_analyze_bool_match_arm(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    unsigned char seen[2],
    bool *covered,
    RudaCheckScalarMatchArm *arm
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
    if (arm == NULL || arm_node == NULL || pattern_wrapper == NULL
        || pattern == NULL || seen == NULL || covered == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!ruda_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(RudaCheckScalarMatchAlternative)
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
        ) || !ruda_checker_private_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!ruda_checker_has_failed(function->checker)) {
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
        RudaCheckScalarMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !ruda_checker_private_analyze_bool_match_alternative(
                function,
                *alternative_pattern,
                &alternative_bindings,
                &alternative
            )) {
            goto cleanup;
        }
        if (alternative.is_catch_all
            && arm->alternatives.length != 0) {
            const RudaCheckScalarMatchAlternative *subsumed =
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
                && seen[alternative.literal ? 1 : 0] != 0)
            || ruda_checker_private_bool_match_alternative_redundant(
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
            const RudaCheckScalarMatchAlternative *alternative =
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
            } else {
                seen[alternative->literal ? 1 : 0] = 1;
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

 bool ruda_checker_private_analyze_scalar_match_alternative(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaVector *bindings,
    RudaTypeId scalar_type,
    RudaCheckScalarMatchAlternative *alternative
) {
    const RudaSyntaxToken *literal = NULL;
    RudaCheckName name;
    uint64_t magnitude = 0;
    RudaOwnedBytes decoded;
    RudaStringLiteralStatus decode_status;
    size_t new_string_bytes = 0;
    memset(&decoded, 0, sizeof(decoded));
    if (pattern == NULL || bindings == NULL || alternative == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->string_constant = RUDA_MIR_NO_STRING;
    alternative->pattern = pattern;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        alternative->is_catch_all = true;
        return true;
    }
    if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        if (!ruda_checker_private_enum_match_name(function, pattern, &name)
            || !ruda_checker_private_enum_match_add_binding(
                function,
                bindings,
                name,
                scalar_type
            )) {
            return false;
        }
        alternative->is_catch_all = true;
        alternative->has_catch_binding = true;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind != RUDA_SYNTAX_LITERAL_PATTERN) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    literal = ruda_checker_private_check_first_token(pattern);
    if (literal == NULL || literal->synthetic) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (scalar_type == RUDA_TYPE_I64 && literal->kind != RUDA_TOKEN_INTEGER) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "literal pattern type must match the integer scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    if (scalar_type == RUDA_TYPE_I64 && !ruda_checker_private_integer_magnitude(
            function->checker,
            literal,
            (uint64_t)INT64_MAX,
            &magnitude
        )) {
        return false;
    }
    if (scalar_type == RUDA_TYPE_I64) {
        alternative->integer = (int64_t)magnitude;
        return true;
    }
    if (scalar_type != RUDA_TYPE_STRING
        || (literal->kind != RUDA_TOKEN_STRING
            && literal->kind != RUDA_TOKEN_RAW_STRING)) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "literal pattern type must match the string scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    decode_status = ruda_string_literal_decode(
        function->checker->source + literal->start,
        literal->end - literal->start,
        literal->kind == RUDA_TOKEN_RAW_STRING,
        function->checker->result->allocator,
        &decoded
    );
    if (decode_status != RUDA_STRING_LITERAL_OK) {
        ruda_checker_fail(
            function->checker,
            decode_status == RUDA_STRING_LITERAL_ALLOCATION_FAILED
                ? RUDA_CHECK_ALLOCATION_FAILED
                : RUDA_CHECK_INTERNAL_ERROR
        );
        return false;
    }
    if (function->mir.string_constants.length
            >= function->mir.options.max_string_constants
        || !ruda_size_add(
            function->mir.string_bytes,
            decoded.length,
            &new_string_bytes
        ) || new_string_bytes > function->mir.options.max_string_bytes) {
        ruda_owned_bytes_dispose(&decoded);
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        return false;
    }
    if (!ruda_mir_add_string_constant(
            &function->mir,
            decoded.data,
            decoded.length,
            &alternative->string_constant
        )) {
        ruda_owned_bytes_dispose(&decoded);
        ruda_checker_fail(
            function->checker,
            RUDA_CHECK_ALLOCATION_FAILED
        );
        return false;
    }
    ruda_owned_bytes_dispose(&decoded);
    return true;
}

 bool ruda_checker_private_scalar_match_key(
    const RudaFunctionChecker *function,
    const RudaCheckScalarMatchAlternative *alternative,
    RudaTypeId scalar_type,
    const void **key,
    size_t *key_length
) {
    const RudaMirStringConstant *string_constant = NULL;
    if (alternative == NULL || alternative->is_catch_all
        || key == NULL || key_length == NULL) {
        return false;
    }
    if (scalar_type == RUDA_TYPE_I64) {
        *key = &alternative->integer;
        *key_length = sizeof(alternative->integer);
        return true;
    }
    string_constant = scalar_type == RUDA_TYPE_STRING
        ? ruda_checker_private_check_match_string_constant(
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

 bool ruda_checker_private_scalar_match_alternative_redundant(
    const RudaFunctionChecker *function,
    const RudaCheckScalarMatchArm *arm,
    const RudaCheckScalarMatchAlternative *alternative,
    RudaTypeId scalar_type
) {
    size_t index = 0;
    for (index = 0; index < arm->alternatives.length; index += 1) {
        const RudaCheckScalarMatchAlternative *earlier = ruda_vector_at_const(
            &arm->alternatives,
            index
        );
        bool equal = scalar_type == RUDA_TYPE_I64
            ? earlier != NULL
                && earlier->integer == alternative->integer
            : earlier != NULL && ruda_checker_private_check_match_string_equal(
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

 bool ruda_checker_private_analyze_scalar_match_arm(
    RudaFunctionChecker *function,
    const RudaAstNode *arm_node,
    RudaMap *seen_literals,
    bool *covered,
    RudaTypeId scalar_type,
    RudaCheckScalarMatchArm *arm
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
    if (arm == NULL || arm_node == NULL || pattern_wrapper == NULL
        || pattern == NULL || seen_literals == NULL || covered == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!ruda_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(RudaCheckScalarMatchAlternative)
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
        ) || !ruda_checker_private_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!ruda_checker_has_failed(function->checker)) {
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
        RudaCheckScalarMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !ruda_checker_private_analyze_scalar_match_alternative(
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
            const RudaCheckScalarMatchAlternative *subsumed =
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
        {
            const void *key = NULL;
            size_t key_length = 0;
            bool has_key = alternative.is_catch_all
                || ruda_checker_private_scalar_match_key(
                    function,
                    &alternative,
                    scalar_type,
                    &key,
                    &key_length
                );
            if (!has_key) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (*covered || (!alternative.is_catch_all
                    && ruda_map_get_const(
                        seen_literals,
                        key,
                        key_length
                    ) != NULL)
                || ruda_checker_private_scalar_match_alternative_redundant(
                    function,
                    arm,
                    &alternative,
                    scalar_type
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
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            unsigned char present = 1;
            const void *key = NULL;
            size_t key_length = 0;
            if (alternative == NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            }
            if (alternative->is_catch_all) {
                *covered = true;
            } else if (!ruda_checker_private_scalar_match_key(
                    function,
                    alternative,
                    scalar_type,
                    &key,
                    &key_length
                )) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
                goto cleanup;
            } else if (!ruda_map_put(
                    seen_literals,
                    key,
                    key_length,
                    &present
                )) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_ALLOCATION_FAILED
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

 bool ruda_checker_private_resolve_record_match_pattern(
    RudaFunctionChecker *function,
    const RudaAstNode *pattern,
    RudaCheckRecordDeclaration *scrutinee_record,
    RudaVector *bindings,
    RudaCheckRecordMatchAlternative *alternative
) {
    const RudaAstNode *constructor_name = NULL;
    const RudaAstNode *named = NULL;
    RudaCheckRecordDeclaration *pattern_record = NULL;
    RudaCheckTypeAliasDeclaration *alias = NULL;
    RudaCheckName name;
    if (pattern == NULL || scrutinee_record == NULL || bindings == NULL
        || alternative == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    memset(alternative, 0, sizeof(*alternative));
    alternative->pattern = pattern;
    if (pattern->kind == RUDA_SYNTAX_WILDCARD_PATTERN) {
        return true;
    }
    if (pattern->kind == RUDA_SYNTAX_NAME_PATTERN) {
        if (!ruda_checker_private_enum_match_name(function, pattern, &name)
            || !ruda_checker_private_enum_match_add_binding(
                function,
                bindings,
                name,
                scrutinee_record->type
            )) {
            return false;
        }
        alternative->has_catch_binding = true;
        alternative->catch_binding = name;
        return true;
    }
    if (pattern->kind != RUDA_SYNTAX_CONSTRUCTOR_PATTERN) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "pattern does not match the nominal record scrutinee",
            pattern->start,
            pattern->end
        );
        return false;
    }
    constructor_name = ruda_checker_private_check_child_node(
        pattern,
        RUDA_SYNTAX_NAME_PATTERN,
        0
    );
    named = ruda_checker_private_check_child_node(
        pattern,
        RUDA_SYNTAX_NAMED_PATTERN,
        0
    );
    if (!ruda_checker_private_enum_match_name(function, constructor_name, &name)) {
        return false;
    }
    pattern_record = ruda_checker_private_checker_find_record(function->checker, name);
    if (pattern_record == NULL) {
        alias = ruda_checker_private_checker_find_type_alias(function->checker, name);
        if (alias != NULL && ruda_checker_private_evaluate_type_alias(
                function->checker,
                alias,
                name.start,
                name.end
            )) {
            pattern_record = ruda_checker_private_checker_find_record_by_type(
                function->checker,
                alias->type
            );
        }
    }
    if (pattern_record == NULL) {
        if (ruda_checker_private_checker_find_top_level(function->checker, name) != NULL) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "record pattern name does not denote a record type",
                name.start,
                name.end
            );
        } else {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_UNKNOWN_TYPE,
                "unknown record pattern type",
                name.start,
                name.end
            );
        }
        return false;
    }
    if (pattern_record->type != scrutinee_record->type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "record pattern type differs from the scrutinee type",
            name.start,
            name.end
        );
        return false;
    }
    if (named == NULL || ruda_checker_private_check_child_node(
            pattern,
            RUDA_SYNTAX_POSITIONAL_PATTERN,
            0
        ) != NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "record pattern requires its named field shape",
            pattern->start,
            pattern->end
        );
        return false;
    }
    if (!ruda_checker_private_record_match_validate_named(
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

 bool ruda_checker_private_analyze_record_match_arm(
    RudaFunctionChecker *function,
    RudaCheckRecordDeclaration *record_declaration,
    const RudaAstNode *arm_node,
    bool *covered,
    RudaCheckRecordMatchArm *arm
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
    if (record_declaration == NULL || arm_node == NULL || covered == NULL
        || arm == NULL || pattern_wrapper == NULL || pattern == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    memset(arm, 0, sizeof(*arm));
    if (!ruda_vector_init(
            &arm->alternatives,
            function->checker->result->allocator,
            sizeof(RudaCheckRecordMatchAlternative)
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
        ) || !ruda_checker_private_match_collect_alternatives(
            function,
            pattern,
            &pattern_nodes
        ) || pattern_nodes.length == 0) {
        if (!ruda_checker_has_failed(function->checker)) {
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
        RudaCheckRecordMatchAlternative alternative;
        alternative_bindings.length = 0;
        if (alternative_pattern == NULL || *alternative_pattern == NULL
            || !ruda_checker_private_resolve_record_match_pattern(
                function,
                *alternative_pattern,
                record_declaration,
                &alternative_bindings,
                &alternative
            )) {
            goto cleanup;
        }
        if (*covered || arm->alternatives.length != 0) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_REDUNDANT_PATTERN,
                "record pattern is unreachable after an irrefutable pattern",
                alternative.pattern->start,
                alternative.pattern->end
            );
            goto cleanup;
        }
        if (!ruda_vector_append(
                &arm->bindings,
                alternative_bindings.data,
                alternative_bindings.length
            ) || !ruda_vector_push(&arm->alternatives, &alternative)) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
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
        ruda_vector_dispose(&alternative_bindings);
    }
    if (pattern_nodes_initialized) {
        ruda_vector_dispose(&pattern_nodes);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}
