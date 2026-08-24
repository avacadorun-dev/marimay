#include "checker_cfg_api.h"
#include "checker_array_mutation_api.h"
#include "checker_core_api.h"
#include "checker_defer_api.h"
#include "checker_expr_scalar_api.h"
#include "checker_lookup_api.h"
#include "checker_match_lower_api.h"
#include "checker_match_pattern_api.h"
#include "checker_range_loop_api.h"
#include "checker_statement_api.h"






bool mmay_checker_private_compile_return(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
) {
    const MarimayAstNode *expression = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    MarimayExpressionValue value;
    if (expression == NULL) {
        if (function->result_type != MMAY_TYPE_VOID) {
            (void)mmay_checker_add_diagnostic(
                function->checker,
                MMAY_DIAGNOSTIC_TYPE_MISMATCH,
                "non-void function must return a value",
                node->start,
                node->end
            );
            return false;
        }
        return mmay_checker_private_function_set_return(
            function,
            MMAY_MIR_NO_REGISTER,
            node->start,
            node->end
        );
    }
    memset(&value, 0, sizeof(value));
    if (!mmay_compile_expression(
            function,
            expression,
            function->result_type == MMAY_TYPE_VOID
                    || function->result_type == MMAY_TYPE_NEVER
                ? MMAY_TYPE_INVALID
                : function->result_type,
            &value
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, &value)) {
        return true;
    }
    if (function->result_type == MMAY_TYPE_VOID) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "void function cannot return a value",
            expression->start,
            expression->end
        );
        return false;
    }
    if (value.type != function->result_type) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "returned value type does not match the function result type",
            expression->start,
            expression->end
        );
        return false;
    }
    return mmay_checker_private_function_set_return(
        function,
        value.register_id,
        node->start,
        node->end
    );
}

bool mmay_checker_private_compile_block(
    MarimayFunctionChecker *function,
    const MarimayAstNode *block
);

bool mmay_checker_private_compile_condition(
    MarimayFunctionChecker *function,
    const MarimayAstNode *expression,
    MarimayExpressionValue *result
) {
    if (expression == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (!mmay_compile_expression(
            function,
            expression,
            MMAY_TYPE_INVALID,
            result
        )) {
        return false;
    }
    if (mmay_expression_is_never(function, result)) {
        return true;
    }
    if (result->type != MMAY_TYPE_BOOL) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "condition must have type bool",
            expression->start,
            expression->end
        );
        return false;
    }
    return true;
}

bool mmay_checker_private_compile_scoped_block(
    MarimayFunctionChecker *function,
    const MarimayAstNode *block
) {
    size_t local_count = function->locals.length;
    size_t defer_count = function->defers.length;
    size_t previous_depth = function->scope_depth;
    bool compiled = false;
    if (block == NULL || previous_depth == SIZE_MAX) {
        mmay_checker_fail(
            function->checker,
            block == NULL
                ? MMAY_CHECK_INVALID_ARGUMENT
                : MMAY_CHECK_LIMIT_REACHED
        );
        return false;
    }
    function->scope_depth = previous_depth + 1;
    compiled = mmay_checker_private_compile_block(function, block);
    if (compiled && !function->terminated) {
        compiled = mmay_checker_private_emit_defers_from(
            function, defer_count
        );
    }
    function->defers.length = defer_count;
    function->locals.length = local_count;
    function->scope_depth = previous_depth;
    return compiled;
}

