#include "checker_defer_api.h"

#include "checker_core_api.h"
#include "checker_internal.h"

static bool mmay_checker_defer_diagnostic(
    MarimayFunctionChecker *function,
    const MarimayAstNode *statement,
    const char *message
) {
    (void)mmay_checker_add_diagnostic(
        function->checker,
        MMAY_DIAGNOSTIC_TYPE_MISMATCH,
        message,
        statement->start,
        statement->end
    );
    return false;
}

static bool mmay_checker_append_defer_instruction(
    MarimayFunctionChecker *function,
    MarimayMirInstructionKind kind,
    const MarimayDeferredCall *deferred
) {
    MarimayMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = deferred->marker;
    instruction.first = MMAY_MIR_NO_REGISTER;
    instruction.second = MMAY_MIR_NO_REGISTER;
    instruction.third = MMAY_MIR_NO_REGISTER;
    instruction.call = deferred->call;
    instruction.span = deferred->span;
    return mmay_function_append_instruction(function, &instruction);
}

bool mmay_checker_private_emit_defers_from(
    MarimayFunctionChecker *function,
    size_t first
) {
    size_t index = function != NULL ? function->defers.length : 0;
    if (function == NULL || first > index) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    while (index > first) {
        const MarimayDeferredCall *deferred = mmay_vector_at_const(
            &function->defers, index - 1
        );
        if (deferred == NULL || !mmay_checker_append_defer_instruction(
                function, MMAY_MIR_DEFER_CALL, deferred
            )) {
            if (deferred == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        index -= 1;
    }
    return true;
}

bool mmay_checker_private_emit_defers_deeper_than(
    MarimayFunctionChecker *function,
    size_t scope_depth
) {
    size_t first = function != NULL ? function->defers.length : 0;
    if (function == NULL) {
        return false;
    }
    while (first != 0) {
        const MarimayDeferredCall *deferred = mmay_vector_at_const(
            &function->defers, first - 1
        );
        if (deferred == NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (deferred->scope_depth <= scope_depth) {
            break;
        }
        first -= 1;
    }
    return mmay_checker_private_emit_defers_from(function, first);
}

bool mmay_checker_private_compile_defer(
    MarimayFunctionChecker *function,
    const MarimayAstNode *statement
) {
    const MarimayAstNode *expression = NULL;
    MarimayExpressionValue value;
    MarimayMirBlock *block = NULL;
    MarimayMirInstruction *call_instruction = NULL;
    MarimayMirCall *call = NULL;
    MarimayDeferredCall deferred;
    size_t calls_before = 0;
    size_t argument_index = 0;
    size_t argument_end = 0;
    if (function == NULL || statement == NULL) {
        if (function != NULL) {
            mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    expression = mmay_checker_private_check_child_node(
        statement, MMAY_SYNTAX_EXPRESSION, 0
    );
    if (expression == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INVALID_ARGUMENT);
        return false;
    }
    block = mmay_vector_at(&function->mir.blocks, function->block);
    if (block == NULL) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    calls_before = function->mir.calls.length;
    memset(&value, 0, sizeof(value));
    if (!mmay_compile_expression(
            function, expression, MMAY_TYPE_INVALID, &value
        )) {
        return false;
    }
    if (!value.valid || value.type != MMAY_TYPE_VOID) {
        return mmay_checker_defer_diagnostic(
            function,
            statement,
            "deferred call must return void; Result and other values must be handled explicitly"
        );
    }
    block = mmay_vector_at(&function->mir.blocks, function->block);
    if (block == NULL || function->mir.calls.length <= calls_before
        || block->instructions.length == 0) {
        return mmay_checker_defer_diagnostic(
            function, statement, "defer requires exactly one call expression"
        );
    }
    call_instruction = mmay_vector_at(
        &block->instructions, block->instructions.length - 1
    );
    if (call_instruction == NULL
        || (call_instruction->kind != MMAY_MIR_CALL
            && call_instruction->kind != MMAY_MIR_CALL_INTERFACE)
        || call_instruction->call < calls_before
        || call_instruction->call >= function->mir.calls.length) {
        return mmay_checker_defer_diagnostic(
            function, statement, "defer requires exactly one call expression"
        );
    }
    call = mmay_vector_at(&function->mir.calls, call_instruction->call);
    if (call == NULL || call->result_type != MMAY_TYPE_VOID
        || !mmay_size_add(
            call->argument_start, call->argument_count, &argument_end
        ) || argument_end > function->mir.call_arguments.length) {
        mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
        return false;
    }
    block->instructions.length -= 1;
    function->mir.instruction_count -= 1;
    memset(&deferred, 0, sizeof(deferred));
    deferred.call = call_instruction->call;
    deferred.scope_depth = function->scope_depth;
    deferred.span.start = statement->start;
    deferred.span.end = statement->end;
    if (!mmay_function_add_register(
            function, MMAY_TYPE_BOOL, &deferred.marker
        )) {
        return false;
    }
    for (argument_index = call->argument_start;
        argument_index < argument_end; argument_index += 1) {
        MarimayMirCallArgument *argument = mmay_vector_at(
            &function->mir.call_arguments, argument_index
        );
        MarimayMirInstruction copy;
        MarimayMirRegister captured = MMAY_MIR_NO_REGISTER;
        if (argument == NULL || !mmay_function_add_register(
                function, argument->type, &captured
            )) {
            if (argument == NULL) {
                mmay_checker_fail(function->checker, MMAY_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        memset(&copy, 0, sizeof(copy));
        copy.kind = MMAY_MIR_COPY;
        copy.target = captured;
        copy.first = argument->register_id;
        copy.second = MMAY_MIR_NO_REGISTER;
        copy.third = MMAY_MIR_NO_REGISTER;
        copy.span = deferred.span;
        if (!mmay_function_append_instruction(function, &copy)) {
            return false;
        }
        argument->register_id = captured;
    }
    if (function->defers.length >= function->mir.options.max_calls
        || !mmay_checker_append_defer_instruction(
            function, MMAY_MIR_DEFER_REGISTER, &deferred
        )) {
        if (function->defers.length >= function->mir.options.max_calls) {
            mmay_checker_fail(function->checker, MMAY_CHECK_LIMIT_REACHED);
        }
        return false;
    }
    if (!mmay_vector_push(&function->defers, &deferred)) {
        mmay_checker_fail(function->checker, MMAY_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}
