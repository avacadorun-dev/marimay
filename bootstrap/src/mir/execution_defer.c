#include "mir_internal.h"

#include <stdint.h>
#include <string.h>

void ruda_mir_panic_dispose(RudaMirPanic *panic) {
    size_t index = 0;
    if (panic == NULL) {
        return;
    }
    ruda_owned_bytes_dispose(&panic->message);
    for (index = 0; index < panic->suppressed.length; index += 1) {
        RudaMirSuppressedPanic *suppressed = ruda_vector_at(
            &panic->suppressed, index
        );
        if (suppressed != NULL) {
            ruda_owned_bytes_dispose(&suppressed->message);
        }
    }
    ruda_vector_dispose(&panic->suppressed);
    memset(panic, 0, sizeof(*panic));
}

static bool ruda_mir_panic_push_suppressed(
    RudaMirPanic *primary,
    RudaMirSuppressedPanic *suppressed,
    RudaAllocator allocator
) {
    if (primary->suppressed.element_size == 0
        && !ruda_vector_init(
            &primary->suppressed, allocator, sizeof(*suppressed)
        )) {
        return false;
    }
    if (!ruda_vector_push(&primary->suppressed, suppressed)) {
        return false;
    }
    memset(&suppressed->message, 0, sizeof(suppressed->message));
    return true;
}

bool ruda_mir_panic_append_suppressed(
    RudaMirPanic *primary,
    RudaMirPanic *secondary,
    RudaAllocator allocator
) {
    RudaMirSuppressedPanic observed;
    size_t index = 0;
    if (primary == NULL || secondary == NULL) {
        return false;
    }
    memset(&observed, 0, sizeof(observed));
    observed.code = secondary->code;
    observed.message = secondary->message;
    observed.symbol = secondary->symbol;
    observed.span = secondary->span;
    if (!ruda_mir_panic_push_suppressed(primary, &observed, allocator)) {
        return false;
    }
    memset(&secondary->message, 0, sizeof(secondary->message));
    for (index = 0; index < secondary->suppressed.length; index += 1) {
        RudaMirSuppressedPanic *nested = ruda_vector_at(
            &secondary->suppressed, index
        );
        if (nested != NULL
            && !ruda_mir_panic_push_suppressed(primary, nested, allocator)) {
            return false;
        }
    }
    return true;
}

RudaMirInterpretStatus ruda_mir_invoke_deferred_call(
    const RudaMirFunction *function,
    RudaMirCallId call_id,
    const RudaMirValue *registers,
    size_t register_count,
    const RudaAllocator *allocator,
    RudaMirInterpretOptions options
) {
    const RudaMirCall *call = ruda_mir_call(function, call_id);
    const RudaSymbolId *selected = NULL;
    const RudaMirValue *payload = NULL;
    RudaMirValue call_result;
    RudaMirInterpretStatus status;
    if (call == NULL || call->result_type != RUDA_TYPE_VOID) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    if (call->interface_slot != UINT32_MAX) {
        const RudaMirCallArgument *receiver = call->argument_count != 0
            ? ruda_mir_call_argument(function, call->argument_start)
            : NULL;
        const RudaMirManagedInterface *interface_value = receiver != NULL
                && receiver->register_id < register_count
                && options.execution != NULL
            ? ruda_mir_execution_interface_value(
                options.execution, registers[receiver->register_id]
            )
            : NULL;
        if (receiver == NULL || interface_value == NULL) {
            return options.execution == NULL
                ? RUDA_MIR_INTERPRET_INVALID_ARGUMENT
                : RUDA_MIR_INTERPRET_INVALID_REFERENCE;
        }
        if (call->interface_slot >= interface_value->method_count) {
            return RUDA_MIR_INTERPRET_INVALID_MIR;
        }
        selected = &interface_value->methods[call->interface_slot];
        payload = &interface_value->payload;
    }
    memset(&call_result, 0, sizeof(call_result));
    status = ruda_mir_invoke_call(
        function, call_id, registers, register_count, allocator, options,
        selected, payload, &call_result
    );
    if (status != RUDA_MIR_INTERPRET_OK) {
        return status;
    }
    return call_result.type == RUDA_TYPE_VOID
        ? RUDA_MIR_INTERPRET_OK
        : RUDA_MIR_INTERPRET_INVALID_MIR;
}
