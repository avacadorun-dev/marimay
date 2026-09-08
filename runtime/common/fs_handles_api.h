#ifndef MMAY_FS_HANDLES_API_H
#define MMAY_FS_HANDLES_API_H
#include "mmay/runtime/fs.h"
bool mmay_fs_handles_init(MarimayFsContext *context);
void mmay_fs_handles_dispose(MarimayFsContext *context);
/* The path owner has already authorized and checked this absolute path. */
MarimayFsStatus mmay_fs_handles_open(
    MarimayFsContext *context, const MarimayOwnedBytes *absolute, int64_t *handle
);
#endif
