#include "test_support.h"

#include <stdio.h>
#include <string.h>

#include "ruda/native/runtime_artifact.h"
#include "ruda/platform/directory.h"
#include "ruda/platform/process.h"

#ifndef RUDA_TEST_C_COMPILER
#define RUDA_TEST_C_COMPILER "cc"
#endif
#ifndef RUDA_TEST_SOURCE_DIR
#define RUDA_TEST_SOURCE_DIR "."
#endif
#ifndef RUDA_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE
#define RUDA_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE "missing-debug-runtime"
#endif
#ifndef RUDA_TEST_NATIVE_RUNTIME_RELEASE_ARCHIVE
#define RUDA_TEST_NATIVE_RUNTIME_RELEASE_ARCHIVE "missing-release-runtime"
#endif
#ifndef RUDA_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST
#define RUDA_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST "missing-debug-manifest"
#endif
#ifndef RUDA_TEST_NATIVE_RUNTIME_RELEASE_MANIFEST
#define RUDA_TEST_NATIVE_RUNTIME_RELEASE_MANIFEST "missing-release-manifest"
#endif
#ifndef RUDA_TEST_NATIVE_BOOTSTRAP_INCLUDE
#define RUDA_TEST_NATIVE_BOOTSTRAP_INCLUDE "."
#endif
#ifndef RUDA_TEST_NATIVE_RUNTIME_INCLUDE
#define RUDA_TEST_NATIVE_RUNTIME_INCLUDE "."
#endif

static const char ruda_cp13_directory[] =
    "cp13 runtime path with spaces";
static const char ruda_cp13_archive[] =
    "cp13 runtime path with spaces/runtime debug archive.a";
static const char ruda_cp13_manifest[] =
    "cp13 runtime path with spaces/ruda-native-runtime.manifest";
static const char ruda_cp13_stale_manifest[] =
    "cp13 runtime path with spaces/stale-runtime.manifest";

static RudaNativeRuntimePaths ruda_cp13_paths(
    const char *archive,
    const char *manifest
) {
    RudaNativeRuntimePaths paths;
    paths.archive = archive;
    paths.manifest = manifest;
    paths.bootstrap_include = RUDA_TEST_NATIVE_BOOTSTRAP_INCLUDE;
    paths.runtime_include = RUDA_TEST_NATIVE_RUNTIME_INCLUDE;
    return paths;
}

