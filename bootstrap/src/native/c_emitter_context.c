#include "c_emitter_internal.h"

static const MarimayMirEnum *mmay_native_c_context_enum(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; function != NULL && index < function->enums.length;
        index += 1) {
        const MarimayMirEnum *layout = mmay_vector_at_const(
            &function->enums, index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static const MarimayMirEnumPayload *mmay_native_c_context_payload(
    const MarimayMirFunction *function,
    const MarimayMirEnum *layout,
    size_t variant_index
) {
    const MarimayMirEnumVariant *variant = layout != NULL
            && variant_index < layout->variant_count
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + variant_index
        ) : NULL;
    return variant != NULL && variant->payload_count == 1
        ? mmay_vector_at_const(
            &function->enum_payloads, variant->payload_start
        ) : NULL;
}

static bool mmay_native_c_context_write_bytes(
    MarimayNativeCBuffer *buffer,
    size_t index,
    MarimayEnvNameView view
) {
    size_t byte = 0;
    if (!mmay_native_c_write(
            buffer, "static const unsigned char mmay_g_env_name_%zu[%zu] = {",
            index, view.length == 0 ? 1U : view.length
        )) return false;
    if (view.length == 0) return mmay_native_c_write(buffer, "0};\n");
    for (byte = 0; byte < view.length; byte += 1) {
        if (!mmay_native_c_write(
                buffer, byte == 0 ? "0x%02x" : ",0x%02x",
                (unsigned int)(unsigned char)view.data[byte]
            )) return false;
    }
    return mmay_native_c_write(buffer, "};\n");
}

bool mmay_native_c_context_options_valid(
    const MarimayNativeCEmitOptions *options
) {
    MarimayEnvOptions env_options = mmay_env_default_options();
    size_t index = 0;
    if (options == NULL
        || options->env_grant_count > MMAY_ENV_MAX_GRANTS
        || (options->env_grant_count != 0 && options->env_grants == NULL)) {
        return false;
    }
    for (index = 0; index < options->env_grant_count; index += 1) {
        MarimayEnvNameView view = options->env_grants[index];
        size_t prior = 0;
        if (!mmay_env_name_valid(view.data, view.length)) return false;
        for (prior = 0; prior < index; prior += 1) {
            if (mmay_env_names_equal(
                    options->env_grants[prior].data,
                    options->env_grants[prior].length,
                    view.data, view.length,
                    env_options.ascii_case_insensitive
                )) return false;
        }
    }
    return true;
}

bool mmay_native_c_emit_context_configuration(
    MarimayNativeCBuffer *buffer,
    const MarimayNativeCEmitOptions *options
) {
    size_t index = 0;
    if (buffer == NULL || !mmay_native_c_context_options_valid(options)) {
        return false;
    }
    for (index = 0; index < options->env_grant_count; index += 1) {
        if (!mmay_native_c_context_write_bytes(
                buffer, index, options->env_grants[index]
            )) return false;
    }
    if (options->env_grant_count != 0) {
        if (!mmay_native_c_write(
                buffer,
                "static const MarimayEnvNameView mmay_g_env_grants[%zu] = {",
                options->env_grant_count
            )) return false;
        for (index = 0; index < options->env_grant_count; index += 1) {
            if (!mmay_native_c_write(
                    buffer, index == 0
                        ? "{(const char *)mmay_g_env_name_%zu,%zu}"
                        : ",{(const char *)mmay_g_env_name_%zu,%zu}",
                    index, options->env_grants[index].length
                )) return false;
        }
        if (!mmay_native_c_write(buffer, "};\n")) return false;
    }
    return mmay_native_c_write(
        buffer,
        "static bool mmay_g_context_init(mmay_g_execution *x) { MarimayEnvOptions options = mmay_env_default_options(); options.grants = %s; options.grant_count = %zu; x->process_context = mmay_process_context_default_options(); x->env_initialized = mmay_env_init(&x->env, x->allocator, &options); return x->env_initialized; }\n",
        options->env_grant_count == 0 ? "NULL" : "mmay_g_env_grants",
        options->env_grant_count
    );
}

bool mmay_native_c_emit_context_runtime(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(buffer,
        "static int mmay_g_context_error(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, size_t variant, mmay_g_string text, bool has_text, int64_t detail, bool has_detail, MarimayManagedReference *out) { MarimayManagedReference text_value = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[2] = {&text_value, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status = mmay_managed_root_push(&x->heap, &roots, locations, 2), pop_status; if (status == MMAY_MANAGED_OK && has_text) { status = mmay_managed_string_new(&x->heap, text.data, text.length, &text_value); payload.reference = text_value; } else if (!has_text) payload.f64_bits = (uint64_t)detail; if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, has_text || has_detail ? &payload : NULL, has_text || has_detail ? 1 : 0, &error); if (status == MMAY_MANAGED_OK) { payload.reference = error; status = mmay_managed_enum_new(&x->heap, result, 1, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
        "static int mmay_g_env_success(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *option_descriptor, const MarimayOwnedBytes *value, bool present, MarimayManagedReference *out) { MarimayManagedReference string = mmay_managed_invalid_reference(), option = mmay_managed_invalid_reference(); MarimayManagedReference *locations[2] = {&string, &option}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status = mmay_managed_root_push(&x->heap, &roots, locations, 2), pop_status; if (status == MMAY_MANAGED_OK && present) status = mmay_managed_string_new(&x->heap, value->data, value->length, &string); payload.reference = string; if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(&x->heap, option_descriptor, present ? 0 : 1, present ? &payload : NULL, present ? 1 : 0, &option); if (status == MMAY_MANAGED_OK) { payload.reference = option; status = mmay_managed_enum_new(&x->heap, result, 0, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_env_get(mmay_g_execution *x, mmay_g_string name, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, const MarimayManagedEnumDescriptor *option_descriptor, MarimayManagedReference *out) { MarimayOwnedBytes value = {0}; size_t invalid = 0; MarimayEnvStatus status = mmay_env_read(&x->env, (const char *)name.data, name.length, &value, &invalid); int result_status; mmay_g_string empty = {0}; if (status == MMAY_ENV_ALLOCATION_FAILED) return 5; if (status == MMAY_ENV_OK || status == MMAY_ENV_MISSING) { result_status = mmay_g_env_success(x, result, option_descriptor, &value, status == MMAY_ENV_OK, out); mmay_owned_bytes_dispose(&value); return result_status; } if (status == MMAY_ENV_INVALID_ARGUMENT) return 7; if (status == MMAY_ENV_CAPABILITY_DENIED) return mmay_g_context_error(x, result, error_descriptor, 1, name, true, 0, false, out); if (status == MMAY_ENV_INVALID_UTF8) return mmay_g_context_error(x, result, error_descriptor, 2, empty, false, (int64_t)invalid, true, out); if (status == MMAY_ENV_LIMIT_EXCEEDED) return mmay_g_context_error(x, result, error_descriptor, 3, empty, false, MMAY_ENV_MAX_VALUE_BYTES, true, out); return mmay_g_context_error(x, result, error_descriptor, status == MMAY_ENV_INVALID_NAME ? 0 : 4, empty, false, 0, false, out); }\n"
        "static int mmay_g_process_success(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result, const MarimayOwnedBytes *path, MarimayManagedReference *out) { MarimayManagedReference string = mmay_managed_invalid_reference(); MarimayManagedReference *locations[1] = {&string}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status = mmay_managed_root_push(&x->heap, &roots, locations, 1), pop_status; if (status == MMAY_MANAGED_OK) status = mmay_managed_string_new(&x->heap, path->data, path->length, &string); if (status == MMAY_MANAGED_OK) { payload.reference = string; status = mmay_managed_enum_new(&x->heap, result, 0, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
        "static int mmay_g_process_context(mmay_g_execution *x, bool executable, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { MarimayOwnedBytes path = {0}; size_t invalid = 0; MarimayProcessContextStatus status = executable ? mmay_process_context_executable_path(&x->process_context, x->allocator, &path, &invalid) : mmay_process_context_current_directory(&x->process_context, x->allocator, &path, &invalid); int result_status; mmay_g_string empty = {0}; size_t variant = 3; if (status == MMAY_PROCESS_CONTEXT_ALLOCATION_FAILED) return 5; if (status == MMAY_PROCESS_CONTEXT_OK) { result_status = mmay_g_process_success(x, result, &path, out); mmay_owned_bytes_dispose(&path); return result_status; } if (status == MMAY_PROCESS_CONTEXT_INVALID_ARGUMENT) return 7; if (status == MMAY_PROCESS_CONTEXT_INVALID_UTF8) return mmay_g_context_error(x, result, error_descriptor, 2, empty, false, (int64_t)invalid, true, out); if (status == MMAY_PROCESS_CONTEXT_CURRENT_DIRECTORY_UNAVAILABLE) variant = 0; else if (status == MMAY_PROCESS_CONTEXT_EXECUTABLE_PATH_UNAVAILABLE) variant = 1; return mmay_g_context_error(x, result, error_descriptor, variant, empty, false, 0, false, out); }\n"
    );
}

static bool mmay_native_c_context_descriptors(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirEnum **result_out,
    const MarimayMirEnum **error_out,
    const MarimayMirEnum **option_out
) {
    const MarimayTypeId *target = mmay_vector_at_const(
        &function->register_types, instruction->target
    );
    const MarimayMirEnum *result = target != NULL
        ? mmay_native_c_context_enum(function, *target) : NULL;
    const MarimayMirEnumPayload *ok = mmay_native_c_context_payload(
        function, result, 0
    );
    const MarimayMirEnumPayload *error_payload = mmay_native_c_context_payload(
        function, result, 1
    );
    const MarimayMirEnum *error = error_payload != NULL
        ? mmay_native_c_context_enum(function, error_payload->type) : NULL;
    const MarimayMirEnum *option = instruction->kind == MMAY_MIR_ENV_READ
            && ok != NULL
        ? mmay_native_c_context_enum(function, ok->type) : NULL;
    if (result == NULL || error == NULL
        || (instruction->kind == MMAY_MIR_ENV_READ && option == NULL)) {
        return false;
    }
    *result_out = result;
    *error_out = error;
    *option_out = option;
    return true;
}

static bool mmay_native_c_context_write_descriptor(
    MarimayNativeCBuffer *buffer,
    const MarimayMirEnum *layout
) {
    return mmay_native_c_write(buffer, "&")
        && mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, layout->type
        );
}

