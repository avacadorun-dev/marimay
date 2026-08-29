#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "mmay/platform/environment.h"

#include <string.h>

static MarimayHostEnvironmentStatus mmay_windows_environment_lookup(
    void *context,
    MarimayAllocator allocator,
    const char *name,
    size_t name_length,
    size_t max_value_bytes,
    MarimayOwnedBytes *value
) {
    wchar_t wide_name[256];
    wchar_t *wide_value = NULL;
    unsigned char *bytes = NULL;
    DWORD required = 0;
    DWORD written = 0;
    DWORD failure = ERROR_SUCCESS;
    int name_count = 0;
    int byte_count = 0;
    size_t wide_bytes = 0;
    size_t allocation_size = 0;
    (void)context;
    if (name == NULL || name_length == 0
        || name_length > 255U
        || value == NULL || max_value_bytes == 0) {
        return MMAY_HOST_ENVIRONMENT_INVALID_ARGUMENT;
    }
    memset(value, 0, sizeof(*value));
    name_count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, name, (int)name_length,
        wide_name, (int)(sizeof(wide_name) / sizeof(wide_name[0]))
    );
    if (name_count <= 0 || (size_t)name_count
            >= sizeof(wide_name) / sizeof(wide_name[0])) {
        return MMAY_HOST_ENVIRONMENT_INVALID_ARGUMENT;
    }
    wide_name[name_count] = 0;
    SetLastError(ERROR_SUCCESS);
    required = GetEnvironmentVariableW(wide_name, NULL, 0);
    failure = GetLastError();
    if (required == 0 && failure == ERROR_ENVVAR_NOT_FOUND) {
        return MMAY_HOST_ENVIRONMENT_MISSING;
    }
    if (required == 0 && failure != ERROR_SUCCESS) {
        return MMAY_HOST_ENVIRONMENT_IO_FAILED;
    }
    if (required == 0) {
        return mmay_owned_bytes_copy(value, allocator, "", 0)
            ? MMAY_HOST_ENVIRONMENT_OK
            : MMAY_HOST_ENVIRONMENT_ALLOCATION_FAILED;
    }
    if (!mmay_size_multiply((size_t)required, sizeof(wchar_t), &wide_bytes)) {
        return MMAY_HOST_ENVIRONMENT_OUTPUT_LIMIT;
    }
    wide_value = mmay_allocate(&allocator, wide_bytes);
    if (wide_value == NULL) return MMAY_HOST_ENVIRONMENT_ALLOCATION_FAILED;
    SetLastError(ERROR_SUCCESS);
    written = GetEnvironmentVariableW(wide_name, wide_value, required);
    failure = GetLastError();
    if (written == 0 && failure != ERROR_SUCCESS) {
        mmay_deallocate(&allocator, wide_value, wide_bytes);
        return failure == ERROR_ENVVAR_NOT_FOUND
            ? MMAY_HOST_ENVIRONMENT_MISSING : MMAY_HOST_ENVIRONMENT_IO_FAILED;
    }
    if (written >= required) {
        mmay_deallocate(&allocator, wide_value, wide_bytes);
        return MMAY_HOST_ENVIRONMENT_IO_FAILED;
    }
    byte_count = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, wide_value, (int)written,
        NULL, 0, NULL, NULL
    );
    if (byte_count < 0 || (byte_count == 0 && written != 0)) {
        mmay_deallocate(&allocator, wide_value, wide_bytes);
        return MMAY_HOST_ENVIRONMENT_INVALID_ENCODING;
    }
    if ((size_t)byte_count > max_value_bytes
        || !mmay_size_add((size_t)byte_count, 1, &allocation_size)) {
        mmay_deallocate(&allocator, wide_value, wide_bytes);
        return MMAY_HOST_ENVIRONMENT_OUTPUT_LIMIT;
    }
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) {
        mmay_deallocate(&allocator, wide_value, wide_bytes);
        return MMAY_HOST_ENVIRONMENT_ALLOCATION_FAILED;
    }
    if (byte_count != 0 && WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, wide_value, (int)written,
            (char *)bytes, byte_count, NULL, NULL
        ) != byte_count) {
        mmay_deallocate(&allocator, bytes, allocation_size);
        mmay_deallocate(&allocator, wide_value, wide_bytes);
        return MMAY_HOST_ENVIRONMENT_INVALID_ENCODING;
    }
    bytes[byte_count] = 0;
    mmay_deallocate(&allocator, wide_value, wide_bytes);
    value->allocator = allocator;
    value->data = bytes;
    value->length = (size_t)byte_count;
    return MMAY_HOST_ENVIRONMENT_OK;
}

MarimayHostEnvironmentOps mmay_platform_environment_ops(void) {
    MarimayHostEnvironmentOps ops;
    ops.context = NULL;
    ops.lookup = mmay_windows_environment_lookup;
    return ops;
}
