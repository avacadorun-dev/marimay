#include "checker_match_lower_api.h"
#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_lookup_api.h"
#include "checker_match_analysis_api.h"
#include "checker_match_binding_api.h"
#include "checker_match_witness_api.h"






 bool mmay_checker_private_compile_yield(
    MarimayFunctionChecker *function,
    const MarimayAstNode *statement
) {
    const MarimaySyntaxToken *keyword = mmay_checker_private_check_first_token(statement);
    const MarimayAstNode *expression = mmay_checker_private_check_child_node(
        statement,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    MarimayMatchYieldContext *context = function->match_yield;
    MarimayExpressionValue value;
    if (keyword == NULL || keyword->kind != MMAY_TOKEN_KW_YIELD
        || expression == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (context == NULL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_YIELD_OUTSIDE_MATCH_ARM,
            "yield is only valid inside a block match arm",
            keyword->start,
            keyword->end
        );
        return false;
    }
    if (context->match_type == NULL || context->target == NULL
        || context->join == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&value, 0, sizeof(value));
    if (!mmay_compile_expression(
            function,
            expression,
            *context->match_type,
            &value
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type == MMAY_TYPE_VOID) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (*context->match_type == MMAY_TYPE_INVALID) {
        *context->match_type = value.type;
    }
    if (value.type != *context->match_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "yielded match value type differs from the other arms",
            expression->start,
            expression->end
        );
        return false;
    }
    if (*context->join == MMAY_MIR_NO_BLOCK
        && !mmay_checker_private_function_add_block(function, context->join)) {
        return false;
    }
    if (*context->target == MMAY_MIR_NO_REGISTER
        && !mmay_function_add_register(
            function,
            *context->match_type,
            context->target
        )) {
        return false;
    }
    if (*context->target == MMAY_MIR_NO_REGISTER
        || !mmay_checker_private_function_emit_copy(
            function,
            *context->target,
            value.register_id,
            expression->start,
            expression->end
        ) || !mmay_checker_private_function_set_goto(
            function,
            function->block,
            *context->join,
            statement->start,
            statement->end
        )) {
        return false;
    }
    context->yielded = true;
    function->terminated = true;
    return true;
}

 bool mmay_checker_private_compile_match_arm_body(
    MarimayFunctionChecker *function,
    const MarimayAstNode *body,
    bool body_is_block,
    MarimayMirBlockId *join,
    MarimayTypeId *match_type,
    MarimayMirRegister *target
) {
    MarimayExpressionValue value;
    MarimayMatchYieldContext context;
    MarimayMatchYieldContext *previous_context = NULL;
    bool compiled = false;
    if (body == NULL || join == NULL || match_type == NULL
        || target == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (body_is_block) {
        memset(&context, 0, sizeof(context));
        context.join = join;
        context.match_type = match_type;
        context.target = target;
        previous_context = function->match_yield;
        function->match_yield = &context;
        compiled = mmay_checker_private_compile_block(function, body);
        function->match_yield = previous_context;
        if (!compiled) {
            return false;
        }
        if (!function->terminated) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_MISSING_MATCH_YIELD,
                "reachable block match arm path does not yield a value",
                body->start,
                body->end
            );
            return false;
        }
        return true;
    }
    memset(&value, 0, sizeof(value));
    if (!mmay_compile_expression(
            function,
            body,
            *match_type,
            &value
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type == MMAY_TYPE_VOID) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (*match_type == MMAY_TYPE_INVALID) {
        *match_type = value.type;
    }
    if (value.type != *match_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "match arm result type differs from the other arms",
            body->start,
            body->end
        );
        return false;
    }
    if (*join == MMAY_MIR_NO_BLOCK
        && !mmay_checker_private_function_add_block(function, join)) {
        return false;
    }
    if (*target == MMAY_MIR_NO_REGISTER
        && !mmay_function_add_register(function, *match_type, target)) {
        return false;
    }
    return *target != MMAY_MIR_NO_REGISTER
        && mmay_checker_private_function_emit_copy(
            function,
            *target,
            value.register_id,
            body->start,
            body->end
        ) && mmay_checker_private_function_set_goto(
            function,
            function->block,
            *join,
            body->end,
            body->end
        );
}

 bool mmay_checker_private_compile_match_guard(
    MarimayFunctionChecker *function,
    const MarimayAstNode *guard,
    MarimayExpressionValue *result,
    bool *never
) {
    if (guard == NULL || result == NULL || never == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *never = false;
    if (!mmay_compile_expression(
            function,
            guard,
            MMAY_TYPE_INVALID,
            result
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, result)) {
        *never = true;
        return true;
    }
    if (result->type != MMAY_TYPE_BOOL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "match guard must have type bool",
            guard->start,
            guard->end
        );
        return false;
    }
    return true;
}

 bool mmay_checker_private_compile_bool_match(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node,
    MarimayTypeId expected,
    MarimayExpressionValue scrutinee,
    MarimayExpressionValue *result
) {
    MarimayCheckScalarMatchArm *arms = NULL;
    unsigned char seen[2] = {0, 0};
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    MarimayMirBlockId join = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId test_block = MMAY_MIR_NO_BLOCK;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    MarimayTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    if (node == NULL || result == NULL || !scrutinee.valid
        || scrutinee.type != MMAY_TYPE_BOOL) {
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
        (void)mmay_checker_private_add_bool_match_witness_diagnostic(
            function,
            node,
            seen,
            NULL,
            0
        );
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
        if (!mmay_checker_private_analyze_bool_match_arm(
                function,
                arm_node,
                seen,
                &covered,
                &arms[arm_index]
            )) {
            goto cleanup;
        }
    }
    if (!covered && (seen[0] == 0 || seen[1] == 0)) {
        (void)mmay_checker_private_add_bool_match_witness_diagnostic(
            function,
            node,
            seen,
            arms,
            arm_count
        );
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
            goto bool_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!mmay_checker_private_declare_match_bindings(function, &arm->bindings)) {
            goto bool_arm_cleanup;
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
                goto bool_arm_cleanup;
            }
            if (!alternative_last) {
                if (!mmay_checker_private_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto bool_arm_cleanup;
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
                    goto bool_arm_cleanup;
                }
            } else if (alternative_next == MMAY_MIR_NO_BLOCK
                || !mmay_function_emit_instruction(
                    function,
                    MMAY_MIR_CONSTANT_BOOL,
                    MMAY_TYPE_BOOL,
                    MMAY_MIR_NO_REGISTER,
                    MMAY_MIR_NO_REGISTER,
                    0,
                    alternative->literal,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &literal_value
                ) || !mmay_function_emit_instruction(
                    function,
                    MMAY_MIR_EQUAL_BOOL,
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
                goto bool_arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->has_catch_binding
                && !mmay_checker_private_assign_match_local(
                    function,
                    alternative->catch_binding,
                    MMAY_TYPE_BOOL,
                    scrutinee.register_id,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                goto bool_arm_cleanup;
            }
            if (!mmay_checker_private_function_set_goto(
                    function,
                    function->block,
                    selected_block,
                    alternative->pattern->end,
                    alternative->pattern->end
                )) {
                goto bool_arm_cleanup;
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
                goto bool_arm_cleanup;
            }
            if (guard_never) {
                arm_compiled = true;
                goto bool_arm_cleanup;
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
                goto bool_arm_cleanup;
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
            goto bool_arm_cleanup;
        }
        arm_compiled = true;

bool_arm_cleanup:
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
        arms,
        arm_bytes
    );
    return accepted && !mmay_checker_has_failed(function->checker);
}

 bool mmay_checker_private_function_emit_string_constant(
    MarimayFunctionChecker *function,
    MarimayMirStringId string_constant,
    size_t start,
    size_t end,
    MarimayExpressionValue *result
) {
    MarimayMirInstruction instruction;
    MarimayMirRegister target = MMAY_MIR_NO_REGISTER;
    if (mmay_checker_private_check_match_string_constant(function, string_constant) == NULL
        || result == NULL
        || !mmay_function_add_register(function, MMAY_TYPE_STRING, &target)) {
        if (result == NULL
            || mmay_checker_private_check_match_string_constant(
                function,
                string_constant
            ) == NULL) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INTERNAL_ERROR
            );
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_CONSTANT_STRING;
    instruction.target = target;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.string_constant = string_constant;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = MMAY_TYPE_STRING;
    result->register_id = target;
    result->valid = true;
    return true;
}
