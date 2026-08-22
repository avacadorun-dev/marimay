#include "checker_interpret_api.h"




#include "mmay/check/checker.h"

typedef struct MarimayCheckedInterpretContext {
    const MarimayCheckResult *checked;
    MarimayAllocator allocator;
    MarimayCheckInterpretOptions options;
    MarimayMirExecution *execution;
    size_t depth;
    size_t remaining_steps;
} MarimayCheckedInterpretContext;

static MarimayMirInterpretStatus mmay_check_interpret_checked(
    MarimayCheckedInterpretContext *context,
    const MarimayCheckedFunction *function,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
);

static const MarimayCheckedFunction *mmay_checked_function_by_symbol(
    const MarimayCheckResult *checked,
    MarimaySymbolId symbol
) {
    const size_t *index = mmay_map_get_const(
        &checked->function_index,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
    const MarimayCheckedFunction *function = index != NULL
        ? mmay_checked_function(checked, *index)
        : NULL;
    if (function != NULL
        && mmay_symbol_id_equal(function->mir.symbol, symbol)) {
        return function;
    }
    return NULL;
}

static MarimayMirInterpretStatus mmay_check_interpret_call(
    void *opaque,
    MarimaySymbolId callee,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayCheckedInterpretContext *context = opaque;
    const MarimayCheckedFunction *function = NULL;
    MarimayMirInterpretStatus status;
    if (context == NULL || result == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (context->depth >= context->options.max_call_depth) {
        return MMAY_MIR_INTERPRET_STEP_LIMIT;
    }
    function = mmay_checked_function_by_symbol(context->checked, callee);
    if (function == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    context->depth += 1;
    status = mmay_check_interpret_checked(
        context,
        function,
        arguments,
        argument_count,
        result
    );
    context->depth -= 1;
    return status;
}

static MarimayMirInterpretStatus mmay_check_interpret_checked(
    MarimayCheckedInterpretContext *context,
    const MarimayCheckedFunction *function,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayMirValue *result
) {
    MarimayMirInterpretOptions options = mmay_mir_interpret_default_options();
    options.max_steps = context->options.max_steps_per_function;
    options.remaining_shared_steps = &context->remaining_steps;
    options.call_context = context;
    options.call = mmay_check_interpret_call;
    options.execution = context->execution;
    options.stdout_sink = context->options.stdout_sink;
    options.panic = context->options.panic;
    return mmay_mir_interpret(
        &function->mir,
        arguments,
        argument_count,
        context->allocator,
        options,
        result
    );
}

static bool mmay_checked_type_is_managed(
    const MarimayCheckResult *checked,
    MarimayTypeId type
) {
    (void)checked;
    return type == MMAY_TYPE_STRING || type >= MMAY_TYPE_FIRST_DYNAMIC;
}

MarimayMirInterpretStatus mmay_check_interpret_function(
    const MarimayCheckResult *checked,
    size_t function_index,
    const MarimayMirValue *arguments,
    size_t argument_count,
    MarimayAllocator allocator,
    MarimayCheckInterpretOptions options,
    MarimayMirValue *result
) {
    MarimayCheckedInterpretContext context;
    MarimayMirExecution local_execution;
    const MarimayCheckedFunction *function = mmay_checked_function(
        checked,
        function_index
    );
    MarimayMirInterpretStatus status;
    size_t argument_index = 0;
    bool owns_execution = false;
    if (function == NULL || result == NULL || options.max_steps_per_function == 0
        || options.max_total_steps == 0
        || options.max_call_depth == 0 || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || (arguments == NULL && argument_count != 0)
        || (options.execution != NULL && !options.execution->initialized)) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (options.execution == NULL
        && mmay_checked_type_is_managed(checked, function->mir.result_type)) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    for (argument_index = 0; argument_index < argument_count;
        argument_index += 1) {
        if (mmay_checked_type_is_managed(
                checked,
                arguments[argument_index].type
            )) {
            if (options.execution == NULL) {
                return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (!mmay_mir_execution_owns_reference(
                    options.execution,
                    arguments[argument_index]
                )) {
                return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
            }
        }
    }
    if (options.execution == NULL) {
        if (!mmay_mir_execution_init(
                &local_execution,
                allocator,
                mmay_mir_execution_default_options()
            )) {
            return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        }
        owns_execution = true;
    }
    context.checked = checked;
    context.allocator = allocator;
    context.options = options;
    context.execution = options.execution != NULL
        ? options.execution
        : &local_execution;
    context.depth = 1;
    context.remaining_steps = options.max_total_steps;
    status = mmay_check_interpret_checked(
        &context,
        function,
        arguments,
        argument_count,
        result
    );
    if (owns_execution) {
        mmay_mir_execution_dispose(&local_execution);
    }
    return status;
}
