#include "checker_match_dispatch_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_lookup_api.h"
#include "checker_match_analysis_api.h"
#include "checker_match_binding_api.h"
#include "checker_match_lower_api.h"
#include "checker_match_witness_api.h"
#include "checker_tuple_match_api.h"






 bool mmay_checker_private_compile_scalar_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue scrutinee,
    MarimayTypeId scalar_type,
    MarimayExpressionValue *result
) {
    static const uint64_t seen_key0 = UINT64_C(0x727564612d693634);
    static const uint64_t seen_key1 = UINT64_C(0x2d6d617463682d31);
    MarimayCheckScalarMatchArm *arms = NULL;
    MarimayMap seen_literals;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    MarimayMirBlockId join = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId test_block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayTypeId match_type = expected;
    bool seen_initialized = false;
    bool covered = false;
    bool accepted = false;
    memset(&seen_literals, 0, sizeof(seen_literals));
    if (node == NULL || result == NULL || !scrutinee.valid
        || scrutinee.type != scalar_type
        || (scalar_type != MMAY_TYPE_I64
            && scalar_type != MMAY_TYPE_STRING)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!mmay_map_init(
            &seen_literals,
            function->checker->result->allocator,
            sizeof(unsigned char),
            seen_key0,
            seen_key1
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    seen_initialized = true;
    while (mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        if (scalar_type == MMAY_TYPE_I64) {
            (void)mmay_checker_private_add_i64_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                NULL,
                0
            );
        } else {
            (void)mmay_checker_private_add_string_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                NULL,
                0
            );
        }
        goto cleanup;
    }
    if (!mmay_size_multiply(
            arm_count,
            sizeof(MarimayCheckScalarMatchArm),
            &arm_bytes
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = mmay_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const MarimayAstNode *arm_node = mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!mmay_checker_private_analyze_scalar_match_arm(
                function,
                arm_node,
                &seen_literals,
                &covered,
                scalar_type,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (!covered) {
        if (scalar_type == MMAY_TYPE_I64) {
            (void)mmay_checker_private_add_i64_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                arms,
                arm_count
            );
        } else {
            (void)mmay_checker_private_add_string_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                arms,
                arm_count
            );
        }
        goto cleanup;
    }
    test_block = function->block;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        MarimayCheckScalarMatchArm *arm = &arms[arm_index];
        MarimayMirBlockId selected_block = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId next_block = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId body_block = MMAY_MIR_NO_BLOCK;
        MarimayExpressionValue guard_value;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        size_t alternative_index = 0;
        bool last = arm_index + 1 == arm_count;
        bool has_catch_all = false;
        bool needs_next = false;
        bool arm_compiled = false;
        bool guard_never = false;
        memset(&guard_value, 0, sizeof(guard_value));
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
            has_catch_all = has_catch_all || alternative->is_catch_all;
        }
        needs_next = (!last && !has_catch_all) || arm->guard != NULL;
        function->block = test_block;
        function->terminated = false;
        if ((last && arm->guard != NULL)
            || arm->alternatives.length == 0
            || !mmay_checker_private_function_add_block(function, &selected_block)
            || (needs_next
                && !mmay_checker_private_function_add_block(function, &next_block))) {
            if (last && arm->guard != NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
            } else if (arm->alternatives.length == 0) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            goto i64_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!mmay_checker_private_declare_match_bindings(function, &arm->bindings)) {
            goto i64_arm_cleanup;
        }
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const MarimayCheckScalarMatchAlternative *alternative =
                mmay_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            MarimayMirBlockId alternative_block = MMAY_MIR_NO_BLOCK;
            MarimayMirBlockId alternative_next = MMAY_MIR_NO_BLOCK;
            MarimayExpressionValue literal_value;
            MarimayExpressionValue matched;
            bool alternative_last = alternative_index + 1
                == arm->alternatives.length;
            bool has_false_edge = !alternative_last || needs_next;
            memset(&literal_value, 0, sizeof(literal_value));
            memset(&matched, 0, sizeof(matched));
            if (alternative == NULL
                || !mmay_checker_private_function_add_block(
                    function,
                    &alternative_block
                )) {
                if (alternative == NULL) {
                    mmay_checker_fail(
                        function->checker,
                        MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            if (!alternative_last) {
                if (!mmay_checker_private_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto i64_arm_cleanup;
                }
            } else if (needs_next) {
                alternative_next = next_block;
            }
            if (alternative->is_catch_all || !has_false_edge) {
                if (!mmay_checker_private_function_set_goto(
                        function,
                        function->block,
                        alternative_block,
                        alternative->pattern->start,
                        alternative->pattern->end
                    )) {
                    goto i64_arm_cleanup;
                }
            } else if (alternative_next == MMAY_MIR_NO_BLOCK
                || (scalar_type == MMAY_TYPE_I64
                    ? !mmay_function_emit_instruction(
                        function,
                        MMAY_MIR_CONSTANT_I64,
                        MMAY_TYPE_I64,
                        MMAY_MIR_NO_REGISTER,
                        MMAY_MIR_NO_REGISTER,
                        alternative->integer,
                        false,
                        alternative->pattern->start,
                        alternative->pattern->end,
                        &literal_value
                    )
                    : !mmay_checker_private_function_emit_string_constant(
                        function,
                        alternative->string_constant,
                        alternative->pattern->start,
                        alternative->pattern->end,
                        &literal_value
                    )) || !mmay_function_emit_instruction(
                    function,
                    scalar_type == MMAY_TYPE_I64
                        ? MMAY_MIR_EQUAL_I64
                        : MMAY_MIR_EQUAL_STRING,
                    MMAY_TYPE_BOOL,
                    scrutinee.register_id,
                    literal_value.register_id,
                    0,
                    false,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &matched
                ) || !mmay_checker_private_function_set_branch(
                    function,
                    function->block,
                    matched.register_id,
                    alternative_block,
                    alternative_next,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                if (alternative_next == MMAY_MIR_NO_BLOCK) {
                    mmay_checker_fail(
                        function->checker,
                        MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->has_catch_binding
                && !mmay_checker_private_assign_match_local(
                    function,
                    alternative->catch_binding,
                    scalar_type,
                    scrutinee.register_id,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                goto i64_arm_cleanup;
            }
            if (!mmay_checker_private_function_set_goto(
                    function,
                    function->block,
                    selected_block,
                    alternative->pattern->end,
                    alternative->pattern->end
                )) {
                goto i64_arm_cleanup;
            }
            if (!alternative_last) {
                function->block = alternative_next;
                function->terminated = false;
            }
        }
        function->block = selected_block;
        function->terminated = false;
        if (arm->guard != NULL) {
            if (!mmay_checker_private_compile_match_guard(
                    function,
                    arm->guard,
                    &guard_value,
                    &guard_never
                )) {
                goto i64_arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto i64_arm_cleanup;
            }
            if (next_block == MMAY_MIR_NO_BLOCK
                || !mmay_checker_private_function_add_block(function, &body_block)
                || !mmay_checker_private_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                if (next_block == MMAY_MIR_NO_BLOCK) {
                    mmay_checker_fail(
                        function->checker,
                        MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!mmay_checker_private_compile_match_arm_body(
                function,
                arm->body,
                arm->body_is_block,
                &join,
                &match_type,
                &target
            )) {
            goto i64_arm_cleanup;
        }
        arm_compiled = true;

i64_arm_cleanup:
        function->locals.length = local_count;
        function->scope_depth = previous_depth;
        if (!arm_compiled) {
            goto cleanup;
        }
        if (next_block != MMAY_MIR_NO_BLOCK) {
            test_block = next_block;
        }
    }
    if (target == MMAY_MIR_NO_REGISTER) {
        if (join != MMAY_MIR_NO_BLOCK) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        mmay_checker_private_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == MMAY_MIR_NO_BLOCK || match_type == MMAY_TYPE_INVALID) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function->block = join;
    function->terminated = false;
    result->type = match_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    if (arms != NULL) {
        for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
            mmay_vector_dispose(&arms[arm_index].bindings);
            mmay_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    if (seen_initialized) {
        mmay_map_dispose(&seen_literals);
    }
    mmay_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_compile_record_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue scrutinee,
    MarimayCheckRecordDeclaration *record_declaration,
    MarimayExpressionValue *result
) {
    MarimayCheckRecordMatchArm *arms = NULL;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    MarimayMirRecordId record = MMAY_MIR_NO_RECORD;
    MarimayMirBlockId join = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId next_block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    if (node == NULL || result == NULL || record_declaration == NULL
        || !scrutinee.valid
        || scrutinee.type != record_declaration->type) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    while (mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        (void)mmay_checker_private_add_record_match_witness_diagnostic(
            function,
            node,
            record_declaration,
            false
        );
        goto cleanup;
    }
    if (!mmay_size_multiply(
            arm_count,
            sizeof(MarimayCheckRecordMatchArm),
            &arm_bytes
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = mmay_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const MarimayAstNode *arm_node = mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!mmay_checker_private_analyze_record_match_arm(
                function,
                record_declaration,
                arm_node,
                &covered,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (!covered) {
        (void)mmay_checker_private_add_record_match_witness_diagnostic(
            function,
            node,
            record_declaration,
            true
        );
        goto cleanup;
    }
    if (!mmay_checker_private_function_record_layout(
            function,
            record_declaration,
            &record
        )) {
        goto cleanup;
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        MarimayCheckRecordMatchArm *arm = &arms[arm_index];
        const MarimayCheckRecordMatchAlternative *alternative =
            mmay_vector_at_const(&arm->alternatives, 0);
        MarimayMirBlockId body_block = MMAY_MIR_NO_BLOCK;
        MarimayExpressionValue guard_value;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        bool arm_compiled = false;
        bool guard_never = false;
        memset(&guard_value, 0, sizeof(guard_value));
        function->terminated = false;
        if (alternative == NULL || arm->alternatives.length != 1
            || (arm->guard != NULL
                && !mmay_checker_private_function_add_block(function, &next_block))) {
            if (alternative == NULL || arm->alternatives.length != 1) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            goto record_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!mmay_checker_private_declare_match_bindings(function, &arm->bindings)
            || !mmay_checker_private_compile_record_match_bindings(
                function,
                alternative,
                record_declaration,
                record,
                scrutinee.register_id
            )) {
            goto record_arm_cleanup;
        }
        if (arm->guard != NULL) {
            if (!mmay_checker_private_compile_match_guard(
                    function,
                    arm->guard,
                    &guard_value,
                    &guard_never
                )) {
                goto record_arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto record_arm_cleanup;
            }
            if (!mmay_checker_private_function_add_block(function, &body_block)
                || !mmay_checker_private_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                goto record_arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!mmay_checker_private_compile_match_arm_body(
                function,
                arm->body,
                arm->body_is_block,
                &join,
                &match_type,
                &target
            )) {
            goto record_arm_cleanup;
        }
        arm_compiled = true;

record_arm_cleanup:
        function->locals.length = local_count;
        function->scope_depth = previous_depth;
        if (!arm_compiled) {
            goto cleanup;
        }
        if (arm->guard != NULL) {
            function->block = next_block;
            function->terminated = false;
            next_block = MMAY_MIR_NO_BLOCK;
        }
    }
    if (target == MMAY_MIR_NO_REGISTER) {
        if (join != MMAY_MIR_NO_BLOCK) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        mmay_checker_private_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == MMAY_MIR_NO_BLOCK || match_type == MMAY_TYPE_INVALID) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function->block = join;
    function->terminated = false;
    result->type = match_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    if (arms != NULL) {
        for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
            mmay_vector_dispose(&arms[arm_index].bindings);
            mmay_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    mmay_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_compile_enum_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue *result
) {
    const MarimayAstNode *scrutinee_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    MarimayExpressionValue scrutinee;
    MarimayCheckEnumDeclaration *enum_declaration = NULL;
    MarimayCheckEnumDeclaration concrete_enum;
    MarimayCheckEnumMatchArm *arms = NULL;
    unsigned char *seen_variants = NULL;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    size_t seen_count = 0;
    MarimayMirEnumId enum_id = MMAY_MIR_NO_ENUM;
    MarimayMirBlockId join = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId test_block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    memset(&scrutinee, 0, sizeof(scrutinee));
    if (function->constant_evaluation) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (scrutinee_node == NULL || result == NULL
        || !mmay_compile_expression(
            function,
            scrutinee_node,
            MMAY_TYPE_INVALID,
            &scrutinee
        )) {
        goto cleanup;
    }
    if (mmay_expression_is_never(function, &scrutinee)) {
        *result = scrutinee;
        accepted = true;
        goto cleanup;
    }
    if (scrutinee.type == MMAY_TYPE_BOOL) {
        return mmay_checker_private_compile_bool_match(
            function,
            node,
            expected,
            scrutinee,
            result
        );
    }
    if (scrutinee.type == MMAY_TYPE_I64) {
        return mmay_checker_private_compile_scalar_match(
            function,
            node,
            expected,
            scrutinee,
            MMAY_TYPE_I64,
            result
        );
    }
    if (scrutinee.type == MMAY_TYPE_STRING) {
        return mmay_checker_private_compile_scalar_match(
            function,
            node,
            expected,
            scrutinee,
            MMAY_TYPE_STRING,
            result
        );
    }
    if (mmay_type_kind(
            &function->checker->result->types,
            scrutinee.type
        ) == MMAY_TYPE_KIND_TUPLE) {
        return mmay_checker_private_compile_tuple_match(
            function,
            node,
            expected,
            scrutinee,
            result
        );
    }
    {
        MarimayCheckRecordDeclaration *record_declaration =
            mmay_checker_private_checker_find_record_by_type(
                function->checker,
                scrutinee.type
            );
        if (record_declaration != NULL) {
            return mmay_checker_private_compile_record_match(
                function,
                node,
                expected,
                scrutinee,
                record_declaration,
                result
            );
        }
    }
    enum_declaration = mmay_checker_private_checker_find_enum_by_type(
        function->checker,
        scrutinee.type
    );
    if (enum_declaration == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    while (mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        if (enum_declaration->variants.length == 0) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            goto cleanup;
        }
        seen_variants = mmay_allocate(
            &function->checker->result->allocator,
            enum_declaration->variants.length
        );
        if (seen_variants == NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen_variants, 0, enum_declaration->variants.length);
        (void)mmay_checker_private_add_enum_match_witness_diagnostic(
            function,
            node,
            enum_declaration,
            seen_variants,
            NULL,
            0
        );
        goto cleanup;
    }
    if (!mmay_size_multiply(
            arm_count,
            sizeof(MarimayCheckEnumMatchArm),
            &arm_bytes
        )) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = mmay_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    if (enum_declaration->variants.length != 0) {
        seen_variants = mmay_allocate(
            &function->checker->result->allocator,
            enum_declaration->variants.length
        );
    }
    if (enum_declaration->variants.length != 0
        && seen_variants == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (seen_variants != NULL) {
        memset(seen_variants, 0, enum_declaration->variants.length);
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const MarimayAstNode *arm_node = mmay_checker_private_check_child_node(
            node,
            MMAY_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!mmay_checker_private_analyze_enum_match_arm(
                function,
                enum_declaration,
                scrutinee.type,
                arm_node,
                seen_variants,
                &covered,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (enum_declaration->variants.length != 0 && seen_variants == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    for (arm_index = 0; arm_index < enum_declaration->variants.length;
        arm_index += 1) {
        if (seen_variants[arm_index] != 0) {
            seen_count += 1;
        }
    }
    if (!covered && seen_count != enum_declaration->variants.length) {
        (void)mmay_checker_private_add_enum_match_witness_diagnostic(
            function,
            node,
            enum_declaration,
            seen_variants,
            arms,
            arm_count
        );
        goto cleanup;
    }
    concrete_enum = *enum_declaration;
    concrete_enum.type = scrutinee.type;
    if (!mmay_checker_private_function_enum_layout(
            function, &concrete_enum, &enum_id
        )) {
        goto cleanup;
    }
    test_block = function->block;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        MarimayCheckEnumMatchArm *arm = &arms[arm_index];
        MarimayMirBlockId selected_block = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId next_block = MMAY_MIR_NO_BLOCK;
        MarimayMirBlockId body_block = MMAY_MIR_NO_BLOCK;
        MarimayExpressionValue guard_value;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        size_t alternative_index = 0;
        bool last = arm_index + 1 == arm_count;
        bool has_catch_all = false;
        bool needs_next = false;
        bool arm_compiled = false;
        bool guard_never = false;
        memset(&guard_value, 0, sizeof(guard_value));
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
            has_catch_all = has_catch_all || alternative->is_catch_all;
            needs_next = needs_next || alternative->has_refutable_nested_pattern;
        }
        needs_next = needs_next || (!last && !has_catch_all)
            || arm->guard != NULL;
        function->block = test_block;
        function->terminated = false;
        if ((last && arm->guard != NULL)
            || arm->alternatives.length == 0
            || !mmay_checker_private_function_add_block(function, &selected_block)
            || (needs_next
                && !mmay_checker_private_function_add_block(function, &next_block))) {
            if (last && arm->guard != NULL) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
            } else if (arm->alternatives.length == 0) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
            goto arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!mmay_checker_private_declare_match_bindings(function, &arm->bindings)) {
            goto arm_cleanup;
        }
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const MarimayCheckEnumMatchAlternative *alternative =
                mmay_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            MarimayMirBlockId alternative_block = MMAY_MIR_NO_BLOCK;
            MarimayMirBlockId alternative_next = MMAY_MIR_NO_BLOCK;
            MarimayExpressionValue matched;
            MarimayExpressionValue guarded;
            bool alternative_last = alternative_index + 1
                == arm->alternatives.length;
            bool has_false_edge = !alternative_last || needs_next;
            memset(&matched, 0, sizeof(matched));
            memset(&guarded, 0, sizeof(guarded));
            if (alternative == NULL
                || !mmay_checker_private_function_add_block(
                    function,
                    &alternative_block
                )) {
                if (alternative == NULL) {
                    mmay_checker_fail(
                        function->checker,
                        MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            if (!alternative_last) {
                if (!mmay_checker_private_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto arm_cleanup;
                }
            } else if (needs_next) {
                alternative_next = next_block;
            }
            if (alternative->is_catch_all || !has_false_edge) {
                if (!mmay_checker_private_function_set_goto(
                        function,
                        function->block,
                        alternative_block,
                        alternative->pattern->start,
                        alternative->pattern->end
                    )) {
                    goto arm_cleanup;
                }
            } else if (alternative_next == MMAY_MIR_NO_BLOCK
                || !mmay_checker_private_function_emit_enum_access(
                    function,
                    MMAY_MIR_ENUM_IS_VARIANT,
                    MMAY_TYPE_BOOL,
                    scrutinee.register_id,
                    enum_id,
                    alternative->variant_index,
                    0,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &matched
                ) || !mmay_checker_private_function_set_branch(
                    function,
                    function->block,
                    matched.register_id,
                    alternative_block,
                    alternative_next,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                if (alternative_next == MMAY_MIR_NO_BLOCK) {
                    mmay_checker_fail(
                        function->checker,
                        MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->is_catch_all) {
                guarded = scrutinee;
            } else if (!mmay_checker_private_function_emit_enum_access(
                    function,
                    MMAY_MIR_ENUM_REQUIRE_VARIANT,
                    scrutinee.type,
                    scrutinee.register_id,
                    enum_id,
                    alternative->variant_index,
                    0,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &guarded
                )) {
                goto arm_cleanup;
            }
            if (!mmay_checker_private_compile_enum_match_bindings_cps(
                    function,
                    alternative,
                    enum_id,
                    guarded.register_id,
                    selected_block,
                    alternative_next
                )) {
                goto arm_cleanup;
            }
            if (!alternative_last) {
                function->block = alternative_next;
                function->terminated = false;
            }
        }
        function->block = selected_block;
        function->terminated = false;
        if (arm->guard != NULL) {
            if (!mmay_checker_private_compile_match_guard(
                    function,
                    arm->guard,
                    &guard_value,
                    &guard_never
                )) {
                goto arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto arm_cleanup;
            }
            if (next_block == MMAY_MIR_NO_BLOCK
                || !mmay_checker_private_function_add_block(function, &body_block)
                || !mmay_checker_private_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                if (next_block == MMAY_MIR_NO_BLOCK) {
                    mmay_checker_fail(
                        function->checker,
                        MMAY_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!mmay_checker_private_compile_match_arm_body(
                function,
                arm->body,
                arm->body_is_block,
                &join,
                &match_type,
                &target
            )) {
            goto arm_cleanup;
        }
        arm_compiled = true;

arm_cleanup:
        function->locals.length = local_count;
        function->scope_depth = previous_depth;
        if (!arm_compiled) {
            goto cleanup;
        }
        if (next_block != MMAY_MIR_NO_BLOCK) {
            test_block = next_block;
        }
    }
    if (target == MMAY_MIR_NO_REGISTER) {
        if (join != MMAY_MIR_NO_BLOCK) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        mmay_checker_private_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == MMAY_MIR_NO_BLOCK || match_type == MMAY_TYPE_INVALID) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    function->block = join;
    function->terminated = false;
    result->type = match_type;
    result->register_id = target;
    result->valid = true;
    accepted = true;

cleanup:
    if (arms != NULL) {
        for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
            mmay_vector_dispose(&arms[arm_index].bindings);
            mmay_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    mmay_deallocate(
        &function->checker->result->allocator,
        seen_variants,
        enum_declaration != NULL ? enum_declaration->variants.length : 0
    );
    mmay_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}
