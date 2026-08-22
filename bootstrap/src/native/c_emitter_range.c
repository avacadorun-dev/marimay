#include "c_emitter_internal.h"

bool ruda_native_c_emit_range_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction
) {
    const RudaMirArray *array = NULL;
    if (buffer == NULL || function == NULL || instruction == NULL) {
        return false;
    }
    array = ruda_vector_at_const(&function->arrays, instruction->array);
    if (array == NULL) {
        return false;
    }
    if (instruction->kind == RUDA_MIR_ARRAY_GET_IN_BOUNDS) {
        return ruda_native_c_write(
                buffer,
                "  { RudaManagedCell ruda_g_cell; ruda_g_m = ruda_managed_array_get_in_bounds(&ruda_g_x->heap, ruda_g_v_%u, (size_t)ruda_g_v_%u, &ruda_g_cell); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ",
                instruction->first,
                instruction->second
            ) && ruda_native_c_write_from_managed_cell(
                buffer, array->element_type, instruction->target
            ) && ruda_native_c_write(buffer, "; }\n");
    }
    if (instruction->kind == RUDA_MIR_ARRAY_SET_IN_BOUNDS) {
        return ruda_native_c_write(
                buffer,
                "  { RudaManagedCell ruda_g_cell; memset(&ruda_g_cell, 0, sizeof(ruda_g_cell)); "
            ) && ruda_native_c_write_to_managed_cell(
                buffer, array->element_type, instruction->third
            ) && ruda_native_c_write(
                buffer,
                "; ruda_g_m = ruda_managed_array_set_in_bounds(&ruda_g_x->heap, ruda_g_v_%u, (size_t)ruda_g_v_%u, ruda_g_cell); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } ruda_g_v_%u = ruda_g_v_%u; }\n",
                instruction->first,
                instruction->second,
                instruction->target,
                instruction->third
            );
    }
    return false;
}
