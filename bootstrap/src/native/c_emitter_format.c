#include "c_emitter_internal.h"

#include <stdio.h>

static const char *mmay_native_c_format_integer_constructor(MarimayTypeId type) {
    switch (type) {
        case MMAY_TYPE_I8: return "mmay_integer_from_i8";
        case MMAY_TYPE_I16: return "mmay_integer_from_i16";
        case MMAY_TYPE_I32: return "mmay_integer_from_i32";
        case MMAY_TYPE_I64: return "mmay_integer_from_i64";
        case MMAY_TYPE_U8: return "mmay_integer_from_u8";
        case MMAY_TYPE_U16: return "mmay_integer_from_u16";
        case MMAY_TYPE_U32: return "mmay_integer_from_u32";
        case MMAY_TYPE_U64: return "mmay_integer_from_u64";
        default: return NULL;
    }
}

static const MarimayMirEnum *mmay_native_c_format_enum_by_type(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index;
    for (index = 0; function != NULL && index < function->enums.length;
        index += 1U) {
        const MarimayMirEnum *layout = mmay_vector_at_const(
            &function->enums, index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

bool mmay_native_c_emit_format_runtime(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(buffer,
        "int mmay_g_parse_integer(mmay_g_execution *x, mmay_g_string text, int64_t radix, const MarimayManagedEnumDescriptor *result_descriptor, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { int64_t parsed = 0; size_t offset = 0, variant; MarimayIntegerParseStatus parse_status; MarimayManagedCell payload; MarimayManagedReference error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[1] = {&error}; MarimayManagedRootFrame roots = {0}; MarimayManagedStatus status, pop_status; memset(&payload, 0, sizeof(payload)); parse_status = mmay_parse_i64(text.data, text.length, radix, &parsed, &offset); if (parse_status == MMAY_INTEGER_PARSE_OK) { payload.f64_bits = (uint64_t)parsed; return mmay_g_managed_status(mmay_managed_enum_new(&x->heap, result_descriptor, 0, &payload, 1, out)); } variant = parse_status == MMAY_INTEGER_PARSE_EMPTY ? 0U : parse_status == MMAY_INTEGER_PARSE_INVALID_DIGIT ? 1U : parse_status == MMAY_INTEGER_PARSE_OUT_OF_RANGE ? 2U : 3U; status = mmay_managed_root_push(&x->heap, &roots, locations, 1); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); if (variant == 1U) { payload.f64_bits = (uint64_t)offset; status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, &payload, 1, &error); } else { status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, NULL, 0, &error); } if (status == MMAY_MANAGED_OK) { payload.reference = error; status = mmay_managed_enum_new(&x->heap, result_descriptor, 1, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
    );
}

static bool mmay_native_c_emit_formatted_string(
    MarimayNativeCBuffer *buffer,
    const MarimayMirInstruction *instruction,
    const char *capacity,
    const char *call,
    const char *ok
) {
    return mmay_native_c_write(
        buffer,
        "  { char mmay_g_text[%s]; size_t mmay_g_length = 0; if (%s != %s) { mmay_g_s = 7; goto mmay_g_cleanup; } mmay_g_m = mmay_managed_string_new(&mmay_g_x->heap, (const unsigned char *)mmay_g_text, mmay_g_length, &mmay_g_v_%u.managed); if (mmay_g_m == MMAY_MANAGED_OK) mmay_managed_string_generated_bytes(&mmay_g_x->heap, mmay_g_v_%u.managed, &mmay_g_v_%u.data, &mmay_g_v_%u.length); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; } }\n",
        capacity, call, ok,
        instruction->target, instruction->target,
        instruction->target, instruction->target
    );
}

bool mmay_native_c_emit_format_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayTypeId *source_type;
    char call[256];
    int written;
    if (buffer == NULL || function == NULL || instruction == NULL) return false;
    if (instruction->kind == MMAY_MIR_PARSE_INTEGER) {
        const MarimayMirEnum *result_layout = mmay_vector_at_const(
            &function->enums, instruction->enum_id
        );
        const MarimayMirEnumVariant *error_variant = result_layout != NULL
            ? mmay_vector_at_const(
                &function->enum_variants, result_layout->variant_start + 1U
            ) : NULL;
        const MarimayMirEnumPayload *error_payload = error_variant != NULL
            ? mmay_vector_at_const(
                &function->enum_payloads, error_variant->payload_start
            ) : NULL;
        const MarimayMirEnum *error_layout = error_payload != NULL
            ? mmay_native_c_format_enum_by_type(
                function, error_payload->type
            ) : NULL;
        if (result_layout == NULL || error_variant == NULL
            || error_payload == NULL || error_layout == NULL) return false;
        return mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_parse_integer(mmay_g_x, mmay_g_v_%u, mmay_g_v_%u, &",
                instruction->first, instruction->second
            ) && mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_ENUM, result_layout->type
            ) && mmay_native_c_write(buffer, ", &")
            && mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_ENUM, error_layout->type
            ) && mmay_native_c_write(
                buffer,
                ", &mmay_g_v_%u); if (mmay_g_s != 0) { goto mmay_g_cleanup; }\n",
                instruction->target
            );
    }
    if (instruction->kind == MMAY_MIR_STRING_CONCAT) {
        return mmay_native_c_write(
            buffer,
            "  mmay_g_m = mmay_managed_string_concat(&mmay_g_x->heap, mmay_g_v_%u.data, mmay_g_v_%u.length, mmay_g_v_%u.data, mmay_g_v_%u.length, &mmay_g_v_%u.managed); if (mmay_g_m == MMAY_MANAGED_OK) mmay_managed_string_generated_bytes(&mmay_g_x->heap, mmay_g_v_%u.managed, &mmay_g_v_%u.data, &mmay_g_v_%u.length); if (mmay_g_m != MMAY_MANAGED_OK) { mmay_g_s = mmay_g_managed_status(mmay_g_m); goto mmay_g_cleanup; }\n",
            instruction->first, instruction->first,
            instruction->second, instruction->second, instruction->target,
            instruction->target, instruction->target, instruction->target
        );
    }
    if (instruction->kind == MMAY_MIR_FORMAT_F32
        || instruction->kind == MMAY_MIR_FORMAT_F64) {
        const char *helper = instruction->kind == MMAY_MIR_FORMAT_F32
            ? "mmay_format_f32" : "mmay_format_f64";
        const char *capacity = instruction->kind == MMAY_MIR_FORMAT_F32
            ? "MMAY_NUMBER_FORMAT_F32_CAPACITY"
            : "MMAY_NUMBER_FORMAT_F64_CAPACITY";
        written = snprintf(
            call, sizeof(call),
            "%s(mmay_g_v_%u, mmay_g_text, sizeof(mmay_g_text), &mmay_g_length)",
            helper, instruction->first
        );
        return written > 0 && (size_t)written < sizeof(call)
            && mmay_native_c_emit_formatted_string(
                buffer, instruction, capacity, call, "MMAY_NUMBER_FORMAT_OK"
            );
    }
    if (instruction->kind == MMAY_MIR_FORMAT_BOOL) {
        written = snprintf(
            call, sizeof(call),
            "mmay_format_bool(mmay_g_v_%u, mmay_g_text, sizeof(mmay_g_text), &mmay_g_length)",
            instruction->first
        );
    } else if (instruction->kind == MMAY_MIR_FORMAT_INTEGER) {
        const char *constructor;
        source_type = mmay_vector_at_const(
            &function->register_types, instruction->first
        );
        constructor = source_type != NULL
            ? mmay_native_c_format_integer_constructor(*source_type) : NULL;
        if (constructor == NULL) return false;
        written = snprintf(
            call, sizeof(call),
            "mmay_format_integer(%s(mmay_g_v_%u), 10U, 1U, mmay_g_text, sizeof(mmay_g_text), &mmay_g_length)",
            constructor, instruction->first
        );
    } else {
        return false;
    }
    return written > 0 && (size_t)written < sizeof(call)
        && mmay_native_c_emit_formatted_string(
            buffer, instruction, "MMAY_INTEGER_TEXT_CAPACITY", call,
            "MMAY_INTEGER_TEXT_OK"
        );
}
