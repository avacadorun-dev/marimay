#include "c_emitter_internal.h"

bool mmay_native_c_emit_array_edit_instruction(
    MarimayNativeCBuffer *buffer, const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirArray *array = mmay_vector_at_const(
        &function->arrays, instruction->array);
    bool copy = instruction->kind == MMAY_MIR_ARRAY_COPY;
    if (array == NULL || (!copy && instruction->kind != MMAY_MIR_ARRAY_REMOVE_AT)
        || !mmay_native_c_write(buffer,
            "  { const MarimayManagedArrayDescriptor *mmay_g_actual = NULL; mmay_g_m = mmay_managed_array_describe(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_actual); if (mmay_g_m == MMAY_MANAGED_OK && mmay_g_actual != %s) mmay_g_m = MMAY_MANAGED_TYPE_MISMATCH; ",
            instruction->first, mmay_native_c_array_descriptor(function, array->type))) return false;
    if (copy) {
        return mmay_native_c_write(buffer,
            "if (mmay_g_m == MMAY_MANAGED_OK) mmay_g_m = mmay_managed_array_copy(&mmay_g_x->heap, mmay_g_v_%u, &mmay_g_v_%u); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } mmay_g_av_%u = (MarimayManagedArrayGeneratedView){0}; }\n",
            instruction->first, instruction->target, instruction->target);
    }
    return mmay_native_c_write(buffer,
            "size_t mmay_g_length = 0; if (mmay_g_m == MMAY_MANAGED_OK) mmay_g_m = mmay_managed_array_remove_at(&mmay_g_x->heap, mmay_g_v_%u, mmay_g_v_%u, &mmay_g_length); if (mmay_g_m == MMAY_MANAGED_BOUNDS) { mmay_g_s = mmay_g_begin_bounds(mmay_g_p, mmay_g_v_%u, mmay_g_length, ",
            instruction->first, instruction->second, instruction->second)
        && mmay_native_c_symbol_data(buffer, function->symbol)
        && mmay_native_c_write(buffer,
            ", %zu, %zu); goto mmay_g_cleanup; } if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } mmay_g_v_%u = mmay_g_v_%u; mmay_g_av_%u = mmay_g_av_%u; }\n",
            instruction->span.start, instruction->span.end,
            instruction->target, instruction->first, instruction->target, instruction->first);
}
