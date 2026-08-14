#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_lookup_api.h"
#include "checker_match_lower_api.h"
#include "checker_match_pattern_api.h"
#include "checker_statement_api.h"






 bool ruda_checker_private_compile_return(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *expression = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    RudaExpressionValue value;
    if (expression == NULL) {
        if (function->result_type != RUDA_TYPE_VOID) {
            (void)ruda_checker_add_diagnostic(
                function->checker,
                RUDA_DIAGNOSTIC_TYPE_MISMATCH,
                "non-void function must return a value",
                node->start,
                node->end
            );
            return false;
        }
        return ruda_checker_private_function_set_return(
            function,
            RUDA_MIR_NO_REGISTER,
            node->start,
            node->end
        );
    }
    memset(&value, 0, sizeof(value));
    if (!ruda_compile_expression(
            function,
            expression,
            function->result_type == RUDA_TYPE_VOID
                    || function->result_type == RUDA_TYPE_NEVER
                ? RUDA_TYPE_INVALID
                : function->result_type,
            &value
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, &value)) {
        return true;
    }
    if (function->result_type == RUDA_TYPE_VOID) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "void function cannot return a value",
            expression->start,
            expression->end
        );
        return false;
    }
    if (value.type != function->result_type) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "returned value type does not match the function result type",
            expression->start,
            expression->end
        );
        return false;
    }
    return ruda_checker_private_function_set_return(
        function,
        value.register_id,
        node->start,
        node->end
    );
}

 bool ruda_checker_private_compile_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
);

 bool ruda_checker_private_compile_condition(
    RudaFunctionChecker *function,
    const RudaAstNode *expression,
    RudaExpressionValue *result
) {
    if (expression == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (!ruda_compile_expression(
            function,
            expression,
            RUDA_TYPE_INVALID,
            result
        )) {
        return false;
    }
    if (ruda_expression_is_never(function, result)) {
        return true;
    }
    if (result->type != RUDA_TYPE_BOOL) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_TYPE_MISMATCH,
            "condition must have type bool",
            expression->start,
            expression->end
        );
        return false;
    }
    return true;
}

 bool ruda_checker_private_compile_scoped_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
) {
    size_t local_count = function->locals.length;
    size_t previous_depth = function->scope_depth;
    bool compiled = false;
    if (block == NULL || previous_depth == SIZE_MAX) {
        ruda_checker_fail(
            function->checker,
            block == NULL
                ? RUDA_CHECK_INVALID_ARGUMENT
                : RUDA_CHECK_LIMIT_REACHED
        );
        return false;
    }
    function->scope_depth = previous_depth + 1;
    compiled = ruda_checker_private_compile_block(function, block);
    function->locals.length = local_count;
    function->scope_depth = previous_depth;
    return compiled;
}

 bool ruda_checker_private_merge_initialization_states(
    RudaFunctionChecker *function,
    const RudaInitializationSnapshot *when_true,
    bool true_continues,
    const RudaInitializationSnapshot *when_false,
    bool false_continues
) {
    size_t index = 0;
    if (when_true == NULL || when_false == NULL
        || when_true->states == NULL || when_false->states == NULL
        || when_true->count != when_false->count
        || when_true->count > function->locals.length
        || (!true_continues && !false_continues)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < when_true->count; index += 1) {
        RudaCheckLocal *local = ruda_vector_at(&function->locals, index);
        bool initialized = false;
        if (local == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (true_continues && false_continues) {
            initialized = when_true->states[index] != 0
                && when_false->states[index] != 0;
        } else if (true_continues) {
            initialized = when_true->states[index] != 0;
        } else {
            initialized = when_false->states[index] != 0;
        }
        local->initialized = initialized;
    }
    return true;
}

 bool ruda_checker_private_compile_if(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *condition = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    const RudaAstNode *when_true_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_BLOCK,
        0
    );
    const RudaAstNode *when_false_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_BLOCK,
        1
    );
    const RudaAstNode *else_if_node = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_IF_STATEMENT,
        0
    );
    RudaInitializationSnapshot initial;
    RudaInitializationSnapshot true_state;
    RudaInitializationSnapshot false_state;
    RudaExpressionValue condition_value;
    RudaMirBlockId branch_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId true_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId false_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId true_end = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId false_end = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId join = RUDA_MIR_NO_BLOCK;
    size_t local_count = function->locals.length;
    bool initial_initialized = false;
    bool true_state_initialized = false;
    bool false_state_initialized = false;
    bool true_continues = false;
    bool false_continues = false;
    bool accepted = false;
    memset(&initial, 0, sizeof(initial));
    memset(&true_state, 0, sizeof(true_state));
    memset(&false_state, 0, sizeof(false_state));
    memset(&condition_value, 0, sizeof(condition_value));
    if (condition == NULL || when_true_node == NULL
        || (when_false_node != NULL && else_if_node != NULL)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    initial_initialized = ruda_checker_private_initialization_snapshot_init(
        function,
        local_count,
        &initial
    );
    true_state_initialized = initial_initialized
        && ruda_checker_private_initialization_snapshot_init(
            function,
            local_count,
            &true_state
        );
    false_state_initialized = true_state_initialized
        && ruda_checker_private_initialization_snapshot_init(
            function,
            local_count,
            &false_state
        );
    if (!initial_initialized || !true_state_initialized
        || !false_state_initialized
        || !ruda_checker_private_compile_condition(function, condition, &condition_value)) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &condition_value)) {
        accepted = true;
        goto cleanup;
    }
    branch_block = function->block;
    if (!ruda_checker_private_function_add_block(function, &true_block)
        || !ruda_checker_private_function_add_block(function, &false_block)
        || !ruda_checker_private_function_set_branch(
            function,
            branch_block,
            condition_value.register_id,
            true_block,
            false_block,
            condition->start,
            condition->end
        )) {
        goto cleanup;
    }

    function->block = true_block;
    function->terminated = false;
    if (!ruda_checker_private_compile_scoped_block(function, when_true_node)) {
        goto cleanup;
    }
    true_continues = !function->terminated;
    true_end = function->block;
    if (!ruda_checker_private_initialization_snapshot_capture(function, &true_state)
        || !ruda_checker_private_initialization_snapshot_restore(function, &initial)) {
        goto cleanup;
    }

    function->block = false_block;
    function->terminated = false;
    if (when_false_node != NULL) {
        if (!ruda_checker_private_compile_scoped_block(function, when_false_node)) {
            goto cleanup;
        }
    } else if (else_if_node != NULL) {
        if (!ruda_checker_private_compile_if(function, else_if_node)) {
            goto cleanup;
        }
    }
    false_continues = !function->terminated;
    false_end = function->block;
    if (!ruda_checker_private_initialization_snapshot_capture(function, &false_state)) {
        goto cleanup;
    }

    if (!true_continues && !false_continues) {
        function->terminated = true;
        accepted = true;
        goto cleanup;
    }
    if (!ruda_checker_private_merge_initialization_states(
            function,
            &true_state,
            true_continues,
            &false_state,
            false_continues
        )) {
        goto cleanup;
    }
    if (true_continues && false_continues) {
        if (!ruda_checker_private_function_add_block(function, &join)
            || !ruda_checker_private_function_set_goto(
                function,
                true_end,
                join,
                node->end,
                node->end
            ) || !ruda_checker_private_function_set_goto(
                function,
                false_end,
                join,
                node->end,
                node->end
            )) {
            goto cleanup;
        }
        function->block = join;
    } else {
        function->block = true_continues ? true_end : false_end;
    }
    function->terminated = false;
    accepted = true;

