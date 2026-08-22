#include "mir_internal.h"

#include <stdint.h>
#include <string.h>

void mmay_mir_panic_dispose(MarimayMirPanic *panic) {
    size_t index = 0;
    if (panic == NULL) {
        return;
    }
    mmay_owned_bytes_dispose(&panic->message);
    for (index = 0; index < panic->suppressed.length; index += 1) {
        MarimayMirSuppressedPanic *suppressed = mmay_vector_at(
            &panic->suppressed, index
        );
        if (suppressed != NULL) {
            mmay_owned_bytes_dispose(&suppressed->message);
        }
    }
    mmay_vector_dispose(&panic->suppressed);
    memset(panic, 0, sizeof(*panic));
}

static bool mmay_mir_panic_push_suppressed(
    MarimayMirPanic *primary,
    MarimayMirSuppressedPanic *suppressed,
    MarimayAllocator allocator
) {
    if (primary->suppressed.element_size == 0
        && !mmay_vector_init(
            &primary->suppressed, allocator, sizeof(*suppressed)
        )) {
        return false;
    }
    if (!mmay_vector_push(&primary->suppressed, suppressed)) {
        return false;
    }
    memset(&suppressed->message, 0, sizeof(suppressed->message));
    return true;
}

bool mmay_mir_panic_append_suppressed(
    MarimayMirPanic *primary,
    MarimayMirPanic *secondary,
    MarimayAllocator allocator
) {
    MarimayMirSuppressedPanic observed;
    size_t index = 0;
    if (primary == NULL || secondary == NULL) {
        return false;
    }
    memset(&observed, 0, sizeof(observed));
    observed.code = secondary->code;
    observed.message = secondary->message;
    observed.symbol = secondary->symbol;
    observed.span = secondary->span;
    if (!mmay_mir_panic_push_suppressed(primary, &observed, allocator)) {
        return false;
    }
    memset(&secondary->message, 0, sizeof(secondary->message));
    for (index = 0; index < secondary->suppressed.length; index += 1) {
        MarimayMirSuppressedPanic *nested = mmay_vector_at(
            &secondary->suppressed, index
        );
        if (nested != NULL
            && !mmay_mir_panic_push_suppressed(primary, nested, allocator)) {
            return false;
        }
    }
    return true;
}

MarimayMirInterpretStatus mmay_mir_invoke_deferred_call(
    const MarimayMirFunction *function,
    MarimayMirCallId call_id,
    const MarimayMirValue *registers,
    size_t register_count,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
) {
    const MarimayMirCall *call = mmay_mir_call(function, call_id);
    const MarimaySymbolId *selected = NULL;
    const MarimayMirValue *payload = NULL;
    MarimayMirValue call_result;
    MarimayMirInterpretStatus status;
    if (call == NULL || call->result_type != MMAY_TYPE_VOID) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (call->interface_slot != UINT32_MAX) {
        const MarimayMirCallArgument *receiver = call->argument_count != 0
            ? mmay_mir_call_argument(function, call->argument_start)
            : NULL;
        const MarimayMirManagedInterface *interface_value = receiver != NULL
                && receiver->register_id < register_count
                && options.execution != NULL
            ? mmay_mir_execution_interface_value(
                options.execution, registers[receiver->register_id]
            )
            : NULL;
        if (receiver == NULL || interface_value == NULL) {
            return options.execution == NULL
                ? MMAY_MIR_INTERPRET_INVALID_ARGUMENT
                : MMAY_MIR_INTERPRET_INVALID_REFERENCE;
        }
        if (call->interface_slot >= interface_value->method_count) {
            return MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        selected = &interface_value->methods[call->interface_slot];
        payload = &interface_value->payload;
    }
    memset(&call_result, 0, sizeof(call_result));
    status = mmay_mir_invoke_call(
        function, call_id, registers, register_count, allocator, options,
        selected, payload, &call_result
    );
    if (status != MMAY_MIR_INTERPRET_OK) {
        return status;
    }
    return call_result.type == MMAY_TYPE_VOID
        ? MMAY_MIR_INTERPRET_OK
        : MMAY_MIR_INTERPRET_INVALID_MIR;
}
