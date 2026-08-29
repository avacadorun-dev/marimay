#ifndef MMAY_PLATFORM_ENVIRONMENT_H
#define MMAY_PLATFORM_ENVIRONMENT_H

#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

typedef enum MarimayHostEnvironmentStatus {
    MMAY_HOST_ENVIRONMENT_OK,
    MMAY_HOST_ENVIRONMENT_MISSING,
    MMAY_HOST_ENVIRONMENT_INVALID_ARGUMENT,
    MMAY_HOST_ENVIRONMENT_ALLOCATION_FAILED,
    MMAY_HOST_ENVIRONMENT_OUTPUT_LIMIT,
    MMAY_HOST_ENVIRONMENT_INVALID_ENCODING,
    MMAY_HOST_ENVIRONMENT_IO_FAILED
} MarimayHostEnvironmentStatus;

typedef MarimayHostEnvironmentStatus (*MarimayHostEnvironmentLookupFn)(
    void *context,
    MarimayAllocator allocator,
    const char *name,
    size_t name_length,
    size_t max_value_bytes,
    MarimayOwnedBytes *value
);

typedef struct MarimayHostEnvironmentOps {
    void *context;
    MarimayHostEnvironmentLookupFn lookup;
} MarimayHostEnvironmentOps;

/* Returns the host implementation. The returned value owns no state. */
MarimayHostEnvironmentOps mmay_platform_environment_ops(void);

#endif
