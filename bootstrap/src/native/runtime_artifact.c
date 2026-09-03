#include "mmay/native/runtime_artifact.h"

#include <stdio.h>
#include <string.h>

#include "mmay/base/hash.h"
#include "mmay/build_config.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/file.h"

enum {
    MMAY_NATIVE_RUNTIME_MAX_PATH = 32768,
    MMAY_NATIVE_RUNTIME_MAX_MANIFEST = 1024
};

static bool mmay_native_runtime_path_length(
    const char *path,
    size_t *length
) {
    size_t index = 0;
    if (path == NULL || length == NULL) return false;
    while (index <= MMAY_NATIVE_RUNTIME_MAX_PATH && path[index] != 0) {
        index += 1;
    }
    if (index == 0 || index > MMAY_NATIVE_RUNTIME_MAX_PATH) return false;
    *length = index;
    return true;
}

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_file_status(
    MarimayFileStatus status
) {
    if (status == MMAY_FILE_ALLOCATION_FAILED) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED;
    }
    if (status == MMAY_FILE_LIMIT_EXCEEDED) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    return MMAY_NATIVE_RUNTIME_ARTIFACT_IO_FAILED;
}

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_copy(
    MarimayAllocator allocator,
    const char *value,
    MarimayOwnedBytes *output
) {
    size_t length = 0;
    if (!mmay_native_runtime_path_length(value, &length)) {
        return value == NULL || value[0] == 0
            ? MMAY_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT
            : MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    return mmay_owned_bytes_copy(output, allocator, value, length)
        ? MMAY_NATIVE_RUNTIME_ARTIFACT_OK
        : MMAY_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED;
}

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_join(
    MarimayAllocator allocator,
    const char *left,
    size_t left_length,
    const char *right,
    MarimayOwnedBytes *output
) {
    size_t right_length = right != NULL ? strlen(right) : 0;
    bool separator = left_length != 0 && left[left_length - 1] != '/'
        && left[left_length - 1] != '\\';
    size_t length = 0;
    size_t allocation_size = 0;
    unsigned char *bytes = NULL;
    memset(output, 0, sizeof(*output));
    if (left == NULL || left_length == 0 || right_length == 0) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT;
    }
    if (!mmay_size_add(left_length, separator ? 1 : 0, &length)
        || !mmay_size_add(length, right_length, &length)
        || length > MMAY_NATIVE_RUNTIME_MAX_PATH
        || !mmay_size_add(length, 1, &allocation_size)) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    bytes = mmay_allocate(&allocator, allocation_size);
    if (bytes == NULL) return MMAY_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED;
    memcpy(bytes, left, left_length);
    if (separator) bytes[left_length] = '/';
    memcpy(bytes + left_length + (separator ? 1 : 0), right, right_length);
    bytes[length] = 0;
    output->allocator = allocator;
    output->data = bytes;
    output->length = length;
    return MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
}

