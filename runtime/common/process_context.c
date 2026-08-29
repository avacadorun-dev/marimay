#include "mmay/runtime/process_context.h"

#include <string.h>

#include "mmay/base/utf8.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/process.h"

static MarimayProcessContextSourceStatus mmay_process_context_host_current(
    void *context,
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
) {
    MarimayDirectoryOps ops = mmay_platform_directory_ops();
    MarimayFileStatus status;
    (void)context;
    if (ops.current == NULL) return MMAY_PROCESS_CONTEXT_SOURCE_UNAVAILABLE;
    status = ops.current(ops.context, allocator, max_path_bytes, path);
    if (status == MMAY_FILE_OK) return MMAY_PROCESS_CONTEXT_SOURCE_OK;
    if (status == MMAY_FILE_ALLOCATION_FAILED) {
        return MMAY_PROCESS_CONTEXT_SOURCE_ALLOCATION_FAILED;
    }
    if (status == MMAY_FILE_LIMIT_EXCEEDED) {
        return MMAY_PROCESS_CONTEXT_SOURCE_LIMIT_EXCEEDED;
    }
    if (status == MMAY_FILE_INVALID_UTF8) {
        return MMAY_PROCESS_CONTEXT_SOURCE_INVALID_ENCODING;
    }
    return MMAY_PROCESS_CONTEXT_SOURCE_UNAVAILABLE;
}

static MarimayProcessContextSourceStatus mmay_process_context_host_executable(
    void *context,
    MarimayAllocator allocator,
    size_t max_path_bytes,
    MarimayOwnedBytes *path
) {
    MarimayProcessStatus status;
    (void)context;
    status = mmay_process_executable_path(allocator, max_path_bytes, path);
    if (status == MMAY_PROCESS_OK) return MMAY_PROCESS_CONTEXT_SOURCE_OK;
    if (status == MMAY_PROCESS_ALLOCATION_FAILED) {
        return MMAY_PROCESS_CONTEXT_SOURCE_ALLOCATION_FAILED;
    }
    if (status == MMAY_PROCESS_OUTPUT_LIMIT) {
        return MMAY_PROCESS_CONTEXT_SOURCE_LIMIT_EXCEEDED;
    }
    return MMAY_PROCESS_CONTEXT_SOURCE_UNAVAILABLE;
}

MarimayProcessContextOptions mmay_process_context_default_options(void) {
    MarimayProcessContextOptions options;
    memset(&options, 0, sizeof(options));
#if defined(_WIN32)
    options.flavor = MMAY_PATH_WINDOWS;
#else
    options.flavor = MMAY_PATH_POSIX;
#endif
    options.current_directory = mmay_process_context_host_current;
    options.executable_path = mmay_process_context_host_executable;
    return options;
}

static MarimayProcessContextStatus mmay_process_context_read(
    const MarimayProcessContextOptions *requested,
    MarimayAllocator allocator,
    bool executable,
    MarimayOwnedBytes *output,
    size_t *invalid_utf8_offset
) {
    MarimayProcessContextOptions defaults;
    const MarimayProcessContextOptions *options = requested;
    MarimayProcessContextReadFn read = NULL;
    void *context = NULL;
    MarimayProcessContextSourceStatus source_status;
    MarimayOwnedBytes source;
    MarimayPath normalized;
    size_t invalid = 0;
    if (output == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return MMAY_PROCESS_CONTEXT_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    memset(&source, 0, sizeof(source));
    memset(&normalized, 0, sizeof(normalized));
    if (invalid_utf8_offset != NULL) *invalid_utf8_offset = 0;
    if (options == NULL) {
        defaults = mmay_process_context_default_options();
        options = &defaults;
    }
    read = executable ? options->executable_path : options->current_directory;
    context = executable
        ? options->executable_path_context : options->current_directory_context;
    if (read == NULL) return MMAY_PROCESS_CONTEXT_INVALID_ARGUMENT;
    source_status = read(
        context, allocator, MMAY_PROCESS_CONTEXT_MAX_PATH_BYTES, &source
    );
    if (source_status == MMAY_PROCESS_CONTEXT_SOURCE_ALLOCATION_FAILED) {
        mmay_owned_bytes_dispose(&source);
        return MMAY_PROCESS_CONTEXT_ALLOCATION_FAILED;
    }
    if (source_status == MMAY_PROCESS_CONTEXT_SOURCE_INVALID_ENCODING) {
        mmay_owned_bytes_dispose(&source);
        return MMAY_PROCESS_CONTEXT_INVALID_UTF8;
    }
    if (source_status != MMAY_PROCESS_CONTEXT_SOURCE_OK) {
        mmay_owned_bytes_dispose(&source);
        return executable
            ? MMAY_PROCESS_CONTEXT_EXECUTABLE_PATH_UNAVAILABLE
            : MMAY_PROCESS_CONTEXT_CURRENT_DIRECTORY_UNAVAILABLE;
    }
    if (source.data == NULL || source.length == 0
        || source.length > MMAY_PROCESS_CONTEXT_MAX_PATH_BYTES) {
        mmay_owned_bytes_dispose(&source);
        return MMAY_PROCESS_CONTEXT_INVALID_PATH;
    }
    if (!mmay_utf8_validate((const char *)source.data, source.length, &invalid)) {
        if (invalid_utf8_offset != NULL) *invalid_utf8_offset = invalid;
        mmay_owned_bytes_dispose(&source);
        return MMAY_PROCESS_CONTEXT_INVALID_UTF8;
    }
    switch (mmay_path_normalize(
        &normalized, allocator, options->flavor,
        (const char *)source.data, source.length
    )) {
        case MMAY_PATH_OK:
            break;
        case MMAY_PATH_ALLOCATION_FAILED:
            mmay_owned_bytes_dispose(&source);
            return MMAY_PROCESS_CONTEXT_ALLOCATION_FAILED;
        case MMAY_PATH_INVALID_UTF8:
            mmay_owned_bytes_dispose(&source);
            return MMAY_PROCESS_CONTEXT_INVALID_UTF8;
        default:
            mmay_owned_bytes_dispose(&source);
            return MMAY_PROCESS_CONTEXT_INVALID_PATH;
    }
    mmay_owned_bytes_dispose(&source);
    if (!normalized.absolute
        || normalized.text.length > MMAY_PROCESS_CONTEXT_MAX_PATH_BYTES) {
        mmay_path_dispose(&normalized);
        return MMAY_PROCESS_CONTEXT_INVALID_PATH;
    }
    *output = normalized.text;
    memset(&normalized.text, 0, sizeof(normalized.text));
    mmay_path_dispose(&normalized);
    return MMAY_PROCESS_CONTEXT_OK;
}

MarimayProcessContextStatus mmay_process_context_current_directory(
    const MarimayProcessContextOptions *options,
    MarimayAllocator allocator,
    MarimayOwnedBytes *path,
    size_t *invalid_utf8_offset
) {
    return mmay_process_context_read(
        options, allocator, false, path, invalid_utf8_offset
    );
}

MarimayProcessContextStatus mmay_process_context_executable_path(
    const MarimayProcessContextOptions *options,
    MarimayAllocator allocator,
    MarimayOwnedBytes *path,
    size_t *invalid_utf8_offset
) {
    return mmay_process_context_read(
        options, allocator, true, path, invalid_utf8_offset
    );
}
