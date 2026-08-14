#include "mir_internal.h"

RudaMirVerifyStatus ruda_mir_verify_interface_layouts(
    const RudaMirFunction *function,
    RudaMap *managed_types,
    size_t *work
) {
    size_t layout_index = 0;
    size_t construction_index = 0;
    size_t expected_method_start = 0;

    for (layout_index = 0; layout_index < function->interfaces.length;
        layout_index += 1) {
        const RudaMirInterface *layout = ruda_vector_at_const(
            &function->interfaces,
            layout_index
        );
        RudaMirManagedLayoutInfo info = {
            RUDA_MIR_MANAGED_LAYOUT_INTERFACE,
            (uint32_t)layout_index
        };
        if (layout == NULL || layout->type < RUDA_TYPE_FIRST_DYNAMIC
            || ruda_map_get_const(
                managed_types,
                &layout->type,
                sizeof(layout->type)
            ) != NULL) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (!ruda_map_put(
                managed_types,
                &layout->type,
                sizeof(layout->type),
                &info
            )) {
            return RUDA_MIR_VERIFY_ALLOCATION_FAILED;
        }
        if (!ruda_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )) {
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        }
    }

    for (construction_index = 0;
        construction_index < function->interface_constructions.length;
        construction_index += 1) {
        const RudaMirInterfaceConstruction *construction =
            ruda_vector_at_const(
                &function->interface_constructions,
                construction_index
            );
        const RudaMirInterface *layout = construction != NULL
                && construction->interface_id < function->interfaces.length
            ? ruda_vector_at_const(
                &function->interfaces,
                construction->interface_id
            )
            : NULL;
        const RudaTypeId *actual = construction != NULL
                && construction->payload < function->register_types.length
            ? ruda_vector_at_const(
                &function->register_types,
                construction->payload
            )
            : NULL;
        const RudaMirManagedLayoutInfo *payload_layout =
            construction != NULL
            ? ruda_map_get_const(
                managed_types,
                &construction->payload_type,
                sizeof(construction->payload_type)
            )
            : NULL;
        size_t method_end = 0;

        if (actual == NULL) {
            return RUDA_MIR_VERIFY_INVALID_REGISTER;
        }
        if (layout == NULL
            || construction->method_start != expected_method_start
            || construction->method_count != layout->method_count
            || !ruda_size_add(
                construction->method_start,
                construction->method_count,
                &method_end
            )
            || method_end > function->interface_methods.length) {
            return RUDA_MIR_VERIFY_INVALID_ARGUMENT;
        }
        if (*actual != construction->payload_type
            || payload_layout == NULL
            || (payload_layout->kind != RUDA_MIR_MANAGED_LAYOUT_RECORD
                && payload_layout->kind != RUDA_MIR_MANAGED_LAYOUT_ENUM)) {
            return RUDA_MIR_VERIFY_TYPE_MISMATCH;
        }
        if (!ruda_mir_consume_verify_work(
                work,
                1,
                function->options.max_verify_work
            )
            || !ruda_mir_consume_verify_work(
                work,
                construction->method_count,
                function->options.max_verify_work
            )) {
            return RUDA_MIR_VERIFY_LIMIT_REACHED;
        }
        expected_method_start = method_end;
    }

    return expected_method_start == function->interface_methods.length
        ? RUDA_MIR_VERIFY_OK
        : RUDA_MIR_VERIFY_INVALID_ARGUMENT;
}
