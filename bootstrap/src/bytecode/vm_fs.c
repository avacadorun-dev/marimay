#include "vm_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/path.h"
#include "mmay/runtime/string.h"

static const MarimayBytecodeEnumPayload *mmay_bytecode_vm_fs_payload(
    const MarimayBytecodeFunction *function,
    const MarimayBytecodeEnumLayout *layout,
    size_t variant_index,
    size_t field
) {
    const MarimayBytecodeEnumVariant *variant = NULL;
    size_t relative = 0;
    if (layout == NULL || variant_index >= layout->variant_count
        || layout->variant_start > function->enum_variants.length
        || variant_index >= function->enum_variants.length - layout->variant_start) {
        return NULL;
    }
    variant = mmay_vector_at_const(
        &function->enum_variants, layout->variant_start + variant_index
    );
    if (variant == NULL || field >= variant->payload_count
        || !mmay_size_add(variant->payload_start, field, &relative)
        || relative >= layout->payload_count
        || layout->payload_start > function->enum_payloads.length
        || relative >= function->enum_payloads.length - layout->payload_start) {
        return NULL;
    }
    return mmay_vector_at_const(
        &function->enum_payloads, layout->payload_start + relative
    );
}

static MarimayBytecodeRunStatus mmay_bytecode_vm_fs_descriptors(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    uint32_t result_id,
    const MarimayManagedEnumDescriptor **result,
    const MarimayManagedEnumDescriptor **error,
    const MarimayBytecodeEnumPayload **ok_payload
) {
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, result_id
    );
    const MarimayBytecodeEnumPayload *error_payload =
        mmay_bytecode_vm_fs_payload(function, layout, 1, 0);
    const void *raw = NULL;
    MarimayBytecodeRunStatus status;
    if (layout == NULL || error_payload == NULL
        || error_payload->type != MMAY_BYTECODE_TYPE_ENUM) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    status = mmay_bytecode_run_nominal_descriptor(
        execution, function, MMAY_BYTECODE_TYPE_ENUM, result_id, &raw
    );
    *result = raw;
    if (status == MMAY_BYTECODE_RUN_OK) status =
        mmay_bytecode_run_nominal_descriptor(
            execution, function, MMAY_BYTECODE_TYPE_ENUM,
            error_payload->layout, &raw
        );
    *error = raw;
    if (ok_payload != NULL) {
        *ok_payload = mmay_bytecode_vm_fs_payload(function, layout, 0, 0);
    }
    return status;
}

static MarimayBytecodeRunStatus mmay_bytecode_vm_fs_success(
    MarimayBytecodeExecution *execution,
    const MarimayManagedEnumDescriptor *result,
    const MarimayManagedCell *payload,
    MarimayBytecodeValue *target
) {
    return mmay_bytecode_run_managed_status(mmay_managed_enum_new(
        &execution->managed_heap, result, 0,
        payload, payload == NULL ? 0U : 1U,
        &target->as.managed_reference
    ));
}

