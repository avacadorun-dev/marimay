#include "checker_range_loop_api.h"

#include "checker_cfg_api.h"
#include "checker_core_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_lookup_api.h"
#include "checker_match_pattern_api.h"
#include "checker_statement_api.h"

static bool mmay_range_block_has_predecessor(
    const MarimayFunctionChecker *function,
    MarimayMirBlockId target
) {
    size_t index = 0;
    for (index = 0; index < function->mir.blocks.length; index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->mir.blocks, index
        );
        if (block == NULL) {
            continue;
        }
        if ((block->terminator.kind == MMAY_MIR_GOTO
                && block->terminator.target == target)
            || (block->terminator.kind == MMAY_MIR_BRANCH
                && (block->terminator.target == target
                    || block->terminator.otherwise == target))) {
            return true;
        }
    }
    return false;
}

bool mmay_checker_private_compile_range_for(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
) {
    const MarimayAstNode *header = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_FOR_HEADER, 0
    );
    const MarimayAstNode *body = mmay_checker_private_check_child_node(
        node, MMAY_SYNTAX_BLOCK, 0
    );
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        header, MMAY_TOKEN_IDENTIFIER, 0
    );
    const MarimaySyntaxToken *in_token = mmay_checker_private_check_child_token(
        header, MMAY_TOKEN_KW_IN, 0
    );
    const MarimayAstNode *iterable = mmay_checker_private_check_child_node(
        header, MMAY_SYNTAX_EXPRESSION, 0
    );
    const MarimayAstNode *range = iterable == NULL ? NULL
        : mmay_checker_private_expression_only_node(iterable);
    const MarimayAstNode *start_node = NULL;
    const MarimayAstNode *end_node = NULL;
    const MarimaySyntaxToken *operator_token = NULL;
    MarimayExpressionValue start;
    MarimayExpressionValue end;
    MarimayExpressionValue one;
    MarimayExpressionValue condition;
    MarimayExpressionValue final_element;
    MarimayExpressionValue next;
    MarimayCheckName name;
    MarimayCheckLocal *item = NULL;
    MarimayMirRegister item_register = MMAY_MIR_NO_REGISTER;
    MarimayInitializationSnapshot initial_state;
    MarimayLoopContext loop;
    MarimayMirBlockId entry = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId condition_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId body_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId step_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId advance_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId exit_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId body_end = MMAY_MIR_NO_BLOCK;
    size_t outer_local_count = function->locals.length;
    size_t previous_depth = function->scope_depth;
    bool inclusive = false;
    bool scope_entered = false;
    bool initial_state_initialized = false;
    bool loop_pushed = false;
    bool body_continues = false;
    bool accepted = false;
    memset(&start, 0, sizeof(start));
    memset(&end, 0, sizeof(end));
    memset(&one, 0, sizeof(one));
    memset(&condition, 0, sizeof(condition));
    memset(&final_element, 0, sizeof(final_element));
    memset(&next, 0, sizeof(next));
    memset(&initial_state, 0, sizeof(initial_state));
    if (header == NULL || body == NULL || name_token == NULL
        || name_token->synthetic || in_token == NULL || range == NULL
        || range->kind != MMAY_SYNTAX_RANGE_EXPRESSION
        || previous_depth == SIZE_MAX
        || !mmay_checker_private_binary_parts(
            range, &start_node, &operator_token, &end_node
        ) || operator_token == NULL
        || (operator_token->kind != MMAY_TOKEN_DOT_DOT
            && operator_token->kind != MMAY_TOKEN_DOT_DOT_EQUAL)) {
        mmay_checker_fail(
            function->checker,
            previous_depth == SIZE_MAX
                ? MMAY_CHECK_LIMIT_REACHED : MMAY_CHECK_INVALID_ARGUMENT
        );
        goto cleanup;
    }
    inclusive = operator_token->kind == MMAY_TOKEN_DOT_DOT_EQUAL;
    if (!mmay_compile_expression(function, start_node, MMAY_TYPE_I64, &start)) {
        goto cleanup;
    }
    if (mmay_expression_is_never(function, &start)) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_compile_expression(function, end_node, MMAY_TYPE_I64, &end)) {
        goto cleanup;
    }
    if (mmay_expression_is_never(function, &end)) {
        accepted = true;
        goto cleanup;
    }
    if (start.type != MMAY_TYPE_I64 || end.type != MMAY_TYPE_I64) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "range bounds must both have exact type int",
            range->start,
            range->end
        );
        goto cleanup;
    }
    if (!mmay_function_emit_instruction(
            function, MMAY_MIR_CONSTANT_I64, MMAY_TYPE_I64,
            MMAY_MIR_NO_REGISTER, MMAY_MIR_NO_REGISTER,
            1, false, operator_token->start, operator_token->end, &one
        )) {
        goto cleanup;
    }
    function->scope_depth = previous_depth + 1;
    scope_entered = true;
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (!mmay_checker_private_function_add_local(
            function, name, MMAY_TYPE_I64, false, true, &item
        ) || item == NULL) {
        goto cleanup;
    }
    item_register = item->register_id;
    if (!mmay_checker_private_function_emit_copy(
            function, item_register, start.register_id,
            start_node->start, start_node->end
        )) {
        goto cleanup;
    }
    entry = function->block;
    initial_state_initialized = mmay_checker_private_initialization_snapshot_init(
        function, outer_local_count, &initial_state
    );
    if (!initial_state_initialized
        || !mmay_checker_private_function_add_block(function, &condition_block)
        || !mmay_checker_private_function_add_block(function, &body_block)
        || !mmay_checker_private_function_add_block(function, &step_block)
        || (inclusive
            && !mmay_checker_private_function_add_block(function, &advance_block))
        || !mmay_checker_private_function_add_block(function, &exit_block)
        || !mmay_checker_private_function_set_goto(
            function, entry, condition_block, node->start, node->start
        )) {
        goto cleanup;
    }
    function->block = condition_block;
    function->terminated = false;
    if (!mmay_function_emit_instruction(
            function,
            inclusive ? MMAY_MIR_LESS_EQUAL_I64 : MMAY_MIR_LESS_I64,
            MMAY_TYPE_BOOL,
            item_register,
            end.register_id,
            0,
            false,
            range->start,
            range->end,
            &condition
        ) || !mmay_checker_private_function_set_branch(
            function, function->block, condition.register_id,
            body_block, exit_block, range->start, range->end
        )) {
        goto cleanup;
    }
    loop.break_target = exit_block;
    loop.continue_target = step_block;
    loop.scope_depth = function->scope_depth;
    if (function->loops.length >= function->mir.options.max_blocks) {
        mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        goto cleanup;
    }
    if (!mmay_vector_push(&function->loops, &loop)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        goto cleanup;
    }
    loop_pushed = true;
    function->block = body_block;
    function->terminated = false;
    if (!mmay_checker_private_compile_scoped_block(function, body)) {
        goto cleanup;
    }
    body_continues = !function->terminated;
    body_end = function->block;
    function->loops.length -= 1;
    loop_pushed = false;
    if (body_continues && !mmay_checker_private_function_set_goto(
            function, body_end, step_block, body->end, body->end
        )) {
        goto cleanup;
    }
    function->block = step_block;
    function->terminated = false;
    if (!mmay_range_block_has_predecessor(function, step_block)) {
        if (!mmay_checker_private_function_set_goto(
                function, step_block, exit_block, body->end, body->end
            )) {
            goto cleanup;
        }
        if (inclusive) {
            function->block = advance_block;
            function->terminated = false;
            if (!mmay_checker_private_function_set_goto(
                    function, advance_block, exit_block, body->end, body->end
                )) {
                goto cleanup;
            }
        }
        goto step_complete;
    }
    if (inclusive) {
        if (!mmay_function_emit_instruction(
                function, MMAY_MIR_EQUAL_I64, MMAY_TYPE_BOOL,
                item_register, end.register_id, 0, false,
                range->start, range->end, &final_element
            ) || !mmay_checker_private_function_set_branch(
                function, function->block, final_element.register_id,
                exit_block, advance_block, range->start, range->end
            )) {
            goto cleanup;
        }
        function->block = advance_block;
        function->terminated = false;
    }
    if (!mmay_function_emit_instruction(
            function, MMAY_MIR_ADD_CHECKED_I64, MMAY_TYPE_I64,
            item_register, one.register_id, 0, false,
            range->start, range->end, &next
        ) || !mmay_checker_private_function_emit_copy(
            function, item_register, next.register_id,
            range->start, range->end
        ) || !mmay_checker_private_function_set_goto(
            function, function->block, condition_block,
            range->end, range->end
        )) {
        goto cleanup;
    }
step_complete:
    if (!mmay_checker_private_initialization_snapshot_restore(
            function, &initial_state
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
        mmay_checker_private_initialization_snapshot_dispose(&initial_state);
    }
    if (scope_entered) {
        function->locals.length = outer_local_count;
        function->scope_depth = previous_depth;
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}
