#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mmay/runtime/program_arguments.h"

#include <limits.h>
#include <string.h>

static bool mmay_host_argument_wide_length(
    const wchar_t *value,
    size_t limit,
    size_t *result
) {
    size_t length = 0;
    if (value == NULL || result == NULL) return false;
    while (length <= limit && value[length] != 0) length += 1;
    if (length > limit) return false;
    *result = length;
    return true;
}

MarimayProgramArgumentsStatus mmay_host_arguments_import(
    MarimayAllocator allocator,
    const MarimayHostArgumentChar *const values[],
    size_t count,
    size_t max_count,
    size_t max_total_bytes,
    MarimayHostArguments *result
) {
    size_t pointer_bytes = 0;
    size_t pointer_count = 0;
    size_t view_bytes = 0;
    size_t string_bytes = 0;
    size_t payload_bytes = 0;
    size_t allocation_size = 0;
    size_t index = 0;
    unsigned char *allocation = NULL;
    unsigned char *cursor = NULL;
    if (result == NULL) return MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if ((values == NULL && count != 0) || count > max_count
        || max_count == 0 || max_total_bytes == 0) {
        return count > max_count
            ? MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT
            : MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT;
    }
    for (index = 0; index < count; index += 1) {
        size_t wide_length = 0;
        size_t scan_limit = max_total_bytes
            < MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES
                ? max_total_bytes : MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES;
        int length = 0;
        if (!mmay_host_argument_wide_length(
                values[index], scan_limit, &wide_length
            ) || wide_length > (size_t)INT_MAX) {
            return values[index] == NULL
                ? MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT
                : MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT;
        }
        length = wide_length == 0 ? 0 : WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, values[index], (int)wide_length,
            NULL, 0, NULL, NULL
        );
        if ((wide_length != 0 && length <= 0)
            || (size_t)length > MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES
            || !mmay_size_add(
                payload_bytes, (size_t)length, &payload_bytes
            ) || payload_bytes > max_total_bytes
            || !mmay_size_add(
                string_bytes, (size_t)length + 1, &string_bytes
            )) {
            return wide_length != 0 && length <= 0
                ? MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8
                : MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT;
        }
    }
    if (!mmay_size_add(count, 1, &pointer_count)
        || !mmay_size_multiply(pointer_count, sizeof(char *), &pointer_bytes)
        || !mmay_size_multiply(count, sizeof(*result->views), &view_bytes)
        || !mmay_size_add(pointer_bytes, view_bytes, &allocation_size)
        || !mmay_size_add(allocation_size, string_bytes, &allocation_size)) {
        return MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT;
    }
    allocation = mmay_allocate(&allocator, allocation_size);
    if (allocation == NULL) return MMAY_PROGRAM_ARGUMENTS_ALLOCATION_FAILED;
    result->allocator = allocator;
    result->values = (char **)allocation;
    result->views = (MarimayProgramArgument *)(allocation + pointer_bytes);
    result->count = count;
    result->allocation_size = allocation_size;
    cursor = allocation + pointer_bytes + view_bytes;
    for (index = 0; index < count; index += 1) {
        size_t wide_length = 0;
        int length = 0;
        int converted = 0;
        if (!mmay_host_argument_wide_length(
                values[index], MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES,
                &wide_length
            ) || wide_length > (size_t)INT_MAX) {
            mmay_host_arguments_dispose(result);
            return MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT;
        }
        length = wide_length == 0 ? 0 : WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, values[index], (int)wide_length,
            NULL, 0, NULL, NULL
        );
        if (wide_length != 0 && length <= 0) {
            mmay_host_arguments_dispose(result);
            return MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8;
        }
        converted = wide_length == 0 ? 0 : WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, values[index], (int)wide_length,
            (char *)cursor, length,
            NULL, NULL
        );
        if (converted != length) {
            mmay_host_arguments_dispose(result);
            return MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8;
        }
        result->values[index] = (char *)cursor;
        result->views[index].data = cursor;
        result->views[index].length = (size_t)length;
        cursor[length] = 0;
        cursor += (size_t)length + 1;
    }
    result->values[count] = NULL;
    for (index = 0; index < count; index += 1) {
        MarimayProgramArgumentsStatus status =
            mmay_program_arguments_validate((MarimayProgramArgumentList){
                &result->views[index], 1
            });
        if (status != MMAY_PROGRAM_ARGUMENTS_OK) {
            mmay_host_arguments_dispose(result);
            return status;
        }
    }
    return MMAY_PROGRAM_ARGUMENTS_OK;
}

void mmay_host_arguments_dispose(MarimayHostArguments *arguments) {
    if (arguments == NULL) return;
    mmay_deallocate(
        &arguments->allocator, arguments->values, arguments->allocation_size
    );
    memset(arguments, 0, sizeof(*arguments));
}
