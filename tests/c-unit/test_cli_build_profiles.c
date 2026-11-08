#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "mmay/native/runtime_artifact.h"
#include "mmay/platform/directory.h"
#include "mmay/platform/process.h"

#ifndef MMAY_TEST_C_COMPILER
#define MMAY_TEST_C_COMPILER "cc"
#endif
#ifndef MMAY_TEST_SOURCE_DIR
#define MMAY_TEST_SOURCE_DIR "."
#endif
#ifndef MMAY_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE
#define MMAY_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE "missing-debug-runtime"
#endif
#ifndef MMAY_TEST_NATIVE_RUNTIME_RELEASE_ARCHIVE
#define MMAY_TEST_NATIVE_RUNTIME_RELEASE_ARCHIVE "missing-release-runtime"
#endif
#ifndef MMAY_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST
#define MMAY_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST "missing-debug-manifest"
#endif
#ifndef MMAY_TEST_NATIVE_RUNTIME_RELEASE_MANIFEST
#define MMAY_TEST_NATIVE_RUNTIME_RELEASE_MANIFEST "missing-release-manifest"
#endif
#ifndef MMAY_TEST_NATIVE_BOOTSTRAP_INCLUDE
#define MMAY_TEST_NATIVE_BOOTSTRAP_INCLUDE "."
#endif
#ifndef MMAY_TEST_NATIVE_RUNTIME_INCLUDE
#define MMAY_TEST_NATIVE_RUNTIME_INCLUDE "."
#endif

static const char mmay_cp13_directory[] =
    "cp13 runtime path with spaces";
static const char mmay_cp13_archive[] =
    "cp13 runtime path with spaces/runtime debug archive.a";
static const char mmay_cp13_manifest[] =
    "cp13 runtime path with spaces/mmay-native-runtime.manifest";
static const char mmay_cp13_stale_manifest[] =
    "cp13 runtime path with spaces/stale-runtime.manifest";

static MarimayNativeRuntimePaths mmay_cp13_paths(
    const char *archive,
    const char *manifest
) {
    MarimayNativeRuntimePaths paths;
    paths.archive = archive;
    paths.manifest = manifest;
    paths.bootstrap_include = MMAY_TEST_NATIVE_BOOTSTRAP_INCLUDE;
    paths.runtime_include = MMAY_TEST_NATIVE_RUNTIME_INCLUDE;
    return paths;
}

static bool mmay_cp13_copy_file(const char *source, const char *destination) {
    unsigned char buffer[8192];
    FILE *input = mmay_test_open_file(source, "rb");
    FILE *output = NULL;
    bool ok = input != NULL;
    if (ok) {
        output = mmay_test_open_file(destination, "wb");
        ok = output != NULL;
    }
    while (ok) {
        size_t count = fread(buffer, 1, sizeof(buffer), input);
        if (count != 0 && fwrite(buffer, 1, count, output) != count) {
            ok = false;
        }
        if (count != sizeof(buffer)) {
            if (ferror(input) != 0) ok = false;
            break;
        }
    }
    if (output != NULL && fclose(output) != 0) ok = false;
    if (input != NULL && fclose(input) != 0) ok = false;
    return ok;
}

static bool mmay_cp13_write_file(const char *path, const char *bytes) {
    size_t length = strlen(bytes);
    FILE *output = mmay_test_open_file(path, "wb");
    bool ok = output != NULL;
    if (ok && fwrite(bytes, 1, length, output) != length) ok = false;
    if (output != NULL && fclose(output) != 0) ok = false;
    return ok;
}

static bool mmay_cp13_append_byte(const char *path) {
    FILE *output = mmay_test_open_file(path, "ab");
    bool ok = output != NULL;
    if (ok && fputc('\n', output) == EOF) ok = false;
    if (output != NULL && fclose(output) != 0) ok = false;
    return ok;
}

static bool mmay_cp13_select(
    MarimayNativeRuntimeProfile profile,
    const MarimayNativeRuntimePaths *paths,
    MarimayNativeRuntimeArtifact *artifact
) {
    return mmay_native_runtime_artifact_select(
        mmay_system_allocator(), profile,
        paths == NULL ? mmay_test_toolchain_executable() : NULL,
        paths, artifact
    ) == MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
}

static void mmay_cp13_expect_probe(
    const MarimayNativeRuntimeArtifact *artifact,
    const char *expected,
    const char *output_path
) {
    char include_argument[32772];
#if defined(_WIN32)
    char executable_argument[32772];
    char object_path[32772];
#endif
    char source_path[32768];
    const char *arguments[16];
    const char *run_arguments[2];
    size_t argument_count = 0;
    int written = snprintf(
        source_path, sizeof(source_path),
        "%s/tests/c-unit/runtime_profile_probe.c", MMAY_TEST_SOURCE_DIR
    );
    MarimayProcessResult process;
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(source_path));
    if (written <= 0 || (size_t)written >= sizeof(source_path)) return;
