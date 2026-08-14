#include "checker_match_dispatch_api.h"
#include "checker_core_api.h"
#include "checker_expr_nominal_api.h"
#include "checker_lookup_api.h"
#include "checker_match_analysis_api.h"
#include "checker_match_binding_api.h"
#include "checker_match_lower_api.h"
#include "checker_match_witness_api.h"






 bool ruda_checker_private_compile_scalar_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaTypeId scalar_type,
    RudaExpressionValue *result
) {
    static const uint64_t seen_key0 = UINT64_C(0x727564612d693634);
    static const uint64_t seen_key1 = UINT64_C(0x2d6d617463682d31);
    RudaCheckScalarMatchArm *arms = NULL;
    RudaMap seen_literals;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId test_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId match_type = expected;
    bool seen_initialized = false;
    bool covered = false;
    bool accepted = false;
    memset(&seen_literals, 0, sizeof(seen_literals));
    if (node == NULL || result == NULL || !scrutinee.valid
        || scrutinee.type != scalar_type
        || (scalar_type != RUDA_TYPE_I64
            && scalar_type != RUDA_TYPE_STRING)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (!ruda_map_init(
            &seen_literals,
            function->checker->result->allocator,
            sizeof(unsigned char),
            seen_key0,
            seen_key1
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    seen_initialized = true;
    while (ruda_checker_private_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        if (scalar_type == RUDA_TYPE_I64) {
            (void)ruda_checker_private_add_i64_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                NULL,
                0
            );
        } else {
            (void)ruda_checker_private_add_string_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                NULL,
                0
            );
        }
        goto cleanup;
    }
    if (!ruda_size_multiply(
            arm_count,
            sizeof(RudaCheckScalarMatchArm),
            &arm_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = ruda_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const RudaAstNode *arm_node = ruda_checker_private_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!ruda_checker_private_analyze_scalar_match_arm(
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
        if (scalar_type == RUDA_TYPE_I64) {
            (void)ruda_checker_private_add_i64_match_witness_diagnostic(
                function,
                node,
                &seen_literals,
                arms,
                arm_count
            );
        } else {
            (void)ruda_checker_private_add_string_match_witness_diagnostic(
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
        RudaCheckScalarMatchArm *arm = &arms[arm_index];
        RudaMirBlockId selected_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId next_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
        RudaExpressionValue guard_value;
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
            has_catch_all = has_catch_all || alternative->is_catch_all;
        }
        needs_next = (!last && !has_catch_all) || arm->guard != NULL;
        function->block = test_block;
        function->terminated = false;
        if ((last && arm->guard != NULL)
            || arm->alternatives.length == 0
            || !ruda_checker_private_function_add_block(function, &selected_block)
            || (needs_next
                && !ruda_checker_private_function_add_block(function, &next_block))) {
            if (last && arm->guard != NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            } else if (arm->alternatives.length == 0) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto i64_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!ruda_checker_private_declare_match_bindings(function, &arm->bindings)) {
            goto i64_arm_cleanup;
        }
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckScalarMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            RudaMirBlockId alternative_block = RUDA_MIR_NO_BLOCK;
            RudaMirBlockId alternative_next = RUDA_MIR_NO_BLOCK;
            RudaExpressionValue literal_value;
            RudaExpressionValue matched;
            bool alternative_last = alternative_index + 1
                == arm->alternatives.length;
            bool has_false_edge = !alternative_last || needs_next;
            memset(&literal_value, 0, sizeof(literal_value));
            memset(&matched, 0, sizeof(matched));
            if (alternative == NULL
                || !ruda_checker_private_function_add_block(
                    function,
                    &alternative_block
                )) {
                if (alternative == NULL) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            if (!alternative_last) {
                if (!ruda_checker_private_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto i64_arm_cleanup;
                }
            } else if (needs_next) {
                alternative_next = next_block;
            }
            if (alternative->is_catch_all || !has_false_edge) {
                if (!ruda_checker_private_function_set_goto(
                        function,
                        function->block,
                        alternative_block,
                        alternative->pattern->start,
                        alternative->pattern->end
                    )) {
                    goto i64_arm_cleanup;
                }
            } else if (alternative_next == RUDA_MIR_NO_BLOCK
                || (scalar_type == RUDA_TYPE_I64
                    ? !ruda_function_emit_instruction(
                        function,
                        RUDA_MIR_CONSTANT_I64,
                        RUDA_TYPE_I64,
                        RUDA_MIR_NO_REGISTER,
                        RUDA_MIR_NO_REGISTER,
                        alternative->integer,
                        false,
                        alternative->pattern->start,
                        alternative->pattern->end,
                        &literal_value
                    )
                    : !ruda_checker_private_function_emit_string_constant(
                        function,
                        alternative->string_constant,
                        alternative->pattern->start,
                        alternative->pattern->end,
                        &literal_value
                    )) || !ruda_function_emit_instruction(
                    function,
                    scalar_type == RUDA_TYPE_I64
                        ? RUDA_MIR_EQUAL_I64
                        : RUDA_MIR_EQUAL_STRING,
                    RUDA_TYPE_BOOL,
                    scrutinee.register_id,
                    literal_value.register_id,
                    0,
                    false,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &matched
                ) || !ruda_checker_private_function_set_branch(
                    function,
                    function->block,
                    matched.register_id,
                    alternative_block,
                    alternative_next,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                if (alternative_next == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->has_catch_binding
                && !ruda_checker_private_assign_match_local(
                    function,
                    alternative->catch_binding,
                    scalar_type,
                    scrutinee.register_id,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                goto i64_arm_cleanup;
            }
            if (!ruda_checker_private_function_set_goto(
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
            if (!ruda_checker_private_compile_match_guard(
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
            if (next_block == RUDA_MIR_NO_BLOCK
                || !ruda_checker_private_function_add_block(function, &body_block)
                || !ruda_checker_private_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                if (next_block == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto i64_arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!ruda_checker_private_compile_match_arm_body(
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
        if (next_block != RUDA_MIR_NO_BLOCK) {
            test_block = next_block;
        }
    }
    if (target == RUDA_MIR_NO_REGISTER) {
        if (join != RUDA_MIR_NO_BLOCK) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        ruda_checker_private_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == RUDA_MIR_NO_BLOCK || match_type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
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
            ruda_vector_dispose(&arms[arm_index].bindings);
            ruda_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    if (seen_initialized) {
        ruda_map_dispose(&seen_literals);
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_compile_record_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaCheckRecordDeclaration *record_declaration,
    RudaExpressionValue *result
) {
    RudaCheckRecordMatchArm *arms = NULL;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    RudaMirRecordId record = RUDA_MIR_NO_RECORD;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId next_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    if (node == NULL || result == NULL || record_declaration == NULL
        || !scrutinee.valid
        || scrutinee.type != record_declaration->type) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    while (ruda_checker_private_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        (void)ruda_checker_private_add_record_match_witness_diagnostic(
            function,
            node,
            record_declaration,
            false
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            arm_count,
            sizeof(RudaCheckRecordMatchArm),
            &arm_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = ruda_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const RudaAstNode *arm_node = ruda_checker_private_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!ruda_checker_private_analyze_record_match_arm(
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
        (void)ruda_checker_private_add_record_match_witness_diagnostic(
            function,
            node,
            record_declaration,
            true
        );
        goto cleanup;
    }
    if (!ruda_checker_private_function_record_layout(
            function,
            record_declaration,
            &record
        )) {
        goto cleanup;
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        RudaCheckRecordMatchArm *arm = &arms[arm_index];
        const RudaCheckRecordMatchAlternative *alternative =
            ruda_vector_at_const(&arm->alternatives, 0);
        RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
        RudaExpressionValue guard_value;
        size_t local_count = function->locals.length;
        size_t previous_depth = function->scope_depth;
        bool arm_compiled = false;
        bool guard_never = false;
        memset(&guard_value, 0, sizeof(guard_value));
        function->terminated = false;
        if (alternative == NULL || arm->alternatives.length != 1
            || (arm->guard != NULL
                && !ruda_checker_private_function_add_block(function, &next_block))) {
            if (alternative == NULL || arm->alternatives.length != 1) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto record_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!ruda_checker_private_declare_match_bindings(function, &arm->bindings)
            || !ruda_checker_private_compile_record_match_bindings(
                function,
                alternative,
                record_declaration,
                record,
                scrutinee.register_id
            )) {
            goto record_arm_cleanup;
        }
        if (arm->guard != NULL) {
            if (!ruda_checker_private_compile_match_guard(
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
            if (!ruda_checker_private_function_add_block(function, &body_block)
                || !ruda_checker_private_function_set_branch(
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
        if (!ruda_checker_private_compile_match_arm_body(
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
            next_block = RUDA_MIR_NO_BLOCK;
        }
    }
    if (target == RUDA_MIR_NO_REGISTER) {
        if (join != RUDA_MIR_NO_BLOCK) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        ruda_checker_private_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == RUDA_MIR_NO_BLOCK || match_type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
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
            ruda_vector_dispose(&arms[arm_index].bindings);
            ruda_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_compile_enum_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue *result
) {
    const RudaAstNode *scrutinee_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaExpressionValue scrutinee;
    RudaCheckEnumDeclaration *enum_declaration = NULL;
    RudaCheckEnumMatchArm *arms = NULL;
    unsigned char *seen_variants = NULL;
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    size_t seen_count = 0;
    RudaMirEnumId enum_id = RUDA_MIR_NO_ENUM;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId test_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    memset(&scrutinee, 0, sizeof(scrutinee));
    if (function->constant_evaluation) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (scrutinee_node == NULL || result == NULL
        || !ruda_compile_expression(
            function,
            scrutinee_node,
            RUDA_TYPE_INVALID,
            &scrutinee
        )) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &scrutinee)) {
        *result = scrutinee;
        accepted = true;
        goto cleanup;
    }
    if (scrutinee.type == RUDA_TYPE_BOOL) {
        return ruda_checker_private_compile_bool_match(
            function,
            node,
            expected,
            scrutinee,
            result
        );
    }
    if (scrutinee.type == RUDA_TYPE_I64) {
        return ruda_checker_private_compile_scalar_match(
            function,
            node,
            expected,
            scrutinee,
            RUDA_TYPE_I64,
            result
        );
    }
    if (scrutinee.type == RUDA_TYPE_STRING) {
        return ruda_checker_private_compile_scalar_match(
            function,
            node,
            expected,
            scrutinee,
            RUDA_TYPE_STRING,
            result
        );
    }
    {
        RudaCheckRecordDeclaration *record_declaration =
            ruda_checker_private_checker_find_record_by_type(
                function->checker,
                scrutinee.type
            );
        if (record_declaration != NULL) {
            return ruda_checker_private_compile_record_match(
                function,
                node,
                expected,
                scrutinee,
                record_declaration,
                result
            );
        }
    }
    enum_declaration = ruda_checker_private_checker_find_enum_by_type(
        function->checker,
        scrutinee.type
    );
    if (enum_declaration == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    while (ruda_checker_private_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_count
        ) != NULL) {
        if (arm_count == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto cleanup;
        }
        arm_count += 1;
    }
    if (arm_count == 0) {
        if (enum_declaration->variants.length == 0) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            goto cleanup;
        }
        seen_variants = ruda_allocate(
            &function->checker->result->allocator,
            enum_declaration->variants.length
        );
        if (seen_variants == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_ALLOCATION_FAILED
            );
            goto cleanup;
        }
        memset(seen_variants, 0, enum_declaration->variants.length);
        (void)ruda_checker_private_add_enum_match_witness_diagnostic(
            function,
            node,
            enum_declaration,
            seen_variants,
            NULL,
            0
        );
        goto cleanup;
    }
    if (!ruda_size_multiply(
            arm_count,
            sizeof(RudaCheckEnumMatchArm),
            &arm_bytes
        )) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    arms = ruda_allocate(
        &function->checker->result->allocator,
        arm_bytes
    );
    if (arms == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    memset(arms, 0, arm_bytes);
    if (enum_declaration->variants.length != 0) {
        seen_variants = ruda_allocate(
            &function->checker->result->allocator,
            enum_declaration->variants.length
        );
    }
    if (enum_declaration->variants.length != 0
        && seen_variants == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    if (seen_variants != NULL) {
        memset(seen_variants, 0, enum_declaration->variants.length);
    }
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        const RudaAstNode *arm_node = ruda_checker_private_check_child_node(
            node,
            RUDA_SYNTAX_MATCH_ARM,
            arm_index
        );
        if (!ruda_checker_private_analyze_enum_match_arm(
                function,
                enum_declaration,
                arm_node,
                seen_variants,
                &covered,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (enum_declaration->variants.length != 0 && seen_variants == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        goto cleanup;
    }
    for (arm_index = 0; arm_index < enum_declaration->variants.length;
        arm_index += 1) {
        if (seen_variants[arm_index] != 0) {
            seen_count += 1;
        }
    }
    if (!covered && seen_count != enum_declaration->variants.length) {
        (void)ruda_checker_private_add_enum_match_witness_diagnostic(
            function,
            node,
            enum_declaration,
            seen_variants,
            arms,
            arm_count
        );
        goto cleanup;
    }
    if (!ruda_checker_private_function_enum_layout(
            function,
            enum_declaration,
            &enum_id
        )) {
        goto cleanup;
    }
    test_block = function->block;
    for (arm_index = 0; arm_index < arm_count; arm_index += 1) {
        RudaCheckEnumMatchArm *arm = &arms[arm_index];
        RudaMirBlockId selected_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId next_block = RUDA_MIR_NO_BLOCK;
        RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
        RudaExpressionValue guard_value;
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
            has_catch_all = has_catch_all || alternative->is_catch_all;
        }
        needs_next = (!last && !has_catch_all) || arm->guard != NULL;
        function->block = test_block;
        function->terminated = false;
        if ((last && arm->guard != NULL)
            || arm->alternatives.length == 0
            || !ruda_checker_private_function_add_block(function, &selected_block)
            || (needs_next
                && !ruda_checker_private_function_add_block(function, &next_block))) {
            if (last && arm->guard != NULL) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            } else if (arm->alternatives.length == 0) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INTERNAL_ERROR
                );
            }
            goto cleanup;
        }
        if (previous_depth == SIZE_MAX) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
            goto arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!ruda_checker_private_declare_match_bindings(function, &arm->bindings)) {
            goto arm_cleanup;
        }
        for (alternative_index = 0;
            alternative_index < arm->alternatives.length;
            alternative_index += 1) {
            const RudaCheckEnumMatchAlternative *alternative =
                ruda_vector_at_const(
                    &arm->alternatives,
                    alternative_index
                );
            RudaMirBlockId alternative_block = RUDA_MIR_NO_BLOCK;
            RudaMirBlockId alternative_next = RUDA_MIR_NO_BLOCK;
            RudaExpressionValue matched;
            RudaExpressionValue guarded;
            bool alternative_last = alternative_index + 1
                == arm->alternatives.length;
            bool has_false_edge = !alternative_last || needs_next;
            memset(&matched, 0, sizeof(matched));
            memset(&guarded, 0, sizeof(guarded));
            if (alternative == NULL
                || !ruda_checker_private_function_add_block(
                    function,
                    &alternative_block
                )) {
                if (alternative == NULL) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            if (!alternative_last) {
                if (!ruda_checker_private_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto arm_cleanup;
                }
            } else if (needs_next) {
                alternative_next = next_block;
            }
            if (alternative->is_catch_all || !has_false_edge) {
                if (!ruda_checker_private_function_set_goto(
                        function,
                        function->block,
                        alternative_block,
                        alternative->pattern->start,
                        alternative->pattern->end
                    )) {
                    goto arm_cleanup;
                }
            } else if (alternative_next == RUDA_MIR_NO_BLOCK
                || !ruda_checker_private_function_emit_enum_access(
                    function,
                    RUDA_MIR_ENUM_IS_VARIANT,
                    RUDA_TYPE_BOOL,
                    scrutinee.register_id,
                    enum_id,
                    alternative->variant_index,
                    0,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &matched
                ) || !ruda_checker_private_function_set_branch(
                    function,
                    function->block,
                    matched.register_id,
                    alternative_block,
                    alternative_next,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                if (alternative_next == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->is_catch_all) {
                guarded = scrutinee;
            } else if (!ruda_checker_private_function_emit_enum_access(
                    function,
                    RUDA_MIR_ENUM_REQUIRE_VARIANT,
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
            if (!ruda_checker_private_compile_enum_match_bindings(
                    function,
                    alternative,
                    enum_id,
                    guarded.register_id
                ) || !ruda_checker_private_function_set_goto(
                    function,
                    function->block,
                    selected_block,
                    alternative->pattern->end,
                    alternative->pattern->end
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
            if (!ruda_checker_private_compile_match_guard(
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
            if (next_block == RUDA_MIR_NO_BLOCK
                || !ruda_checker_private_function_add_block(function, &body_block)
                || !ruda_checker_private_function_set_branch(
                    function,
                    function->block,
                    guard_value.register_id,
                    body_block,
                    next_block,
                    arm->guard->start,
                    arm->guard->end
                )) {
                if (next_block == RUDA_MIR_NO_BLOCK) {
                    ruda_checker_fail(
                        function->checker,
                        RUDA_CHECK_INTERNAL_ERROR
                    );
                }
                goto arm_cleanup;
            }
            function->block = body_block;
            function->terminated = false;
        }
        if (!ruda_checker_private_compile_match_arm_body(
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
        if (next_block != RUDA_MIR_NO_BLOCK) {
            test_block = next_block;
        }
    }
    if (target == RUDA_MIR_NO_REGISTER) {
        if (join != RUDA_MIR_NO_BLOCK) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            goto cleanup;
        }
        ruda_checker_private_expression_set_never(function, result);
        accepted = true;
        goto cleanup;
    }
    if (join == RUDA_MIR_NO_BLOCK || match_type == RUDA_TYPE_INVALID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
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
            ruda_vector_dispose(&arms[arm_index].bindings);
            ruda_vector_dispose(&arms[arm_index].alternatives);
        }
    }
    ruda_deallocate(
        &function->checker->result->allocator,
        seen_variants,
        enum_declaration != NULL ? enum_declaration->variants.length : 0
    );
    ruda_deallocate(
        &function->checker->result->allocator,
        arms,
        arm_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}