static bool ruda_cp13_copy_file(const char *source, const char *destination) {
    unsigned char buffer[8192];
    FILE *input = fopen(source, "rb");
    FILE *output = NULL;
    bool ok = input != NULL;
    if (ok) {
        output = fopen(destination, "wb");
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

static bool ruda_cp13_write_file(const char *path, const char *bytes) {
    size_t length = strlen(bytes);
    FILE *output = fopen(path, "wb");
    bool ok = output != NULL;
    if (ok && fwrite(bytes, 1, length, output) != length) ok = false;
    if (output != NULL && fclose(output) != 0) ok = false;
    return ok;
}

static bool ruda_cp13_append_byte(const char *path) {
    FILE *output = fopen(path, "ab");
    bool ok = output != NULL;
    if (ok && fputc('\n', output) == EOF) ok = false;
    if (output != NULL && fclose(output) != 0) ok = false;
    return ok;
}

static bool ruda_cp13_select(
    RudaNativeRuntimeProfile profile,
    const RudaNativeRuntimePaths *paths,
    RudaNativeRuntimeArtifact *artifact
) {
    return ruda_native_runtime_artifact_select(
        ruda_system_allocator(), profile, NULL, paths, artifact
    ) == RUDA_NATIVE_RUNTIME_ARTIFACT_OK;
}

static void ruda_cp13_expect_probe(
    const RudaNativeRuntimeArtifact *artifact,
    const char *expected,
    const char *output_path
) {
    char include_argument[32772];
#if defined(_WIN32)
    char executable_argument[32772];
#endif
    char source_path[32768];
    const char *arguments[12];
    const char *run_arguments[2];
    size_t argument_count = 0;
    int written = snprintf(
        source_path, sizeof(source_path),
        "%s/tests/c-unit/runtime_profile_probe.c", RUDA_TEST_SOURCE_DIR
    );
    RudaProcessResult process;
    RUDA_EXPECT(written > 0 && (size_t)written < sizeof(source_path));
    if (written <= 0 || (size_t)written >= sizeof(source_path)) return;
#if defined(_WIN32)
    written = snprintf(
        include_argument, sizeof(include_argument), "/I%s",
        (const char *)artifact->runtime_include.data
    );
    RUDA_EXPECT(written > 0 && (size_t)written < sizeof(include_argument));
    written = snprintf(
        executable_argument, sizeof(executable_argument),
        "/Fe:%s", output_path
    );
    RUDA_EXPECT(
        written > 0 && (size_t)written < sizeof(executable_argument)
    );
    arguments[argument_count++] = RUDA_TEST_C_COMPILER;
    arguments[argument_count++] = "/nologo";
    arguments[argument_count++] = "/std:c17";
#if defined(RUDA_TEST_C_SANITIZER)
    arguments[argument_count++] = RUDA_TEST_C_SANITIZER;
#endif
    arguments[argument_count++] = include_argument;
    arguments[argument_count++] = source_path;
    arguments[argument_count++] = (const char *)artifact->archive.data;
    arguments[argument_count++] = executable_argument;
#else
    written = snprintf(
        include_argument, sizeof(include_argument), "-I%s",
        (const char *)artifact->runtime_include.data
    );
    RUDA_EXPECT(written > 0 && (size_t)written < sizeof(include_argument));
    arguments[argument_count++] = RUDA_TEST_C_COMPILER;
    arguments[argument_count++] = "-std=c17";
#if defined(RUDA_TEST_C_SANITIZER)
    arguments[argument_count++] = RUDA_TEST_C_SANITIZER;
#endif
    arguments[argument_count++] = include_argument;
    arguments[argument_count++] = source_path;
    arguments[argument_count++] = (const char *)artifact->archive.data;
    arguments[argument_count++] = "-o";
    arguments[argument_count++] = output_path;
#endif
    arguments[argument_count] = NULL;
    memset(&process, 0, sizeof(process));
    if (ruda_process_run(
            ruda_system_allocator(), arguments, argument_count,
            16384, &process
        ) != RUDA_PROCESS_OK
        || process.signaled || process.exit_code != 0) {
        if (process.output.data != NULL) {
            (void)fwrite(
                process.output.data, 1, process.output.length, stderr
            );
        }
        RUDA_EXPECT(false);
        ruda_process_result_dispose(&process);
        return;
    }
    ruda_process_result_dispose(&process);
    run_arguments[0] = output_path;
    run_arguments[1] = NULL;
    memset(&process, 0, sizeof(process));
    RUDA_EXPECT(ruda_process_run(
        ruda_system_allocator(), run_arguments, 1, 16384, &process
    ) == RUDA_PROCESS_OK);
    RUDA_EXPECT(!process.signaled && process.exit_code == 0);
    RUDA_EXPECT(process.output.length == strlen(expected)
        && memcmp(process.output.data, expected, process.output.length) == 0);
    ruda_process_result_dispose(&process);
    RUDA_EXPECT(remove(output_path) == 0);
}

static void ruda_cp13_test_profiles(void) {
    RudaNativeRuntimeArtifact debug = {0};
    RudaNativeRuntimeArtifact release = {0};
    RUDA_EXPECT(ruda_cp13_select(RUDA_NATIVE_RUNTIME_DEBUG, NULL, &debug));
    RUDA_EXPECT(ruda_cp13_select(RUDA_NATIVE_RUNTIME_RELEASE, NULL, &release));
    RUDA_EXPECT(debug.archive.data != NULL && release.archive.data != NULL);
    RUDA_EXPECT(strcmp(
        (const char *)debug.archive.data,
        (const char *)release.archive.data
    ) != 0);
    RUDA_EXPECT(strcmp(debug.content_hash, release.content_hash) != 0);
    RUDA_EXPECT(!debug.installed && !release.installed);
#if defined(_WIN32)
    ruda_cp13_expect_probe(&debug, "debug\n", "cp13-profile-debug.exe");
    ruda_cp13_expect_probe(&release, "release\n", "cp13-profile-release.exe");
#else
    ruda_cp13_expect_probe(&debug, "debug\n", "./cp13-profile-debug");
    ruda_cp13_expect_probe(&release, "release\n", "./cp13-profile-release");
#endif
    ruda_native_runtime_artifact_dispose(&release);
    ruda_native_runtime_artifact_dispose(&debug);
}

static void ruda_cp13_test_paths_and_hash(void) {
    RudaNativeRuntimePaths paths;
    RudaNativeRuntimeArtifact first = {0};
    RudaNativeRuntimeArtifact second = {0};
    RUDA_EXPECT(ruda_directory_create(
        ruda_system_allocator(), ruda_cp13_directory,
        sizeof(ruda_cp13_directory) - 1
    ) == RUDA_FILE_OK);
    RUDA_EXPECT(ruda_cp13_copy_file(
        RUDA_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE, ruda_cp13_archive
    ));
    RUDA_EXPECT(ruda_cp13_copy_file(
        RUDA_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST, ruda_cp13_manifest
    ));
    paths = ruda_cp13_paths(ruda_cp13_archive, ruda_cp13_manifest);
    RUDA_EXPECT(ruda_cp13_select(RUDA_NATIVE_RUNTIME_DEBUG, &paths, &first));
#if defined(_WIN32)
    ruda_cp13_expect_probe(
        &first, "debug\n",
        "cp13 runtime path with spaces/profile probe.exe"
    );
#else
    ruda_cp13_expect_probe(
        &first, "debug\n",
        "./cp13 runtime path with spaces/profile probe"
    );
#endif
    RUDA_EXPECT(ruda_cp13_append_byte(ruda_cp13_archive));
    RUDA_EXPECT(ruda_cp13_select(
        RUDA_NATIVE_RUNTIME_DEBUG, &paths, &second
    ));
    RUDA_EXPECT(strcmp(first.content_hash, second.content_hash) != 0);
    ruda_native_runtime_artifact_dispose(&second);
    ruda_native_runtime_artifact_dispose(&first);
    RUDA_EXPECT(remove(ruda_cp13_manifest) == 0);
    RUDA_EXPECT(remove(ruda_cp13_archive) == 0);
}

static void ruda_cp13_test_invalid_artifacts(void) {
    static const char stale[] =
        "ruda-native-runtime-v1\n"
        "profile=debug\n"
        "runtime=stale\n"
        "host=stale\n"
        "processor=stale\n"
        "compiler=stale\n";
    RudaNativeRuntimePaths paths = ruda_cp13_paths(
        "cp13 definitely missing runtime archive",
        RUDA_TEST_NATIVE_RUNTIME_DEBUG_MANIFEST
    );
    RudaNativeRuntimeArtifact artifact = {0};
    RUDA_EXPECT(ruda_native_runtime_artifact_select(
        ruda_system_allocator(), RUDA_NATIVE_RUNTIME_DEBUG,
        NULL, &paths, &artifact
    ) == RUDA_NATIVE_RUNTIME_ARTIFACT_MISSING);
    ruda_native_runtime_artifact_dispose(&artifact);
    paths = ruda_cp13_paths(
        RUDA_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE,
        RUDA_TEST_NATIVE_RUNTIME_RELEASE_MANIFEST
    );
    RUDA_EXPECT(ruda_native_runtime_artifact_select(
        ruda_system_allocator(), RUDA_NATIVE_RUNTIME_RELEASE,
        NULL, &paths, &artifact
    ) == RUDA_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE);
    ruda_native_runtime_artifact_dispose(&artifact);
    RUDA_EXPECT(ruda_cp13_write_file(ruda_cp13_stale_manifest, stale));
    paths = ruda_cp13_paths(
        RUDA_TEST_NATIVE_RUNTIME_DEBUG_ARCHIVE, ruda_cp13_stale_manifest
    );
    RUDA_EXPECT(ruda_native_runtime_artifact_select(
        ruda_system_allocator(), RUDA_NATIVE_RUNTIME_DEBUG,
        NULL, &paths, &artifact
    ) == RUDA_NATIVE_RUNTIME_ARTIFACT_INCOMPATIBLE);
    ruda_native_runtime_artifact_dispose(&artifact);
    RUDA_EXPECT(remove(ruda_cp13_stale_manifest) == 0);
}

static void ruda_cp13_test_allocations(void) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 256; failure_index += 1) {
        RudaFailAllocator state;
        RudaNativeRuntimeArtifact artifact = {0};
        RudaNativeRuntimeArtifactStatus status;
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        status = ruda_native_runtime_artifact_select(
            ruda_fail_allocator(&state), RUDA_NATIVE_RUNTIME_RELEASE,
            ruda_test_toolchain_executable(), NULL, &artifact
        );
        if (status == RUDA_NATIVE_RUNTIME_ARTIFACT_OK) reached_success = true;
        else RUDA_EXPECT(
            status == RUDA_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED
        );
        ruda_native_runtime_artifact_dispose(&artifact);
        RUDA_EXPECT(state.live_allocations == 0);
        RUDA_EXPECT(state.live_bytes == 0);
        if (reached_success) break;
    }
    RUDA_EXPECT(reached_success);
}

void ruda_test_cli_build_profiles(void) {
    ruda_cp13_test_profiles();
    ruda_cp13_test_paths_and_hash();
    ruda_cp13_test_invalid_artifacts();
    ruda_cp13_test_allocations();
}
