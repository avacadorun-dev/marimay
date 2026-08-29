#include "mmay/runtime/env.h"

#include <string.h>

#include "mmay/base/utf8.h"

static unsigned char mmay_env_fold(unsigned char byte) {
    return byte >= 'a' && byte <= 'z'
        ? (unsigned char)(byte - ('a' - 'A')) : byte;
}

bool mmay_env_name_valid(const char *name, size_t length) {
    size_t index = 0;
    if (name == NULL || length == 0 || length > MMAY_ENV_MAX_NAME_BYTES) {
        return false;
    }
    if (!((name[0] >= 'A' && name[0] <= 'Z')
            || (name[0] >= 'a' && name[0] <= 'z')
            || name[0] == '_')) {
        return false;
    }
    for (index = 1; index < length; index += 1) {
        if (!((name[index] >= 'A' && name[index] <= 'Z')
                || (name[index] >= 'a' && name[index] <= 'z')
                || (name[index] >= '0' && name[index] <= '9')
                || name[index] == '_')) {
            return false;
        }
    }
    return true;
}

bool mmay_env_names_equal(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length,
    bool ascii_case_insensitive
) {
    size_t index = 0;
    if (left == NULL || right == NULL || left_length != right_length) {
        return false;
    }
    if (!ascii_case_insensitive) {
        return left_length == 0 || memcmp(left, right, left_length) == 0;
    }
    for (index = 0; index < left_length; index += 1) {
        if (mmay_env_fold((unsigned char)left[index])
            != mmay_env_fold((unsigned char)right[index])) {
            return false;
        }
    }
    return true;
}

MarimayEnvOptions mmay_env_default_options(void) {
    MarimayEnvOptions options;
    memset(&options, 0, sizeof(options));
#if defined(_WIN32)
    options.ascii_case_insensitive = true;
#endif
    options.host = mmay_platform_environment_ops();
    return options;
}

static bool mmay_env_has_grant(
    const MarimayEnvContext *context,
    const char *name,
    size_t name_length
) {
    size_t index = 0;
    for (index = 0; context != NULL && index < context->grants.length;
        index += 1) {
        const MarimayOwnedBytes *grant = mmay_vector_at_const(
            &context->grants, index
        );
        if (grant != NULL && mmay_env_names_equal(
                (const char *)grant->data, grant->length,
                name, name_length, context->ascii_case_insensitive
            )) {
            return true;
        }
    }
    return false;
}

MarimayEnvStatus mmay_env_init_status(
    MarimayEnvContext *context,
    MarimayAllocator allocator,
    const MarimayEnvOptions *options
) {
    MarimayEnvOptions selected;
    size_t index = 0;
    if (context == NULL || allocator.allocate == NULL
        || allocator.resize == NULL || allocator.deallocate == NULL) {
        return MMAY_ENV_INVALID_ARGUMENT;
    }
    memset(context, 0, sizeof(*context));
    selected = options != NULL ? *options : mmay_env_default_options();
    if (selected.grant_count > MMAY_ENV_MAX_GRANTS
        || (selected.grant_count != 0 && selected.grants == NULL)
        || selected.host.lookup == NULL
        || !mmay_vector_init(
            &context->grants, allocator, sizeof(MarimayOwnedBytes)
        )) {
        return selected.host.lookup == NULL
                || selected.grant_count > MMAY_ENV_MAX_GRANTS
                || (selected.grant_count != 0 && selected.grants == NULL)
            ? MMAY_ENV_INVALID_ARGUMENT : MMAY_ENV_ALLOCATION_FAILED;
    }
    context->allocator = allocator;
    context->host = selected.host;
    context->ascii_case_insensitive = selected.ascii_case_insensitive;
    context->initialized = true;
    for (index = 0; index < selected.grant_count; index += 1) {
        const MarimayEnvNameView *view = &selected.grants[index];
        MarimayOwnedBytes owned;
        if (!mmay_env_name_valid(view->data, view->length)
            || mmay_env_has_grant(context, view->data, view->length)) {
            mmay_env_dispose(context);
            return MMAY_ENV_INVALID_NAME;
        }
        memset(&owned, 0, sizeof(owned));
        if (!mmay_owned_bytes_copy(
                &owned, allocator, view->data, view->length
            ) || !mmay_vector_push(&context->grants, &owned)) {
            mmay_owned_bytes_dispose(&owned);
            mmay_env_dispose(context);
            return MMAY_ENV_ALLOCATION_FAILED;
        }
    }
    return MMAY_ENV_OK;
}

