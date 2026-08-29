#include "test_support.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/env.h"
#include "mmay/runtime/process_context.h"

typedef struct MarimayCp35EnvHost {
    const unsigned char *bytes;
    size_t length;
    MarimayHostEnvironmentStatus status;
    size_t calls;
    char observed_name[256];
    size_t observed_length;
    bool publish_on_failure;
} MarimayCp35EnvHost;

static MarimayHostEnvironmentStatus mmay_cp35_env_lookup(
    void *context,
    MarimayAllocator allocator,
    const char *name,
    size_t name_length,
    size_t maximum,
    MarimayOwnedBytes *value
) {
    MarimayCp35EnvHost *host = context;
    host->calls += 1U;
    host->observed_length = name_length;
    if (name_length <= sizeof(host->observed_name)) {
        memcpy(host->observed_name, name, name_length);
    }
    if (host->length > maximum) {
        return MMAY_HOST_ENVIRONMENT_OUTPUT_LIMIT;
    }
    if (host->status == MMAY_HOST_ENVIRONMENT_OK
        || host->publish_on_failure) {
        if (!mmay_owned_bytes_copy(
                value, allocator, host->bytes, host->length
            )) return MMAY_HOST_ENVIRONMENT_ALLOCATION_FAILED;
    }
    return host->status;
}

static MarimayEnvOptions mmay_cp35_env_options(
    MarimayCp35EnvHost *host,
    const MarimayEnvNameView *grants,
    size_t grant_count,
    bool insensitive
) {
    MarimayEnvOptions options;
    memset(&options, 0, sizeof(options));
    options.grants = grants;
    options.grant_count = grant_count;
    options.ascii_case_insensitive = insensitive;
    options.host.context = host;
    options.host.lookup = mmay_cp35_env_lookup;
    return options;
}

static void mmay_cp35_names_and_capabilities(void) {
    static const MarimayEnvNameView grants[] = {
        {"MMAY_ALPHA_TEST", 15}, {"CaseKey", 7}
    };
    static const MarimayEnvNameView duplicate[] = {
        {"CaseKey", 7}, {"casekey", 7}
    };
    static const unsigned char value_bytes[] = "secret-value";
    MarimayCp35EnvHost host = {
        .bytes = value_bytes,
        .length = sizeof(value_bytes) - 1U,
        .status = MMAY_HOST_ENVIRONMENT_OK
    };
    MarimayEnvOptions options = mmay_cp35_env_options(
        &host, grants, sizeof(grants) / sizeof(grants[0]), false
    );
    MarimayEnvContext context;
    MarimayOwnedBytes value = {0};
    char unterminated[] = {'M', 'M', 'A', 'Y', '_', 'A', 'L', 'P', 'H', 'A',
        '_', 'T', 'E', 'S', 'T', 'X'};
    char too_long[256];
    memset(too_long, 'A', sizeof(too_long));
    MMAY_EXPECT(mmay_env_name_valid("A", 1));
    MMAY_EXPECT(mmay_env_name_valid("_A9", 3));
    MMAY_EXPECT(!mmay_env_name_valid(NULL, 1));
    MMAY_EXPECT(!mmay_env_name_valid("", 0));
    MMAY_EXPECT(!mmay_env_name_valid("9A", 2));
    MMAY_EXPECT(!mmay_env_name_valid("A-B", 3));
    MMAY_EXPECT(!mmay_env_name_valid(too_long, sizeof(too_long)));
    MMAY_EXPECT(mmay_env_names_equal("Case", 4, "Case", 4, false));
    MMAY_EXPECT(!mmay_env_names_equal("Case", 4, "case", 4, false));
    MMAY_EXPECT(mmay_env_names_equal("Case", 4, "case", 4, true));

    MMAY_EXPECT(mmay_env_init_status(
        &context, mmay_system_allocator(), &options
    ) == MMAY_ENV_OK);
    MMAY_EXPECT(mmay_env_read(
        &context, unterminated, 15, &value, NULL
    ) == MMAY_ENV_OK);
    MMAY_EXPECT(host.observed_length == 15);
    MMAY_EXPECT(memcmp(host.observed_name, "MMAY_ALPHA_TEST", 15) == 0);
    MMAY_EXPECT(value.length == sizeof(value_bytes) - 1U);
    MMAY_EXPECT(memcmp(value.data, value_bytes, value.length) == 0);
    mmay_owned_bytes_dispose(&value);
    MMAY_EXPECT(mmay_env_read(
        &context, "casekey", 7, &value, NULL
    ) == MMAY_ENV_CAPABILITY_DENIED);
    MMAY_EXPECT(host.calls == 1);
    MMAY_EXPECT(mmay_env_read(
        &context, "BAD-NAME", 8, &value, NULL
    ) == MMAY_ENV_INVALID_NAME);
    MMAY_EXPECT(host.calls == 1);
    mmay_env_dispose(&context);

    options = mmay_cp35_env_options(&host, duplicate, 2, true);
    MMAY_EXPECT(mmay_env_init_status(
        &context, mmay_system_allocator(), &options
    ) == MMAY_ENV_INVALID_NAME);
    MMAY_EXPECT(!context.initialized);
}

