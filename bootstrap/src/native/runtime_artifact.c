#include "ruda/native/runtime_artifact.h"

#include <stdio.h>
#include <string.h>

#include "ruda/base/hash.h"
#include "ruda/build_config.h"
#include "ruda/platform/directory.h"
#include "ruda/platform/file.h"

enum {
    RUDA_NATIVE_RUNTIME_MAX_PATH = 32768,
    RUDA_NATIVE_RUNTIME_MAX_MANIFEST = 1024
};

static bool ruda_native_runtime_path_length(
    const char *path,
    size_t *length
) {
    size_t index = 0;
    if (path == NULL || length == NULL) return false;
    while (index <= RUDA_NATIVE_RUNTIME_MAX_PATH && path[index] != 0) {
        index += 1;
    }
    if (index == 0 || index > RUDA_NATIVE_RUNTIME_MAX_PATH) return false;
    *length = index;
    return true;
}

static RudaNativeRuntimeArtifactStatus ruda_native_runtime_file_status(
    RudaFileStatus status
) {
    if (status == RUDA_FILE_ALLOCATION_FAILED) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED;
    }
    if (status == RUDA_FILE_LIMIT_EXCEEDED) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    return RUDA_NATIVE_RUNTIME_ARTIFACT_IO_FAILED;
}

