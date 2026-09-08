#include "fs_handles_api.h"
#include "mmay/runtime/line_reader.h"
#include <string.h>

#define MMAY_FS_READ_BUFFER_BYTES 4096U

typedef struct MarimayFsOpenFile {
    MarimayFileHandle platform_handle;
    unsigned char *buffer;
    size_t offset, length;
    bool open, end_pending;
} MarimayFsOpenFile;

bool mmay_fs_handles_init(MarimayFsContext *context) {
    return mmay_vector_init(&context->files, context->allocator, sizeof(MarimayFsOpenFile));
}

void mmay_fs_handles_dispose(MarimayFsContext *context) {
    for (size_t index = 0; index < context->files.length; index += 1) {
        MarimayFsOpenFile *file = mmay_vector_at(&context->files, index);
        if (file == NULL) continue;
        if (file->open && context->read_ops.close != NULL)
            (void)context->read_ops.close(context->read_ops.context, file->platform_handle);
        mmay_deallocate(&context->allocator, file->buffer, MMAY_FS_READ_BUFFER_BYTES);
    }
    mmay_vector_dispose(&context->files);
}

MarimayFsStatus mmay_fs_handles_open(
    MarimayFsContext *context, const MarimayOwnedBytes *absolute, int64_t *handle
) {
    size_t open_count = 0, reusable = SIZE_MAX;
    MarimayFsOpenFile file = {0};
    for (size_t index = 0; index < context->files.length; index += 1) {
        const MarimayFsOpenFile *existing = mmay_vector_at_const(&context->files, index);
        if (existing != NULL && existing->open) open_count += 1;
        else if (reusable == SIZE_MAX) reusable = index;
    }
    if (open_count >= context->max_open_files) return MMAY_FS_LIMIT_EXCEEDED;
    MarimayFileStatus status = context->read_ops.open(context->read_ops.context,
        context->allocator, (const char *)absolute->data, absolute->length, &file.platform_handle);
    if (status != MMAY_FILE_OK) return status == MMAY_FILE_ALLOCATION_FAILED
        ? MMAY_FS_ALLOCATION_FAILED : MMAY_FS_OPEN_FAILED;
    file.open = true;
    if (reusable != SIZE_MAX) {
        MarimayFsOpenFile *slot = mmay_vector_at(&context->files, reusable);
        if (slot == NULL) {
            (void)context->read_ops.close(context->read_ops.context, file.platform_handle);
            return MMAY_FS_INTERNAL_FAILED;
        }
        *slot = file;
        *handle = (int64_t)reusable + 1;
    } else if (!mmay_vector_push(&context->files, &file)) {
        (void)context->read_ops.close(context->read_ops.context, file.platform_handle);
        return MMAY_FS_ALLOCATION_FAILED;
    } else *handle = (int64_t)context->files.length;
    return MMAY_FS_OK;
}

static MarimayFsOpenFile *mmay_fs_open_file(
    MarimayFsContext *context,
    int64_t handle
) {
    MarimayFsOpenFile *file = context != NULL && context->initialized
            && handle > 0 && (uint64_t)handle <= context->files.length
        ? mmay_vector_at(&context->files, (size_t)handle - 1U) : NULL;
    return file != NULL && file->open ? file : NULL;
}

/* Whole-file reads drain prefetched bytes before reading directly into their
 * own chunk. Line reads refill one bounded handle-owned buffer, avoiding a host
 * syscall for each byte. A reported final chunk is drained before EOF. */
static MarimayFsStatus mmay_fs_handle_read(
    MarimayFsContext *context, MarimayFsOpenFile *file, unsigned char *destination,
    size_t capacity, size_t *count, bool *end, bool prefetch
) {
    *count = 0; *end = false;
    if (file->offset < file->length) {
        size_t available = file->length - file->offset;
        if (available > capacity) available = capacity;
        memcpy(destination, file->buffer + file->offset, available);
        file->offset += available; *count = available;
        return MMAY_FS_OK;
    }
    if (file->end_pending) { file->end_pending = false; *end = true; return MMAY_FS_OK; }
    if (!prefetch) {
        MarimayFileStatus status = context->read_ops.read(context->read_ops.context,
            file->platform_handle, destination, capacity, count, end);
        return status == MMAY_FILE_OK && *count <= capacity && (*count != 0 || *end)
            ? MMAY_FS_OK : MMAY_FS_READ_FAILED;
    }
    if (file->buffer == NULL) {
        file->buffer = mmay_allocate(&context->allocator, MMAY_FS_READ_BUFFER_BYTES);
        if (file->buffer == NULL) return MMAY_FS_ALLOCATION_FAILED;
    }
    size_t received = 0;
    bool final = false;
    MarimayFileStatus status = context->read_ops.read(context->read_ops.context,
        file->platform_handle, file->buffer, MMAY_FS_READ_BUFFER_BYTES, &received, &final);
    if (status != MMAY_FILE_OK || received > MMAY_FS_READ_BUFFER_BYTES || (received == 0 && !final))
        return MMAY_FS_READ_FAILED;
    file->offset = 0; file->length = received; file->end_pending = final;
    if (received == 0) { file->end_pending = false; *end = true; return MMAY_FS_OK; }
    if (received > capacity) received = capacity;
    memcpy(destination, file->buffer, received);
    file->offset = received; *count = received;
    return MMAY_FS_OK;
}