bool mmay_checker_private_merge_initialization_states(
    MarimayFunctionChecker *function,
    const MarimayInitializationSnapshot *when_true,
    bool true_continues,
    const MarimayInitializationSnapshot *when_false,
    bool false_continues
) {
    size_t index = 0;
    if (when_true == NULL || when_false == NULL
        || when_true->states == NULL || when_false->states == NULL
        || when_true->count != when_false->count
        || when_true->count > function->locals.length
        || (!true_continues && !false_continues)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    for (index = 0; index < when_true->count; index += 1) {
        MarimayCheckLocal *local = mmay_vector_at(&function->locals, index);
        bool initialized = false;
        if (local == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
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

bool mmay_checker_private_compile_if(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
) {
    const MarimayAstNode *condition = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    const MarimayAstNode *when_true_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_BLOCK,
        0
    );
    const MarimayAstNode *when_false_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_BLOCK,
        1
    );
    const MarimayAstNode *else_if_node = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_IF_STATEMENT,
        0
    );
    MarimayInitializationSnapshot initial;
    MarimayInitializationSnapshot true_state;
    MarimayInitializationSnapshot false_state;
    MarimayExpressionValue condition_value;
    MarimayMirBlockId branch_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId true_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId false_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId true_end = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId false_end = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId join = MMAY_MIR_NO_BLOCK;
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
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    initial_initialized = mmay_checker_private_initialization_snapshot_init(
        function,
        local_count,
        &initial
    );
    true_state_initialized = initial_initialized
        && mmay_checker_private_initialization_snapshot_init(
            function,
            local_count,
            &true_state
        );
    false_state_initialized = true_state_initialized
        && mmay_checker_private_initialization_snapshot_init(
            function,
            local_count,
            &false_state
        );
    if (!initial_initialized || !true_state_initialized
        || !false_state_initialized
        || !mmay_checker_private_compile_condition(function, condition, &condition_value)) {
        goto cleanup;
    }
    if (mmay_expression_is_never(function, &condition_value)) {
        accepted = true;
        goto cleanup;
    }
    branch_block = function->block;
    if (!mmay_checker_private_function_add_block(function, &true_block)
        || !mmay_checker_private_function_add_block(function, &false_block)
        || !mmay_checker_private_function_set_branch(
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
    if (!mmay_checker_private_compile_scoped_block(function, when_true_node)) {
        goto cleanup;
    }
    true_continues = !function->terminated;
    true_end = function->block;
    if (!mmay_checker_private_initialization_snapshot_capture(function, &true_state)
        || !mmay_checker_private_initialization_snapshot_restore(function, &initial)) {
        goto cleanup;
    }

    function->block = false_block;
    function->terminated = false;
    if (when_false_node != NULL) {
        if (!mmay_checker_private_compile_scoped_block(function, when_false_node)) {
            goto cleanup;
        }
    } else if (else_if_node != NULL) {
        if (!mmay_checker_private_compile_if(function, else_if_node)) {
            goto cleanup;
        }
    }
    false_continues = !function->terminated;
    false_end = function->block;
    if (!mmay_checker_private_initialization_snapshot_capture(function, &false_state)) {
        goto cleanup;
    }

    if (!true_continues && !false_continues) {
        function->terminated = true;
        accepted = true;
        goto cleanup;
    }
    if (!mmay_checker_private_merge_initialization_states(
            function,
            &true_state,
            true_continues,
            &false_state,
            false_continues
        )) {
        goto cleanup;
    }
    if (true_continues && false_continues) {
        if (!mmay_checker_private_function_add_block(function, &join)
            || !mmay_checker_private_function_set_goto(
                function,
                true_end,
                join,
                node->end,
                node->end
            ) || !mmay_checker_private_function_set_goto(
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
        mmay_checker_private_initialization_snapshot_dispose(&false_state);
    }
    if (true_state_initialized) {
        mmay_checker_private_initialization_snapshot_dispose(&true_state);
    }
    if (initial_initialized) {
        mmay_checker_private_initialization_snapshot_dispose(&initial);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

bool mmay_checker_private_compile_while(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
) {
    const MarimayAstNode *condition = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    const MarimayAstNode *body = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_BLOCK,
        0
    );
    MarimayInitializationSnapshot initial;
    MarimayExpressionValue condition_value;
    MarimayMirBlockId entry = function->block;
    MarimayMirBlockId condition_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId condition_end = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId body_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId exit_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId body_end = MMAY_MIR_NO_BLOCK;
    MarimayLoopContext loop;
    bool initial_initialized = false;
    bool loop_pushed = false;
    bool body_continues = false;
    bool accepted = false;
    memset(&initial, 0, sizeof(initial));
    memset(&condition_value, 0, sizeof(condition_value));
    if (condition == NULL || body == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        goto cleanup;
    }
    initial_initialized = mmay_checker_private_initialization_snapshot_init(
        function,
        function->locals.length,
        &initial
    );
    if (!initial_initialized
        || !mmay_checker_private_function_add_block(function, &condition_block)
        || !mmay_checker_private_function_set_goto(
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
    if (!mmay_checker_private_compile_condition(function, condition, &condition_value)) {
        goto cleanup;
    }
    if (mmay_expression_is_never(function, &condition_value)) {
        accepted = true;
        goto cleanup;
    }
    condition_end = function->block;
    if (!mmay_checker_private_function_add_block(function, &body_block)
        || !mmay_checker_private_function_add_block(function, &exit_block)
        || !mmay_checker_private_function_set_branch(
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
            function,
            body_end,
            condition_block,
            body->end,
            body->end
        )) {
        goto cleanup;
    }
    if (!mmay_checker_private_initialization_snapshot_restore(function, &initial)) {
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
        mmay_checker_private_initialization_snapshot_dispose(&initial);
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

bool mmay_checker_private_compile_array_for(
    MarimayFunctionChecker *function,
    const MarimayAstNode *node
) {
    const MarimayAstNode *header = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_FOR_HEADER,
        0
    );
    const MarimayAstNode *body = mmay_checker_private_check_child_node(
        node,
        MMAY_SYNTAX_BLOCK,
        0
    );
    const MarimaySyntaxToken *in_token = mmay_checker_private_check_child_token(
        header,
        MMAY_TOKEN_KW_IN,
        0
    );
    const MarimaySyntaxToken *name_token = mmay_checker_private_check_child_token(
        header,
        MMAY_TOKEN_IDENTIFIER,
        0
    );
    const MarimayAstNode *iterable_node = mmay_checker_private_check_child_node(
        header,
        MMAY_SYNTAX_EXPRESSION,
        0
    );
    MarimayExpressionValue iterable;
    MarimayExpressionValue zero;
    MarimayExpressionValue one;
    MarimayExpressionValue condition;
    MarimayExpressionValue next;
    MarimayTypeId element_type = MMAY_TYPE_INVALID;
    MarimayMirArrayId array = MMAY_MIR_NO_ARRAY;
    MarimayMirRegister generation = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister generation_checked = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister length = MMAY_MIR_NO_REGISTER;
    MarimayMirRegister item_value = MMAY_MIR_NO_REGISTER;
    MarimayCheckName name;
    MarimayCheckLocal *item = NULL;
    MarimayInitializationSnapshot initial_state;
    MarimayLoopContext loop;
    MarimayMirBlockId entry = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId condition_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId body_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId step_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId exit_block = MMAY_MIR_NO_BLOCK;
    MarimayMirBlockId body_end = MMAY_MIR_NO_BLOCK;
    MarimayMirInstruction instruction;
    size_t outer_local_count = function->locals.length;
    size_t previous_depth = function->scope_depth;
    bool scope_entered = false;
    bool initial_state_initialized = false;
    bool loop_pushed = false;
    bool body_continues = false;
    bool accepted = false;
    memset(&iterable, 0, sizeof(iterable));
    memset(&zero, 0, sizeof(zero));
    memset(&one, 0, sizeof(one));
    memset(&condition, 0, sizeof(condition));
    memset(&next, 0, sizeof(next));
    memset(&initial_state, 0, sizeof(initial_state));
    if (header == NULL || body == NULL || in_token == NULL
        || name_token == NULL || name_token->synthetic
        || iterable_node == NULL || previous_depth == SIZE_MAX) {
        mmay_checker_fail(
            function->checker,
            previous_depth == SIZE_MAX
                ? MMAY_CHECK_LIMIT_REACHED
                : MMAY_CHECK_INVALID_ARGUMENT
        );
        goto cleanup;
    }
    if (!mmay_compile_expression(
            function,
            iterable_node,
            MMAY_TYPE_INVALID,
            &iterable
        )) {
        goto cleanup;
    }
    if (mmay_expression_is_never(function, &iterable)) {
        accepted = true;
        goto cleanup;
    }
    if (!mmay_type_array_element(
            &function->checker->result->types,
            iterable.type,
            &element_type
        )) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_TYPE_MISMATCH,
            "for-in expression must have type []T or an int range",
            iterable_node->start,
            iterable_node->end
        );
        goto cleanup;
    }
    if (!mmay_array_layout(function, iterable.type, &array)
        || !mmay_checker_private_array_capture_generation(
            function, array, iterable.register_id, node->start, header->end,
            &generation
        )
        || !mmay_function_add_register(function, MMAY_TYPE_I64, &length)) {
        goto cleanup;
    }
    instruction.kind = MMAY_MIR_ARRAY_LENGTH;
    instruction.target = length;
    instruction.first = iterable.register_id;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = iterable_node->start;
    instruction.span.end = iterable_node->end;
    if (!mmay_function_append_instruction(function, &instruction)
        || !mmay_function_emit_instruction(
            function,
            MMAY_MIR_CONSTANT_I64,
            MMAY_TYPE_I64,
            MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER,
            0,
            false,
            header->start,
            header->start,
            &zero
        ) || !mmay_function_emit_instruction(
            function,
            MMAY_MIR_CONSTANT_I64,
            MMAY_TYPE_I64,
            MMAY_MIR_NO_REGISTER,
            MMAY_MIR_NO_REGISTER,
            1,
            false,
            header->start,
            header->start,
            &one
        )) {
        goto cleanup;
    }
    function->scope_depth = previous_depth + 1;
    scope_entered = true;
    name.snapshot = function->checker->ast->snapshot;
    name.start = name_token->start;
    name.end = name_token->end;
    if (!mmay_checker_private_function_add_local(
            function,
            name,
            element_type,
            false,
            true,
            &item
        ) || item == NULL) {
        goto cleanup;
    }
    entry = function->block;
    initial_state_initialized = mmay_checker_private_initialization_snapshot_init(
        function,
        outer_local_count,
        &initial_state
    );
    if (!initial_state_initialized
        || !mmay_checker_private_function_add_block(function, &condition_block)
        || !mmay_checker_private_function_add_block(function, &body_block)
        || !mmay_checker_private_function_add_block(function, &step_block)
        || !mmay_checker_private_function_add_block(function, &exit_block)
        || !mmay_checker_private_function_set_goto(
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
    if (!mmay_checker_private_array_require_generation(
            function, array, iterable.type, iterable.register_id, generation,
            node->start, header->end, &generation_checked
        )) {
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_LENGTH;
    instruction.target = length;
    instruction.first = iterable.register_id;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = iterable_node->start;
    instruction.span.end = iterable_node->end;
    if (!mmay_function_append_instruction(function, &instruction)
        || !mmay_function_emit_instruction(
            function,
            MMAY_MIR_LESS_I64,
            MMAY_TYPE_BOOL,
            zero.register_id,
            length,
            0,
            false,
            iterable_node->start,
            iterable_node->end,
            &condition
        ) || !mmay_checker_private_function_set_branch(
            function,
            function->block,
            condition.register_id,
            body_block,
            exit_block,
            iterable_node->start,
            iterable_node->end
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
    if (!mmay_function_add_register(function, element_type, &item_value)) {
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_ASSUME_INDEX;
    instruction.target = generation_checked;
    instruction.first = iterable.register_id;
    instruction.second = zero.register_id;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = node->start;
    instruction.span.end = header->end;
    if (!mmay_function_append_instruction(function, &instruction)) {
        goto cleanup;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = MMAY_MIR_ARRAY_GET_IN_BOUNDS;
    instruction.target = item_value;
    instruction.first = generation_checked;
    instruction.second = zero.register_id;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.array = array;
    instruction.span.start = node->start;
    instruction.span.end = header->end;
    if (!mmay_function_append_instruction(function, &instruction)
        || !mmay_checker_private_function_emit_copy(
            function,
            item->register_id,
            item_value,
            name_token->start,
            name_token->end
        ) || !mmay_checker_private_compile_scoped_block(function, body)) {
        goto cleanup;
    }
    body_continues = !function->terminated;
    body_end = function->block;
    function->loops.length -= 1;
    loop_pushed = false;
    if (body_continues && !mmay_checker_private_function_set_goto(
            function,
            body_end,
            step_block,
            body->end,
            body->end
        )) {
        goto cleanup;
    }
    function->block = step_block;
    function->terminated = false;
    if (!mmay_function_emit_instruction(
            function,
            MMAY_MIR_ADD_CHECKED_I64,
            MMAY_TYPE_I64,
            zero.register_id,
            one.register_id,
            0,
            false,
            header->start,
            header->end,
            &next
        ) || !mmay_checker_private_function_emit_copy(
            function,
            zero.register_id,
            next.register_id,
            header->start,
            header->end
        ) || !mmay_checker_private_function_set_goto(
            function,
            function->block,
            condition_block,
            header->end,
            header->end
        ) || !mmay_checker_private_initialization_snapshot_restore(
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
        mmay_checker_private_initialization_snapshot_dispose(&initial_state);
    }
    if (scope_entered) {
        function->locals.length = outer_local_count;
        function->scope_depth = previous_depth;
    }
    return accepted && !mmay_checker_has_failed(function->checker);
}

bool mmay_checker_private_compile_loop_control(
    MarimayFunctionChecker *function,
    const MarimayAstNode *statement
) {
    const MarimaySyntaxToken *keyword = mmay_checker_private_check_first_token(statement);
    const MarimayLoopContext *loop = NULL;
    MarimayMirBlockId target = MMAY_MIR_NO_BLOCK;
    if (keyword == NULL || (keyword->kind != MMAY_TOKEN_KW_BREAK
            && keyword->kind != MMAY_TOKEN_KW_CONTINUE)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (function->loops.length == 0) {
        (void)mmay_checker_add_diagnostic(
            function->checker,
            MMAY_DIAGNOSTIC_LOOP_CONTROL_OUTSIDE_LOOP,
            "loop control statement is outside a loop",
            keyword->start,
            keyword->end
        );
        return false;
    }
    loop = mmay_vector_at_const(
        &function->loops,
        function->loops.length - 1
    );
    if (loop == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    target = keyword->kind == MMAY_TOKEN_KW_BREAK
        ? loop->break_target
        : loop->continue_target;
    if (!mmay_checker_private_emit_defers_deeper_than(
            function, loop->scope_depth
        ) || !mmay_checker_private_function_set_goto(
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

bool mmay_checker_private_compile_statement(
    MarimayFunctionChecker *function,
    const MarimayAstNode *statement
) {
    const MarimayAstNode *block = NULL;
    if (statement == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    if (statement->kind == MMAY_SYNTAX_CONST_DECL
        || statement->kind == MMAY_SYNTAX_LET_DECL) {
        return mmay_checker_private_compile_binding(function, statement);
    }
    if (statement->kind == MMAY_SYNTAX_RETURN_STATEMENT) {
        return mmay_checker_private_compile_return(function, statement);
    }
    if (statement->kind == MMAY_SYNTAX_DEFER_STATEMENT) {
        return mmay_checker_private_compile_defer(function, statement);
    }
    if (statement->kind == MMAY_SYNTAX_EXPRESSION_STATEMENT) {
        return mmay_checker_private_compile_expression_statement(function, statement);
    }
    if (statement->kind == MMAY_SYNTAX_IF_STATEMENT) {
        return mmay_checker_private_compile_if(function, statement);
    }
    if (statement->kind == MMAY_SYNTAX_WHILE_STATEMENT) {
        return mmay_checker_private_compile_while(function, statement);
    }
    if (statement->kind == MMAY_SYNTAX_FOR_STATEMENT) {
        const MarimayAstNode *header = mmay_checker_private_check_child_node(
            statement,
            MMAY_SYNTAX_FOR_HEADER,
            0
        );
        const MarimayAstNode *iterable = mmay_checker_private_check_child_node(
            header,
            MMAY_SYNTAX_EXPRESSION,
            0
        );
        const MarimayAstNode *inner = iterable == NULL ? NULL
            : mmay_checker_private_expression_only_node(iterable);
        return inner != NULL && inner->kind == MMAY_SYNTAX_RANGE_EXPRESSION
            ? mmay_checker_private_compile_range_for(function, statement)
            : mmay_checker_private_compile_array_for(function, statement);
    }
    if (statement->kind == MMAY_SYNTAX_BREAK_STATEMENT
        || statement->kind == MMAY_SYNTAX_CONTINUE_STATEMENT) {
        return mmay_checker_private_compile_loop_control(function, statement);
    }
    if (statement->kind == MMAY_SYNTAX_BLOCK_STATEMENT) {
        block = mmay_checker_private_check_child_node(statement, MMAY_SYNTAX_BLOCK, 0);
        return mmay_checker_private_compile_scoped_block(function, block);
    }
    mmay_checker_fail(function->checker, MMAY_CHECK_INCOMPLETE_LANGUAGE);
    return false;
}

bool mmay_checker_private_compile_block(
    MarimayFunctionChecker *function,
    const MarimayAstNode *block
) {
    size_t index = 0;
    if (block == NULL || block->kind != MMAY_SYNTAX_BLOCK) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    for (index = 0; index < block->child_count
        && !mmay_checker_has_failed(function->checker); index += 1) {
        const MarimayAstElement *element = mmay_ast_child(block, index);
        if (element == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
            return false;
        }
        if (element->kind == MMAY_AST_ELEMENT_TOKEN) {
            if (element->as.token.synthetic
                || (element->as.token.kind != MMAY_TOKEN_LEFT_BRACE
                    && element->as.token.kind != MMAY_TOKEN_RIGHT_BRACE)) {
                mmay_checker_fail(
                    function->checker,
                    MMAY_CHECK_INVALID_ARGUMENT
                );
                return false;
            }
            continue;
        }
        if (function->terminated) {
            mmay_checker_fail(
                function->checker,
                MMAY_CHECK_INCOMPLETE_LANGUAGE
            );
            return false;
        }
        if (!mmay_checker_private_compile_statement(function, element->as.node)) {
            return false;
        }
    }
    return !mmay_checker_has_failed(function->checker);
}