static bool mmay_native_runtime_match_byte(
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

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_hash_archive(
    MarimayAllocator allocator,
    const char *path,
    const char *profile,
    char output[65]
) {
    char profile_marker[128];
    char version_marker[128];
    unsigned char buffer[8192];
    unsigned char digest[32];
    MarimayFileReadOps ops = mmay_platform_file_read_ops();
    MarimayFileHandle handle = 0;
    MarimaySha256 hash;
    MarimayFileStatus file_status;
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
        "mmay-native-runtime-profile-v1:%s", profile
    );
    MarimayNativeRuntimeArtifactStatus status =
        MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
    if (written < 0 || (size_t)written >= sizeof(profile_marker)) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    profile_length = (size_t)written;
    written = snprintf(
        version_marker, sizeof(version_marker),
        "mmay-native-runtime-version-v1:%s", MMAY_RUNTIME_VERSION
    );
    if (written < 0 || (size_t)written >= sizeof(version_marker)) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    version_length = (size_t)written;
    file_status = ops.open(
        ops.context, allocator, path, strlen(path), &handle
    );
    if (file_status != MMAY_FILE_OK) {
        return mmay_native_runtime_file_status(file_status);
    }
    opened = true;
    mmay_sha256_init(&hash);
    while (!end) {
        size_t count = 0;
        size_t index = 0;
        file_status = ops.read(
            ops.context, handle, buffer, sizeof(buffer), &count, &end
        );
        if (file_status != MMAY_FILE_OK) {
            status = mmay_native_runtime_file_status(file_status);
            goto cleanup;
        }
        if (!mmay_sha256_update(&hash, buffer, count)) {
            status = MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
            goto cleanup;
        }
        for (index = 0; index < count; index += 1) {
            if (!profile_found && mmay_native_runtime_match_byte(
                    profile_marker, profile_length,
                    &profile_matched, buffer[index]
                )) profile_found = true;
            if (!version_found && mmay_native_runtime_match_byte(
                    version_marker, version_length,
                    &version_matched, buffer[index]
                )) version_found = true;
        }
    }
    mmay_sha256_final(&hash, digest);
    mmay_sha256_hex(digest, output);
    if (!profile_found || !version_found) {
        status = MMAY_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE;
    }
cleanup:
    if (opened && ops.close(ops.context, handle) != MMAY_FILE_OK
        && status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK) {
        status = MMAY_NATIVE_RUNTIME_ARTIFACT_IO_FAILED;
    }
    return status;
}

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_manifest(
    MarimayAllocator allocator,
    const char *path,
    const char *profile
) {
    char expected[MMAY_NATIVE_RUNTIME_MAX_MANIFEST];
    MarimayOwnedBytes actual = {0};
    MarimayFileStatus file_status;
    MarimayNativeRuntimeArtifactStatus status =
        MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
    int written = snprintf(
        expected, sizeof(expected),
        "mmay-native-runtime-v1\nprofile=%s\nruntime=%s\nhost=%s\n"
        "processor=%s\ncompiler=%s-%s\n",
        profile, MMAY_RUNTIME_VERSION, MMAY_HOST_SYSTEM,
        MMAY_HOST_PROCESSOR, MMAY_NATIVE_C_COMPILER_ID,
        MMAY_NATIVE_C_COMPILER_VERSION
    );
    if (written < 0 || (size_t)written >= sizeof(expected)) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    file_status = mmay_file_read_all(
        allocator, path, strlen(path),
        MMAY_NATIVE_RUNTIME_MAX_MANIFEST, &actual
    );
    if (file_status != MMAY_FILE_OK) {
        return mmay_native_runtime_file_status(file_status);
    }
    if (actual.length != (size_t)written
        || memcmp(actual.data, expected, actual.length) != 0) {
        status = MMAY_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE;
    }
    mmay_owned_bytes_dispose(&actual);
    return status;
}

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_kind(
    MarimayAllocator allocator,
    const char *path,
    MarimayDirectoryEntryKind expected,
    bool *missing
) {
    MarimayDirectoryEntryKind kind = MMAY_DIRECTORY_ENTRY_MISSING;
    MarimayFileStatus file_status = mmay_directory_kind(
        allocator, path, strlen(path), &kind
    );
    if (file_status != MMAY_FILE_OK) {
        return mmay_native_runtime_file_status(file_status);
    }
    if (kind == MMAY_DIRECTORY_ENTRY_MISSING) {
        *missing = true;
        return MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
    }
    return kind == expected
        ? MMAY_NATIVE_RUNTIME_ARTIFACT_OK
        : MMAY_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE;
}

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_header(
    MarimayAllocator allocator,
    const char *include_root,
    const char *relative
) {
    MarimayOwnedBytes path = {0};
    bool missing = false;
    MarimayNativeRuntimeArtifactStatus status = mmay_native_runtime_join(
        allocator, include_root, strlen(include_root), relative, &path
    );
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK) {
        status = mmay_native_runtime_kind(
            allocator, (const char *)path.data,
            MMAY_DIRECTORY_ENTRY_REGULAR, &missing
        );
    }
    mmay_owned_bytes_dispose(&path);
    return status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK && missing
        ? MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING : status;
}

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_inspect(
    MarimayAllocator allocator,
    MarimayNativeRuntimeProfile selected_profile,
    const MarimayNativeRuntimePaths *paths,
    bool installed,
    bool *present,
    MarimayNativeRuntimeArtifact *artifact
) {
    const char *profile = selected_profile == MMAY_NATIVE_RUNTIME_RELEASE
        ? "release" : "debug";
    bool archive_missing = false;
    bool manifest_missing = false;
    bool include_missing = false;
    size_t ignored_length = 0;
    MarimayNativeRuntimeArtifactStatus status;
    if (paths == NULL
        || !mmay_native_runtime_path_length(
            paths->archive, &ignored_length
        )
        || !mmay_native_runtime_path_length(
            paths->manifest, &ignored_length
        )
        || !mmay_native_runtime_path_length(
            paths->bootstrap_include, &ignored_length
        )
        || !mmay_native_runtime_path_length(
            paths->runtime_include, &ignored_length
        )) return MMAY_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT;
    status = mmay_native_runtime_kind(
        allocator, paths->archive, MMAY_DIRECTORY_ENTRY_REGULAR,
        &archive_missing
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_kind(
        allocator, paths->manifest, MMAY_DIRECTORY_ENTRY_REGULAR,
        &manifest_missing
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    *present = !archive_missing || !manifest_missing;
    if (archive_missing && manifest_missing) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
    }
    if (archive_missing || manifest_missing) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING;
    }
    status = mmay_native_runtime_kind(
        allocator, paths->bootstrap_include, MMAY_DIRECTORY_ENTRY_DIRECTORY,
        &include_missing
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    if (include_missing) return MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING;
    status = mmay_native_runtime_kind(
        allocator, paths->runtime_include, MMAY_DIRECTORY_ENTRY_DIRECTORY,
        &include_missing
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    if (include_missing) return MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING;
    status = mmay_native_runtime_header(
        allocator, paths->bootstrap_include, "mmay/base/allocator.h"
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_header(
        allocator, paths->runtime_include, "mmay/runtime/core.h"
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_manifest(
        allocator, paths->manifest, profile
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_hash_archive(
        allocator, paths->archive, profile, artifact->content_hash
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_copy(
        allocator, paths->archive, &artifact->archive
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) goto cleanup;
    status = mmay_native_runtime_copy(
        allocator, paths->manifest, &artifact->manifest
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) goto cleanup;
    status = mmay_native_runtime_copy(
        allocator, paths->bootstrap_include, &artifact->bootstrap_include
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) goto cleanup;
    status = mmay_native_runtime_copy(
        allocator, paths->runtime_include, &artifact->runtime_include
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) goto cleanup;
    artifact->profile = selected_profile;
    artifact->installed = installed;
    return MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
cleanup:
    mmay_native_runtime_artifact_dispose(artifact);
    return status;
}

static MarimayNativeRuntimeArtifactStatus mmay_native_runtime_installed_paths(
    MarimayAllocator allocator,
    MarimayNativeRuntimeProfile profile,
    const char *tool_path,
    MarimayNativeRuntimePaths *paths,
    MarimayOwnedBytes owned[6]
) {
    size_t length = 0;
    size_t separator = SIZE_MAX;
    size_t index = 0;
    const char *profile_name = profile == MMAY_NATIVE_RUNTIME_RELEASE
        ? "release" : "debug";
    const char *archive_name = profile == MMAY_NATIVE_RUNTIME_RELEASE
        ? MMAY_NATIVE_RUNTIME_RELEASE_FILENAME
        : MMAY_NATIVE_RUNTIME_DEBUG_FILENAME;
    MarimayNativeRuntimeArtifactStatus status;
    if (tool_path == NULL) return MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING;
    if (!mmay_native_runtime_path_length(tool_path, &length)) {
        return tool_path[0] == 0
            ? MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING
            : MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
    }
    for (index = 0; index < length; index += 1) {
        if (tool_path[index] == '/' || tool_path[index] == '\\') {
            separator = index;
        }
    }
    if (separator == SIZE_MAX) return MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING;
    if (!mmay_owned_bytes_copy(
            &owned[0], allocator, tool_path, separator == 0 ? 1 : separator
        )) return MMAY_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED;
    status = mmay_native_runtime_join(
        allocator, (const char *)owned[0].data, owned[0].length,
        MMAY_NATIVE_INSTALLED_RUNTIME_RELATIVE, &owned[1]
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_join(
        allocator, (const char *)owned[1].data, owned[1].length,
        profile_name, &owned[2]
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_join(
        allocator, (const char *)owned[2].data, owned[2].length,
        archive_name, &owned[3]
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_join(
        allocator, (const char *)owned[2].data, owned[2].length,
        "mmay-native-runtime.manifest", &owned[4]
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    status = mmay_native_runtime_join(
        allocator, (const char *)owned[0].data, owned[0].length,
        MMAY_NATIVE_INSTALLED_INCLUDE_RELATIVE, &owned[5]
    );
    if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return status;
    paths->archive = (const char *)owned[3].data;
    paths->manifest = (const char *)owned[4].data;
    paths->bootstrap_include = (const char *)owned[5].data;
    paths->runtime_include = (const char *)owned[5].data;
    return MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
}

static void mmay_native_runtime_owned_paths_dispose(MarimayOwnedBytes owned[6]) {
    size_t index = 0;
    for (index = 0; index < 6; index += 1) {
        mmay_owned_bytes_dispose(&owned[index]);
    }
}

MarimayNativeRuntimeArtifactStatus mmay_native_runtime_artifact_select(
    MarimayAllocator allocator,
    MarimayNativeRuntimeProfile profile,
    const char *tool_executable_path,
    const MarimayNativeRuntimePaths *override_paths,
    MarimayNativeRuntimeArtifact *artifact
) {
    MarimayNativeRuntimePaths configured;
    MarimayNativeRuntimePaths installed_paths;
    MarimayOwnedBytes installed_owned[6];
    bool present = false;
    MarimayNativeRuntimeArtifactStatus status;
    if (artifact == NULL || (profile != MMAY_NATIVE_RUNTIME_DEBUG
            && profile != MMAY_NATIVE_RUNTIME_RELEASE)) {
        return MMAY_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT;
    }
    memset(artifact, 0, sizeof(*artifact));
    memset(installed_owned, 0, sizeof(installed_owned));
    if (override_paths != NULL) {
        if (override_paths->archive == NULL
            || override_paths->manifest == NULL
            || override_paths->bootstrap_include == NULL
            || override_paths->runtime_include == NULL) {
            return MMAY_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT;
        }
        status = mmay_native_runtime_inspect(
            allocator, profile, override_paths, false, &present, artifact
        );
        return status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK && !present
            ? MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING : status;
    }
    memset(&installed_paths, 0, sizeof(installed_paths));
    status = mmay_native_runtime_installed_paths(
        allocator, profile, tool_executable_path,
        &installed_paths, installed_owned
    );
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK) {
        status = mmay_native_runtime_inspect(
            allocator, profile, &installed_paths, true, &present, artifact
        );
        if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK || present) {
            mmay_native_runtime_owned_paths_dispose(installed_owned);
            return status;
        }
    } else if (status != MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING) {
        mmay_native_runtime_owned_paths_dispose(installed_owned);
        return status;
    }
    mmay_native_runtime_owned_paths_dispose(installed_owned);
    configured.archive = profile == MMAY_NATIVE_RUNTIME_RELEASE
        ? MMAY_NATIVE_RUNTIME_RELEASE_ARCHIVE
        : MMAY_NATIVE_RUNTIME_DEBUG_ARCHIVE;
    configured.manifest = profile == MMAY_NATIVE_RUNTIME_RELEASE
        ? MMAY_NATIVE_RUNTIME_RELEASE_MANIFEST
        : MMAY_NATIVE_RUNTIME_DEBUG_MANIFEST;
    configured.bootstrap_include = MMAY_NATIVE_BOOTSTRAP_INCLUDE;
    configured.runtime_include = MMAY_NATIVE_RUNTIME_INCLUDE;
    status = mmay_native_runtime_inspect(
        allocator, profile, &configured, false, &present, artifact
    );
    return status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK && !present
        ? MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING : status;
}

void mmay_native_runtime_artifact_dispose(
    MarimayNativeRuntimeArtifact *artifact
) {
    if (artifact == NULL) return;
    mmay_owned_bytes_dispose(&artifact->archive);
    mmay_owned_bytes_dispose(&artifact->manifest);
    mmay_owned_bytes_dispose(&artifact->bootstrap_include);
    mmay_owned_bytes_dispose(&artifact->runtime_include);
    memset(artifact->content_hash, 0, sizeof(artifact->content_hash));
    artifact->profile = MMAY_NATIVE_RUNTIME_DEBUG;
    artifact->installed = false;
}