static MarimayBytecodeRunStatus mmay_bytecode_vm_fs_error(
    MarimayBytecodeExecution *execution,
    const MarimayManagedEnumDescriptor *result,
    const MarimayManagedEnumDescriptor *error,
    MarimayFsStatus fs_status,
    bool write,
    const unsigned char *path,
    size_t path_length,
    int64_t detail,
    MarimayBytecodeValue *target
) {
    MarimayManagedReference capability = mmay_managed_invalid_reference();
    MarimayManagedReference path_value = mmay_managed_invalid_reference();
    MarimayManagedReference error_value = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[3] = {
        &capability, &path_value, &error_value
    };
    MarimayManagedRootFrame roots = {0};
    MarimayManagedCell payloads[2] = {{0}, {0}};
    MarimayManagedStatus status;
    size_t variant = 16;
    if (fs_status == MMAY_FS_ALLOCATION_FAILED) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    switch (fs_status) {
        case MMAY_FS_CAPABILITY_DENIED: variant = 0; break;
        case MMAY_FS_INVALID_LIMIT: variant = 1; break;
        case MMAY_FS_LIMIT_EXCEEDED: variant = 2; break;
        case MMAY_FS_NOT_FOUND: variant = 3; break;
        case MMAY_FS_NOT_FILE: variant = 4; break;
        case MMAY_FS_NOT_DIRECTORY: variant = 5; break;
        case MMAY_FS_SYMLINK: variant = 6; break;
        case MMAY_FS_CLOSED: variant = 7; break;
        case MMAY_FS_OPEN_FAILED: variant = 8; break;
        case MMAY_FS_READ_FAILED: variant = 9; break;
        case MMAY_FS_WRITE_FAILED: variant = 10; break;
        case MMAY_FS_SYNC_FAILED: variant = 11; break;
        case MMAY_FS_CLOSE_FAILED: variant = 12; break;
        case MMAY_FS_REPLACE_FAILED: variant = 13; break;
        case MMAY_FS_INVALID_UTF8: variant = 14; break;
        case MMAY_FS_TOO_MANY_ENTRIES: variant = 15; break;
        case MMAY_FS_DIRECTORY_FAILED: variant = 16; break;
        default: return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    status = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, 3
    );
    if (status == MMAY_MANAGED_OK && variant == 0) {
        static const unsigned char read_name[] = "fs.read";
        static const unsigned char write_name[] = "fs.write";
        const unsigned char *name = write ? write_name : read_name;
        size_t name_length = write
            ? sizeof(write_name) - 1U : sizeof(read_name) - 1U;
        status = mmay_managed_string_new(
            &execution->managed_heap, name, name_length, &capability
        );
        if (status == MMAY_MANAGED_OK) status = mmay_managed_string_new(
            &execution->managed_heap, path, path_length, &path_value
        );
        payloads[0].reference = capability;
        payloads[1].reference = path_value;
    } else if (status == MMAY_MANAGED_OK
        && (variant == 1 || variant == 2 || variant == 14 || variant == 15)) {
        payloads[0].f64_bits = (uint64_t)detail;
    }
    if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(
        &execution->managed_heap, error, variant,
        payloads, variant == 0 ? 2U
            : ((variant == 1 || variant == 2 || variant == 14 || variant == 15)
                ? 1U : 0U),
        &error_value
    );
    if (status == MMAY_MANAGED_OK) {
        payloads[0].reference = error_value;
        status = mmay_managed_enum_new(
            &execution->managed_heap, result, 1,
            payloads, 1, &target->as.managed_reference
        );
    }
    {
        MarimayManagedStatus pop = mmay_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (status == MMAY_MANAGED_OK) status = pop;
    }
    return mmay_bytecode_run_managed_status(status);
}

static MarimayBytecodeRunStatus mmay_bytecode_vm_path_error(
    MarimayBytecodeExecution *execution,
    const MarimayManagedEnumDescriptor *result,
    const MarimayManagedEnumDescriptor *error,
    size_t variant,
    int64_t limit,
    MarimayBytecodeValue *target
) {
    MarimayManagedReference error_value = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[1] = {&error_value};
    MarimayManagedRootFrame roots = {0};
    MarimayManagedCell payload = {0};
    MarimayManagedStatus status = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, 1
    );
    if (variant == 3) payload.f64_bits = (uint64_t)limit;
    if (status == MMAY_MANAGED_OK) status = mmay_managed_enum_new(
        &execution->managed_heap, error, variant,
        variant == 3 ? &payload : NULL, variant == 3 ? 1U : 0U,
        &error_value
    );
    if (status == MMAY_MANAGED_OK) {
        payload.reference = error_value;
        status = mmay_managed_enum_new(
            &execution->managed_heap, result, 1, &payload, 1,
            &target->as.managed_reference
        );
    }
    {
        MarimayManagedStatus pop = mmay_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (status == MMAY_MANAGED_OK) status = pop;
    }
    return mmay_bytecode_run_managed_status(status);
}