MarimayFsStatus mmay_fs_read(
    MarimayFsContext *context,
    int64_t handle,
    int64_t limit,
    MarimayOwnedBytes *output
) {
    MarimayFsOpenFile *file = NULL;
    MarimayVector bytes;
    unsigned char chunk[4096];
    MarimayFsStatus status = MMAY_FS_OK;
    if (output == NULL || context == NULL || !context->initialized) {
        return MMAY_FS_INVALID_ARGUMENT;
    }
    memset(output, 0, sizeof(*output));
    if (limit < 0 || (uint64_t)limit > MMAY_FS_MAX_FILE_BYTES) {
        return MMAY_FS_INVALID_LIMIT;
    }
    file = mmay_fs_open_file(context, handle);
    if (file == NULL) return MMAY_FS_CLOSED;
    if (!mmay_vector_init(&bytes, context->allocator, sizeof(unsigned char))) {
        return MMAY_FS_ALLOCATION_FAILED;
    }
    for (;;) {
        size_t read_count = 0;
        bool end = false;
        status = mmay_fs_handle_read(context, file, chunk, sizeof(chunk), &read_count, &end, false);
        if (status != MMAY_FS_OK) {
            break;
        }
        if (read_count > (size_t)limit - bytes.length) {
            status = MMAY_FS_LIMIT_EXCEEDED;
            break;
        }
        if (!mmay_vector_append(&bytes, chunk, read_count)) {
            status = MMAY_FS_ALLOCATION_FAILED;
            break;
        }
        if (end) break;
    }
    if (status == MMAY_FS_OK && !mmay_owned_bytes_copy(
            output, context->allocator, bytes.data, bytes.length
        )) status = MMAY_FS_ALLOCATION_FAILED;
    mmay_vector_dispose(&bytes);
    if (status != MMAY_FS_OK) mmay_owned_bytes_dispose(output);
    return status;
}

MarimayFsStatus mmay_fs_close(MarimayFsContext *context, int64_t handle) {
    MarimayFsOpenFile *file;
    MarimayFileStatus status;
    if (context == NULL || !context->initialized || handle <= 0
        || (uint64_t)handle > context->files.length) return MMAY_FS_CLOSED;
    file = mmay_vector_at(&context->files, (size_t)handle - 1U);
    if (file == NULL || !file->open) return MMAY_FS_OK;
    file->open = false;
    mmay_deallocate(&context->allocator, file->buffer, MMAY_FS_READ_BUFFER_BYTES);
    file->buffer = NULL;
    file->length = file->offset = 0;
    file->end_pending = false;
    status = context->read_ops.close(
        context->read_ops.context, file->platform_handle
    );
    return status == MMAY_FILE_OK ? MMAY_FS_OK : MMAY_FS_CLOSE_FAILED;
}

void mmay_fs_close_best_effort(MarimayFsContext *context, int64_t handle) {
    (void)mmay_fs_close(context, handle);
}

typedef struct MarimayFsLineInput {
    MarimayFsContext *context;
    MarimayFsOpenFile *file;
    MarimayFsStatus failure;
} MarimayFsLineInput;

static MarimayIoTransferStatus mmay_fs_line_input(
    void *opaque, unsigned char *bytes, size_t capacity, size_t *count
) {
    MarimayFsLineInput *input = opaque;
    bool end = false;
    input->failure = mmay_fs_handle_read(input->context, input->file, bytes, capacity, count, &end, true);
    if (input->failure != MMAY_FS_OK) return MMAY_IO_TRANSFER_FAILED;
    return end ? MMAY_IO_TRANSFER_END : MMAY_IO_TRANSFER_OK;
}

MarimayFsStatus mmay_fs_read_line(
    MarimayFsContext *context, int64_t handle, int64_t limit,
    MarimayOwnedBytes *line, bool *eof, size_t *invalid_offset
) {
    if (context == NULL || !context->initialized || line == NULL || eof == NULL)
        return MMAY_FS_INVALID_ARGUMENT;
    memset(line, 0, sizeof(*line)); *eof = false;
    if (invalid_offset != NULL) *invalid_offset = 0;
    if (limit < 0 || (uint64_t)limit > MMAY_FS_MAX_FILE_BYTES) return MMAY_FS_INVALID_LIMIT;
    MarimayFsOpenFile *file = mmay_fs_open_file(context, handle);
    if (file == NULL) return MMAY_FS_CLOSED;
    MarimayFsLineInput source = {context, file, MMAY_FS_OK};
    MarimayIoInput input = {true, &source, mmay_fs_line_input};
    MarimayIoStatus status = mmay_read_utf8_line(&input, context->allocator,
        (size_t)limit, line, eof, invalid_offset);
    if (source.failure != MMAY_FS_OK) return source.failure;
    switch (status) {
        case MMAY_IO_OK: return MMAY_FS_OK;
        case MMAY_IO_ALLOCATION_FAILED: return MMAY_FS_ALLOCATION_FAILED;
        case MMAY_IO_INVALID_UTF8: return MMAY_FS_INVALID_UTF8;
        case MMAY_IO_LINE_TOO_LONG: return MMAY_FS_LIMIT_EXCEEDED;
        case MMAY_IO_READ_FAILED: return MMAY_FS_READ_FAILED;
        default: return MMAY_FS_INTERNAL_FAILED;
    }
}