static MarimayEnvStatus mmay_cp35_read_status(
    MarimayCp35EnvHost *host,
    MarimayOwnedBytes *value,
    size_t *invalid_offset
) {
    static const MarimayEnvNameView grant = {"VALUE", 5};
    MarimayEnvOptions options = mmay_cp35_env_options(
        host, &grant, 1, false
    );
    MarimayEnvContext context;
    MarimayEnvStatus status;
    if (mmay_env_init_status(
            &context, mmay_system_allocator(), &options
        ) != MMAY_ENV_OK) return MMAY_ENV_ALLOCATION_FAILED;
    status = mmay_env_read(
        &context, "VALUE", 5, value, invalid_offset
    );
    mmay_env_dispose(&context);
    return status;
}

static void mmay_cp35_env_values_and_errors(void) {
    static const unsigned char unicode[] = "h\xC3\xA9llo world";
    static const unsigned char invalid[] = {'a', 0xffU, 'b'};
    MarimayCp35EnvHost host;
    MarimayOwnedBytes value = {0};
    size_t invalid_offset = SIZE_MAX;
    memset(&host, 0, sizeof(host));
    host.status = MMAY_HOST_ENVIRONMENT_OK;
    host.bytes = unicode;
    host.length = sizeof(unicode) - 1U;
    MMAY_EXPECT(mmay_cp35_read_status(
        &host, &value, &invalid_offset
    ) == MMAY_ENV_OK);
    MMAY_EXPECT(value.length == sizeof(unicode) - 1U);
    MMAY_EXPECT(memcmp(value.data, unicode, value.length) == 0);
    mmay_owned_bytes_dispose(&value);

    host.bytes = NULL;
    host.length = 0;
    MMAY_EXPECT(mmay_cp35_read_status(
        &host, &value, &invalid_offset
    ) == MMAY_ENV_OK);
    MMAY_EXPECT(value.length == 0);
    mmay_owned_bytes_dispose(&value);

    host.status = MMAY_HOST_ENVIRONMENT_MISSING;
    MMAY_EXPECT(mmay_cp35_read_status(
        &host, &value, &invalid_offset
    ) == MMAY_ENV_MISSING);
    host.status = MMAY_HOST_ENVIRONMENT_OK;
    host.bytes = invalid;
    host.length = sizeof(invalid);
    MMAY_EXPECT(mmay_cp35_read_status(
        &host, &value, &invalid_offset
    ) == MMAY_ENV_INVALID_UTF8);
    MMAY_EXPECT(invalid_offset == 1);

    host.status = MMAY_HOST_ENVIRONMENT_OUTPUT_LIMIT;
    host.bytes = unicode;
    host.length = sizeof(unicode) - 1U;
    host.publish_on_failure = true;
    MMAY_EXPECT(mmay_cp35_read_status(
        &host, &value, &invalid_offset
    ) == MMAY_ENV_LIMIT_EXCEEDED);
    MMAY_EXPECT(value.data == NULL);
    host.status = MMAY_HOST_ENVIRONMENT_INVALID_ENCODING;
    MMAY_EXPECT(mmay_cp35_read_status(
        &host, &value, &invalid_offset
    ) == MMAY_ENV_INVALID_UTF8);
    MMAY_EXPECT(value.data == NULL);
    host.status = MMAY_HOST_ENVIRONMENT_IO_FAILED;
    MMAY_EXPECT(mmay_cp35_read_status(
        &host, &value, &invalid_offset
    ) == MMAY_ENV_READ_FAILED);
    MMAY_EXPECT(value.data == NULL);
}

