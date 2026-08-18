#ifndef RUDA_NATIVE_RUNTIME_ARTIFACT_H
#define RUDA_NATIVE_RUNTIME_ARTIFACT_H

#include <stdbool.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"

typedef enum RudaNativeRuntimeProfile {
    RUDA_NATIVE_RUNTIME_DEBUG,
    RUDA_NATIVE_RUNTIME_RELEASE
} RudaNativeRuntimeProfile;

typedef struct RudaNativeRuntimePaths {
    const char *archive;
    const char *manifest;
    const char *bootstrap_include;
    const char *runtime_include;
} RudaNativeRuntimePaths;

typedef struct RudaNativeRuntimeArtifact {
    RudaOwnedBytes archive;
    RudaOwnedBytes manifest;
    RudaOwnedBytes bootstrap_include;
    RudaOwnedBytes runtime_include;
    char content_hash[65];
    RudaNativeRuntimeProfile profile;
    bool installed;
} RudaNativeRuntimeArtifact;

typedef enum RudaNativeRuntimeArtifactStatus {
    RUDA_NATIVE_RUNTIME_ARTIFACT_OK,
    RUDA_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT,
    RUDA_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED,
    RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED,
    RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING,
    RUDA_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE,
    RUDA_NATIVE_RUNTIME_ARTIFACT_IO_FAILED
} RudaNativeRuntimeArtifactStatus;

RudaNativeRuntimeArtifactStatus ruda_native_runtime_artifact_select(
    RudaAllocator allocator,
    RudaNativeRuntimeProfile profile,
    const char *tool_executable_path,
    const RudaNativeRuntimePaths *override_paths,
    RudaNativeRuntimeArtifact *artifact
);

/* A successful artifact owns all four paths until this call. Failures own none. */
void ruda_native_runtime_artifact_dispose(
    RudaNativeRuntimeArtifact *artifact
);

#endif
