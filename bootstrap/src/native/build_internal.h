#ifndef MMAY_NATIVE_BUILD_INTERNAL_H
#define MMAY_NATIVE_BUILD_INTERNAL_H

#include "mmay/native/build.h"
#include "mmay/native/host_toolchain.h"

/* Pure cache identity; callers must still validate runtime/toolchain artifacts. */
bool mmay_native_build_hash(
    const MarimayOwnedBytes *source,
    const MarimayNativeBuildRequest *request,
    const MarimayNativeRuntimeArtifact *runtime,
    const MarimayNativeHostToolchain *toolchain,
    char output[65]
);

#endif