static MarimayBytecodeRunStatus mmay_bytecode_vm_fs_bytes_result(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    const MarimayManagedEnumDescriptor *result,
    const MarimayBytecodeEnumPayload *ok_payload,
    const MarimayOwnedBytes *bytes,
    MarimayBytecodeValue *target
) {
    const MarimayManagedArrayDescriptor *array_descriptor =
        mmay_bytecode_array_descriptor_for(
            execution, function, ok_payload->type, ok_payload->layout
        );
    MarimayManagedCell *cells = NULL;
    size_t cell_bytes = 0;
    size_t index = 0;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedStatus status;
    MarimayManagedCell payload = {0};
    if (array_descriptor == NULL || !mmay_size_multiply(
            bytes->length, sizeof(*cells), &cell_bytes
        )) return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    cells = cell_bytes == 0 ? NULL
        : mmay_allocate(&execution->allocator, cell_bytes);
    if (cell_bytes != 0 && cells == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    for (index = 0; index < bytes->length; index += 1) {
        memset(&cells[index], 0, sizeof(cells[index]));
        cells[index].f64_bits = bytes->data[index];
    }
    status = mmay_managed_array_new(
        &execution->managed_heap, array_descriptor,
        cells, bytes->length, &array
    );
    mmay_deallocate(&execution->allocator, cells, cell_bytes);
    if (status != MMAY_MANAGED_OK) {
        return mmay_bytecode_run_managed_status(status);
    }
    payload.reference = array;
    return mmay_bytecode_vm_fs_success(execution, result, &payload, target);
}

static MarimayBytecodeRunStatus mmay_bytecode_vm_fs_input_bytes(
    MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value,
    MarimayOwnedBytes *bytes
) {
    size_t length = 0;
    size_t index = 0;
    MarimayManagedStatus status = mmay_managed_array_length(
        &execution->managed_heap, value.as.managed_reference, &length
    );
    if (status != MMAY_MANAGED_OK) return mmay_bytecode_run_managed_status(status);
    bytes->allocator = execution->allocator;
    bytes->data = length == 0 ? NULL
        : mmay_allocate(&execution->allocator, length);
    if (length != 0 && bytes->data == NULL) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    bytes->length = length;
    for (index = 0; index < length; index += 1) {
        MarimayManagedCell cell = {0};
        size_t ignored = 0;
        status = mmay_managed_array_get(
            &execution->managed_heap, value.as.managed_reference,
            (int64_t)index, &cell, &ignored
        );
        if (status != MMAY_MANAGED_OK) {
            mmay_owned_bytes_dispose(bytes);
            return mmay_bytecode_run_managed_status(status);
        }
        bytes->data[index] = (unsigned char)cell.f64_bits;
    }
    return MMAY_BYTECODE_RUN_OK;
}

static MarimayBytecodeRunStatus mmay_bytecode_vm_fs_metadata_result(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    const MarimayManagedEnumDescriptor *result,
    const MarimayBytecodeEnumPayload *ok_payload,
    MarimayFsMetadata metadata,
    MarimayBytecodeValue *target
) {
    const MarimayBytecodeRecordLayout *record_layout =
        mmay_bytecode_record_layout(function, ok_payload->layout);
    const MarimayBytecodeRecordField *kind_field = record_layout != NULL
        ? mmay_bytecode_record_field(function, record_layout, 0) : NULL;
    const void *record_descriptor = NULL;
    const void *kind_descriptor = NULL;
    MarimayManagedReference kind = mmay_managed_invalid_reference();
    MarimayManagedReference record = mmay_managed_invalid_reference();
    MarimayManagedReference *locations[2] = {&kind, &record};
    MarimayManagedRootFrame roots = {0};
    MarimayManagedCell fields[2] = {{0}, {0}};
    MarimayManagedStatus managed;
    MarimayBytecodeRunStatus status;
    if (kind_field == NULL || metadata.size > INT64_MAX) {
        return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    status = mmay_bytecode_run_nominal_descriptor(
        execution, function, MMAY_BYTECODE_TYPE_RECORD,
        ok_payload->layout, &record_descriptor
    );
    if (status == MMAY_BYTECODE_RUN_OK) status =
        mmay_bytecode_run_nominal_descriptor(
            execution, function, MMAY_BYTECODE_TYPE_ENUM,
            kind_field->layout, &kind_descriptor
        );
    if (status != MMAY_BYTECODE_RUN_OK) return status;
    managed = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, 2
    );
    if (managed == MMAY_MANAGED_OK) managed = mmay_managed_enum_new(
        &execution->managed_heap, kind_descriptor,
        metadata.directory ? 1U : 0U, NULL, 0, &kind
    );
    fields[0].reference = kind;
    fields[1].f64_bits = metadata.size;
    if (managed == MMAY_MANAGED_OK) managed = mmay_managed_record_new(
        &execution->managed_heap, record_descriptor, fields, 2, &record
    );
    if (managed == MMAY_MANAGED_OK) {
        fields[0].reference = record;
        managed = mmay_managed_enum_new(
            &execution->managed_heap, result, 0,
            fields, 1, &target->as.managed_reference
        );
    }
    {
        MarimayManagedStatus pop = mmay_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (managed == MMAY_MANAGED_OK) managed = pop;
    }
    return mmay_bytecode_run_managed_status(managed);
}

static MarimayBytecodeRunStatus mmay_bytecode_vm_fs_directory_result(
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    const MarimayManagedEnumDescriptor *result,
    const MarimayBytecodeEnumPayload *ok_payload,
    const MarimayVector *entries,
    MarimayBytecodeValue *target
) {
    const MarimayBytecodeArrayType *array_layout = mmay_bytecode_array_type(
        function, ok_payload->layout
    );
    const MarimayBytecodeRecordLayout *record_layout = array_layout != NULL
        ? mmay_bytecode_record_layout(function, array_layout->element_layout)
        : NULL;
    const MarimayBytecodeRecordField *kind_field = record_layout != NULL
        ? mmay_bytecode_record_field(function, record_layout, 1) : NULL;
    const MarimayManagedArrayDescriptor *array_descriptor =
        mmay_bytecode_array_descriptor_for(
            execution, function, ok_payload->type, ok_payload->layout
        );
    const void *record_descriptor = NULL;
    const void *kind_descriptor = NULL;
    MarimayManagedCell *cells = NULL;
    MarimayManagedReference **locations = NULL;
    size_t cell_bytes = 0;
    size_t location_bytes = 0;
    size_t index = 0;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedRootFrame roots = {0};
    MarimayManagedStatus managed = MMAY_MANAGED_OK;
    MarimayBytecodeRunStatus status;
    if (array_descriptor == NULL || kind_field == NULL
        || !mmay_size_multiply(entries->length, sizeof(*cells), &cell_bytes)
        || !mmay_size_multiply(
            entries->length, sizeof(*locations), &location_bytes
        )) return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    status = mmay_bytecode_run_nominal_descriptor(
        execution, function, MMAY_BYTECODE_TYPE_RECORD,
        array_layout->element_layout, &record_descriptor
    );
    if (status == MMAY_BYTECODE_RUN_OK) status =
        mmay_bytecode_run_nominal_descriptor(
            execution, function, MMAY_BYTECODE_TYPE_ENUM,
            kind_field->layout, &kind_descriptor
        );
    if (status != MMAY_BYTECODE_RUN_OK) return status;
    cells = cell_bytes == 0 ? NULL
        : mmay_allocate(&execution->allocator, cell_bytes);
    locations = location_bytes == 0 ? NULL
        : mmay_allocate(&execution->allocator, location_bytes);
    if ((cell_bytes != 0 && cells == NULL)
        || (location_bytes != 0 && locations == NULL)) {
        mmay_deallocate(&execution->allocator, cells, cell_bytes);
        mmay_deallocate(&execution->allocator, locations, location_bytes);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    memset(cells, 0, cell_bytes);
    for (index = 0; index < entries->length; index += 1) {
        cells[index].reference = mmay_managed_invalid_reference();
        locations[index] = &cells[index].reference;
    }
    managed = mmay_managed_root_push(
        &execution->managed_heap, &roots, locations, entries->length
    );
    for (index = 0; managed == MMAY_MANAGED_OK
        && index < entries->length; index += 1) {
        const MarimayFsEntry *entry = mmay_vector_at_const(entries, index);
        MarimayManagedReference name = mmay_managed_invalid_reference();
        MarimayManagedReference kind = mmay_managed_invalid_reference();
        MarimayManagedReference record = mmay_managed_invalid_reference();
        MarimayManagedReference *temporary[3] = {&name, &kind, &record};
        MarimayManagedRootFrame temporary_roots = {0};
        MarimayManagedCell fields[2] = {{0}, {0}};
        size_t variant = entry->kind == MMAY_DIRECTORY_ENTRY_REGULAR ? 0U
            : (entry->kind == MMAY_DIRECTORY_ENTRY_DIRECTORY ? 1U
                : (entry->kind == MMAY_DIRECTORY_ENTRY_SYMLINK ? 2U : 3U));
        managed = mmay_managed_root_push(
            &execution->managed_heap, &temporary_roots, temporary, 3
        );
        if (managed == MMAY_MANAGED_OK) managed = mmay_managed_string_new(
            &execution->managed_heap, entry->name.data,
            entry->name.length, &name
        );
        if (managed == MMAY_MANAGED_OK) managed = mmay_managed_enum_new(
            &execution->managed_heap, kind_descriptor,
            variant, NULL, 0, &kind
        );
        fields[0].reference = name;
        fields[1].reference = kind;
        if (managed == MMAY_MANAGED_OK) managed = mmay_managed_record_new(
            &execution->managed_heap, record_descriptor,
            fields, 2, &record
        );
        if (managed == MMAY_MANAGED_OK) cells[index].reference = record;
        {
            MarimayManagedStatus pop = mmay_managed_root_pop(
                &execution->managed_heap, &temporary_roots
            );
            if (managed == MMAY_MANAGED_OK) managed = pop;
        }
    }
    if (managed == MMAY_MANAGED_OK) managed = mmay_managed_array_new(
        &execution->managed_heap, array_descriptor,
        cells, entries->length, &array
    );
    if (managed == MMAY_MANAGED_OK) {
        MarimayManagedCell payload = {0};
        payload.reference = array;
        managed = mmay_managed_enum_new(
            &execution->managed_heap, result, 0,
            &payload, 1, &target->as.managed_reference
        );
    }
    {
        MarimayManagedStatus pop = mmay_managed_root_pop(
            &execution->managed_heap, &roots
        );
        if (managed == MMAY_MANAGED_OK) managed = pop;
    }
    mmay_deallocate(&execution->allocator, cells, cell_bytes);
    mmay_deallocate(&execution->allocator, locations, location_bytes);
    return mmay_bytecode_run_managed_status(managed);
}

MarimayBytecodeRunStatus mmay_bytecode_run_fs_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function
) {
    size_t target_index = (size_t)instruction->operands[0].unsigned_value;
    size_t first_index = (size_t)instruction->operands[1].unsigned_value;
    size_t second_index = (size_t)instruction->operands[2].unsigned_value;
    uint32_t result_id = mmay_bytecode_register_layout(
        function, (uint32_t)target_index
    );
    const MarimayManagedEnumDescriptor *result = NULL;
    const MarimayManagedEnumDescriptor *error = NULL;
    const MarimayBytecodeEnumPayload *ok_payload = NULL;
    const unsigned char *path = NULL;
    size_t path_length = 0;
    MarimayBytecodeRunStatus status;
    MarimayFsStatus fs_status = MMAY_FS_INTERNAL_FAILED;
    bool write = instruction->opcode == MMAY_OPCODE_FS_WRITE_ATOMIC;
    int64_t detail = 0;
    if (execution == NULL || !execution->initialized) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    if (instruction->opcode == MMAY_OPCODE_PATH_NORMALIZE
        || instruction->opcode == MMAY_OPCODE_PATH_IS_ABSOLUTE
        || instruction->opcode == MMAY_OPCODE_FS_OPEN_READ
        || instruction->opcode == MMAY_OPCODE_FS_READ_ALL
        || instruction->opcode == MMAY_OPCODE_FS_WRITE_ATOMIC
        || instruction->opcode == MMAY_OPCODE_FS_METADATA
        || instruction->opcode == MMAY_OPCODE_FS_READ_DIRECTORY) {
        if (!mmay_bytecode_execution_string_bytes(
                execution, registers[first_index], &path, &path_length
            )) return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
    }
    if (instruction->opcode == MMAY_OPCODE_PATH_IS_ABSOLUTE) {
        MarimayPath normalized;
#if defined(_WIN32)
        MarimayPathFlavor flavor = MMAY_PATH_WINDOWS;
#else
        MarimayPathFlavor flavor = MMAY_PATH_POSIX;
#endif
        MarimayPathStatus path_status = mmay_path_normalize(
            &normalized, execution->allocator, flavor,
            (const char *)path, path_length
        );
        if (path_status == MMAY_PATH_ALLOCATION_FAILED) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        if (path_status != MMAY_PATH_OK) return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        registers[target_index].as.boolean = normalized.absolute;
        mmay_path_dispose(&normalized);
        return MMAY_BYTECODE_RUN_OK;
    }
    status = mmay_bytecode_vm_fs_descriptors(
        execution, function, result_id, &result, &error, &ok_payload
    );
    if (status != MMAY_BYTECODE_RUN_OK) return status;
    if (instruction->opcode == MMAY_OPCODE_PATH_NORMALIZE) {
        MarimayPath normalized;
        MarimayPathStatus path_status;
#if defined(_WIN32)
        MarimayPathFlavor flavor = MMAY_PATH_WINDOWS;
#else
        MarimayPathFlavor flavor = MMAY_PATH_POSIX;
#endif
        if (path_length > MMAY_FS_MAX_PATH_BYTES) return mmay_bytecode_vm_path_error(
            execution, result, error, 3, MMAY_FS_MAX_PATH_BYTES,
            &registers[target_index]
        );
        path_status = mmay_path_normalize(
            &normalized, execution->allocator, flavor,
            (const char *)path, path_length
        );
        if (path_status == MMAY_PATH_ALLOCATION_FAILED) {
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        }
        if (path_status != MMAY_PATH_OK) return mmay_bytecode_vm_path_error(
            execution, result, error,
            path_status == MMAY_PATH_UNSUPPORTED_PREFIX ? 1U
                : (path_status == MMAY_PATH_ESCAPES_ROOT ? 2U : 0U),
            0, &registers[target_index]
        );
        {
            MarimayManagedCell payload = {0};
            MarimayManagedStatus managed = mmay_managed_string_new(
                &execution->managed_heap, normalized.text.data,
                normalized.text.length, &payload.reference
            );
            mmay_path_dispose(&normalized);
            return managed == MMAY_MANAGED_OK
                ? mmay_bytecode_vm_fs_success(
                    execution, result, &payload, &registers[target_index]
                ) : mmay_bytecode_run_managed_status(managed);
        }
    }
    if (instruction->opcode == MMAY_OPCODE_FS_CLOSE_BEST_EFFORT) {
        if (execution->fs_initialized) mmay_fs_close_best_effort(
            &execution->fs, registers[first_index].as.i64
        );
        return MMAY_BYTECODE_RUN_OK;
    }
    if (instruction->opcode == MMAY_OPCODE_FS_READ
        || instruction->opcode == MMAY_OPCODE_FS_READ_ALL
        || instruction->opcode == MMAY_OPCODE_FS_READ_DIRECTORY) {
        detail = registers[second_index].as.i64;
    }
    if (!execution->fs_initialized) return mmay_bytecode_vm_fs_error(
        execution, result, error, MMAY_FS_CAPABILITY_DENIED,
        write, path, path_length, detail, &registers[target_index]
    );
    switch (instruction->opcode) {
        case MMAY_OPCODE_FS_OPEN_READ: {
            int64_t handle = 0;
            MarimayManagedCell payload = {0};
            fs_status = mmay_fs_open_read(
                &execution->fs, (const char *)path, path_length, &handle
            );
            if (fs_status == MMAY_FS_OK) {
                payload.f64_bits = (uint64_t)handle;
                return mmay_bytecode_vm_fs_success(
                    execution, result, &payload, &registers[target_index]
                );
            }
            break;
        }
        case MMAY_OPCODE_FS_READ:
        case MMAY_OPCODE_FS_READ_ALL: {
            MarimayOwnedBytes bytes = {0};
            fs_status = instruction->opcode == MMAY_OPCODE_FS_READ
                ? mmay_fs_read(
                    &execution->fs, registers[first_index].as.i64,
                    registers[second_index].as.i64, &bytes
                ) : mmay_fs_read_all(
                    &execution->fs, (const char *)path, path_length,
                    registers[second_index].as.i64, &bytes
                );
            if (fs_status == MMAY_FS_OK) {
                status = mmay_bytecode_vm_fs_bytes_result(
                    execution, function, result, ok_payload,
                    &bytes, &registers[target_index]
                );
                mmay_owned_bytes_dispose(&bytes);
                return status;
            }
            break;
        }
        case MMAY_OPCODE_FS_CLOSE:
            fs_status = mmay_fs_close(
                &execution->fs, registers[first_index].as.i64
            );
            if (fs_status == MMAY_FS_OK) return mmay_bytecode_vm_fs_success(
                execution, result, NULL, &registers[target_index]
            );
            break;
        case MMAY_OPCODE_FS_WRITE_ATOMIC: {
            MarimayOwnedBytes bytes = {0};
            status = mmay_bytecode_vm_fs_input_bytes(
                execution, registers[second_index], &bytes
            );
            if (status != MMAY_BYTECODE_RUN_OK) return status;
            fs_status = mmay_fs_write_atomic(
                &execution->fs, (const char *)path, path_length,
                bytes.data, bytes.length
            );
            mmay_owned_bytes_dispose(&bytes);
            if (fs_status == MMAY_FS_OK) return mmay_bytecode_vm_fs_success(
                execution, result, NULL, &registers[target_index]
            );
            break;
        }
        case MMAY_OPCODE_FS_METADATA: {
            MarimayFsMetadata metadata = {0};
            fs_status = mmay_fs_metadata(
                &execution->fs, (const char *)path, path_length, &metadata
            );
            if (fs_status == MMAY_FS_OK) return mmay_bytecode_vm_fs_metadata_result(
                execution, function, result, ok_payload,
                metadata, &registers[target_index]
            );
            break;
        }
        case MMAY_OPCODE_FS_READ_DIRECTORY: {
            MarimayVector entries;
            fs_status = mmay_fs_read_directory(
                &execution->fs, (const char *)path, path_length,
                registers[second_index].as.i64, &entries
            );
            if (fs_status == MMAY_FS_OK) {
                status = mmay_bytecode_vm_fs_directory_result(
                    execution, function, result, ok_payload,
                    &entries, &registers[target_index]
                );
                mmay_fs_entries_dispose(&entries);
                return status;
            }
            break;
        }
        default: return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return mmay_bytecode_vm_fs_error(
        execution, result, error, fs_status, write,
        path, path_length, detail, &registers[target_index]
    );
}
