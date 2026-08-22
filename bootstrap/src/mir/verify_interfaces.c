#include "mir_internal.h"

MarimayMirVerifyStatus mmay_mir_verify_interface_layouts(
    const MarimayMirFunction *function,
    MarimayMap *managed_types,
    size_t *work
) {
    size_t layout_index = 0;
    size_t construction_index = 0;
    size_t expected_method_start = 0;

    for (layout_index = 0; layout_index < function->interfaces.length;
        layout_index += 1) {
        const MarimayMirInterface *layout = mmay_vector_at_const(
            &function->interfaces,
            layout_index
        );
        MarimayMirManagedLayoutInfo info = {
            MMAY_MIR_MANAGED_LAYOUT_INTERFACE,
            (uint32_t)layout_index
        };
        if (layout == NULL || layout->type < MMAY_TYPE_FIRST_DYNAMIC
            || mmay_map_get_const(
                managed_types,
                &layout->type,
                sizeof(layout->type)
            ) != NULL) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!mmay_map_put(
                managed_types,
                &layout->type,
                sizeof(layout->type),
                &info
            )) {
            return MMAY_MIR_VERIFY_ALLOCATION_FAILED;
        }
        if (!mmay_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        }
    }

    for (construction_index = 0;
        construction_index < function->interface_constructions.length;
        construction_index += 1) {
        const MarimayMirInterfaceConstruction *construction =
            mmay_vector_at_const(
                &function->interface_constructions,
                construction_index
            );
        const MarimayMirInterface *layout = construction != NULL
                && construction->interface_id < function->interfaces.length
            ? mmay_vector_at_const(
                &function->interfaces,
                construction->interface_id
            )
            : NULL;
        const MarimayTypeId *actual = construction != NULL
                && construction->payload < function->register_types.length
            ? mmay_vector_at_const(
                &function->register_types,
                construction->payload
            )
            : NULL;
        const MarimayMirManagedLayoutInfo *payload_layout =
            construction != NULL
            ? mmay_map_get_const(
                managed_types,
                &construction->payload_type,
                sizeof(construction->payload_type)
            )
            : NULL;
        size_t method_end = 0;

        if (actual == NULL) {
            return MMAY_MIR_VERIFY_INVALID_REGISTER;
        }
        if (layout == NULL
            || construction->method_start != expected_method_start
            || construction->method_count != layout->method_count
            || !mmay_size_add(
                construction->method_start,
                construction->method_count,
                &method_end
            )
            || method_end > function->interface_methods.length) {
            return MMAY_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (*actual != construction->payload_type
            || payload_layout == NULL
            || (payload_layout->kind != MMAY_MIR_MANAGED_LAYOUT_RECORD
                && payload_layout->kind != MMAY_MIR_MANAGED_LAYOUT_ENUM)) {
            return MMAY_MIR_VERIFY_TYPE_MISMATCH;
        }
        if (!mmay_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )
            || !mmay_mir_consume_verify_work(
                work,
                construction->method_count,
                function->options.max_verify_work
            )) {
            return MMAY_MIR_VERIFY_LIMIT_REACHED;
        }
        expected_method_start = method_end;
    }

    return expected_method_start == function->interface_methods.length
        ? MMAY_MIR_VERIFY_OK
        : MMAY_MIR_VERIFY_INVALID_ARGUMENT;
}
