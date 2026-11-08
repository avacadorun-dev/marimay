#include "mmay/native/toolchain.h"

#include <string.h>

#include "mmay/platform/process.h"

enum { MMAY_NATIVE_TOOLCHAIN_MAX_OUTPUT = 64 * 1024 };

#if defined(_WIN32)
static size_t mmay_native_toolchain_version_length(
    const unsigned char *version,
    size_t length
) {
    size_t index = 0;
    size_t components = 1;
    bool has_digit = false;
    for (index = 0; index < length; index += 1) {
        unsigned char byte = version[index];
        if (byte >= '0' && byte <= '9') {
            has_digit = true;
        } else if (byte == '.' && has_digit && components < 4) {
            components += 1;
            has_digit = false;
        } else {
            return 0;
        }
    }
    if (!has_digit || components < 3) return 0;
    /* MSVC's omitted fourth field is revision zero, not any revision. */
    return components == 4 && length >= 2
            && version[length - 2] == '.' && version[length - 1] == '0'
        ? length - 2 : length;
}
#else
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
#endif

static bool mmay_native_toolchain_contains(
    const MarimayOwnedBytes *output,
    const char *expected,
    size_t expected_length
) {
    size_t index = 0;
    if (expected_length == 0 || output->length < expected_length) return false;
#if defined(_WIN32)
    while (index < output->length) {
        size_t start = index;
        size_t length = 0;
        unsigned char byte = output->data[index];
        if ((byte < '0' || byte > '9')
            && byte != '.') {
            index += 1;
            continue;
        }
        do {
            index += 1;
            if (index == output->length) break;
            byte = output->data[index];
        } while ((byte >= '0' && byte <= '9') || byte == '.');
        length = mmay_native_toolchain_version_length(
            output->data + start, index - start
        );
        if (length == expected_length
            && memcmp(output->data + start, expected, length) == 0) return true;
    }
#else
    for (index = 0; index <= output->length - expected_length; index += 1) {
        if (memcmp(output->data + index, expected, expected_length) == 0) {
            return true;
        }
    }
#endif
    return false;
}

MarimayNativeToolchainStatus mmay_native_toolchain_probe(
    MarimayAllocator allocator,
    const char *compiler,
    const char *expected_version,
    MarimayOwnedBytes *identity_output
) {
    const char *arguments[3];
    MarimayProcessResult process;
    MarimayProcessStatus status;
    size_t expected_length = 0;
    if (expected_version != NULL) {
#if defined(_WIN32)
        expected_length = mmay_native_toolchain_version_length(
            (const unsigned char *)expected_version, strlen(expected_version)
        );
#else
        expected_length = mmay_native_toolchain_version_prefix_length(expected_version);
#endif
    }
    if (identity_output == NULL || compiler == NULL || compiler[0] == '\0'
        || expected_version == NULL || expected_length == 0) {
        return MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT;
    }
    memset(identity_output, 0, sizeof(*identity_output));
    memset(&process, 0, sizeof(process));
    arguments[0] = compiler;
#if defined(_WIN32)
    /* Help includes the compiler's version banner and succeeds without a
     * source filename. /Bv is a compilation option, not a standalone query. */
    arguments[1] = "/?";
#else
    arguments[1] = "--version";
#endif
    arguments[2] = NULL;
    status = mmay_process_run(
        allocator, arguments, 2, MMAY_NATIVE_TOOLCHAIN_MAX_OUTPUT, &process
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