bool mmay_native_c_emit_context_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirEnum *result = NULL;
    const MarimayMirEnum *error = NULL;
    const MarimayMirEnum *option = NULL;
    if (buffer == NULL || function == NULL || instruction == NULL
        || !mmay_native_c_context_descriptors(
            function, instruction, &result, &error, &option
        )) return false;
    if (instruction->kind == MMAY_MIR_ENV_READ) {
        if (!mmay_native_c_write(
                buffer, "  mmay_g_s = mmay_g_env_get(mmay_g_x, mmay_g_v_%u, ",
                instruction->first
            ) || !mmay_native_c_context_write_descriptor(buffer, result)
            || !mmay_native_c_write(buffer, ", ")
            || !mmay_native_c_context_write_descriptor(buffer, error)
            || !mmay_native_c_write(buffer, ", ")
            || !mmay_native_c_context_write_descriptor(buffer, option)) {
            return false;
        }
    } else if (!mmay_native_c_write(
            buffer, "  mmay_g_s = mmay_g_process_context(mmay_g_x, %s, ",
            instruction->context_operation == MMAY_MIR_CONTEXT_EXECUTABLE_PATH
                ? "true" : "false"
        ) || !mmay_native_c_context_write_descriptor(buffer, result)
        || !mmay_native_c_write(buffer, ", ")
        || !mmay_native_c_context_write_descriptor(buffer, error)) {
        return false;
    }
    return mmay_native_c_write(
        buffer,
        ", &mmay_g_v_%u); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
        instruction->target, instruction->span.start, instruction->span.end
    );
}