#if defined(_WIN32)
    written = snprintf(
        include_argument, sizeof(include_argument), "%s",
        (const char *)artifact->runtime_include.data
    );
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(include_argument));
    written = snprintf(
        executable_argument, sizeof(executable_argument),
        "%s", output_path
    );
    MMAY_EXPECT(
        written > 0 && (size_t)written < sizeof(executable_argument)
    );
    written = snprintf(object_path, sizeof(object_path), "%s.obj", output_path);
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(object_path));
    arguments[argument_count++] = MMAY_TEST_C_COMPILER;
    arguments[argument_count++] = "/nologo";
    arguments[argument_count++] = "/std:c17";
    arguments[argument_count++] = "/utf-8";
#if defined(MMAY_TEST_C_SANITIZER)
    arguments[argument_count++] = MMAY_TEST_C_SANITIZER;
    arguments[argument_count++] = "/Z7";
#endif
    arguments[argument_count++] = "/I";
    arguments[argument_count++] = include_argument;
    arguments[argument_count++] = "/Fo:";
    arguments[argument_count++] = object_path;
    arguments[argument_count++] = source_path;
    arguments[argument_count++] = (const char *)artifact->archive.data;
    arguments[argument_count++] = "/Fe:";
    arguments[argument_count++] = executable_argument;
#else
    written = snprintf(
        include_argument, sizeof(include_argument), "-I%s",
        (const char *)artifact->runtime_include.data
    );
    MMAY_EXPECT(written > 0 && (size_t)written < sizeof(include_argument));
    arguments[argument_count++] = MMAY_TEST_C_COMPILER;
    arguments[argument_count++] = "-std=c17";
#if defined(MMAY_TEST_C_SANITIZER)
    arguments[argument_count++] = MMAY_TEST_C_SANITIZER;
#endif
    arguments[argument_count++] = include_argument;
    arguments[argument_count++] = source_path;
    arguments[argument_count++] = (const char *)artifact->archive.data;
    arguments[argument_count++] = "-o";
    arguments[argument_count++] = output_path;
#endif
    arguments[argument_count] = NULL;
    memset(&process, 0, sizeof(process));
    if (mmay_process_run(
            mmay_system_allocator(), arguments, argument_count,
            16384, &process
        ) != MMAY_PROCESS_OK
        || process.signaled || process.exit_code != 0) {
        if (process.output.data != NULL) {
            (void)fwrite(
                process.output.data, 1, process.output.length, stderr
            );
        }
        MMAY_EXPECT(false);
        mmay_process_result_dispose(&process);
#if defined(_WIN32)
        (void)mmay_test_remove_file(object_path);
#endif
        return;
    }
    mmay_process_result_dispose(&process);
#if defined(_WIN32)
    MMAY_EXPECT(mmay_test_remove_file(object_path) == 0);
#endif
    run_arguments[0] = output_path;
    run_arguments[1] = NULL;
    memset(&process, 0, sizeof(process));
    MMAY_EXPECT(mmay_process_run(
        mmay_system_allocator(), run_arguments, 1, 16384, &process
    ) == MMAY_PROCESS_OK);
    MMAY_EXPECT(!process.signaled && process.exit_code == 0);
    MMAY_EXPECT(process.output.length == strlen(expected)
        && memcmp(process.output.data, expected, process.output.length) == 0);
    mmay_process_result_dispose(&process);
    MMAY_EXPECT(remove(output_path) == 0);
}

static void mmay_cp13_test_profiles(void) {
    MarimayNativeRuntimeArtifact debug = {0};
    MarimayNativeRuntimeArtifact release = {0};
    MMAY_EXPECT(mmay_cp13_select(MMAY_NATIVE_RUNTIME_DEBUG, NULL, &debug));
    MMAY_EXPECT(mmay_cp13_select(MMAY_NATIVE_RUNTIME_RELEASE, NULL, &release));
    MMAY_EXPECT(debug.archive.data != NULL && release.archive.data != NULL);
    MMAY_EXPECT(strcmp(
        (const char *)debug.archive.data,
        (const char *)release.archive.data
    ) != 0);
    MMAY_EXPECT(strcmp(debug.content_hash, release.content_hash) != 0);
    MMAY_EXPECT(!debug.installed && !release.installed);
#if defined(_WIN32)
    /* The raw C probe uses the host CRT's text-mode puts newline. */
    mmay_cp13_expect_probe(&debug, "debug\r\n", "cp13-profile-debug.exe");
    mmay_cp13_expect_probe(&release, "release\r\n", "cp13-profile-release.exe");
#else
    mmay_cp13_expect_probe(&debug, "debug\n", "./cp13-profile-debug");
    mmay_cp13_expect_probe(&release, "release\n", "./cp13-profile-release");
#endif
    mmay_native_runtime_artifact_dispose(&release);
    mmay_native_runtime_artifact_dispose(&debug);
}

