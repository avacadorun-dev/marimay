#include "c_emitter_internal.h"

bool mmay_native_c_emit_format_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirInstruction *instruction
) {
    const char *helper;
    const char *capacity;
    if (instruction == NULL) {
        return false;
    }
    helper = instruction->kind == MMAY_MIR_FORMAT_F32
        ? "mmay_format_f32" : "mmay_format_f64";
    capacity = instruction->kind == MMAY_MIR_FORMAT_F32
        ? "MMAY_NUMBER_FORMAT_F32_CAPACITY"
        : "MMAY_NUMBER_FORMAT_F64_CAPACITY";
    return mmay_native_c_write(
        buffer,
        "  { char mmay_g_text[%s]; size_t mmay_g_length = 0; if (%s(mmay_g_v_%u, mmay_g_text, sizeof(mmay_g_text), &mmay_g_length) != MMAY_NUMBER_FORMAT_OK) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_m = mmay_managed_string_new(&mmay_g_x->heap, (const unsigned char *)mmay_g_text, mmay_g_length, &mmay_g_v_%u.managed); if (mmay_g_m == MMAY_MANAGED_OK) mmay_g_m = mmay_managed_string_bytes(&mmay_g_x->heap, mmay_g_v_%u.managed, &mmay_g_v_%u.data, &mmay_g_v_%u.length); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }\n",
        capacity,
        helper,
        instruction->first,
        instruction->target,
        instruction->target,
        instruction->target,
        instruction->target
    );
}
