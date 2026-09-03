#ifndef MMAY_NATIVE_TOOLCHAIN_H
#define MMAY_NATIVE_TOOLCHAIN_H

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

typedef enum MarimayNativeToolchainStatus {
    MMAY_NATIVE_TOOLCHAIN_OK,
    MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT,
    MMAY_NATIVE_TOOLCHAIN_ALLOCATION_FAILED,
    MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE
} MarimayNativeToolchainStatus;

MarimayNativeToolchainStatus mmay_native_toolchain_probe(
    MarimayAllocator allocator,
    const char *compiler,
    const char *expected_version,
    MarimayOwnedBytes *identity_output
);

#endif
