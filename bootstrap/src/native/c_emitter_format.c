#include "c_emitter_internal.h"

bool ruda_native_c_emit_format_instruction(
    RudaNativeCBuffer *buffer,
    const RudaMirInstruction *instruction
) {
    const char *helper;
    const char *capacity;
    if (instruction == NULL) {
        return false;
    }
    helper = instruction->kind == RUDA_MIR_FORMAT_F32
        ? "ruda_format_f32" : "ruda_format_f64";
    capacity = instruction->kind == RUDA_MIR_FORMAT_F32
        ? "RUDA_NUMBER_FORMAT_F32_CAPACITY"
        : "RUDA_NUMBER_FORMAT_F64_CAPACITY";
    return ruda_native_c_write(
        buffer,
        "  { char ruda_g_text[%s]; size_t ruda_g_length = 0; if (%s(ruda_g_v_%u, ruda_g_text, sizeof(ruda_g_text), &ruda_g_length) != RUDA_NUMBER_FORMAT_OK) { ruda_g_s = 7; goto ruda_g_cleanup; } ruda_g_m = ruda_managed_string_new(&ruda_g_x->heap, (const unsigned char *)ruda_g_text, ruda_g_length, &ruda_g_v_%u.managed); if (ruda_g_m == RUDA_MANAGED_OK) ruda_g_m = ruda_managed_string_bytes(&ruda_g_x->heap, ruda_g_v_%u.managed, &ruda_g_v_%u.data, &ruda_g_v_%u.length); if (ruda_g_m != RUDA_MANAGED_OK) { ruda_g_s = ruda_g_managed_status(ruda_g_m); goto ruda_g_cleanup; } }\n",
        capacity,
        helper,
        instruction->first,
        instruction->target,
        instruction->target,
        instruction->target,
        instruction->target
    );
}
