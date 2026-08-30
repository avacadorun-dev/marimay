#include "test_support.h"

#include <string.h>

#include "cli_standard_packages.h"
#include "mmay/check/service.h"

static bool mmay_cp37_standard_target_init(
    MarimayCliCheckTarget *target,
    MarimayAllocator allocator
) {
    MarimayCompilerOptions options = mmay_compiler_default_options();
    memset(target, 0, sizeof(*target));
    target->allocator = allocator;
    options.max_documents = MMAY_CLI_CHECK_MAX_FILES;
    options.max_path_bytes = MMAY_CLI_CHECK_MAX_PATH_BYTES;
    options.max_source_bytes = MMAY_CLI_CHECK_MAX_SOURCE_BYTES;
    if (mmay_compiler_service_init(
            &target->service, allocator, options
        ) != MMAY_COMPILER_OK) {
        return false;
    }
    target->service_initialized = true;
    if (!mmay_vector_init(
            &target->packages, allocator, sizeof(MarimayCliCheckPackage)
        )) {
        return false;
    }
    target->packages_initialized = true;
    return true;
}

static void mmay_cp37_standard_package_allocation_failures(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    size_t allocation_attempts = 0;
    for (failure_index = 0; failure_index < 4096; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayCliCheckTarget target;
        MarimayCliCheckTargetStatus status =
            MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        if (mmay_cp37_standard_target_init(
                &target, mmay_fail_allocator(&state)
            )) {
            status = mmay_cli_standard_packages_add(&target);
        }
        reached_success = status == MMAY_CLI_CHECK_TARGET_OK;
        MMAY_EXPECT(reached_success
            || status == MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED);
        if (reached_success) {
            MMAY_EXPECT(target.packages.length == 9);
            MMAY_EXPECT(target.service.documents.length == 14);
            allocation_attempts = state.attempts;
        }
        mmay_cli_check_target_dispose(&target);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
    MMAY_EXPECT(allocation_attempts == failure_index);
}

void mmay_test_cp37_cli_parser(void) {
    mmay_cp37_standard_package_allocation_failures();
}
