#ifndef MMAY_TEST_FILE_LINE_HOST_H
#define MMAY_TEST_FILE_LINE_HOST_H
#include "../../runtime/common/fs_handles_api.h"
#include <string.h>

/* Deterministic already-authorized handle. Capability/path behavior belongs to
 * the public process fixture; this host isolates allocation and GC lifetimes. */
typedef struct MmayLineHost { size_t offset; unsigned scenario; } MmayLineHost;
static inline MarimayFileStatus mmay_line_host_open(void *opaque, MarimayAllocator allocator,
    const char *path, size_t length, MarimayFileHandle *handle) {
    (void)opaque; (void)allocator; (void)path; (void)length; *handle = 1; return MMAY_FILE_OK;
}
static inline MarimayFileStatus mmay_line_host_read(void *opaque, MarimayFileHandle handle,
    unsigned char *bytes, size_t capacity, size_t *count, bool *end) {
    MmayLineHost *host = opaque;
    static const unsigned char valid[] = "\r\n\xc3\xa9\r\nz";
    static const unsigned char invalid[] = "a\xff\n";
    const unsigned char *data = host->scenario == 1 ? invalid : valid;
    size_t length = host->scenario == 1 ? sizeof(invalid) - 1 : sizeof(valid) - 1;
    (void)handle;
    if (host->scenario == 2) return MMAY_FILE_IO_FAILED;
    *count = length - host->offset;
    if (*count > capacity) *count = capacity;
    if (*count > 2) *count = 2;
    if (*count != 0) memcpy(bytes, data + host->offset, *count);
    host->offset += *count; *end = host->offset == length;
    return MMAY_FILE_OK;
}
static inline MarimayFileStatus mmay_line_host_close(void *opaque, MarimayFileHandle handle) {
    (void)opaque; (void)handle; return MMAY_FILE_OK;
}
static inline bool mmay_line_host_init(MarimayFsContext *context, MarimayAllocator allocator,
    MmayLineHost *host) {
    const MarimayOwnedBytes path = {.data = (unsigned char *)"authorized", .length = 10};
    int64_t handle = 0;
    context->allocator = allocator; context->initialized = true; context->max_open_files = 1;
    context->read_ops = (MarimayFileReadOps){host, mmay_line_host_open, mmay_line_host_read, mmay_line_host_close};
    return mmay_fs_handles_init(context) && mmay_fs_handles_open(context, &path, &handle) == MMAY_FS_OK;
}
#endif
