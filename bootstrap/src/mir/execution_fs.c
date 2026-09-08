#include "mir_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/path.h"

static const MarimayMirEnum *mmay_mir_fs_enum(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->enums.length; index += 1) {
        const MarimayMirEnum *layout = mmay_mir_enum(
            function, (MarimayMirEnumId)index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static const MarimayMirRecord *mmay_mir_fs_record(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->records.length; index += 1) {
        const MarimayMirRecord *layout = mmay_mir_record(
            function, (MarimayMirRecordId)index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static const MarimayMirArray *mmay_mir_fs_array(
    const MarimayMirFunction *function,
    MarimayTypeId type
) {
    size_t index = 0;
    for (index = 0; index < function->arrays.length; index += 1) {
        const MarimayMirArray *layout = mmay_mir_array(
            function, (MarimayMirArrayId)index
        );
        if (layout != NULL && layout->type == type) return layout;
    }
    return NULL;
}

static const MarimayMirEnumPayload *mmay_mir_fs_result_payload(
    const MarimayMirFunction *function,
    const MarimayMirEnum *result,
    size_t variant
) {
    const MarimayMirEnumVariant *item = mmay_mir_enum_variant(
        function, result, variant
    );
    return item != NULL && item->payload_count == 1
        ? mmay_mir_enum_payload(function, item, 0) : NULL;
}

static MarimayMirInterpretStatus mmay_mir_fs_status_error(
    const MarimayMirFunction *function,
    MarimayMirInterpretOptions options,
    const MarimayMirEnum *result,
    MarimayFsStatus fs_status,
    bool write,
    const unsigned char *path,
    size_t path_length,
    int64_t detail,
    MarimayMirValue *target
) {
    const MarimayMirEnumPayload *error_payload = mmay_mir_fs_result_payload(
        function, result, 1
    );
    const MarimayMirEnum *error = error_payload != NULL
        ? mmay_mir_fs_enum(function, error_payload->type) : NULL;
    MarimayMirValue payloads[2];
    MarimayMirValue error_value;
    size_t variant = 16;
    MarimayMirInterpretStatus status;
    memset(payloads, 0, sizeof(payloads));
    memset(&error_value, 0, sizeof(error_value));
    if (fs_status == MMAY_FS_ALLOCATION_FAILED) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    if (fs_status == MMAY_FS_INTERNAL_FAILED || error == NULL) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
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
        case MMAY_FS_INVALID_ARGUMENT:
        case MMAY_FS_OK:
        case MMAY_FS_ALLOCATION_FAILED:
        case MMAY_FS_INTERNAL_FAILED:
            return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    if (variant == 0) {
        static const unsigned char read_name[] = "fs.read";
        static const unsigned char write_name[] = "fs.write";
        status = mmay_mir_execution_new_string(
            options.execution,
            write ? write_name : read_name,
            write ? sizeof(write_name) - 1U : sizeof(read_name) - 1U,
            &payloads[0].as.reference
        );
        payloads[0].type = MMAY_TYPE_STRING;
        if (status == MMAY_MIR_INTERPRET_OK) {
            status = mmay_mir_execution_new_string(
                options.execution, path, path_length,
                &payloads[1].as.reference
            );
            payloads[1].type = MMAY_TYPE_STRING;
        }
        if (status != MMAY_MIR_INTERPRET_OK) return status;
    } else if (variant == 1 || variant == 2
        || variant == 14 || variant == 15) {
        payloads[0].type = MMAY_TYPE_I64;
        payloads[0].as.i64 = detail;
    }
    error_value.type = error->type;
    status = mmay_mir_execution_new_enum(
        options.execution, function, error, variant,
        payloads, variant == 0 ? 2U
            : ((variant == 1 || variant == 2 || variant == 14 || variant == 15)
                ? 1U : 0U),
        &error_value.as.reference
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    payloads[0] = error_value;
    return mmay_mir_execution_new_enum(
        options.execution, function, result, 1,
        payloads, 1, &target->as.reference
    );
}

static MarimayMirInterpretStatus mmay_mir_fs_success(
    const MarimayMirFunction *function,
    MarimayMirInterpretOptions options,
    const MarimayMirEnum *result,
    const MarimayMirValue *value,
    MarimayMirValue *target
) {
    return mmay_mir_execution_new_enum(
        options.execution, function, result, 0,
        value, value != NULL ? 1U : 0U, &target->as.reference
    );
}

static MarimayMirInterpretStatus mmay_mir_path_error(
    const MarimayMirFunction *function,
    MarimayMirInterpretOptions options,
    const MarimayMirEnum *result,
    size_t variant,
    int64_t limit,
    MarimayMirValue *target
) {
    const MarimayMirEnumPayload *error_payload = mmay_mir_fs_result_payload(
        function, result, 1
    );
    const MarimayMirEnum *error = error_payload != NULL
        ? mmay_mir_fs_enum(function, error_payload->type) : NULL;
    MarimayMirValue payload;
    MarimayMirValue error_value;
    MarimayMirInterpretStatus status;
    if (error == NULL || variant >= 4U) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    memset(&payload, 0, sizeof(payload));
    memset(&error_value, 0, sizeof(error_value));
    payload.type = MMAY_TYPE_I64;
    payload.as.i64 = limit;
    error_value.type = error->type;
    status = mmay_mir_execution_new_enum(
        options.execution, function, error, variant,
        variant == 3U ? &payload : NULL, variant == 3U ? 1U : 0U,
        &error_value.as.reference
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    return mmay_mir_execution_new_enum(
        options.execution, function, result, 1,
        &error_value, 1, &target->as.reference
    );
}

static MarimayMirInterpretStatus mmay_mir_fs_bytes(
    const MarimayMirFunction *function,
    MarimayMirInterpretOptions options,
    const MarimayMirEnum *result,
    const MarimayOwnedBytes *bytes,
    MarimayMirValue *target
) {
    const MarimayMirEnumPayload *payload = mmay_mir_fs_result_payload(
        function, result, 0
    );
    const MarimayMirArray *array = payload != NULL
        ? mmay_mir_fs_array(function, payload->type) : NULL;
    MarimayMirValue *values = NULL;
    MarimayMirValue array_value;
    size_t allocation = 0;
    size_t index = 0;
    MarimayMirInterpretStatus status;
    if (array == NULL || array->element_type != MMAY_TYPE_U8
        || !mmay_size_multiply(bytes->length, sizeof(*values), &allocation)) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    values = allocation == 0 ? NULL
        : mmay_allocate(&options.execution->allocator, allocation);
    if (allocation != 0 && values == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    for (index = 0; index < bytes->length; index += 1) {
        memset(&values[index], 0, sizeof(values[index]));
        values[index].type = MMAY_TYPE_U8;
        values[index].as.integer_bits = bytes->data[index];
    }
    memset(&array_value, 0, sizeof(array_value));
    array_value.type = array->type;
    status = mmay_mir_execution_new_array(
        options.execution, array, values, bytes->length,
        &array_value.as.reference
    );
    mmay_deallocate(&options.execution->allocator, values, allocation);
    return status == MMAY_MIR_INTERPRET_OK
        ? mmay_mir_fs_success(function, options, result, &array_value, target)
        : status;
}

static MarimayMirInterpretStatus mmay_mir_fs_array_bytes(
    MarimayMirExecution *execution,
    const MarimayMirValue *array,
    MarimayAllocator allocator,
    MarimayOwnedBytes *bytes
) {
    size_t count = 0;
    size_t allocation = 0;
    size_t index = 0;
    if (!mmay_mir_execution_array_length(execution, *array, &count)
        || !mmay_size_add(count, 1U, &allocation)) {
        return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    bytes->allocator = allocator;
    bytes->data = mmay_allocate(&allocator, allocation);
    if (bytes->data == NULL) return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    bytes->length = count;
    for (index = 0; index < count; index += 1) {
        MarimayMirValue value;
        if (!mmay_mir_execution_array_element(
                execution, *array, index, &value
            ) || value.type != MMAY_TYPE_U8) {
            mmay_owned_bytes_dispose(bytes);
            return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
        }
        bytes->data[index] = (unsigned char)value.as.integer_bits;
    }
    bytes->data[count] = 0;
    return MMAY_MIR_INTERPRET_OK;
}

static MarimayMirInterpretStatus mmay_mir_fs_metadata_value(
    const MarimayMirFunction *function,
    MarimayMirInterpretOptions options,
    const MarimayMirEnum *result,
    MarimayFsMetadata metadata,
    MarimayMirValue *target
) {
    const MarimayMirEnumPayload *payload = mmay_mir_fs_result_payload(
        function, result, 0
    );
    const MarimayMirRecord *record = payload != NULL
        ? mmay_mir_fs_record(function, payload->type) : NULL;
    const MarimayMirRecordField *kind_field = record != NULL
        ? mmay_mir_record_field(function, record, 0) : NULL;
    const MarimayMirRecordField *size_field = record != NULL
        ? mmay_mir_record_field(function, record, 1) : NULL;
    const MarimayMirEnum *kind = kind_field != NULL
        ? mmay_mir_fs_enum(function, kind_field->type) : NULL;
    MarimayMirValue fields[2];
    MarimayMirValue value;
    MarimayMirRecordField layout[2];
    MarimayMirInterpretStatus status;
    if (record == NULL || record->field_count != 2 || kind == NULL
        || kind->variant_count != 2 || size_field == NULL
        || size_field->type != MMAY_TYPE_I64 || metadata.size > INT64_MAX) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    memset(fields, 0, sizeof(fields));
    fields[0].type = kind->type;
    status = mmay_mir_execution_new_enum(
        options.execution, function, kind, metadata.directory ? 1U : 0U,
        NULL, 0, &fields[0].as.reference
    );
    if (status != MMAY_MIR_INTERPRET_OK) return status;
    fields[1].type = MMAY_TYPE_I64;
    fields[1].as.i64 = (int64_t)metadata.size;
    layout[0] = *kind_field;
    layout[1] = *size_field;
    memset(&value, 0, sizeof(value));
    value.type = record->type;
    status = mmay_mir_execution_new_record(
        options.execution, record->type, fields, layout, 2,
        &value.as.reference
    );
    return status == MMAY_MIR_INTERPRET_OK
        ? mmay_mir_fs_success(function, options, result, &value, target)
        : status;
}

static MarimayMirInterpretStatus mmay_mir_fs_directory_value(
    const MarimayMirFunction *function,
    MarimayMirInterpretOptions options,
    const MarimayMirEnum *result,
    const MarimayVector *entries,
    MarimayMirValue *target
) {
    const MarimayMirEnumPayload *payload = mmay_mir_fs_result_payload(
        function, result, 0
    );
    const MarimayMirArray *array = payload != NULL
        ? mmay_mir_fs_array(function, payload->type) : NULL;
    const MarimayMirRecord *record = array != NULL
        ? mmay_mir_fs_record(function, array->element_type) : NULL;
    const MarimayMirRecordField *name_field = record != NULL
        ? mmay_mir_record_field(function, record, 0) : NULL;
    const MarimayMirRecordField *kind_field = record != NULL
        ? mmay_mir_record_field(function, record, 1) : NULL;
    const MarimayMirEnum *kind = kind_field != NULL
        ? mmay_mir_fs_enum(function, kind_field->type) : NULL;
    MarimayMirValue *values = NULL;
    size_t allocation = 0;
    size_t index = 0;
    MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_OK;
    if (array == NULL || record == NULL || record->field_count != 2
        || name_field == NULL || name_field->type != MMAY_TYPE_STRING
        || kind == NULL || kind->variant_count != 4
        || !mmay_size_multiply(entries->length, sizeof(*values), &allocation)) {
        return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    values = allocation == 0 ? NULL
        : mmay_allocate(&options.execution->allocator, allocation);
    if (allocation != 0 && values == NULL) {
        return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    memset(values, 0, allocation);
    for (index = 0; index < entries->length; index += 1) {
        const MarimayFsEntry *entry = mmay_vector_at_const(entries, index);
        MarimayMirValue fields[2];
        MarimayMirRecordField layout[2] = {*name_field, *kind_field};
        size_t variant = entry->kind == MMAY_DIRECTORY_ENTRY_REGULAR ? 0U
            : (entry->kind == MMAY_DIRECTORY_ENTRY_DIRECTORY ? 1U
                : (entry->kind == MMAY_DIRECTORY_ENTRY_SYMLINK ? 2U : 3U));
        memset(fields, 0, sizeof(fields));
        fields[0].type = MMAY_TYPE_STRING;
        status = mmay_mir_execution_new_string(
            options.execution, entry->name.data, entry->name.length,
            &fields[0].as.reference
        );
        fields[1].type = kind->type;
        if (status == MMAY_MIR_INTERPRET_OK) status = mmay_mir_execution_new_enum(
            options.execution, function, kind, variant, NULL, 0,
            &fields[1].as.reference
        );
        values[index].type = record->type;
        if (status == MMAY_MIR_INTERPRET_OK) status = mmay_mir_execution_new_record(
            options.execution, record->type, fields, layout, 2,
            &values[index].as.reference
        );
        if (status != MMAY_MIR_INTERPRET_OK) break;
    }
    if (status == MMAY_MIR_INTERPRET_OK) {
        MarimayMirValue value;
        memset(&value, 0, sizeof(value));
        value.type = array->type;
        status = mmay_mir_execution_new_array(
            options.execution, array, values, entries->length,
            &value.as.reference
        );
        if (status == MMAY_MIR_INTERPRET_OK) status = mmay_mir_fs_success(
            function, options, result, &value, target
        );
    }
    mmay_deallocate(&options.execution->allocator, values, allocation);
    return status;
}

static MarimayMirInterpretStatus mmay_mir_fs_line(
    const MarimayMirFunction *function, MarimayMirInterpretOptions options,
    const MarimayMirEnum *result, int64_t handle, int64_t limit, MarimayMirValue *target
) {
    MarimayOwnedBytes line = {0};
    bool eof = false;
    size_t invalid = 0;
    MarimayFsStatus fs_status = mmay_fs_read_line(&options.execution->fs,
        handle, limit, &line, &eof, &invalid);
    if (fs_status != MMAY_FS_OK) return mmay_mir_fs_status_error(function, options,
        result, fs_status, false, NULL, 0, fs_status == MMAY_FS_INVALID_UTF8 ? (int64_t)invalid : limit, target);
    const MarimayMirEnumPayload *payload = mmay_mir_fs_result_payload(function, result, 0);
    const MarimayMirEnum *option = payload != NULL ? mmay_mir_fs_enum(function, payload->type) : NULL;
    MarimayMirValue text = {0}, value = {0};
    MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_INVALID_MIR;
    if (option != NULL) {
        text.type = MMAY_TYPE_STRING; value.type = option->type;
        status = eof ? MMAY_MIR_INTERPRET_OK : mmay_mir_execution_new_string(
            options.execution, line.data, line.length, &text.as.reference);
        if (status == MMAY_MIR_INTERPRET_OK) status = mmay_mir_execution_new_enum(
            options.execution, function, option, eof ? 1 : 0, eof ? NULL : &text,
            eof ? 0 : 1, &value.as.reference);
        if (status == MMAY_MIR_INTERPRET_OK) status = mmay_mir_fs_success(function, options, result, &value, target);
    }
    mmay_owned_bytes_dispose(&line);
    return status;
}

MarimayMirInterpretStatus mmay_mir_execute_fs_instruction(
    const MarimayMirFunction *function,
    const MarimayMirInstruction *instruction,
    const MarimayMirValue *first,
    const MarimayMirValue *second,
    MarimayMirValue *target,
    const MarimayAllocator *allocator,
    MarimayMirInterpretOptions options
) {
    const unsigned char *path = NULL;
    size_t path_length = 0;
    const MarimayMirEnum *result = NULL;
    MarimayFsStatus fs_status = MMAY_FS_INTERNAL_FAILED;
    int64_t detail = second != NULL ? second->as.i64 : 0;
    bool write = instruction->fs_operation == MMAY_MIR_FS_WRITE_ATOMIC;
    if (options.execution == NULL || !options.execution->initialized) {
        return MMAY_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (instruction->fs_operation == MMAY_MIR_PATH_NORMALIZE
        || instruction->fs_operation == MMAY_MIR_PATH_IS_ABSOLUTE
        || instruction->fs_operation == MMAY_MIR_FS_OPEN_READ
        || instruction->fs_operation == MMAY_MIR_FS_READ_ALL
        || instruction->fs_operation == MMAY_MIR_FS_WRITE_ATOMIC
        || instruction->fs_operation == MMAY_MIR_FS_METADATA
        || instruction->fs_operation == MMAY_MIR_FS_READ_DIRECTORY) {
        if (!mmay_mir_execution_string_bytes(
                options.execution, *first, &path, &path_length
            )) return MMAY_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (instruction->fs_operation == MMAY_MIR_PATH_IS_ABSOLUTE) {
        MarimayPath normalized;
#if defined(_WIN32)
        MarimayPathFlavor flavor = MMAY_PATH_WINDOWS;
#else
        MarimayPathFlavor flavor = MMAY_PATH_POSIX;
#endif
        MarimayPathStatus path_status = mmay_path_normalize(
            &normalized, *allocator, flavor,
            (const char *)path, path_length
        );
        if (path_status != MMAY_PATH_OK) {
            return path_status == MMAY_PATH_ALLOCATION_FAILED
                ? MMAY_MIR_INTERPRET_ALLOCATION_FAILED
                : MMAY_MIR_INTERPRET_INVALID_MIR;
        }
        target->as.boolean = normalized.absolute;
        mmay_path_dispose(&normalized);
        return MMAY_MIR_INTERPRET_OK;
    }
    if (instruction->fs_operation == MMAY_MIR_FS_CLOSE_BEST_EFFORT) {
        if (options.execution->fs_initialized)
            mmay_fs_close_best_effort(&options.execution->fs, first->as.i64);
        return MMAY_MIR_INTERPRET_OK;
    }
    result = mmay_mir_fs_enum(
        function, mmay_mir_register_type(function, instruction->target)
    );
    if (result == NULL) return MMAY_MIR_INTERPRET_INVALID_MIR;
    if (instruction->fs_operation == MMAY_MIR_PATH_NORMALIZE) {
        MarimayPath normalized;
        MarimayPathStatus path_status;
        MarimayMirValue value;
#if defined(_WIN32)
        MarimayPathFlavor flavor = MMAY_PATH_WINDOWS;
#else
        MarimayPathFlavor flavor = MMAY_PATH_POSIX;
#endif
        if (path_length > MMAY_FS_MAX_PATH_BYTES) {
            return mmay_mir_path_error(
                function, options, result, 3,
                MMAY_FS_MAX_PATH_BYTES, target
            );
        }
        path_status = mmay_path_normalize(
            &normalized, *allocator, flavor,
            (const char *)path, path_length
        );
        if (path_status == MMAY_PATH_ALLOCATION_FAILED) {
            return MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        }
        if (path_status != MMAY_PATH_OK) {
            size_t variant = path_status == MMAY_PATH_UNSUPPORTED_PREFIX ? 1U
                : (path_status == MMAY_PATH_ESCAPES_ROOT ? 2U : 0U);
            return mmay_mir_path_error(
                function, options, result, variant, 0, target
            );
        }
        memset(&value, 0, sizeof(value));
        value.type = MMAY_TYPE_STRING;
        {
            MarimayMirInterpretStatus status = mmay_mir_execution_new_string(
                options.execution, normalized.text.data,
                normalized.text.length, &value.as.reference
            );
            mmay_path_dispose(&normalized);
            return status == MMAY_MIR_INTERPRET_OK
                ? mmay_mir_fs_success(function, options, result, &value, target)
                : status;
        }
    }
    if (!options.execution->fs_initialized) {
        return mmay_mir_fs_status_error(
            function, options, result, MMAY_FS_CAPABILITY_DENIED,
            write, path, path_length, detail, target
        );
    }
    switch (instruction->fs_operation) {
        case MMAY_MIR_FS_OPEN_READ: {
            int64_t handle = 0;
            MarimayMirValue value;
            fs_status = mmay_fs_open_read(
                &options.execution->fs, (const char *)path, path_length, &handle
            );
            if (fs_status == MMAY_FS_OK) {
                memset(&value, 0, sizeof(value));
                value.type = MMAY_TYPE_I64;
                value.as.i64 = handle;
                return mmay_mir_fs_success(
                    function, options, result, &value, target
                );
            }
            break;
        }
        case MMAY_MIR_FS_READ_LINE:
            return mmay_mir_fs_line(function, options, result, first->as.i64, second->as.i64, target);
        case MMAY_MIR_FS_READ:
        case MMAY_MIR_FS_READ_ALL: {
            MarimayOwnedBytes bytes = {0};
            fs_status = instruction->fs_operation == MMAY_MIR_FS_READ
                ? mmay_fs_read(
                    &options.execution->fs, first->as.i64, second->as.i64, &bytes
                ) : mmay_fs_read_all(
                    &options.execution->fs, (const char *)path, path_length,
                    second->as.i64, &bytes
                );
            if (fs_status == MMAY_FS_OK) {
                MarimayMirInterpretStatus status = mmay_mir_fs_bytes(
                    function, options, result, &bytes, target
                );
                mmay_owned_bytes_dispose(&bytes);
                return status;
            }
            break;
        }
        case MMAY_MIR_FS_CLOSE:
            fs_status = mmay_fs_close(&options.execution->fs, first->as.i64);
            if (fs_status == MMAY_FS_OK) {
                return mmay_mir_fs_success(function, options, result, NULL, target);
            }
            break;
        case MMAY_MIR_FS_WRITE_ATOMIC: {
            MarimayOwnedBytes bytes = {0};
            MarimayMirInterpretStatus status = mmay_mir_fs_array_bytes(
                options.execution, second, *allocator, &bytes
            );
            if (status != MMAY_MIR_INTERPRET_OK) return status;
            fs_status = mmay_fs_write_atomic(
                &options.execution->fs, (const char *)path, path_length,
                bytes.data, bytes.length
            );
            mmay_owned_bytes_dispose(&bytes);
            if (fs_status == MMAY_FS_OK) return mmay_mir_fs_success(
                function, options, result, NULL, target
            );
            break;
        }
        case MMAY_MIR_FS_METADATA: {
            MarimayFsMetadata metadata;
            fs_status = mmay_fs_metadata(
                &options.execution->fs, (const char *)path, path_length, &metadata
            );
            if (fs_status == MMAY_FS_OK) return mmay_mir_fs_metadata_value(
                function, options, result, metadata, target
            );
            break;
        }
        case MMAY_MIR_FS_READ_DIRECTORY: {
            MarimayVector entries;
            fs_status = mmay_fs_read_directory(
                &options.execution->fs, (const char *)path, path_length,
                second->as.i64, &entries
            );
            if (fs_status == MMAY_FS_OK) {
                MarimayMirInterpretStatus status = mmay_mir_fs_directory_value(
                    function, options, result, &entries, target
                );
                mmay_fs_entries_dispose(&entries);
                return status;
            }
            break;
        }
        case MMAY_MIR_FS_CLOSE_BEST_EFFORT:
        case MMAY_MIR_PATH_NORMALIZE:
        case MMAY_MIR_PATH_IS_ABSOLUTE:
            return MMAY_MIR_INTERPRET_INVALID_MIR;
    }
    return mmay_mir_fs_status_error(
        function, options, result, fs_status, write,
        path, path_length, detail, target
    );
}
