#define _POSIX_C_SOURCE 200809L

#include "mmay/platform/environment.h"

#include <stdlib.h>
#include <string.h>

static MarimayHostEnvironmentStatus mmay_posix_environment_lookup(
    void *context,
    MarimayAllocator allocator,
    const char *name,
    size_t name_length,
    size_t max_value_bytes,
    MarimayOwnedBytes *value
) {
    char terminated[256];
    const char *found = NULL;
    size_t length = 0;
    (void)context;
    if (name == NULL || name_length == 0
        || name_length > 255U
        || value == NULL || max_value_bytes == 0) {
        return MMAY_HOST_ENVIRONMENT_INVALID_ARGUMENT;
    }
    memset(value, 0, sizeof(*value));
    memcpy(terminated, name, name_length);
    terminated[name_length] = 0;
    found = getenv(terminated);
    if (found == NULL) return MMAY_HOST_ENVIRONMENT_MISSING;
    length = strlen(found);
    if (length > max_value_bytes) return MMAY_HOST_ENVIRONMENT_OUTPUT_LIMIT;
    return mmay_owned_bytes_copy(value, allocator, found, length)
        ? MMAY_HOST_ENVIRONMENT_OK
        : MMAY_HOST_ENVIRONMENT_ALLOCATION_FAILED;
}

MarimayHostEnvironmentOps mmay_platform_environment_ops(void) {
    MarimayHostEnvironmentOps ops;
    ops.context = NULL;
    ops.lookup = mmay_posix_environment_lookup;
    return ops;
}