typedef struct MarimayCp35PathHost {
    const unsigned char *bytes;
    size_t length;
    MarimayProcessContextSourceStatus status;
    bool publish_on_failure;
} MarimayCp35PathHost;

static MarimayProcessContextSourceStatus mmay_cp35_path_read(
    void *context,
    MarimayAllocator allocator,
    size_t maximum,
    MarimayOwnedBytes *path
) {
    MarimayCp35PathHost *host = context;
    if (host->length > maximum) {
        return MMAY_PROCESS_CONTEXT_SOURCE_LIMIT_EXCEEDED;
    }
    if (host->status == MMAY_PROCESS_CONTEXT_SOURCE_OK
        || host->publish_on_failure) {
        if (!mmay_owned_bytes_copy(
                path, allocator, host->bytes, host->length
            )) return MMAY_PROCESS_CONTEXT_SOURCE_ALLOCATION_FAILED;
    }
    return host->status;
}

static MarimayProcessContextOptions mmay_cp35_path_options(
    MarimayCp35PathHost *host
) {
    MarimayProcessContextOptions options;
    memset(&options, 0, sizeof(options));
    options.flavor = MMAY_PATH_POSIX;
    options.current_directory_context = host;
    options.current_directory = mmay_cp35_path_read;
    options.executable_path_context = host;
    options.executable_path = mmay_cp35_path_read;
    return options;
}

static void mmay_cp35_process_paths_and_errors(void) {
    static const unsigned char input[] = "/tmp/space \xC3\xBC/./child";
    static const char expected[] = "/tmp/space \xC3\xBC/child";
    static const unsigned char invalid[] = {'/', 0xffU};
    MarimayCp35PathHost host = {
        input, sizeof(input) - 1U, MMAY_PROCESS_CONTEXT_SOURCE_OK, false
    };
    MarimayProcessContextOptions options = mmay_cp35_path_options(&host);
    MarimayOwnedBytes path = {0};
    size_t invalid_offset = SIZE_MAX;
    MMAY_EXPECT(mmay_process_context_current_directory(
        &options, mmay_system_allocator(), &path, &invalid_offset
    ) == MMAY_PROCESS_CONTEXT_OK);
    MMAY_EXPECT(path.length == sizeof(expected) - 1U);
    MMAY_EXPECT(memcmp(path.data, expected, path.length) == 0);
    mmay_owned_bytes_dispose(&path);
    MMAY_EXPECT(mmay_process_context_executable_path(
        &options, mmay_system_allocator(), &path, &invalid_offset
    ) == MMAY_PROCESS_CONTEXT_OK);
    mmay_owned_bytes_dispose(&path);

    host.bytes = (const unsigned char *)"relative/path";
    host.length = 13;
    MMAY_EXPECT(mmay_process_context_current_directory(
        &options, mmay_system_allocator(), &path, &invalid_offset
    ) == MMAY_PROCESS_CONTEXT_INVALID_PATH);
    host.bytes = invalid;
    host.length = sizeof(invalid);
    MMAY_EXPECT(mmay_process_context_current_directory(
        &options, mmay_system_allocator(), &path, &invalid_offset
    ) == MMAY_PROCESS_CONTEXT_INVALID_UTF8);
    MMAY_EXPECT(invalid_offset == 1);

    host.bytes = input;
    host.length = sizeof(input) - 1U;
    host.status = MMAY_PROCESS_CONTEXT_SOURCE_UNAVAILABLE;
    host.publish_on_failure = true;
    MMAY_EXPECT(mmay_process_context_current_directory(
        &options, mmay_system_allocator(), &path, &invalid_offset
    ) == MMAY_PROCESS_CONTEXT_CURRENT_DIRECTORY_UNAVAILABLE);
    MMAY_EXPECT(path.data == NULL);
    MMAY_EXPECT(mmay_process_context_executable_path(
        &options, mmay_system_allocator(), &path, &invalid_offset
    ) == MMAY_PROCESS_CONTEXT_EXECUTABLE_PATH_UNAVAILABLE);
    MMAY_EXPECT(path.data == NULL);
    host.status = MMAY_PROCESS_CONTEXT_SOURCE_INVALID_ENCODING;
    MMAY_EXPECT(mmay_process_context_current_directory(
        &options, mmay_system_allocator(), &path, &invalid_offset
    ) == MMAY_PROCESS_CONTEXT_INVALID_UTF8);
    MMAY_EXPECT(path.data == NULL);
}

