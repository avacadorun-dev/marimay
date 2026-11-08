#ifndef MMAY_NATIVE_HOST_TOOLCHAIN_H
#define MMAY_NATIVE_HOST_TOOLCHAIN_H

#include "mmay/native/toolchain.h"

enum {
    MMAY_NATIVE_HOST_VERSION_CAPACITY = 64,
    MMAY_NATIVE_HOST_BUILD_CAPACITY = 96,
    MMAY_NATIVE_HOST_TARGET_CAPACITY = 128,
    MMAY_NATIVE_HOST_ARCH_CAPACITY = 16
};

typedef struct MarimayNativeHostToolchain {
    /* Borrowed distribution-owned strings; this value owns no allocations. */
    const char *path;
    const char *id;
    const char *policy;
    bool portable_macos;
    char architecture[MMAY_NATIVE_HOST_ARCH_CAPACITY];
    char version[MMAY_NATIVE_HOST_VERSION_CAPACITY];
    char build[MMAY_NATIVE_HOST_BUILD_CAPACITY];
    char target[MMAY_NATIVE_HOST_TARGET_CAPACITY];
    char fingerprint[65];
} MarimayNativeHostToolchain;

/* The configured build-tree executable retains the pinned policy. */
bool mmay_native_host_toolchain_is_installed(const char *tool_executable_path);

/* Selects and probes one distribution-owned host policy. Portable installed
 * macOS rejects a different compiler_override. Other policies retain the
 * existing exact configured-compiler probe and trusted internal override.
 * Output owns bounded probe bytes only on success; dispose it after use. */
MarimayNativeToolchainStatus mmay_native_host_toolchain_probe(
    MarimayAllocator allocator,
    const char *tool_executable_path,
    const char *compiler_override,
    MarimayNativeHostToolchain *toolchain,
    MarimayOwnedBytes *identity_output
);

/* Writes the borrowed, nonterminated command prefix shared by probing and
 * compilation. Returns zero on invalid input/insufficient capacity. */
size_t mmay_native_host_toolchain_command(
    const MarimayNativeHostToolchain *toolchain,
    const char **arguments,
    size_t capacity
);

#endif
