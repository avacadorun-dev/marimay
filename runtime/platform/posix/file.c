#define _POSIX_C_SOURCE 200809L

#include "mmay/platform/file.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mmay/base/owned_bytes.h"
#include "mmay/base/utf8.h"

static MarimayFileStatus mmay_posix_owned_path(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayOwnedBytes *owned
) {
    if (!mmay_owned_bytes_copy(owned, allocator, path, path_length)) {
        return MMAY_FILE_ALLOCATION_FAILED;
    }
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_posix_create_exclusive(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileHandle *handle
) {
    MarimayOwnedBytes owned = {0};
    MarimayFileStatus status = MMAY_FILE_OK;
    int descriptor = -1;
    (void)context;
    status = mmay_posix_owned_path(allocator, path, path_length, &owned);
    if (status != MMAY_FILE_OK) {
        return status;
    }
    descriptor = open((const char *)owned.data, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (descriptor < 0) {
        status = errno == EEXIST ? MMAY_FILE_ALREADY_EXISTS : MMAY_FILE_IO_FAILED;
    } else {
        (void)fcntl(descriptor, F_SETFD, FD_CLOEXEC);
        *handle = (MarimayFileHandle)(uintptr_t)descriptor;
    }
    mmay_owned_bytes_dispose(&owned);
    return status;
}

static MarimayFileStatus mmay_posix_open_read(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileHandle *handle
) {
    MarimayOwnedBytes owned = {0};
    MarimayFileStatus status = MMAY_FILE_OK;
    int descriptor = -1;
    (void)context;
    status = mmay_posix_owned_path(allocator, path, path_length, &owned);
    if (status != MMAY_FILE_OK) {
        return status;
    }
    do {
        descriptor = open((const char *)owned.data, O_RDONLY);
    } while (descriptor < 0 && errno == EINTR);
    if (descriptor < 0) {
        status = MMAY_FILE_IO_FAILED;
    } else {
        (void)fcntl(descriptor, F_SETFD, FD_CLOEXEC);
        *handle = (MarimayFileHandle)(uintptr_t)descriptor;
    }
    mmay_owned_bytes_dispose(&owned);
    return status;
}

static MarimayFileStatus mmay_posix_read(
    void *context,
    MarimayFileHandle handle,
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
        return MMAY_FILE_IO_FAILED;
    }
    *read_count = (size_t)result;
    *end = result == 0;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_posix_write(
    void *context,
    MarimayFileHandle handle,
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
        return MMAY_FILE_IO_FAILED;
    }
    *written = (size_t)result;
    return MMAY_FILE_OK;
}

static MarimayFileStatus mmay_posix_sync(void *context, MarimayFileHandle handle) {
    int result = 0;
    (void)context;
    do {
        result = fsync((int)handle);
    } while (result != 0 && errno == EINTR);
    return result == 0 ? MMAY_FILE_OK : MMAY_FILE_IO_FAILED;
}

static MarimayFileStatus mmay_posix_close(void *context, MarimayFileHandle handle) {
    (void)context;
    return close((int)handle) == 0 ? MMAY_FILE_OK : MMAY_FILE_IO_FAILED;
}

static MarimayFileStatus mmay_posix_replace(
    void *context,
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    MarimayOwnedBytes temporary = {0};
    MarimayOwnedBytes destination = {0};
    MarimayFileStatus status = MMAY_FILE_OK;
    (void)context;
    status = mmay_posix_owned_path(
        allocator,
        temporary_path,
        temporary_path_length,
        &temporary
    );
    if (status != MMAY_FILE_OK) {
        return status;
    }
    status = mmay_posix_owned_path(
        allocator,
        destination_path,
        destination_path_length,
        &destination
    );
    if (status == MMAY_FILE_OK) {
        struct stat destination_status;
        if (stat((const char *)destination.data, &destination_status) == 0) {
            if (chmod(
                    (const char *)temporary.data,
                    destination_status.st_mode & 07777
                ) != 0) {
                status = MMAY_FILE_IO_FAILED;
            }
        } else if (errno != ENOENT) {
            status = MMAY_FILE_IO_FAILED;
        }
    }
    if (status == MMAY_FILE_OK
        && rename((const char *)temporary.data, (const char *)destination.data) != 0) {
        status = MMAY_FILE_IO_FAILED;
    }
    mmay_owned_bytes_dispose(&destination);
    mmay_owned_bytes_dispose(&temporary);
    return status;
}

static void mmay_posix_remove(
    void *context,
    MarimayAllocator allocator,
    const char *path,
    size_t path_length
) {
    MarimayOwnedBytes owned = {0};
    (void)context;
    if (mmay_posix_owned_path(allocator, path, path_length, &owned) == MMAY_FILE_OK) {
        (void)unlink((const char *)owned.data);
        mmay_owned_bytes_dispose(&owned);
    }
}

MarimayFileOps mmay_platform_file_ops(void) {
    MarimayFileOps ops;
    ops.context = NULL;
    ops.create_exclusive = mmay_posix_create_exclusive;
    ops.write = mmay_posix_write;
    ops.sync = mmay_posix_sync;
    ops.close = mmay_posix_close;
    ops.replace = mmay_posix_replace;
    ops.remove = mmay_posix_remove;
    return ops;
}

MarimayFileReadOps mmay_platform_file_read_ops(void) {
    MarimayFileReadOps ops;
    ops.context = NULL;
    ops.open = mmay_posix_open_read;
    ops.read = mmay_posix_read;
    ops.close = mmay_posix_close;
    return ops;
}

MarimayFileStatus mmay_file_metadata(
    MarimayAllocator allocator,
    const char *path,
    size_t path_length,
    MarimayFileMetadata *metadata
) {
    MarimayOwnedBytes owned = {0};
    struct stat information;
    MarimayFileStatus status;
    if (metadata == NULL || path == NULL || path_length == 0
        || memchr(path, 0, path_length) != NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    memset(metadata, 0, sizeof(*metadata));
    if (!mmay_utf8_validate(path, path_length, NULL)) {
        return MMAY_FILE_INVALID_UTF8;
    }
    status = mmay_posix_owned_path(allocator, path, path_length, &owned);
    if (status != MMAY_FILE_OK) return status;
    if (lstat((const char *)owned.data, &information) != 0) {
        status = errno == ENOENT || errno == ENOTDIR
            ? MMAY_FILE_LIMIT_EXCEEDED : MMAY_FILE_IO_FAILED;
    } else {
        metadata->regular = S_ISREG(information.st_mode);
        metadata->directory = S_ISDIR(information.st_mode);
        metadata->symlink = S_ISLNK(information.st_mode);
        if (metadata->regular) {
            if (information.st_size < 0) status = MMAY_FILE_IO_FAILED;
            else metadata->size = (uint64_t)information.st_size;
        }
    }
    mmay_owned_bytes_dispose(&owned);
    return status;
}

MarimayFileStatus mmay_file_replace_executable(
    MarimayAllocator allocator,
    const char *temporary_path,
    size_t temporary_path_length,
    const char *destination_path,
    size_t destination_path_length
) {
    MarimayOwnedBytes temporary = {0};
    MarimayOwnedBytes destination = {0};
    struct stat information;
    MarimayFileStatus status;
    if (temporary_path == NULL || temporary_path_length == 0
        || destination_path == NULL || destination_path_length == 0
        || memchr(temporary_path, 0, temporary_path_length) != NULL
        || memchr(destination_path, 0, destination_path_length) != NULL) {
        return MMAY_FILE_INVALID_ARGUMENT;
    }
    if (!mmay_utf8_validate(temporary_path, temporary_path_length, NULL)
        || !mmay_utf8_validate(
            destination_path, destination_path_length, NULL
        )) return MMAY_FILE_INVALID_UTF8;
    status = mmay_posix_owned_path(
        allocator, temporary_path, temporary_path_length, &temporary
    );
    if (status == MMAY_FILE_OK) status = mmay_posix_owned_path(
        allocator, destination_path, destination_path_length, &destination
    );
    if (status == MMAY_FILE_OK
        && (stat((const char *)temporary.data, &information) != 0
            || !S_ISREG(information.st_mode)
            || chmod((const char *)temporary.data,
                information.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) != 0
            || rename((const char *)temporary.data,
                (const char *)destination.data) != 0)) {
        status = MMAY_FILE_IO_FAILED;
    }
    mmay_owned_bytes_dispose(&destination);
    mmay_owned_bytes_dispose(&temporary);
    return status;
}
