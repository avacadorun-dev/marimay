#include "c_emitter_internal.h"

static const MarimayMirEnum *mmay_native_c_fs_enum(
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

static const MarimayMirRecord *mmay_native_c_fs_record(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; function != NULL && index < function->records.length;
        index += 1) {
        const MarimayMirRecord *layout = mmay_vector_at_const(
            &function->records, index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static const MarimayMirArray *mmay_native_c_fs_array(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; function != NULL && index < function->arrays.length;
        index += 1) {
        const MarimayMirArray *layout = mmay_vector_at_const(
            &function->arrays, index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static const MarimayMirEnumPayload *mmay_native_c_fs_payload(
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
            &function->enum_payloads,
            variant->payload_start
        ) : NULL;
}

static bool mmay_native_c_fs_write_bytes(
    MarimayNativeCBuffer *buffer,
    const char *prefix,
    size_t index,
    MarimayFsPathView view
) {
    size_t byte = 0;
    if (!mmay_native_c_write(
            buffer, "static const unsigned char %s_%zu[%zu] = {",
            prefix, index, view.length == 0 ? 1U : view.length
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

static bool mmay_native_c_fs_write_roots(
    MarimayNativeCBuffer *buffer,
    const char *prefix,
    const char *array_name,
    const MarimayFsPathView *roots,
    size_t count
) {
    size_t index = 0;
    for (index = 0; index < count; index += 1) {
        if (!mmay_native_c_fs_write_bytes(
                buffer, prefix, index, roots[index]
            )) return false;
    }
    if (count == 0) return true;
    if (!mmay_native_c_write(
            buffer, "static const MarimayFsPathView %s[%zu] = {",
            array_name, count
        )) return false;
    for (index = 0; index < count; index += 1) {
        if (!mmay_native_c_write(
                buffer, index == 0
                    ? "{(const char *)%s_%zu,%zu}"
                    : ",{(const char *)%s_%zu,%zu}",
                prefix, index, roots[index].length
            )) return false;
    }
    return mmay_native_c_write(buffer, "};\n");
}

bool mmay_native_c_emit_fs_configuration(
    MarimayNativeCBuffer *buffer,
    const MarimayNativeCEmitOptions *options
) {
    if (buffer == NULL || options == NULL
        || options->read_root_count > MMAY_FS_MAX_CAPABILITY_ROOTS
        || options->write_root_count > MMAY_FS_MAX_CAPABILITY_ROOTS
        || (options->read_root_count != 0 && options->read_roots == NULL)
        || (options->write_root_count != 0 && options->write_roots == NULL)) {
        return false;
    }
    if (!mmay_native_c_fs_write_roots(
            buffer, "mmay_g_fs_read_root", "mmay_g_fs_read_roots",
            options->read_roots, options->read_root_count
        ) || !mmay_native_c_fs_write_roots(
            buffer, "mmay_g_fs_write_root", "mmay_g_fs_write_roots",
            options->write_roots, options->write_root_count
        )) return false;
    return mmay_native_c_write(
        buffer,
        "static bool mmay_g_fs_init(mmay_g_execution *x) { MarimayFsOptions options = mmay_fs_default_options(); options.read_roots = %s; options.read_root_count = %zu; options.write_roots = %s; options.write_root_count = %zu; x->fs_initialized = mmay_fs_init(&x->fs, x->allocator, &options); return x->fs_initialized; }\n",
        options->read_root_count == 0 ? "NULL" : "mmay_g_fs_read_roots",
        options->read_root_count,
        options->write_root_count == 0 ? "NULL" : "mmay_g_fs_write_roots",
        options->write_root_count
    );
}

bool mmay_native_c_emit_fs_runtime(MarimayNativeCBuffer *buffer) {
    return mmay_native_c_write(buffer,
        "#if defined(_WIN32)\n#define MMAY_G_PATH_FLAVOR MMAY_PATH_WINDOWS\n#else\n#define MMAY_G_PATH_FLAVOR MMAY_PATH_POSIX\n#endif\n"
        "static int mmay_g_fs_success(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result, const MarimayManagedCell *payload, MarimayManagedReference *out) { return mmay_g_managed_status(mmay_managed_enum_new(&x->heap, result, 0, payload, payload == NULL ? 0 : 1, out)); }\n"
        "static int mmay_g_fs_error(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, MarimayFsStatus fs_status, bool write, mmay_g_string path, int64_t detail, MarimayManagedReference *out) { MarimayManagedReference capability = mmay_managed_invalid_reference(), path_value = mmay_managed_invalid_reference(), error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[3] = {&capability, &path_value, &error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payloads[2] = {{0}, {0}}; MarimayManagedStatus status, pop_status; size_t variant = 16; static const unsigned char read_name[] = \"fs.read\", write_name[] = \"fs.write\"; if (fs_status == MMAY_FS_ALLOCATION_FAILED) return 5; switch (fs_status) { case MMAY_FS_CAPABILITY_DENIED: variant = 0; break; case MMAY_FS_INVALID_LIMIT: variant = 1; break; case MMAY_FS_LIMIT_EXCEEDED: variant = 2; break; case MMAY_FS_NOT_FOUND: variant = 3; break; case MMAY_FS_NOT_FILE: variant = 4; break; case MMAY_FS_NOT_DIRECTORY: variant = 5; break; case MMAY_FS_SYMLINK: variant = 6; break; case MMAY_FS_CLOSED: variant = 7; break; case MMAY_FS_OPEN_FAILED: variant = 8; break; case MMAY_FS_READ_FAILED: variant = 9; break; case MMAY_FS_WRITE_FAILED: variant = 10; break; case MMAY_FS_SYNC_FAILED: variant = 11; break; case MMAY_FS_CLOSE_FAILED: variant = 12; break; case MMAY_FS_REPLACE_FAILED: variant = 13; break; case MMAY_FS_INVALID_UTF8: variant = 14; break; case MMAY_FS_TOO_MANY_ENTRIES: variant = 15; break; case MMAY_FS_DIRECTORY_FAILED: variant = 16; break; default: return 7; } status = mmay_managed_root_push(&x->heap, &roots, locations, 3); if (status == MMAY_MANAGED_OK && variant == 0) { status = mmay_managed_string_new(&x->heap, write ? write_name : read_name, write ? sizeof(write_name) - 1U : sizeof(read_name) - 1U, &capability); if (status == MMAY_MANAGED_OK) status = mmay_managed_string_new(&x->heap, path.data, path.length, &path_value); payloads[0].reference = capability; payloads[1].reference = path_value; } else if (status == MMAY_MANAGED_OK && (variant == 1 || variant == 2 || variant == 14 || variant == 15)) payloads[0].f64_bits = (uint64_t)detail; if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, payloads, variant == 0 ? 2 : ((variant == 1 || variant == 2 || variant == 14 || variant == 15) ? 1 : 0), &error); if (status == MMAY_MANAGED_OK) { payloads[0].reference = error; status = mmay_managed_enum_new(&x->heap, result, 1, payloads, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_path_error(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, size_t variant, int64_t limit, MarimayManagedReference *out) { MarimayManagedReference error = mmay_managed_invalid_reference(); MarimayManagedReference *locations[1] = {&error}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell payload = {0}; MarimayManagedStatus status = mmay_managed_root_push(&x->heap, &roots, locations, 1), pop_status; if (variant == 3) payload.f64_bits = (uint64_t)limit; if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(&x->heap, error_descriptor, variant, variant == 3 ? &payload : NULL, variant == 3 ? 1 : 0, &error); if (status == MMAY_MANAGED_OK) { payload.reference = error; status = mmay_managed_enum_new(&x->heap, result, 1, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
        "static int mmay_g_path_normalize(mmay_g_execution *x, mmay_g_string input, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { MarimayPath path; MarimayPathStatus path_status; MarimayManagedCell payload = {0}; MarimayManagedStatus status; if (input.length > MMAY_FS_MAX_PATH_BYTES) return mmay_g_path_error(x, result, error_descriptor, 3, MMAY_FS_MAX_PATH_BYTES, out); path_status = mmay_path_normalize(&path, x->allocator, MMAY_G_PATH_FLAVOR, (const char *)input.data, input.length); if (path_status == MMAY_PATH_ALLOCATION_FAILED) return 5; if (path_status != MMAY_PATH_OK) return mmay_g_path_error(x, result, error_descriptor, path_status == MMAY_PATH_UNSUPPORTED_PREFIX ? 1 : path_status == MMAY_PATH_ESCAPES_ROOT ? 2 : 0, 0, out); status = mmay_managed_string_new(&x->heap, path.text.data, path.text.length, &payload.reference); mmay_path_dispose(&path); if (status != MMAY_MANAGED_OK) return mmay_g_managed_status(status); return mmay_g_fs_success(x, result, &payload, out); }\n"
        "static int mmay_g_path_absolute(mmay_g_execution *x, mmay_g_string input, bool *out) { MarimayPath path; MarimayPathStatus status = mmay_path_normalize(&path, x->allocator, MMAY_G_PATH_FLAVOR, (const char *)input.data, input.length); if (status == MMAY_PATH_ALLOCATION_FAILED) return 5; if (status != MMAY_PATH_OK) return 7; *out = path.absolute; mmay_path_dispose(&path); return 0; }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_fs_bytes(mmay_g_execution *x, const MarimayManagedEnumDescriptor *result, const MarimayManagedArrayDescriptor *array_descriptor, const MarimayOwnedBytes *bytes, MarimayManagedReference *out) { MarimayManagedCell *cells = NULL, payload = {0}; MarimayManagedReference array = mmay_managed_invalid_reference(); MarimayManagedReference *locations[1] = {&array}; MarimayManagedRootFrame roots = {0}; MarimayManagedStatus status, pop_status; size_t allocation = 0, index; if (!mmay_size_multiply(bytes->length, sizeof(*cells), &allocation)) return 6; cells = allocation == 0 ? NULL : mmay_allocate(&x->allocator, allocation); if (allocation != 0 && cells == NULL) return 5; for (index = 0; index < bytes->length; index += 1) { memset(&cells[index], 0, sizeof(cells[index])); cells[index].f64_bits = bytes->data[index]; } status = mmay_managed_root_push(&x->heap, &roots, locations, 1); if (status == MMAY_MANAGED_OK) status = mmay_managed_array_new(&x->heap, array_descriptor, cells, bytes->length, &array); if (status == MMAY_MANAGED_OK) { payload.reference = array; status = mmay_managed_enum_new(&x->heap, result, 0, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; mmay_deallocate(&x->allocator, cells, allocation); return mmay_g_managed_status(status); }\n"
        "static int mmay_g_fs_open(mmay_g_execution *x, mmay_g_string path, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { int64_t handle = 0; MarimayManagedCell payload = {0}; MarimayFsStatus status = mmay_fs_open_read(&x->fs, (const char *)path.data, path.length, &handle); if (status != MMAY_FS_OK) return mmay_g_fs_error(x, result, error_descriptor, status, false, path, 0, out); payload.f64_bits = (uint64_t)handle; return mmay_g_fs_success(x, result, &payload, out); }\n"
        "static int mmay_g_fs_read(mmay_g_execution *x, int64_t handle, int64_t limit, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, const MarimayManagedArrayDescriptor *array_descriptor, MarimayManagedReference *out) { MarimayOwnedBytes bytes = {0}; mmay_g_string empty = {0}; MarimayFsStatus fs_status = mmay_fs_read(&x->fs, handle, limit, &bytes); int status; if (fs_status != MMAY_FS_OK) return mmay_g_fs_error(x, result, error_descriptor, fs_status, false, empty, limit, out); status = mmay_g_fs_bytes(x, result, array_descriptor, &bytes, out); mmay_owned_bytes_dispose(&bytes); return status; }\n"
        "static int mmay_g_fs_close(mmay_g_execution *x, int64_t handle, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { mmay_g_string empty = {0}; MarimayFsStatus status = mmay_fs_close(&x->fs, handle); return status == MMAY_FS_OK ? mmay_g_fs_success(x, result, NULL, out) : mmay_g_fs_error(x, result, error_descriptor, status, false, empty, 0, out); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_fs_read_all(mmay_g_execution *x, mmay_g_string path, int64_t limit, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, const MarimayManagedArrayDescriptor *array_descriptor, MarimayManagedReference *out) { MarimayOwnedBytes bytes = {0}; MarimayFsStatus fs_status = mmay_fs_read_all(&x->fs, (const char *)path.data, path.length, limit, &bytes); int status; if (fs_status != MMAY_FS_OK) return mmay_g_fs_error(x, result, error_descriptor, fs_status, false, path, limit, out); status = mmay_g_fs_bytes(x, result, array_descriptor, &bytes, out); mmay_owned_bytes_dispose(&bytes); return status; }\n"
        "static int mmay_g_fs_write(mmay_g_execution *x, mmay_g_string path, MarimayManagedReference array, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, MarimayManagedReference *out) { unsigned char *bytes = NULL; size_t length = 0; MarimayFsStatus fs_status; int status = mmay_g_text_array_bytes(x, array, &bytes, &length); if (status != 0) return status; fs_status = mmay_fs_write_atomic(&x->fs, (const char *)path.data, path.length, bytes, length); mmay_deallocate(&x->allocator, bytes, length); return fs_status == MMAY_FS_OK ? mmay_g_fs_success(x, result, NULL, out) : mmay_g_fs_error(x, result, error_descriptor, fs_status, true, path, 0, out); }\n"
        "static int mmay_g_fs_metadata_value(mmay_g_execution *x, MarimayFsMetadata metadata, const MarimayManagedEnumDescriptor *result, const MarimayManagedRecordDescriptor *record_descriptor, const MarimayManagedEnumDescriptor *kind_descriptor, MarimayManagedReference *out) { MarimayManagedReference kind = mmay_managed_invalid_reference(), record = mmay_managed_invalid_reference(); MarimayManagedReference *locations[2] = {&kind, &record}; MarimayManagedRootFrame roots = {0}; MarimayManagedCell fields[2] = {{0}, {0}}; MarimayManagedStatus status = mmay_managed_root_push(&x->heap, &roots, locations, 2), pop_status; if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(&x->heap, kind_descriptor, metadata.directory ? 1 : 0, NULL, 0, &kind); fields[0].reference = kind; fields[1].f64_bits = metadata.size; if (status == MMAY_MANAGED_OK) status = mmay_managed_record_new(&x->heap, record_descriptor, fields, 2, &record); if (status == MMAY_MANAGED_OK) { fields[0].reference = record; status = mmay_managed_enum_new(&x->heap, result, 0, fields, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; return mmay_g_managed_status(status); }\n"
        "static int mmay_g_fs_metadata(mmay_g_execution *x, mmay_g_string path, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, const MarimayManagedRecordDescriptor *record_descriptor, const MarimayManagedEnumDescriptor *kind_descriptor, MarimayManagedReference *out) { MarimayFsMetadata metadata = {0}; MarimayFsStatus status = mmay_fs_metadata(&x->fs, (const char *)path.data, path.length, &metadata); return status == MMAY_FS_OK ? mmay_g_fs_metadata_value(x, metadata, result, record_descriptor, kind_descriptor, out) : mmay_g_fs_error(x, result, error_descriptor, status, false, path, 0, out); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_fs_directory_value(mmay_g_execution *x, const MarimayVector *entries, const MarimayManagedEnumDescriptor *result, const MarimayManagedArrayDescriptor *array_descriptor, const MarimayManagedRecordDescriptor *record_descriptor, const MarimayManagedEnumDescriptor *kind_descriptor, MarimayManagedReference *out) { MarimayManagedCell *cells = NULL; MarimayManagedReference **locations = NULL; MarimayManagedReference array = mmay_managed_invalid_reference(); MarimayManagedRootFrame roots = {0}; MarimayManagedStatus status = MMAY_MANAGED_OK, pop_status; size_t cell_bytes = 0, location_bytes = 0, index; if (!mmay_size_multiply(entries->length, sizeof(*cells), &cell_bytes) || !mmay_size_multiply(entries->length, sizeof(*locations), &location_bytes)) return 6; cells = cell_bytes == 0 ? NULL : mmay_allocate(&x->allocator, cell_bytes); locations = location_bytes == 0 ? NULL : mmay_allocate(&x->allocator, location_bytes); if ((cell_bytes != 0 && cells == NULL) || (location_bytes != 0 && locations == NULL)) { mmay_deallocate(&x->allocator, cells, cell_bytes); mmay_deallocate(&x->allocator, locations, location_bytes); return 5; } memset(cells, 0, cell_bytes); for (index = 0; index < entries->length; index += 1) { cells[index].reference = mmay_managed_invalid_reference(); locations[index] = &cells[index].reference; } status = mmay_managed_root_push(&x->heap, &roots, locations, entries->length); for (index = 0; status == MMAY_MANAGED_OK && index < entries->length; index += 1) { const MarimayFsEntry *entry = mmay_vector_at_const(entries, index); MarimayManagedReference name = mmay_managed_invalid_reference(), kind = mmay_managed_invalid_reference(), record = mmay_managed_invalid_reference(); MarimayManagedReference *temporary[3] = {&name, &kind, &record}; MarimayManagedRootFrame temporary_roots = {0}; MarimayManagedCell fields[2] = {{0}, {0}}; size_t variant = entry->kind == MMAY_DIRECTORY_ENTRY_REGULAR ? 0 : entry->kind == MMAY_DIRECTORY_ENTRY_DIRECTORY ? 1 : entry->kind == MMAY_DIRECTORY_ENTRY_SYMLINK ? 2 : 3; status = mmay_managed_root_push(&x->heap, &temporary_roots, temporary, 3); if (status == MMAY_MANAGED_OK) status = mmay_managed_string_new(&x->heap, entry->name.data, entry->name.length, &name); if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(&x->heap, kind_descriptor, variant, NULL, 0, &kind); fields[0].reference = name; fields[1].reference = kind; if (status == MMAY_MANAGED_OK) status = mmay_managed_record_new(&x->heap, record_descriptor, fields, 2, &record); if (status == MMAY_MANAGED_OK) cells[index].reference = record; pop_status = mmay_managed_root_pop(&x->heap, &temporary_roots); if (status == MMAY_MANAGED_OK) status = pop_status; } if (status == MMAY_MANAGED_OK) status = mmay_managed_array_new(&x->heap, array_descriptor, cells, entries->length, &array); if (status == MMAY_MANAGED_OK) { MarimayManagedCell payload = {0}; payload.reference = array; status = mmay_managed_enum_new(&x->heap, result, 0, &payload, 1, out); } pop_status = mmay_managed_root_pop(&x->heap, &roots); if (status == MMAY_MANAGED_OK) status = pop_status; mmay_deallocate(&x->allocator, cells, cell_bytes); mmay_deallocate(&x->allocator, locations, location_bytes); return mmay_g_managed_status(status); }\n"
    ) && mmay_native_c_write(buffer,
        "static int mmay_g_fs_directory(mmay_g_execution *x, mmay_g_string path, int64_t limit, const MarimayManagedEnumDescriptor *result, const MarimayManagedEnumDescriptor *error_descriptor, const MarimayManagedArrayDescriptor *array_descriptor, const MarimayManagedRecordDescriptor *record_descriptor, const MarimayManagedEnumDescriptor *kind_descriptor, MarimayManagedReference *out) { MarimayVector entries; MarimayFsStatus fs_status = mmay_fs_read_directory(&x->fs, (const char *)path.data, path.length, limit, &entries); int status; if (fs_status != MMAY_FS_OK) return mmay_g_fs_error(x, result, error_descriptor, fs_status, false, path, limit, out); status = mmay_g_fs_directory_value(x, &entries, result, array_descriptor, record_descriptor, kind_descriptor, out); mmay_fs_entries_dispose(&entries); return status; }\n"
    );
}

static bool mmay_native_c_fs_descriptors(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirEnum **result_out,
    const MarimayMirEnum **error_out,
    const MarimayMirEnumPayload **ok_out
) {
    const MarimayTypeId *target = mmay_vector_at_const(
        &function->register_types, instruction->target
    );
    const MarimayMirEnum *result = target != NULL
        ? mmay_native_c_fs_enum(function, *target) : NULL;
    const MarimayMirEnumPayload *error_payload = mmay_native_c_fs_payload(
        function, result, 1
    );
    const MarimayMirEnum *error = error_payload != NULL
        ? mmay_native_c_fs_enum(function, error_payload->type) : NULL;
    (void)buffer;
    if (result == NULL || error == NULL) return false;
    *result_out = result;
    *error_out = error;
    if (ok_out != NULL) *ok_out = mmay_native_c_fs_payload(function, result, 0);
    return true;
}

static bool mmay_native_c_fs_write_result_error(
    MarimayNativeCBuffer *buffer,
    const MarimayMirEnum *result,
    const MarimayMirEnum *error
) {
    return mmay_native_c_write(buffer, "&")
        && mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, result->type
        ) && mmay_native_c_write(buffer, ", &")
        && mmay_native_c_write_descriptor_name(
            buffer, MMAY_NATIVE_C_MANAGED_ENUM, error->type
        );
}

static bool mmay_native_c_fs_finish(
    MarimayNativeCBuffer *buffer,
    const MarimayMirInstruction *instruction
) {
    return mmay_native_c_write(
        buffer,
        ", &mmay_g_v_%u); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
        instruction->target, instruction->span.start, instruction->span.end
    );
}

bool mmay_native_c_emit_fs_instruction(
    MarimayNativeCBuffer *buffer,
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction
) {
    const MarimayMirEnum *result = NULL;
    const MarimayMirEnum *error = NULL;
    const MarimayMirEnumPayload *ok = NULL;
    if (buffer == NULL || function == NULL || instruction == NULL) return false;
    if (instruction->fs_operation == MMAY_MIR_PATH_IS_ABSOLUTE) {
        return mmay_native_c_write(
            buffer,
            "  mmay_g_s = mmay_g_path_absolute(mmay_g_x, mmay_g_v_%u, &mmay_g_v_%u); if (mmay_g_s != 0) { *mmay_g_start = %zu; *mmay_g_end = %zu; goto mmay_g_cleanup; }\n",
            instruction->first, instruction->target,
            instruction->span.start, instruction->span.end
        );
    }
    if (instruction->fs_operation == MMAY_MIR_FS_CLOSE_BEST_EFFORT) {
        return mmay_native_c_write(
            buffer,
            "  mmay_fs_close_best_effort(&mmay_g_x->fs, mmay_g_v_%u);\n",
            instruction->first
        );
    }
    if (!mmay_native_c_fs_descriptors(
            buffer, function, instruction, &result, &error, &ok
        )) return false;
    if (instruction->fs_operation == MMAY_MIR_PATH_NORMALIZE) {
        return mmay_native_c_write(
                buffer, "  mmay_g_s = mmay_g_path_normalize(mmay_g_x, mmay_g_v_%u, ",
                instruction->first
            ) && mmay_native_c_fs_write_result_error(buffer, result, error)
            && mmay_native_c_fs_finish(buffer, instruction);
    }
    if (instruction->fs_operation == MMAY_MIR_FS_OPEN_READ) {
        return mmay_native_c_write(
                buffer, "  mmay_g_s = mmay_g_fs_open(mmay_g_x, mmay_g_v_%u, ",
                instruction->first
            ) && mmay_native_c_fs_write_result_error(buffer, result, error)
            && mmay_native_c_fs_finish(buffer, instruction);
    }
    if (instruction->fs_operation == MMAY_MIR_FS_READ
        || instruction->fs_operation == MMAY_MIR_FS_READ_ALL) {
        const MarimayMirArray *array = ok != NULL
            ? mmay_native_c_fs_array(function, ok->type) : NULL;
        if (array == NULL || !mmay_native_c_write(
                buffer,
                instruction->fs_operation == MMAY_MIR_FS_READ
                    ? "  mmay_g_s = mmay_g_fs_read(mmay_g_x, mmay_g_v_%u, mmay_g_v_%u, "
                    : "  mmay_g_s = mmay_g_fs_read_all(mmay_g_x, mmay_g_v_%u, mmay_g_v_%u, ",
                instruction->first, instruction->second
            ) || !mmay_native_c_fs_write_result_error(buffer, result, error)
            || !mmay_native_c_write(
                buffer, ", %s", mmay_native_c_array_descriptor(function, array->type)
            )) return false;
        return mmay_native_c_fs_finish(buffer, instruction);
    }
    if (instruction->fs_operation == MMAY_MIR_FS_CLOSE) {
        return mmay_native_c_write(
                buffer, "  mmay_g_s = mmay_g_fs_close(mmay_g_x, mmay_g_v_%u, ",
                instruction->first
            ) && mmay_native_c_fs_write_result_error(buffer, result, error)
            && mmay_native_c_fs_finish(buffer, instruction);
    }
    if (instruction->fs_operation == MMAY_MIR_FS_WRITE_ATOMIC) {
        return mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_fs_write(mmay_g_x, mmay_g_v_%u, mmay_g_v_%u, ",
                instruction->first, instruction->second
            ) && mmay_native_c_fs_write_result_error(buffer, result, error)
            && mmay_native_c_fs_finish(buffer, instruction);
    }
    if (instruction->fs_operation == MMAY_MIR_FS_METADATA) {
        const MarimayMirRecord *record = ok != NULL
            ? mmay_native_c_fs_record(function, ok->type) : NULL;
        const MarimayMirRecordField *kind_field = record != NULL
            ? mmay_vector_at_const(&function->record_fields, record->field_start)
            : NULL;
        if (record == NULL || kind_field == NULL || !mmay_native_c_write(
                buffer, "  mmay_g_s = mmay_g_fs_metadata(mmay_g_x, mmay_g_v_%u, ",
                instruction->first
            ) || !mmay_native_c_fs_write_result_error(buffer, result, error)
            || !mmay_native_c_write(buffer, ", &")
            || !mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_RECORD, record->type
            ) || !mmay_native_c_write(buffer, ", &")
            || !mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_ENUM, kind_field->type
            )) return false;
        return mmay_native_c_fs_finish(buffer, instruction);
    }
    if (instruction->fs_operation == MMAY_MIR_FS_READ_DIRECTORY) {
        const MarimayMirArray *array = ok != NULL
            ? mmay_native_c_fs_array(function, ok->type) : NULL;
        const MarimayMirRecord *record = array != NULL
            ? mmay_native_c_fs_record(function, array->element_type) : NULL;
        const MarimayMirRecordField *kind_field = record != NULL
            ? mmay_vector_at_const(
                &function->record_fields, record->field_start + 1
            ) : NULL;
        if (array == NULL || record == NULL || kind_field == NULL
            || !mmay_native_c_write(
                buffer,
                "  mmay_g_s = mmay_g_fs_directory(mmay_g_x, mmay_g_v_%u, mmay_g_v_%u, ",
                instruction->first, instruction->second
            ) || !mmay_native_c_fs_write_result_error(buffer, result, error)
            || !mmay_native_c_write(
                buffer, ", %s, &",
                mmay_native_c_array_descriptor(function, array->type)
            ) || !mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_RECORD, record->type
            ) || !mmay_native_c_write(buffer, ", &")
            || !mmay_native_c_write_descriptor_name(
                buffer, MMAY_NATIVE_C_MANAGED_ENUM, kind_field->type
            )) return false;
        return mmay_native_c_fs_finish(buffer, instruction);
    }
    return false;
}
