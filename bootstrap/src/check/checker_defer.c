#include "checker_defer_api.h"

#include "checker_core_api.h"
#include "checker_internal.h"

static bool ruda_checker_defer_diagnostic(
    RudaFunctionChecker *function,
    const RudaAstNode *statement,
    const char *message
) {
    (void)ruda_checker_add_diagnostic(
        function->checker,
        RUDA_DIAGNOSTIC_TYPE_MISMATCH,
        message,
        statement->start,
        statement->end
    );
    return false;
}

static bool ruda_checker_append_defer_instruction(
    RudaFunctionChecker *function,
    RudaMirInstructionKind kind,
    const RudaDeferredCall *deferred
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = deferred->marker;
    instruction.first = RUDA_MIR_NO_REGISTER;
    instruction.second = RUDA_MIR_NO_REGISTER;
    instruction.third = RUDA_MIR_NO_REGISTER;
    instruction.call = deferred->call;
    instruction.span = deferred->span;
    return ruda_function_append_instruction(function, &instruction);
}

bool ruda_checker_private_emit_defers_from(
    RudaFunctionChecker *function,
    size_t first
) {
    size_t index = function != NULL ? function->defers.length : 0;
    if (function == NULL || first > index) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        }
        return false;
    }
    while (index > first) {
        const RudaDeferredCall *deferred = ruda_vector_at_const(
            &function->defers, index - 1
        );
        if (deferred == NULL || !ruda_checker_append_defer_instruction(
                function, RUDA_MIR_DEFER_CALL, deferred
            )) {
            if (deferred == NULL) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        index -= 1;
    }
    return true;
}

bool ruda_checker_private_emit_defers_deeper_than(
    RudaFunctionChecker *function,
    size_t scope_depth
) {
    size_t first = function != NULL ? function->defers.length : 0;
    if (function == NULL) {
        return false;
    }
    while (first != 0) {
        const RudaDeferredCall *deferred = ruda_vector_at_const(
            &function->defers, first - 1
        );
        if (deferred == NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            return false;
        }
        if (deferred->scope_depth <= scope_depth) {
            break;
        }
        first -= 1;
    }
    return ruda_checker_private_emit_defers_from(function, first);
}

bool ruda_checker_private_compile_defer(
    RudaFunctionChecker *function,
    const RudaAstNode *statement
) {
    const RudaAstNode *expression = NULL;
    RudaExpressionValue value;
    RudaMirBlock *block = NULL;
    RudaMirInstruction *call_instruction = NULL;
    RudaMirCall *call = NULL;
    RudaDeferredCall deferred;
    size_t calls_before = 0;
    size_t argument_index = 0;
    size_t argument_end = 0;
    if (function == NULL || statement == NULL) {
        if (function != NULL) {
            ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        }
        return false;
    }
    expression = ruda_checker_private_check_child_node(
        statement, RUDA_SYNTAX_EXPRESSION, 0
    );
    if (expression == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INVALID_ARGUMENT);
        return false;
    }
    block = ruda_vector_at(&function->mir.blocks, function->block);
    if (block == NULL) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    calls_before = function->mir.calls.length;
    memset(&value, 0, sizeof(value));
    if (!ruda_compile_expression(
            function, expression, RUDA_TYPE_INVALID, &value
        )) {
        return false;
    }
    if (!value.valid || value.type != RUDA_TYPE_VOID) {
        return ruda_checker_defer_diagnostic(
            function,
            statement,
            "deferred call must return void; Result and other values must be handled explicitly"
        );
    }
    block = ruda_vector_at(&function->mir.blocks, function->block);
    if (block == NULL || function->mir.calls.length <= calls_before
        || block->instructions.length == 0) {
        return ruda_checker_defer_diagnostic(
            function, statement, "defer requires exactly one call expression"
        );
    }
    call_instruction = ruda_vector_at(
        &block->instructions, block->instructions.length - 1
    );
    if (call_instruction == NULL
        || (call_instruction->kind != RUDA_MIR_CALL
            && call_instruction->kind != RUDA_MIR_CALL_INTERFACE)
        || call_instruction->call < calls_before
        || call_instruction->call >= function->mir.calls.length) {
        return ruda_checker_defer_diagnostic(
            function, statement, "defer requires exactly one call expression"
        );
    }
    call = ruda_vector_at(&function->mir.calls, call_instruction->call);
    if (call == NULL || call->result_type != RUDA_TYPE_VOID
        || !ruda_size_add(
            call->argument_start, call->argument_count, &argument_end
        ) || argument_end > function->mir.call_arguments.length) {
        ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
        return false;
    }
    block->instructions.length -= 1;
    function->mir.instruction_count -= 1;
    memset(&deferred, 0, sizeof(deferred));
    deferred.call = call_instruction->call;
    deferred.scope_depth = function->scope_depth;
    deferred.span.start = statement->start;
    deferred.span.end = statement->end;
    if (!ruda_function_add_register(
            function, RUDA_TYPE_BOOL, &deferred.marker
        )) {
        return false;
    }
    for (argument_index = call->argument_start;
        argument_index < argument_end; argument_index += 1) {
        RudaMirCallArgument *argument = ruda_vector_at(
            &function->mir.call_arguments, argument_index
        );
        RudaMirInstruction copy;
        RudaMirRegister captured = RUDA_MIR_NO_REGISTER;
        if (argument == NULL || !ruda_function_add_register(
                function, argument->type, &captured
            )) {
            if (argument == NULL) {
                ruda_checker_fail(function->checker, RUDA_CHECK_INTERNAL_ERROR);
            }
            return false;
        }
        memset(&copy, 0, sizeof(copy));
        copy.kind = RUDA_MIR_COPY;
        copy.target = captured;
        copy.first = argument->register_id;
        copy.second = RUDA_MIR_NO_REGISTER;
        copy.third = RUDA_MIR_NO_REGISTER;
        copy.span = deferred.span;
        if (!ruda_function_append_instruction(function, &copy)) {
            return false;
        }
        argument->register_id = captured;
    }
    if (function->defers.length >= function->mir.options.max_calls
        || !ruda_checker_append_defer_instruction(
            function, RUDA_MIR_DEFER_REGISTER, &deferred
        )) {
        if (function->defers.length >= function->mir.options.max_calls) {
            ruda_checker_fail(function->checker, RUDA_CHECK_LIMIT_REACHED);
        }
        return false;
    }
    if (!ruda_vector_push(&function->defers, &deferred)) {
        ruda_checker_fail(function->checker, RUDA_CHECK_ALLOCATION_FAILED);
        return false;
    }
    return true;
}