typedef enum MarimayCp35AllocationOperation {
    MMAY_CP35_ALLOC_ENV_INIT,
    MMAY_CP35_ALLOC_ENV_READ,
    MMAY_CP35_ALLOC_PROCESS
} MarimayCp35AllocationOperation;

static bool mmay_cp35_allocation_attempt(
    size_t failure_index,
    MarimayCp35AllocationOperation operation
) {
    static const MarimayEnvNameView grants[] = {
        {"FIRST", 5}, {"SECOND", 6}
    };
    static const unsigned char value_bytes[] = "allocation-value";
    static const unsigned char path_bytes[] = "/tmp/allocation/path";
    MarimayFailAllocator state;
    MarimayAllocator allocator;
    MarimayCp35EnvHost env_host = {
        .bytes = value_bytes,
        .length = sizeof(value_bytes) - 1U,
        .status = MMAY_HOST_ENVIRONMENT_OK
    };
    MarimayEnvOptions env_options = mmay_cp35_env_options(
        &env_host, grants, sizeof(grants) / sizeof(grants[0]), false
    );
    MarimayEnvContext env;
    MarimayOwnedBytes output = {0};
    bool env_initialized = false;
    bool success = false;
    mmay_fail_allocator_init(&state, mmay_system_allocator(), failure_index);
    allocator = mmay_fail_allocator(&state);
    if (operation == MMAY_CP35_ALLOC_PROCESS) {
        MarimayCp35PathHost path_host = {
            path_bytes, sizeof(path_bytes) - 1U,
            MMAY_PROCESS_CONTEXT_SOURCE_OK, false
        };
        MarimayProcessContextOptions path_options =
            mmay_cp35_path_options(&path_host);
        MarimayProcessContextStatus status =
            mmay_process_context_current_directory(
                &path_options, allocator, &output, NULL
            );
        success = status == MMAY_PROCESS_CONTEXT_OK;
        MMAY_EXPECT(success
            || status == MMAY_PROCESS_CONTEXT_ALLOCATION_FAILED);
    } else {
        MarimayEnvStatus status = mmay_env_init_status(
            &env, allocator, &env_options
        );
        env_initialized = status == MMAY_ENV_OK;
        if (operation == MMAY_CP35_ALLOC_ENV_INIT) {
            success = env_initialized;
            MMAY_EXPECT(success || status == MMAY_ENV_ALLOCATION_FAILED);
        } else if (env_initialized) {
            status = mmay_env_read(&env, "FIRST", 5, &output, NULL);
            success = status == MMAY_ENV_OK;
            MMAY_EXPECT(success || status == MMAY_ENV_ALLOCATION_FAILED);
        } else {
            MMAY_EXPECT(status == MMAY_ENV_ALLOCATION_FAILED);
        }
    }
    mmay_owned_bytes_dispose(&output);
    if (env_initialized) mmay_env_dispose(&env);
    MMAY_EXPECT(state.live_allocations == 0);
    MMAY_EXPECT(state.live_bytes == 0);
    return success;
}

static void mmay_cp35_allocation_failures(void) {
    MarimayCp35AllocationOperation operation;
    for (operation = MMAY_CP35_ALLOC_ENV_INIT;
        operation <= MMAY_CP35_ALLOC_PROCESS; operation += 1) {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 64; failure_index += 1U) {
            if (mmay_cp35_allocation_attempt(failure_index, operation)) {
                reached_success = true;
                break;
            }
        }
        MMAY_EXPECT(reached_success);
    }
}

void mmay_test_cp35_environment(void) {
    mmay_cp35_names_and_capabilities();
    mmay_cp35_env_values_and_errors();
    mmay_cp35_process_paths_and_errors();
    mmay_cp35_allocation_failures();
}