static void mmay_cp13_test_paths_and_hash(void) {
    MarimayNativeRuntimePaths paths;
    MarimayNativeRuntimeArtifact first = {0};
    MarimayNativeRuntimeArtifact second = {0};
    MMAY_EXPECT(mmay_directory_create(
        mmay_system_allocator(), mmay_cp13_directory,
        sizeof(mmay_cp13_directory) - 1
    ) == MMAY_FILE_OK);
    MMAY_EXPECT(mmay_cp13_copy_file(
        MMAY_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE, mmay_cp13_archive
    ));
    MMAY_EXPECT(mmay_cp13_copy_file(
        MMAY_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST, mmay_cp13_manifest
    ));
    paths = mmay_cp13_paths(mmay_cp13_archive, mmay_cp13_manifest);
    MMAY_EXPECT(mmay_cp13_select(MMAY_NATIVE_RUNTIME_DEBUG, &paths, &first));
#if defined(_WIN32)
    mmay_cp13_expect_probe(
        &first, "debug\r\n",
        "cp13 runtime path with spaces/profile probe.exe"
    );
#else
    mmay_cp13_expect_probe(
        &first, "debug\n",
        "./cp13 runtime path with spaces/profile probe"
    );
#endif
    MMAY_EXPECT(mmay_cp13_append_byte(mmay_cp13_archive));
    MMAY_EXPECT(mmay_cp13_select(
        MMAY_NATIVE_RUNTIME_DEBUG, &paths, &second
    ));
    MMAY_EXPECT(strcmp(first.content_hash, second.content_hash) != 0);
    mmay_native_runtime_artifact_dispose(&second);
    mmay_native_runtime_artifact_dispose(&first);
    MMAY_EXPECT(remove(mmay_cp13_manifest) == 0);
    MMAY_EXPECT(remove(mmay_cp13_archive) == 0);
}

static void mmay_cp13_test_invalid_artifacts(void) {
    static const char stale[] =
        "mmay-native-runtime-v1\n"
        "profile=debug\n"
        "runtime=stale\n"
        "host=stale\n"
        "processor=stale\n"
        "compiler=stale\n";
    MarimayNativeRuntimePaths paths = mmay_cp13_paths(
        "cp13 definitely missing runtime archive",
        MMAY_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST
    );
    MarimayNativeRuntimeArtifact artifact = {0};
    MMAY_EXPECT(mmay_native_runtime_artifact_select(
        mmay_system_allocator(), MMAY_NATIVE_RUNTIME_DEBUG,
        NULL, NULL, &artifact
    ) == MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING);
    mmay_native_runtime_artifact_dispose(&artifact);
    MMAY_EXPECT(mmay_native_runtime_artifact_select(
        mmay_system_allocator(), MMAY_NATIVE_RUNTIME_DEBUG,
        NULL, &paths, &artifact
    ) == MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING);
    mmay_native_runtime_artifact_dispose(&artifact);
    paths = mmay_cp13_paths(
        MMAY_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE,
        MMAY_TEST_NATIVE_RUNTIME_RELEASE_MANIFEST
    );
    MMAY_EXPECT(mmay_native_runtime_artifact_select(
        mmay_system_allocator(), MMAY_NATIVE_RUNTIME_RELEASE,
        NULL, &paths, &artifact
    ) == MMAY_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE);
    mmay_native_runtime_artifact_dispose(&artifact);
    MMAY_EXPECT(mmay_cp13_write_file(mmay_cp13_stale_manifest, stale));
    paths = mmay_cp13_paths(
        MMAY_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE, mmay_cp13_stale_manifest
    );
    MMAY_EXPECT(mmay_native_runtime_artifact_select(
        mmay_system_allocator(), MMAY_NATIVE_RUNTIME_DEBUG,
        NULL, &paths, &artifact
    ) == MMAY_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE);
    mmay_native_runtime_artifact_dispose(&artifact);
    MMAY_EXPECT(remove(mmay_cp13_stale_manifest) == 0);
}

static void mmay_cp13_test_allocations(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 256; failure_index += 1) {
        MarimayFailAllocator state;
        MarimayNativeRuntimeArtifact artifact = {0};
        MarimayNativeRuntimeArtifactStatus status;
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        status = mmay_native_runtime_artifact_select(
            mmay_fail_allocator(&state), MMAY_NATIVE_RUNTIME_RELEASE,
            mmay_test_toolchain_executable(), NULL, &artifact
        );
        if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK) reached_success = true;
        else MMAY_EXPECT(
            status == MMAY_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED
        );
        mmay_native_runtime_artifact_dispose(&artifact);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_cli_build_profiles(void) {
    mmay_cp13_test_profiles();
    mmay_cp13_test_paths_and_hash();
    mmay_cp13_test_invalid_artifacts();
    mmay_cp13_test_allocations();
}
