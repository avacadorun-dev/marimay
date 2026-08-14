#include "checker_match_lower_api.h"
#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_lookup_api.h"
#include "checker_match_analysis_api.h"
#include "checker_match_binding_api.h"
#include "checker_match_witness_api.h"






 bool ruda_checker_private_compile_yield(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
) {
    const RudaSyntaxToken *keyword = ruda_checker_private_check_first_token(statement);
    const RudaAstNode *expression = ruda_checker_private_check_child_node(
        statement,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaMatchYieldContext *context = function->match_yield;
    RudaExpressionValue value;
    if (keyword == NULL || keyword->kind != RUDA_TOKEN_KW_YIELD
        || expression == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (context == NULL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_YIELD_OUTSIDE_MATCH_ARM,
            "yield is only valid inside a block match arm",
            keyword->start,
            keyword->end
        );
        return false;
    }
    if (context->match_type == NULL || context->target == NULL
        || context->join == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    memset(&value, 0, sizeof(value));
    if (!ruda_compile_expression(
            function,
            expression,
            *context->match_type,
            &value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type == RUDA_TYPE_VOID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (*context->match_type == RUDA_TYPE_INVALID) {
        *context->match_type = value.type;
    }
    if (value.type != *context->match_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "yielded match value type differs from the other arms",
            expression->start,
            expression->end
        );
        return false;
    }
    if (*context->join == RUDA_MIR_NO_BLOCK
        && !ruda_checker_private_function_add_block(function, context->join)) {
        return false;
    }
    if (*context->target == RUDA_MIR_NO_REGISTER
        && !ruda_function_add_register(
            function,
            *context->match_type,
            context->target
        )) {
        return false;
    }
    if (*context->target == RUDA_MIR_NO_REGISTER
        || !ruda_checker_private_function_emit_copy(
            function,
            *context->target,
            value.register_id,
            expression->start,
            expression->end
        ) || !ruda_checker_private_function_set_goto(
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

 bool ruda_checker_private_compile_match_arm_body(
    RudaFunctionChecker *function,
    const RudaAstNode *body,
    bool body_is_block,
    RudaMirBlockId *join,
    RudaTypeId *match_type,
    RudaMirRegister *target
) {
    RudaExpressionValue value;
    RudaMatchYieldContext context;
    RudaMatchYieldContext *previous_context = NULL;
    bool compiled = false;
    if (body == NULL || join == NULL || match_type == NULL
        || target == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (body_is_block) {
        memset(&context, 0, sizeof(context));
        context.join = join;
        context.match_type = match_type;
        context.target = target;
        previous_context = function->match_yield;
        function->match_yield = &context;
        compiled = ruda_checker_private_compile_block(function, body);
        function->match_yield = previous_context;
        if (!compiled) {
            return false;
        }
        if (!function->terminated) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_MISSING_MATCH_YIELD,
                "reachable block match arm path does not yield a value",
                body->start,
                body->end
            );
            return false;
        }
        return true;
    }
    memset(&value, 0, sizeof(value));
    if (!ruda_compile_expression(
            function,
            body,
            *match_type,
            &value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (value.type == RUDA_TYPE_VOID) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        return false;
    }
    if (*match_type == RUDA_TYPE_INVALID) {
        *match_type = value.type;
    }
    if (value.type != *match_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "match arm result type differs from the other arms",
            body->start,
            body->end
        );
        return false;
    }
    if (*join == RUDA_MIR_NO_BLOCK
        && !ruda_checker_private_function_add_block(function, join)) {
        return false;
    }
    if (*target == RUDA_MIR_NO_REGISTER
        && !ruda_function_add_register(function, *match_type, target)) {
        return false;
    }
    return *target != RUDA_MIR_NO_REGISTER
        && ruda_checker_private_function_emit_copy(
            function,
            *target,
            value.register_id,
            body->start,
            body->end
        ) && ruda_checker_private_function_set_goto(
            function,
            function->block,
            *join,
            body->end,
            body->end
        );
}

 bool ruda_checker_private_compile_match_guard(
    RudaFunctionChecker *function,
    const RudaAstNode *guard,
    RudaExpressionValue *result,
    bool *never
) {
    if (guard == NULL || result == NULL || never == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    *never = false;
    if (!ruda_compile_expression(
            function,
            guard,
            RUDA_TYPE_INVALID,
            result
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, result)) {
        *never = true;
        return true;
    }
    if (result->type != RUDA_TYPE_BOOL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "match guard must have type bool",
            guard->start,
            guard->end
        );
        return false;
    }
    return true;
}

 bool ruda_checker_private_compile_bool_match(
    RudaFunctionChecker *function,
    const RudaAstNode *node,
    RudaTypeId expected,
    RudaExpressionValue scrutinee,
    RudaExpressionValue *result
) {
    RudaCheckScalarMatchArm *arms = NULL;
    unsigned char seen[2] = {0, 0};
    size_t arm_count = 0;
    size_t arm_bytes = 0;
    size_t arm_index = 0;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId test_block = RUDA_MIR_NO_BLOCK;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    RudaTypeId match_type = expected;
    bool covered = false;
    bool accepted = false;
    if (node == NULL || result == NULL || !scrutinee.valid
        || scrutinee.type != RUDA_TYPE_BOOL) {
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
        (void)ruda_checker_private_add_bool_match_witness_diagnostic(
            function,
            node,
            seen,
            NULL,
            0
        );
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
        if (!ruda_checker_private_analyze_bool_match_arm(
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
        (void)ruda_checker_private_add_bool_match_witness_diagnostic(
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
            goto bool_arm_cleanup;
        }
        function->scope_depth = previous_depth + 1;
        if (!ruda_checker_private_declare_match_bindings(function, &arm->bindings)) {
            goto bool_arm_cleanup;
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
                goto bool_arm_cleanup;
            }
            if (!alternative_last) {
                if (!ruda_checker_private_function_add_block(
                        function,
                        &alternative_next
                    )) {
                    goto bool_arm_cleanup;
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
                    goto bool_arm_cleanup;
                }
            } else if (alternative_next == RUDA_MIR_NO_BLOCK
                || !ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_CONSTANT_BOOL,
                    RUDA_TYPE_BOOL,
                    RUDA_MIR_NO_REGISTER,
                    RUDA_MIR_NO_REGISTER,
                    0,
                    alternative->literal,
                    alternative->pattern->start,
                    alternative->pattern->end,
                    &literal_value
                ) || !ruda_function_emit_instruction(
                    function,
                    RUDA_MIR_EQUAL_BOOL,
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
                goto bool_arm_cleanup;
            }
            function->block = alternative_block;
            function->terminated = false;
            if (alternative->has_catch_binding
                && !ruda_checker_private_assign_match_local(
                    function,
                    alternative->catch_binding,
                    RUDA_TYPE_BOOL,
                    scrutinee.register_id,
                    alternative->pattern->start,
                    alternative->pattern->end
                )) {
                goto bool_arm_cleanup;
            }
            if (!ruda_checker_private_function_set_goto(
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
            if (!ruda_checker_private_compile_match_guard(
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
                goto bool_arm_cleanup;
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
            goto bool_arm_cleanup;
        }
        arm_compiled = true;

bool_arm_cleanup:
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
        arms,
        arm_bytes
    );
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_function_emit_string_constant(
    RudaFunctionChecker *function,
    RudaMirStringId string_constant,
    size_t start,
    size_t end,
    RudaExpressionValue *result
) {
    RudaMirInstruction instruction;
    RudaMirRegister target = RUDA_MIR_NO_REGISTER;
    if (ruda_checker_private_check_match_string_constant(function, string_constant) == NULL
        || result == NULL
        || !ruda_function_add_register(function, RUDA_TYPE_STRING, &target)) {
        if (result == NULL
            || ruda_checker_private_check_match_string_constant(
                function,
                string_constant
            ) == NULL) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INTERNAL_ERROR
            );
        }
        return false;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = RUDA_MIR_CONSTANT_STRING;
    instruction.target = target;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.string_constant = string_constant;
    instruction.span.start = start;
    instruction.span.end = end;
    if (!ruda_function_append_instruction(function, &instruction)) {
        return false;
    }
    result->type = RUDA_TYPE_STRING;
    result->register_id = target;
    result->valid = true;
    return true;
}