static RudaNativeRuntimeArtifactStatus ruda_native_runtime_copy(
    RudaAllocator allocator,
    const char *value,
    RudaOwnedBytes *output
) {
    size_t length = 0;
    if (!ruda_native_runtime_path_length(value, &length)) {
        return value == NULL || value[0] == 0
            ? RUDA_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT
            : RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    return ruda_owned_bytes_copy(output, allocator, value, length)
        ? RUDA_NATIVE_RUNTIME_ARTIFACT_OK
        : RUDA_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED;
}

static RudaNativeRuntimeArtifactStatus ruda_native_runtime_join(
    RudaAllocator allocator,
    const char *left,
    size_t left_length,
    const char *right,
    RudaOwnedBytes *output
) {
    size_t right_length = right != NULL ? strlen(right) : 0;
    bool separator = left_length != 0 && left[left_length - 1] != '/'
        && left[left_length - 1] != '\\';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (left == NULL || left_length == 0 || right_length == 0) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT;
    }
    if (!ruda_size_add(left_length, separator ? 1 : 0, &length)
        || !ruda_size_add(length, right_length, &length)
        || length > RUDA_NATIVE_RUNTIME_MAX_PATH
        || !ruda_size_add(length, 1, &allocation_size)) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    bytes = ruda_allocate(&allocator, allocation_size);
    if (bytes == NULL) return RUDA_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED;
    memcpy(bytes, left, left_length);
    if (separator) bytes[left_length] = '/';
    memcpy(bytes + left_length + (separator ? 1 : 0), right, right_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return RUDA_NATIVE_RUNTIME_ARTIFACT_OK;
}

static bool ruda_native_runtime_match_byte(
    const char *marker,
    size_t marker_length,
    size_t *matched,
    unsigned char byte
) {
    if (byte == (unsigned char)marker[*matched]) {
        *matched += 1;
    } else {
        *matched = byte == (unsigned char)marker[0] ? 1 : 0;
    }
    if (*matched == marker_length) {
        *matched = 0;
        return true;
    }
    return false;
}

static RudaNativeRuntimeArtifactStatus ruda_native_runtime_hash_archive(
    RudaAllocator allocator,
    const char *path,
    const char *profile,
    char output[65]
) {
    char profile_marker[128];
    char version_marker[128];
    unsigned char buffer[8192];
    unsigned char digest[32];
    RudaFileReadOps ops = ruda_platform_file_read_ops();
    RudaFileHandle handle = 0;
    RudaSha256 hash;
    RudaFileStatus file_status;
    size_t profile_length = 0;
    size_t version_length = 0;
    size_t profile_matched = 0;
    size_t version_matched = 0;
    bool profile_found = false;
    bool version_found = false;
    bool opened = false;
    bool end = false;
    int written = snprintf(
        profile_marker, sizeof(profile_marker),
        "ruda-native-runtime-profile-v1:%s", profile
    );
    RudaNativeRuntimeArtifactStatus status =
        RUDA_NATIVE_RUNTIME_ARTIFACT_OK;
    if (written < 0 || (size_t)written >= sizeof(profile_marker)) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    profile_length = (size_t)written;
    written = snprintf(
        version_marker, sizeof(version_marker),
        "ruda-native-runtime-version-v1:%s", RUDA_RUNTIME_VERSION
    );
    if (written < 0 || (size_t)written >= sizeof(version_marker)) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    version_length = (size_t)written;
    file_status = ops.open(
        ops.context, allocator, path, strlen(path), &handle
    );
    if (file_status != RUDA_FILE_OK) {
        return ruda_native_runtime_file_status(file_status);
    }
    opened = true;
    ruda_sha256_init(&hash);
    while (!end) {
        size_t count = 0;
        size_t index = 0;
        file_status = ops.read(
            ops.context, handle, buffer, sizeof(buffer), &count, &end
        );
        if (file_status != RUDA_FILE_OK) {
            status = ruda_native_runtime_file_status(file_status);
            goto cleanup;
        }
        if (!ruda_sha256_update(&hash, buffer, count)) {
            status = RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
            goto cleanup;
        }
        for (index = 0; index < count; index += 1) {
            if (!profile_found && ruda_native_runtime_match_byte(
                    profile_marker, profile_length,
                    &profile_matched, buffer[index]
                )) profile_found = true;
            if (!version_found && ruda_native_runtime_match_byte(
                    version_marker, version_length,
                    &version_matched, buffer[index]
                )) version_found = true;
        }
    }
    ruda_sha256_final(&hash, digest);
    ruda_sha256_hex(digest, output);
    if (!profile_found || !version_found) {
        status = RUDA_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE;
    }
cleanup:
    if (opened && ops.close(ops.context, handle) != RUDA_FILE_OK
        && status == RUDA_NATIVE_RUNTIME_ARTIFACT_OK) {
        status = RUDA_NATIVE_RUNTIME_ARTIFACT_IO_FAILED;
    }
    return status;
}

static RudaNativeRuntimeArtifactStatus ruda_native_runtime_manifest(
    RudaAllocator allocator,
    const char *path,
    const char *profile
) {
    char expected[RUDA_NATIVE_RUNTIME_MAX_MANIFEST];
    RudaOwnedBytes actual = {0};
    RudaFileStatus file_status;
    RudaNativeRuntimeArtifactStatus status =
        RUDA_NATIVE_RUNTIME_ARTIFACT_OK;
    int written = snprintf(
        expected, sizeof(expected),
        "ruda-native-runtime-v1\nprofile=%s\nruntime=%s\nhost=%s\n"
        "processor=%s\ncompiler=%s-%s\n",
        profile, RUDA_RUNTIME_VERSION, RUDA_HOST_SYSTEM,
        RUDA_HOST_PROCESSOR, RUDA_NATIVE_C_COMPILER_ID,
        RUDA_NATIVE_C_COMPILER_VERSION
    );
    if (written < 0 || (size_t)written >= sizeof(expected)) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    file_status = ruda_file_read_all(
        allocator, path, strlen(path),
        RUDA_NATIVE_RUNTIME_MAX_MANIFEST, &actual
    );
    if (file_status != RUDA_FILE_OK) {
        return ruda_native_runtime_file_status(file_status);
    }
    if (actual.length != (size_t)written
        || memcmp(actual.data, expected, actual.length) != 0) {
        status = RUDA_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE;
    }
    ruda_owned_bytes_dispose(&actual);
    return status;
}

static RudaNativeRuntimeArtifactStatus ruda_native_runtime_kind(
    RudaAllocator allocator,
    const char *path,
    RudaDirectoryEntryKind expected,
    bool *missing
) {
    RudaDirectoryEntryKind kind = RUDA_DIRECTORY_ENTRY_MISSING;
    RudaFileStatus file_status = ruda_directory_kind(
        allocator, path, strlen(path), &kind
    );
    if (file_status != RUDA_FILE_OK) {
        return ruda_native_runtime_file_status(file_status);
    }
    if (kind == RUDA_DIRECTORY_ENTRY_MISSING) {
        *missing = true;
        return RUDA_NATIVE_RUNTIME_ARTIFACT_OK;
    }
    return kind == expected
        ? RUDA_NATIVE_RUNTIME_ARTIFACT_OK
        : RUDA_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE;
}

static RudaNativeRuntimeArtifactStatus ruda_native_runtime_inspect(
    RudaAllocator allocator,
    RudaNativeRuntimeProfile selected_profile,
    const RudaNativeRuntimePaths *paths,
    bool installed,
    bool *present,
    RudaNativeRuntimeArtifact *artifact
) {
    const char *profile = selected_profile == RUDA_NATIVE_RUNTIME_RELEASE
        ? "release" : "debug";
    bool archive_missing = false;
    bool manifest_missing = false;
    bool include_missing = false;
    size_t ignored_length = 0;
    RudaNativeRuntimeArtifactStatus status;
    if (paths == NULL
        || !ruda_native_runtime_path_length(
            paths->archive, &ignored_length
        )
        || !ruda_native_runtime_path_length(
            paths->manifest, &ignored_length
        )
        || !ruda_native_runtime_path_length(
            paths->bootstrap_include, &ignored_length
        )
        || !ruda_native_runtime_path_length(
            paths->runtime_include, &ignored_length
        )) return RUDA_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT;
    status = ruda_native_runtime_kind(
        allocator, paths->archive, RUDA_DIRECTORY_ENTRY_REGULAR,
        &archive_missing
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = ruda_native_runtime_kind(
        allocator, paths->manifest, RUDA_DIRECTORY_ENTRY_REGULAR,
        &manifest_missing
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    *present = !archive_missing || !manifest_missing;
    if (archive_missing && manifest_missing) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_OK;
    }
    if (archive_missing || manifest_missing) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING;
    }
    status = ruda_native_runtime_kind(
        allocator, paths->bootstrap_include, RUDA_DIRECTORY_ENTRY_DIRECTORY,
        &include_missing
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    if (include_missing) return RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING;
    status = ruda_native_runtime_kind(
        allocator, paths->runtime_include, RUDA_DIRECTORY_ENTRY_DIRECTORY,
        &include_missing
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    if (include_missing) return RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING;
    status = ruda_native_runtime_manifest(
        allocator, paths->manifest, profile
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = ruda_native_runtime_hash_archive(
        allocator, paths->archive, profile, artifact->content_hash
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = ruda_native_runtime_copy(
        allocator, paths->archive, &artifact->archive
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) goto cleanup;
    status = ruda_native_runtime_copy(
        allocator, paths->manifest, &artifact->manifest
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) goto cleanup;
    status = ruda_native_runtime_copy(
        allocator, paths->bootstrap_include, &artifact->bootstrap_include
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) goto cleanup;
    status = ruda_native_runtime_copy(
        allocator, paths->runtime_include, &artifact->runtime_include
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) goto cleanup;
    artifact->profile = selected_profile;
    artifact->installed = installed;
    return RUDA_NATIVE_RUNTIME_ARTIFACT_OK;
cleanup:
    ruda_native_runtime_artifact_dispose(artifact);
    return status;
}

static RudaNativeRuntimeArtifactStatus ruda_native_runtime_installed_paths(
    RudaAllocator allocator,
    RudaNativeRuntimeProfile profile,
    const char *tool_path,
    RudaNativeRuntimePaths *paths,
    RudaOwnedBytes owned[6]
) {
    size_t length = 0;
    size_t separator = SIZE_MAX;
    size_t index = 0;
    const char *profile_name = profile == RUDA_NATIVE_RUNTIME_RELEASE
        ? "release" : "debug";
    const char *archive_name = profile == RUDA_NATIVE_RUNTIME_RELEASE
        ? RUDA_NATIVE_RUNTIME_RELEASE_FILENAME
        : RUDA_NATIVE_RUNTIME_DEBUG_FILENAME;
    RudaNativeRuntimeArtifactStatus status;
    if (tool_path == NULL) return RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING;
    if (!ruda_native_runtime_path_length(tool_path, &length)) {
        return tool_path[0] == 0
            ? RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING
            : RUDA_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    for (index = 0; index < length; index += 1) {
        if (tool_path[index] == '/' || tool_path[index] == '\\') {
            separator = index;
        }
    }
    if (separator == SIZE_MAX) return RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING;
    if (!ruda_owned_bytes_copy(
            &owned[0], allocator, tool_path, separator == 0 ? 1 : separator
        )) return RUDA_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED;
    status = ruda_native_runtime_join(
        allocator, (const char *)owned[0].data, owned[0].length,
        RUDA_NATIVE_INSTALLED_RUNTIME_RELATIVE, &owned[1]
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = ruda_native_runtime_join(
        allocator, (const char *)owned[1].data, owned[1].length,
        profile_name, &owned[2]
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = ruda_native_runtime_join(
        allocator, (const char *)owned[2].data, owned[2].length,
        archive_name, &owned[3]
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = ruda_native_runtime_join(
        allocator, (const char *)owned[2].data, owned[2].length,
        "ruda-native-runtime.manifest", &owned[4]
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = ruda_native_runtime_join(
        allocator, (const char *)owned[0].data, owned[0].length,
        RUDA_NATIVE_INSTALLED_INCLUDE_RELATIVE, &owned[5]
    );
    if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    paths->archive = (const char *)owned[3].data;
    paths->manifest = (const char *)owned[4].data;
    paths->bootstrap_include = (const char *)owned[5].data;
    paths->runtime_include = (const char *)owned[5].data;
    return RUDA_NATIVE_RUNTIME_ARTIFACT_OK;
}

static void ruda_native_runtime_owned_paths_dispose(RudaOwnedBytes owned[6]) {
    size_t index = 0;
    for (index = 0; index < 6; index += 1) {
        ruda_owned_bytes_dispose(&owned[index]);
    }
}

RudaNativeRuntimeArtifactStatus ruda_native_runtime_artifact_select(
    RudaAllocator allocator,
    RudaNativeRuntimeProfile profile,
    const char *tool_executable_path,
    const RudaNativeRuntimePaths *override_paths,
    RudaNativeRuntimeArtifact *artifact
) {
    RudaNativeRuntimePaths configured;
    RudaNativeRuntimePaths installed_paths;
    RudaOwnedBytes installed_owned[6] = {{0}};
    bool present = false;
    RudaNativeRuntimeArtifactStatus status;
    if (artifact == NULL || (profile != RUDA_NATIVE_RUNTIME_DEBUG
            && profile != RUDA_NATIVE_RUNTIME_RELEASE)) {
        return RUDA_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT;
    }
    memset(artifact, 0, sizeof(*artifact));
    if (override_paths != NULL) {
        if (override_paths->archive == NULL
            || override_paths->manifest == NULL
            || override_paths->bootstrap_include == NULL
            || override_paths->runtime_include == NULL) {
            return RUDA_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT;
        }
        status = ruda_native_runtime_inspect(
            allocator, profile, override_paths, false, &present, artifact
        );
        return status == RUDA_NATIVE_RUNTIME_ARTIFACT_OK && !present
            ? RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING : status;
    }
    memset(&installed_paths, 0, sizeof(installed_paths));
    status = ruda_native_runtime_installed_paths(
        allocator, profile, tool_executable_path,
        &installed_paths, installed_owned
    );
    if (status == RUDA_NATIVE_RUNTIME_ARTIFACT_OK) {
        status = ruda_native_runtime_inspect(
            allocator, profile, &installed_paths, true, &present, artifact
        );
        if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_OK || present) {
            ruda_native_runtime_owned_paths_dispose(installed_owned);
            return status;
        }
    } else if (status != RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING) {
        ruda_native_runtime_owned_paths_dispose(installed_owned);
        return status;
    }
    ruda_native_runtime_owned_paths_dispose(installed_owned);
    configured.archive = profile == RUDA_NATIVE_RUNTIME_RELEASE
        ? RUDA_NATIVE_RUNTIME_RELEASE_ARCHIVE
        : RUDA_NATIVE_RUNTIME_DEBUG_ARCHIVE;
    configured.manifest = profile == RUDA_NATIVE_RUNTIME_RELEASE
        ? RUDA_NATIVE_RUNTIME_RELEASE_MANIFEST
        : RUDA_NATIVE_RUNTIME_DEBUG_MANIFEST;
    configured.bootstrap_include = RUDA_NATIVE_BOOTSTRAP_INCLUDE;
    configured.runtime_include = RUDA_NATIVE_RUNTIME_INCLUDE;
    status = ruda_native_runtime_inspect(
        allocator, profile, &configured, false, &present, artifact
    );
    return status == RUDA_NATIVE_RUNTIME_ARTIFACT_OK && !present
        ? RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING : status;
}

void ruda_native_runtime_artifact_dispose(
    RudaNativeRuntimeArtifact *artifact
) {
    if (artifact == NULL) return;
    ruda_owned_bytes_dispose(&artifact->archive);
    ruda_owned_bytes_dispose(&artifact->manifest);
    ruda_owned_bytes_dispose(&artifact->bootstrap_include);
    ruda_owned_bytes_dispose(&artifact->runtime_include);
    memset(artifact->content_hash, 0, sizeof(artifact->content_hash));
    artifact->profile = RUDA_NATIVE_RUNTIME_DEBUG;
    artifact->installed = false;
}
