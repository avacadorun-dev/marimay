#ifndef MMAY_NATIVE_HOST_TOOLCHAIN_INTERNAL_H
#define MMAY_NATIVE_HOST_TOOLCHAIN_INTERNAL_H

#include "mmay/native/host_toolchain.h"
#include "mmay/platform/process.h"

/* The process boundary is injectable only inside this owner and its focused
 * tests. Production always uses the ordinary bounded platform process owner. */
typedef MarimayProcessStatus (*MarimayNativeHostProbeRunner)(
    void *context,
    MarimayAllocator allocator,
    const char *const arguments[],
    size_t argument_count,
    size_t max_output,
    MarimayProcessResult *result
);

/* Stable serialization of the selected, bounded identity into cache input. */
bool mmay_native_host_toolchain_fingerprint(MarimayNativeHostToolchain *toolchain);

bool mmay_native_host_toolchain_parse_macos(
    const MarimayOwnedBytes *output,
    const char *architecture,
    MarimayNativeHostToolchain *toolchain
);

MarimayNativeToolchainStatus mmay_native_host_toolchain_probe_macos(
    MarimayAllocator allocator,
    const char *architecture,
    MarimayNativeHostProbeRunner runner,
    void *context,
    MarimayNativeHostToolchain *toolchain,
    MarimayOwnedBytes *identity_output
);

#endif
