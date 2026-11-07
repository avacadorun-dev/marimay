#include "mmay/native/toolchain.h"

#include <string.h>

#include "mmay/platform/process.h"

enum { MMAY_NATIVE_TOOLCHAIN_MAX_OUTPUT = 64 * 1024 };

static size_t mmay_native_toolchain_version_prefix_length(
    const char *version
) {
    size_t length = 0;
    size_t separators = 0;
    while (version[length] != '\0') {
        unsigned char byte = (unsigned char)version[length];
        if (byte >= '0' && byte <= '9') {
            length += 1;
        } else if (byte == '.' && separators < 2) {
            separators += 1;
            length += 1;
        } else {
            break;
        }
    }
    return separators == 2 ? length : 0;
}

static bool mmay_native_toolchain_contains(
    const MarimayOwnedBytes *output,
    const char *expected,
    size_t expected_length
) {
    size_t index = 0;
    if (expected_length == 0 || output->length < expected_length) return false;
    for (index = 0; index <= output->length - expected_length; index += 1) {
        if (memcmp(output->data + index, expected, expected_length) == 0) {
            return true;
        }
    }
    return false;
}

MarimayNativeToolchainStatus mmay_native_toolchain_probe(
    MarimayAllocator allocator,
    const char *compiler,
    const char *expected_version,
    MarimayOwnedBytes *identity_output
) {
    const char *arguments[4];
    size_t argument_count = 2;
    MarimayProcessResult process;
    MarimayProcessStatus status;
    size_t expected_length = expected_version != NULL
        ? mmay_native_toolchain_version_prefix_length(expected_version) : 0;
    if (identity_output == NULL || compiler == NULL || compiler[0] == '\0'
        || expected_version == NULL || expected_length == 0) {
        return MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT;
    }
    memset(identity_output, 0, sizeof(*identity_output));
    memset(&process, 0, sizeof(process));
    arguments[0] = compiler;
#if defined(_WIN32)
    /* /Bv alone prints a missing-source error. Help keeps the version query
     * successful without compiling a file or creating a temporary artifact. */
    arguments[1] = "/Bv";
    arguments[2] = "/?";
    argument_count = 3;
#else
    arguments[1] = "--version";
#endif
    arguments[argument_count] = NULL;
    status = mmay_process_run(
        allocator, arguments, argument_count, MMAY_NATIVE_TOOLCHAIN_MAX_OUTPUT, &process
    );
    if (status != MMAY_PROCESS_OK) {
        mmay_process_result_dispose(&process);
        return status == MMAY_PROCESS_ALLOCATION_FAILED
            ? MMAY_NATIVE_TOOLCHAIN_ALLOCATION_FAILED
            : MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE;
    }
    if (process.signaled || process.exit_code != 0
        || !mmay_native_toolchain_contains(
            &process.output, expected_version, expected_length
        )) {
        mmay_process_result_dispose(&process);
        return MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE;
    }
    *identity_output = process.output;
    memset(&process.output, 0, sizeof(process.output));
    mmay_process_result_dispose(&process);
    return MMAY_NATIVE_TOOLCHAIN_OK;
}