cleanup:
    if (false_state_initialized) {
        ruda_checker_private_initialization_snapshot_dispose(&false_state);
    }
    if (true_state_initialized) {
        ruda_checker_private_initialization_snapshot_dispose(&true_state);
    }
    if (initial_initialized) {
        ruda_checker_private_initialization_snapshot_dispose(&initial);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_compile_while(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *condition = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    const RudaAstNode *body = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_BLOCK,
        0
    );
    RudaInitializationSnapshot initial;
    RudaExpressionValue condition_value;
    RudaMirBlockId entry = function->block;
    RudaMirBlockId condition_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId condition_end = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId exit_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId body_end = RUDA_MIR_NO_BLOCK;
    RudaLoopContext loop;
    bool initial_initialized = false;
    bool loop_pushed = false;
    bool body_continues = false;
    bool accepted = false;
    memset(&initial, 0, sizeof(initial));
    memset(&condition_value, 0, sizeof(condition_value));
    if (condition == NULL || body == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    initial_initialized = ruda_checker_private_initialization_snapshot_init(
        function,
        function->locals.length,
        &initial
    );
    if (!initial_initialized
        || !ruda_checker_private_function_add_block(function, &condition_block)
        || !ruda_checker_private_function_set_goto(
            function,
            entry,
            condition_block,
            node->start,
            node->start
        )) {
        goto cleanup;
    }
    function->block = condition_block;
    function->terminated = false;
    if (!ruda_checker_private_compile_condition(function, condition, &condition_value)) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &condition_value)) {
        accepted = true;
        goto cleanup;
    }
    condition_end = function->block;
    if (!ruda_checker_private_function_add_block(function, &body_block)
        || !ruda_checker_private_function_add_block(function, &exit_block)
        || !ruda_checker_private_function_set_branch(
            function,
            condition_end,
            condition_value.register_id,
            body_block,
            exit_block,
            condition->start,
            condition->end
        )) {
        goto cleanup;
    }
    loop.break_target = exit_block;
    loop.continue_target = condition_block;
    if (function->loops.length >= function->mir.options.max_blocks) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_vector_push(&function->loops, &loop)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    loop_pushed = true;
    function->block = body_block;
    function->terminated = false;
    if (!ruda_checker_private_compile_scoped_block(function, body)) {
        goto cleanup;
    }
    body_continues = !function->terminated;
    body_end = function->block;
    function->loops.length -= 1;
    loop_pushed = false;
    if (body_continues && !ruda_checker_private_function_set_goto(
            function,
            body_end,
            condition_block,
            body->end,
            body->end
        )) {
        goto cleanup;
    }
    if (!ruda_checker_private_initialization_snapshot_restore(function, &initial)) {
        goto cleanup;
    }
    function->block = exit_block;
    function->terminated = false;
    accepted = true;

