#ifndef MMAY_RUNTIME_ENV_H
#define MMAY_RUNTIME_ENV_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/vector.h"
#include "mmay/platform/environment.h"

#define MMAY_ENV_MAX_NAME_BYTES 255U
#define MMAY_ENV_MAX_VALUE_BYTES 1048576U
#define MMAY_ENV_MAX_GRANTS 64U

typedef enum MarimayEnvStatus {
    MMAY_ENV_OK,
    MMAY_ENV_MISSING,
    MMAY_ENV_INVALID_ARGUMENT,
    MMAY_ENV_INVALID_NAME,
    MMAY_ENV_CAPABILITY_DENIED,
    MMAY_ENV_INVALID_UTF8,
    MMAY_ENV_LIMIT_EXCEEDED,
    MMAY_ENV_READ_FAILED,
    MMAY_ENV_ALLOCATION_FAILED
} MarimayEnvStatus;

typedef struct MarimayEnvNameView {
    const char *data;
    size_t length;
} MarimayEnvNameView;

typedef struct MarimayEnvOptions {
    const MarimayEnvNameView *grants;
    size_t grant_count;
    bool ascii_case_insensitive;
    MarimayHostEnvironmentOps host;
} MarimayEnvOptions;

typedef struct MarimayEnvContext {
    MarimayAllocator allocator;
    MarimayVector grants;
    MarimayHostEnvironmentOps host;
    bool ascii_case_insensitive;
    bool initialized;
} MarimayEnvContext;

bool mmay_env_name_valid(const char *name, size_t length);
bool mmay_env_names_equal(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length,
    bool ascii_case_insensitive
);
MarimayEnvOptions mmay_env_default_options(void);
MarimayEnvStatus mmay_env_init_status(
    MarimayEnvContext *context,
    MarimayAllocator allocator,
    const MarimayEnvOptions *options
);
bool mmay_env_init(
    MarimayEnvContext *context,
    MarimayAllocator allocator,
    const MarimayEnvOptions *options
);
void mmay_env_dispose(MarimayEnvContext *context);
MarimayEnvStatus mmay_env_read(
    MarimayEnvContext *context,
    const char *name,
    size_t name_length,
    MarimayOwnedBytes *value,
    size_t *invalid_utf8_offset
);

#endif