bool mmay_env_init(
    MarimayEnvContext *context,
    MarimayAllocator allocator,
    const MarimayEnvOptions *options
) {
    return mmay_env_init_status(context, allocator, options) == MMAY_ENV_OK;
}

void mmay_env_dispose(MarimayEnvContext *context) {
    size_t index = 0;
    if (context == NULL || !context->initialized) return;
    for (index = 0; index < context->grants.length; index += 1) {
        MarimayOwnedBytes *grant = mmay_vector_at(&context->grants, index);
        if (grant != NULL) mmay_owned_bytes_dispose(grant);
    }
    mmay_vector_dispose(&context->grants);
    memset(context, 0, sizeof(*context));
}

MarimayEnvStatus mmay_env_read(
    MarimayEnvContext *context,
    const char *name,
    size_t name_length,
    MarimayOwnedBytes *value,
    size_t *invalid_utf8_offset
) {
    MarimayHostEnvironmentStatus status;
    size_t invalid = 0;
    if (value == NULL) return MMAY_ENV_INVALID_ARGUMENT;
    memset(value, 0, sizeof(*value));
    if (invalid_utf8_offset != NULL) *invalid_utf8_offset = 0;
    if (context == NULL || !context->initialized) {
        return MMAY_ENV_INVALID_ARGUMENT;
    }
    if (!mmay_env_name_valid(name, name_length)) return MMAY_ENV_INVALID_NAME;
    if (!mmay_env_has_grant(context, name, name_length)) {
        return MMAY_ENV_CAPABILITY_DENIED;
    }
    status = context->host.lookup(
        context->host.context, context->allocator, name,
        name_length,
        MMAY_ENV_MAX_VALUE_BYTES, value
    );
    if (status == MMAY_HOST_ENVIRONMENT_OK) {
        /* The successful payload is validated below. */
    } else {
        /* Host adapters never publish a payload on failure, but injected
         * adapters are still held to the runtime ownership boundary. */
        mmay_owned_bytes_dispose(value);
    }
    if (status == MMAY_HOST_ENVIRONMENT_MISSING) return MMAY_ENV_MISSING;
    if (status == MMAY_HOST_ENVIRONMENT_ALLOCATION_FAILED) {
        return MMAY_ENV_ALLOCATION_FAILED;
    }
    if (status == MMAY_HOST_ENVIRONMENT_OUTPUT_LIMIT) {
        return MMAY_ENV_LIMIT_EXCEEDED;
    }
    if (status == MMAY_HOST_ENVIRONMENT_INVALID_ENCODING) {
        return MMAY_ENV_INVALID_UTF8;
    }
    if (status != MMAY_HOST_ENVIRONMENT_OK) return MMAY_ENV_READ_FAILED;
    if ((value->data == NULL && value->length != 0)
        || value->length > MMAY_ENV_MAX_VALUE_BYTES) {
        bool over_limit = value->length > MMAY_ENV_MAX_VALUE_BYTES;
        mmay_owned_bytes_dispose(value);
        return over_limit
            ? MMAY_ENV_LIMIT_EXCEEDED : MMAY_ENV_READ_FAILED;
    }
    if (!mmay_utf8_validate((const char *)value->data, value->length, &invalid)) {
        if (invalid_utf8_offset != NULL) *invalid_utf8_offset = invalid;
        mmay_owned_bytes_dispose(value);
        return MMAY_ENV_INVALID_UTF8;
    }
    return MMAY_ENV_OK;
}
