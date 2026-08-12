#include "ruda/platform/file.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ruda/base/owned_bytes.h"

static RudaFileStatus ruda_posix_owned_path(
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaOwnedBytes *owned
) {
    if (!ruda_owned_bytes_copy(owned, allocator, path, path_length)) {
        return RUDA_FILE_ALLOCATION_FAILED;
    }
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_posix_create_exclusive(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaFileHandle *handle
) {
    RudaOwnedBytes owned = {0};
    RudaFileStatus status = RUDA_FILE_OK;
    int descriptor = -1;
    (void)context;
    status = ruda_posix_owned_path(allocator, path, path_length, &owned);
    if (status != RUDA_FILE_OK) {
        return status;
    }
    descriptor = open((const char *)owned.data, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) {
        status = errno == EEXIST ? RUDA_FILE_ALREADY_EXISTS : RUDA_FILE_IO_FAILED;
    } else {
        (void)fcntl(descriptor, F_SETFD, FD_CLOEXEC);
        *handle = (RudaFileHandle)(uintptr_t)descriptor;
    }
    ruda_owned_bytes_dispose(&owned);
    return status;
}

static RudaFileStatus ruda_posix_open_read(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length,
    RudaFileHandle *handle
) {
    RudaOwnedBytes owned = {0};
    RudaFileStatus status = RUDA_FILE_OK;
    int descriptor = -1;
    (void)context;
    status = ruda_posix_owned_path(allocator, path, path_length, &owned);
    if (status != RUDA_FILE_OK) {
        return status;
    }
    do {
        descriptor = open((const char *)owned.data, O_RDONLY);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) {
        status = RUDA_FILE_IO_FAILED;
    } else {
        (void)fcntl(descriptor, F_SETFD, FD_CLOEXEC);
        *handle = (RudaFileHandle)(uintptr_t)descriptor;
    }
    ruda_owned_bytes_dispose(&owned);
    return status;
}

static RudaFileStatus ruda_posix_read(
    void *context,
    RudaFileHandle handle,
    unsigned char *bytes,
    size_t capacity,
    size_t *read_count,
    bool *end
) {
    ssize_t result = 0;
    (void)context;
    do {
        result = read((int)handle, bytes, capacity);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        return RUDA_FILE_IO_FAILED;
    }
    *read_count = (size_t)result;
    *end = result == 0;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_posix_write(
    void *context,
    RudaFileHandle handle,
    const unsigned char *bytes,
    size_t length,
    size_t *written
) {
    size_t request = length;
    ssize_t result = 0;
    int descriptor = (int)handle;
    (void)context;
    if (request > 1073741824U) {
        request = 1073741824U;
    }
    do {
        result = write(descriptor, bytes, request);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        return RUDA_FILE_IO_FAILED;
    }
    *written = (size_t)result;
    return RUDA_FILE_OK;
}

static RudaFileStatus ruda_posix_sync(void *context, RudaFileHandle handle) {
    int result = 0;
    (void)context;
    do {
        result = fsync((int)handle);
    } while (result != 0 && errno == EINTR);
    return result == 0 ? RUDA_FILE_OK : RUDA_FILE_IO_FAILED;
}

static RudaFileStatus ruda_posix_close(void *context, RudaFileHandle handle) {
    (void)context;
    return close((int)handle) == 0 ? RUDA_FILE_OK : RUDA_FILE_IO_FAILED;
}

static RudaFileStatus ruda_posix_replace(
    void *context,
    RudaAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    RudaOwnedBytes temporary = {0};
    RudaOwnedBytes destination = {0};
    RudaFileStatus status = RUDA_FILE_OK;
    (void)context;
    status = ruda_posix_owned_path(
        allocator,
        temporary_path,
        temporary_path_length,
        &temporary
    );
    if (status != RUDA_FILE_OK) {
        return status;
    }
    status = ruda_posix_owned_path(
        allocator,
        destination_path,
        destination_path_length,
        &destination
    );
    if (status == RUDA_FILE_OK) {
        struct stat destination_status;
        if (stat((const char *)destination.data, &destination_status) == 0) {
            if (chmod(
                    (const char *)temporary.data,
                    destination_status.st_mode & 07777
                ) != 0) {
                status = RUDA_FILE_IO_FAILED;
            }
        } else if (errno != ENOENT) {
            status = RUDA_FILE_IO_FAILED;
        }
    }
    if (status == RUDA_FILE_OK
        && rename((const char *)temporary.data, (const char *)destination.data) != 0) {
        status = RUDA_FILE_IO_FAILED;
    }
    ruda_owned_bytes_dispose(&destination);
    ruda_owned_bytes_dispose(&temporary);
    return status;
}

static void ruda_posix_remove(
    void *context,
    RudaAllocator allocator,
    const char *path,
    size_t path_length
) {
    RudaOwnedBytes owned = {0};
    (void)context;
    if (ruda_posix_owned_path(allocator, path, path_length, &owned) == RUDA_FILE_OK) {
        (void)unlink((const char *)owned.data);
        ruda_owned_bytes_dispose(&owned);
    }
}

RudaFileOps ruda_platform_file_ops(void) {
    RudaFileOps ops;
    ops.context = NULL;
    ops.create_exclusive = ruda_posix_create_exclusive;
    ops.write = ruda_posix_write;
    ops.sync = ruda_posix_sync;
    ops.close = ruda_posix_close;
    ops.replace = ruda_posix_replace;
    ops.remove = ruda_posix_remove;
    return ops;
}

RudaFileReadOps ruda_platform_file_read_ops(void) {
    RudaFileReadOps ops;
    ops.context = NULL;
    ops.open = ruda_posix_open_read;
    ops.read = ruda_posix_read;
    ops.close = ruda_posix_close;
    return ops;
}