cleanup:
    if (loop_pushed) {
        function->loops.length -= 1;
    }
    if (initial_initialized) {
        ruda_checker_private_initialization_snapshot_dispose(&initial);
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_compile_c_for(
    RudaFunctionChecker *function,
    const RudaAstNode *node
) {
    const RudaAstNode *header = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_FOR_HEADER,
        0
    );
    const RudaAstNode *body = ruda_checker_private_check_child_node(
        node,
        RUDA_SYNTAX_BLOCK,
        0
    );
    const RudaSyntaxToken *let_token = ruda_checker_private_check_child_token(
        header,
        RUDA_TOKEN_KW_LET,
        0
    );
    const RudaSyntaxToken *name_token = ruda_checker_private_check_child_token(
        header,
        RUDA_TOKEN_IDENTIFIER,
        0
    );
    const RudaAstNode *initializer = ruda_checker_private_check_child_node(
        header,
        RUDA_SYNTAX_EXPRESSION,
        0
    );
    const RudaAstNode *condition = ruda_checker_private_check_child_node(
        header,
        RUDA_SYNTAX_EXPRESSION,
        1
    );
    const RudaAstNode *step = ruda_checker_private_check_child_node(
        header,
        RUDA_SYNTAX_EXPRESSION,
        2
    );
    const RudaAstNode *step_inner = NULL;
    RudaCheckName name;
    RudaCheckLocal *local = NULL;
    RudaExpressionValue initial_value;
    RudaExpressionValue condition_value;
    RudaInitializationSnapshot initial_state;
    RudaLoopContext loop;
    RudaMirBlockId entry = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId condition_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId condition_end = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId body_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId step_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId exit_block = RUDA_MIR_NO_BLOCK;
    RudaMirBlockId body_end = RUDA_MIR_NO_BLOCK;
    size_t outer_local_count = function->locals.length;
    size_t previous_depth = function->scope_depth;
    bool scope_entered = false;
    bool initial_state_initialized = false;
    bool loop_pushed = false;
    bool body_continues = false;
    bool accepted = false;
    memset(&initial_value, 0, sizeof(initial_value));
    memset(&condition_value, 0, sizeof(condition_value));
    memset(&initial_state, 0, sizeof(initial_state));
    if (header == NULL || body == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    if (let_token == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (name_token == NULL || name_token->synthetic || initializer == NULL
        || condition == NULL || step == NULL || previous_depth == SIZE_MAX) {
        ruda_checker_fail(
            function->checker,
            previous_depth == SIZE_MAX
                ? RUDA_CHECK_LIMIT_REACHED
                : RUDA_CHECK_INVALID_ARGUMENT
        );
        goto cleanup;
    }
    function->scope_depth = previous_depth + 1;
    scope_entered = true;
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (!ruda_compile_expression(
            function,
            initializer,
            RUDA_TYPE_INVALID,
            &initial_value
        )) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &initial_value)) {
        accepted = true;
        goto cleanup;
    }
    if (!ruda_checker_private_function_add_local(
            function,
            name,
            initial_value.type,
            true,
            true,
            &local
        ) || local == NULL
        || !ruda_checker_private_function_emit_copy(
            function,
            local->register_id,
            initial_value.register_id,
            initializer->start,
            initializer->end
        )) {
        goto cleanup;
    }
    entry = function->block;
    initial_state_initialized = ruda_checker_private_initialization_snapshot_init(
        function,
        outer_local_count,
        &initial_state
    );
    if (!initial_state_initialized
        || !ruda_checker_private_function_add_block(function, &condition_block)
        || !ruda_checker_private_function_set_goto(
            function,
            entry,
            condition_block,
            node->start,
            node->start
        )) {
        goto cleanup;
    }
    function->block = condition_block;
    function->terminated = false;
    if (!ruda_checker_private_compile_condition(function, condition, &condition_value)) {
        goto cleanup;
    }
    if (ruda_expression_is_never(function, &condition_value)) {
        accepted = true;
        goto cleanup;
    }
    condition_end = function->block;
    if (!ruda_checker_private_function_add_block(function, &body_block)
        || !ruda_checker_private_function_add_block(function, &step_block)
        || !ruda_checker_private_function_add_block(function, &exit_block)
        || !ruda_checker_private_function_set_branch(
            function,
            condition_end,
            condition_value.register_id,
            body_block,
            exit_block,
            condition->start,
            condition->end
        )) {
        goto cleanup;
    }
    loop.break_target = exit_block;
    loop.continue_target = step_block;
    if (function->loops.length >= function->mir.options.max_blocks) {
        ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!ruda_vector_push(&function->loops, &loop)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    loop_pushed = true;
    function->block = body_block;
    function->terminated = false;
    if (!ruda_checker_private_compile_scoped_block(function, body)) {
        goto cleanup;
    }
    body_continues = !function->terminated;
    body_end = function->block;
    function->loops.length -= 1;
    loop_pushed = false;
    if (body_continues && !ruda_checker_private_function_set_goto(
            function,
            body_end,
            step_block,
            body->end,
            body->end
        )) {
        goto cleanup;
    }
    step_inner = ruda_checker_private_expression_only_node(step);
    function->block = step_block;
    function->terminated = false;
    if (step_inner == NULL
        || step_inner->kind != RUDA_SYNTAX_ASSIGNMENT_EXPRESSION) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
        goto cleanup;
    }
    if (!ruda_checker_private_compile_assignment(function, step_inner)) {
        goto cleanup;
    }
    if ((!function->terminated && !ruda_checker_private_function_set_goto(
            function,
            function->block,
            condition_block,
            step->end,
            step->end
        )) || !ruda_checker_private_initialization_snapshot_restore(
            function,
            &initial_state
        )) {
        goto cleanup;
    }
    function->locals.length = outer_local_count;
    function->scope_depth = previous_depth;
    scope_entered = false;
    function->block = exit_block;
    function->terminated = false;
    accepted = true;

cleanup:
    if (loop_pushed) {
        function->loops.length -= 1;
    }
    if (initial_state_initialized) {
        ruda_checker_private_initialization_snapshot_dispose(&initial_state);
    }
    if (scope_entered) {
        function->locals.length = outer_local_count;
        function->scope_depth = previous_depth;
    }
    return accepted && !ruda_checker_has_failed(function->checker);
}

 bool ruda_checker_private_compile_loop_control(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
) {
    const RudaSyntaxToken *keyword = ruda_checker_private_check_first_token(statement);
    const RudaLoopContext *loop = NULL;
    RudaMirBlockId target = RUDA_MIR_NO_BLOCK;
    if (keyword == NULL || (keyword->kind != RUDA_TOKEN_KW_BREAK
            && keyword->kind != RUDA_TOKEN_KW_CONTINUE)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (function->loops.length == 0) {
        (void)ruda_checker_add_diagnostic(
            function->checker,
            RUDA_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
            "loop control statement is outside a loop",
            keyword->start,
            keyword->end
        );
        return false;
    }
    loop = ruda_vector_at_const(
        &function->loops,
        function->loops.length - 1
    );
    if (loop == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    target = keyword->kind == RUDA_TOKEN_KW_BREAK
        ? loop->break_target
        : loop->continue_target;
    if (!ruda_checker_private_function_set_goto(
            function,
            function->block,
            target,
            statement->start,
            statement->end
        )) {
        return false;
    }
    function->terminated = true;
    return true;
}

 bool ruda_checker_private_compile_statement(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
) {
    const RudaAstNode *block = NULL;
    if (statement == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (statement->kind == RUDA_SYNTAX_CONST_DECL
        || statement->kind == RUDA_SYNTAX_LET_DECL) {
        return ruda_checker_private_compile_binding(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_RETURN_STATEMENT) {
        return ruda_checker_private_compile_return(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_YIELD_STATEMENT) {
        return ruda_checker_private_compile_yield(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_EXPRESSION_STATEMENT) {
        return ruda_checker_private_compile_expression_statement(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_IF_STATEMENT) {
        return ruda_checker_private_compile_if(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_WHILE_STATEMENT) {
        return ruda_checker_private_compile_while(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_FOR_STATEMENT) {
        return ruda_checker_private_compile_c_for(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_BREAK_STATEMENT
        || statement->kind == RUDA_SYNTAX_CONTINUE_STATEMENT) {
        return ruda_checker_private_compile_loop_control(function, statement);
    }
    if (statement->kind == RUDA_SYNTAX_BLOCK_STATEMENT) {
        block = ruda_checker_private_check_child_node(statement, RUDA_SYNTAX_BLOCK, 0);
        return ruda_checker_private_compile_scoped_block(function, block);
    }
    ruda_checker_fail(function->checker, RUDA_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

 bool ruda_checker_private_compile_block(
    RudaFunctionChecker *function,
    const RudaAstNode *block
) {
    size_t index = 0;
    if (block == NULL || block->kind != RUDA_SYNTAX_BLOCK) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < block->child_count
        && !ruda_checker_has_failed(function->checker); index += 1) {
        const RudaAstElement *element = ruda_ast_child(block, index);
        if (element == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind == RUDA_AST_ELEMENT_TOKEN) {
            if (element->as.token.synthetic
                || (element->as.token.kind != RUDA_TOKEN_LEFT_BRACE
                    && element->as.token.kind != RUDA_TOKEN_RIGHT_BRACE)) {
                ruda_checker_fail(
                    function->checker,
                    RUDA_CHECK_INVALID_ARGUMENT
                );
                return false;
            }
            continue;
        }
        if (function->terminated) {
            ruda_checker_fail(
                function->checker,
                RUDA_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (!ruda_checker_private_compile_statement(function, element->as.node)) {
            return false;
        }
    }
    return !ruda_checker_has_failed(function->checker);
}
