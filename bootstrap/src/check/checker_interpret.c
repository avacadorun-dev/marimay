#include "checker_interpret_api.h"




#include "ruda/check/checker.h"

typedef struct RudaCheckedInterpretContext {
    const RudaCheckResult *checked;
    RudaAllocator allocator;
    RudaCheckInterpretOptions options;
    RudaMirExecution *execution;
    size_t depth;
    size_t remaining_steps;
} RudaCheckedInterpretContext;

static RudaMirInterpretStatus ruda_check_interpret_checked(
    RudaCheckedInterpretContext *context,
    const RudaCheckedFunction *function,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
);

static const RudaCheckedFunction *ruda_checked_function_by_symbol(
    const RudaCheckResult *checked,
    RudaSymbolId symbol
) {
    const size_t *index = ruda_map_get_const(
        &checked->function_index,
        symbol.bytes,
        sizeof(symbol.bytes)
    );
    const RudaCheckedFunction *function = index != NULL
        ? ruda_checked_function(checked, *index)
        : NULL;
    if (function != NULL
        && ruda_symbol_id_equal(function->mir.symbol, symbol)) {
        return function;
    }
    return NULL;
}

static RudaMirInterpretStatus ruda_check_interpret_call(
    void *opaque,
    RudaSymbolId callee,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaCheckedInterpretContext *context = opaque;
    const RudaCheckedFunction *function = NULL;
    RudaMirInterpretStatus status;
    if (context == NULL || result == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (context->depth >= context->options.max_call_depth) {
        return RUDA_MIR_INTERPRET_STEP_LIMIT;
    }
    function = ruda_checked_function_by_symbol(context->checked, callee);
    if (function == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    context->depth += 1;
    status = ruda_check_interpret_checked(
        context,
        function,
        arguments,
        argument_count,
        result
    );
    context->depth -= 1;
    return status;
}

static RudaMirInterpretStatus ruda_check_interpret_checked(
    RudaCheckedInterpretContext *context,
    const RudaCheckedFunction *function,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaMirValue *result
) {
    RudaMirInterpretOptions options = ruda_mir_interpret_default_options();
    options.max_steps = context->options.max_steps_per_function;
    options.remaining_shared_steps = &context->remaining_steps;
    options.call_context = context;
    options.call = ruda_check_interpret_call;
    options.execution = context->execution;
    options.stdout_sink = context->options.stdout_sink;
    options.panic = context->options.panic;
    return ruda_mir_interpret(
        &function->mir,
        arguments,
        argument_count,
        context->allocator,
        options,
        result
    );
}

static bool ruda_checked_type_is_managed(
    const RudaCheckResult *checked,
    RudaTypeId type
) {
    (void)checked;
    return type == RUDA_TYPE_STRING || type >= RUDA_TYPE_FIRST_DYNAMIC;
}

RudaMirInterpretStatus ruda_check_interpret_function(
    const RudaCheckResult *checked,
    size_t function_index,
    const RudaMirValue *arguments,
    size_t argument_count,
    RudaAllocator allocator,
    RudaCheckInterpretOptions options,
    RudaMirValue *result
) {
    RudaCheckedInterpretContext context;
    RudaMirExecution local_execution;
    const RudaCheckedFunction *function = ruda_checked_function(
        checked,
        function_index
    );
    RudaMirInterpretStatus status;
    size_t argument_index = 0;
    bool owns_execution = false;
    if (function == NULL || result == NULL || options.max_steps_per_function == 0
        || options.max_total_steps == 0
        || options.max_call_depth == 0 || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL
        || (arguments == NULL && argument_count != 0)
        || (options.execution != NULL && !options.execution->initialized)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (options.execution == NULL
        && ruda_checked_type_is_managed(checked, function->mir.result_type)) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    for (argument_index = 0; argument_index < argument_count;
        argument_index += 1) {
        if (ruda_checked_type_is_managed(
                checked,
                arguments[argument_index].type
            )) {
            if (options.execution == NULL) {
                return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
            }
            if (!ruda_mir_execution_owns_reference(
                    options.execution,
                    arguments[argument_index]
                )) {
                return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
            }
        }
    }
    if (options.execution == NULL) {
        if (!ruda_mir_execution_init(
                &local_execution,
                allocator,
                ruda_mir_execution_default_options()
            )) {
            return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
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
    status = ruda_check_interpret_checked(
        &context,
        function,
        arguments,
        argument_count,
        result
    );
    if (owns_execution) {
        ruda_mir_execution_dispose(&local_execution);
    }
    return status;
}
