#ifndef MMAY_NATIVE_RUNTIME_ARTIFACT_H
#define MMAY_NATIVE_RUNTIME_ARTIFACT_H

#include <stdbool.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"

typedef enum MarimayNativeRuntimeProfile {
    MMAY_NATIVE_RUNTIME_DEBUG,
    MMAY_NATIVE_RUNTIME_RELEASE
} MarimayNativeRuntimeProfile;

typedef struct MarimayNativeRuntimePaths {
    const char *archive;
    const char *manifest;
    const char *bootstrap_include;
    const char *runtime_include;
} MarimayNativeRuntimePaths;

typedef struct MarimayNativeRuntimeArtifact {
    MarimayOwnedBytes archive;
    MarimayOwnedBytes manifest;
    MarimayOwnedBytes bootstrap_include;
    MarimayOwnedBytes runtime_include;
    char content_hash[65];
    MarimayNativeRuntimeProfile profile;
    bool installed;
} MarimayNativeRuntimeArtifact;

typedef enum MarimayNativeRuntimeArtifactStatus {
    MMAY_NATIVE_RUNTIME_ARTIFACT_OK,
    MMAY_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT,
    MMAY_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED,
    MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED,
    MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING,
    MMAY_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE,
    MMAY_NATIVE_RUNTIME_ARTIFACT_IO_FAILED
} MarimayNativeRuntimeArtifactStatus;

MarimayNativeRuntimeArtifactStatus mmay_native_runtime_artifact_select(
    MarimayAllocator allocator,
    MarimayNativeRuntimeProfile profile,
    const char *tool_executable_path,
    const MarimayNativeRuntimePaths *override_paths,
    MarimayNativeRuntimeArtifact *artifact
);

/* A successful artifact owns all four paths until this call. Failures own none. */
void mmay_native_runtime_artifact_dispose(
    MarimayNativeRuntimeArtifact *artifact
);

#endif
