#include "test_support.h"

#include <string.h>

#include "host_toolchain_internal.h"
#include "mmay/build_config.h"

static const char mmay_host_valid[] =
    "Apple clang version 16.0.0 (clang-1600.0.26.6)\n"
    "Target: arm64-apple-darwin24.0.0\nThread model: posix\n";

static MarimayOwnedBytes mmay_host_bytes(const char *text) {
    MarimayOwnedBytes bytes = {0};
    bytes.data = (unsigned char *)text;
    bytes.length = strlen(text);
    return bytes;
}

static void mmay_host_expect_empty(const MarimayNativeHostToolchain *identity) {
    MMAY_EXPECT(identity->version[0] == 0);
    MMAY_EXPECT(identity->build[0] == 0);
    MMAY_EXPECT(identity->target[0] == 0);
    MMAY_EXPECT(identity->fingerprint[0] == 0);
}

static void mmay_host_parse_cases(void) {
    static const char *const rejected[] = {
        "clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 15.9.9 (clang-1500.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0.1 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 4294967296.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16..0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0-dev (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (other-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600..26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6) trailing\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\n",
        "Target: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: x86_64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-ios18.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-unknown-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0 extra\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nApple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n"
    };
    static const char *const changed[] = {
        "Apple clang version 17.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.7)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.1.0\n"
    };
    MarimayNativeHostToolchain control;
    MarimayNativeHostToolchain parsed;
    MarimayOwnedBytes output = mmay_host_bytes(mmay_host_valid);
    size_t index = 0;
    MMAY_EXPECT(mmay_native_host_toolchain_parse_macos(&output, "arm64", &control));
    MMAY_EXPECT(strcmp(control.path, "/usr/bin/clang") == 0);
    MMAY_EXPECT(strcmp(control.id, "AppleClang") == 0);
    MMAY_EXPECT(strcmp(control.policy, "macos-system-clang-v1") == 0);
    MMAY_EXPECT(strcmp(control.version, "16.0.0") == 0);
    MMAY_EXPECT(strcmp(control.build, "clang-1600.0.26.6") == 0);
    MMAY_EXPECT(strcmp(control.target, "arm64-apple-darwin24.0.0") == 0);
    MMAY_EXPECT(strlen(control.fingerprint) == 64);
    for (index = 0; index < sizeof(rejected) / sizeof(rejected[0]); index += 1) {
        output = mmay_host_bytes(rejected[index]);
        MMAY_EXPECT(!mmay_native_host_toolchain_parse_macos(&output, "arm64", &parsed));
        mmay_host_expect_empty(&parsed);
    }
    for (index = 0; index < sizeof(changed) / sizeof(changed[0]); index += 1) {
        output = mmay_host_bytes(changed[index]);
        MMAY_EXPECT(mmay_native_host_toolchain_parse_macos(&output, "arm64", &parsed));
        MMAY_EXPECT(strcmp(control.fingerprint, parsed.fingerprint) != 0);
    }
    parsed = control;
    parsed.policy = "another-policy";
    MMAY_EXPECT(mmay_native_host_toolchain_fingerprint(&parsed));
    MMAY_EXPECT(strcmp(control.fingerprint, parsed.fingerprint) != 0);
    parsed = control;
    parsed.id = "another-family";
    MMAY_EXPECT(mmay_native_host_toolchain_fingerprint(&parsed));
    MMAY_EXPECT(strcmp(control.fingerprint, parsed.fingerprint) != 0);
    {
        char long_field[201];
        char text[512];
        int written = 0;
        memset(long_field, '1', sizeof(long_field) - 1);
        long_field[sizeof(long_field) - 1] = 0;
        written = snprintf(text, sizeof(text),
            "Apple clang version 16.0.0 (clang-%s)\nTarget: arm64-apple-darwin24.0.0\n",
            long_field);
        MMAY_EXPECT(written > 0 && (size_t)written < sizeof(text));
        output = mmay_host_bytes(text);
        MMAY_EXPECT(!mmay_native_host_toolchain_parse_macos(&output, "arm64", &parsed));
        mmay_host_expect_empty(&parsed);
        written = snprintf(text, sizeof(text),
            "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin%s\n",
            long_field);
        MMAY_EXPECT(written > 0 && (size_t)written < sizeof(text));
        output = mmay_host_bytes(text);
        MMAY_EXPECT(!mmay_native_host_toolchain_parse_macos(&output, "arm64", &parsed));
        mmay_host_expect_empty(&parsed);
    }
    output = mmay_host_bytes("platform diagnostic before identity\r\n"
        "Apple clang version 21.0.0 (clang-2100.3.34.2)\r\n"
        "Target: x86_64-apple-macosx15.0.0\r\n");
    MMAY_EXPECT(mmay_native_host_toolchain_parse_macos(&output, "x86_64", &parsed));
    MMAY_EXPECT(strcmp(parsed.version, "21.0.0") == 0);
    output = mmay_host_bytes(mmay_host_valid);
    MMAY_EXPECT(!mmay_native_host_toolchain_parse_macos(&output, "aarch64", &parsed));
    mmay_host_expect_empty(&parsed);
    output.length = 64 * 1024 + 1;
    MMAY_EXPECT(!mmay_native_host_toolchain_parse_macos(&output, "arm64", &parsed));
    mmay_host_expect_empty(&parsed);
    {
        char binary[sizeof(mmay_host_valid)];
        memcpy(binary, mmay_host_valid, sizeof(binary));
        binary[5] = 0;
        output.data = (unsigned char *)binary;
        output.length = sizeof(binary) - 1;
        MMAY_EXPECT(!mmay_native_host_toolchain_parse_macos(&output, "arm64", &parsed));
        mmay_host_expect_empty(&parsed);
    }
}

typedef struct MarimayHostProbeFixture {
    MarimayProcessStatus status;
    uint32_t exit_code;
    bool signaled;
    bool called;
    const char *output;
} MarimayHostProbeFixture;

static MarimayProcessStatus mmay_host_fixture_run(
    void *context, MarimayAllocator allocator, const char *const arguments[],
    size_t count, size_t limit, MarimayProcessResult *result
) {
    MarimayHostProbeFixture *fixture = context;
    fixture->called = true;
    MMAY_EXPECT(count == 9 && limit == 64 * 1024);
    MMAY_EXPECT(strcmp(arguments[0], "/usr/bin/env") == 0);
    MMAY_EXPECT(strcmp(arguments[1], "-i") == 0);
    MMAY_EXPECT(strcmp(arguments[2], "PATH=/usr/bin:/bin:/usr/sbin:/sbin") == 0);
    MMAY_EXPECT(strcmp(arguments[3], "LC_ALL=C") == 0);
    MMAY_EXPECT(strcmp(arguments[4], "/usr/bin/clang") == 0);
    MMAY_EXPECT(strcmp(arguments[5], "-arch") == 0);
    MMAY_EXPECT(strcmp(arguments[6], "arm64") == 0);
    MMAY_EXPECT(strcmp(arguments[7], "-mmacosx-version-min=15.0") == 0);
    MMAY_EXPECT(strcmp(arguments[8], "--version") == 0 && arguments[9] == NULL);
    memset(result, 0, sizeof(*result));
    result->exit_code = fixture->exit_code;
    result->signaled = fixture->signaled;
    if (fixture->output != NULL && !mmay_owned_bytes_copy(&result->output,
            allocator, fixture->output, strlen(fixture->output))) {
        return MMAY_PROCESS_ALLOCATION_FAILED;
    }
    return fixture->status;
}

static void mmay_host_transport_cases(void) {
    static const struct {
        MarimayProcessStatus status;
        uint32_t exit_code;
        bool signaled;
        const char *output;
    } cases[] = {
        {MMAY_PROCESS_OK, 0, false, mmay_host_valid},
        {MMAY_PROCESS_OK, 1, false, mmay_host_valid},
        {MMAY_PROCESS_OK, 0, true, mmay_host_valid},
        {MMAY_PROCESS_SPAWN_FAILED, 0, false, mmay_host_valid},
        {MMAY_PROCESS_IO_FAILED, 0, false, mmay_host_valid},
        {MMAY_PROCESS_OUTPUT_LIMIT, 0, false, mmay_host_valid},
        {MMAY_PROCESS_WAIT_FAILED, 0, false, mmay_host_valid},
        {MMAY_PROCESS_OK, 0, false, "foreign compiler output\n"},
        {MMAY_PROCESS_ALLOCATION_FAILED, 0, false, mmay_host_valid}
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        size_t failure = 0;
        for (failure = 0; failure < 2; failure += 1) {
            MarimayFailAllocator state;
            MarimayHostProbeFixture fixture = {0};
            MarimayNativeHostToolchain selected;
            MarimayOwnedBytes output = {0};
            MarimayNativeToolchainStatus status;
            fixture.status = cases[index].status;
            fixture.exit_code = cases[index].exit_code;
            fixture.signaled = cases[index].signaled;
            fixture.output = cases[index].output;
            mmay_fail_allocator_init(&state, mmay_system_allocator(), failure);
            status = mmay_native_host_toolchain_probe_macos(mmay_fail_allocator(&state),
                "arm64", mmay_host_fixture_run, &fixture, &selected, &output);
            MMAY_EXPECT(fixture.called);
            if (failure == 0 || cases[index].status == MMAY_PROCESS_ALLOCATION_FAILED) {
                MMAY_EXPECT(status == MMAY_NATIVE_TOOLCHAIN_ALLOCATION_FAILED);
            } else if (index == 0) {
                MMAY_EXPECT(status == MMAY_NATIVE_TOOLCHAIN_OK);
                MMAY_EXPECT(output.length == strlen(mmay_host_valid));
            } else {
                MMAY_EXPECT(status == MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE);
            }
            if (status != MMAY_NATIVE_TOOLCHAIN_OK) {
                MMAY_EXPECT(output.data == NULL);
                mmay_host_expect_empty(&selected);
            }
            mmay_owned_bytes_dispose(&output);
            MMAY_EXPECT(state.live_allocations == 0);
            MMAY_EXPECT(state.live_bytes == 0);
        }
    }
    {
        MarimayNativeHostToolchain selected;
        MarimayOwnedBytes output = {0};
        MarimayHostProbeFixture fixture = {0};
        MMAY_EXPECT(mmay_native_host_toolchain_probe_macos(mmay_system_allocator(),
            "invalid", mmay_host_fixture_run, &fixture, &selected, &output)
            == MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT);
        MMAY_EXPECT(!fixture.called && output.data == NULL);
    }
}

static void mmay_host_selected_policy(void) {
    MarimayNativeHostToolchain selected;
    MarimayOwnedBytes output = {0};
    const char *arguments[8];
    MMAY_EXPECT(mmay_native_host_toolchain_probe(mmay_system_allocator(),
        MMAY_NATIVE_BUILD_TREE_EXECUTABLE_PATH,
        NULL, &selected, &output) == MMAY_NATIVE_TOOLCHAIN_OK);
    MMAY_EXPECT(strcmp(selected.path, MMAY_NATIVE_C_COMPILER) == 0);
    MMAY_EXPECT(strcmp(selected.version, MMAY_NATIVE_C_COMPILER_VERSION) == 0);
    MMAY_EXPECT(strcmp(selected.policy, "pinned") == 0);
    MMAY_EXPECT(!mmay_native_host_toolchain_is_installed(MMAY_NATIVE_BUILD_TREE_EXECUTABLE_PATH));
    MMAY_EXPECT(mmay_native_host_toolchain_is_installed("/installed/bin/mmay"));
    MMAY_EXPECT(selected.build[0] == 0 && selected.target[0] == 0);
    MMAY_EXPECT(mmay_native_host_toolchain_command(&selected, arguments, 1) == 1);
    MMAY_EXPECT(arguments[0] == selected.path);
    mmay_owned_bytes_dispose(&output);
    MMAY_EXPECT(mmay_native_host_toolchain_probe(mmay_system_allocator(),
        MMAY_NATIVE_BUILD_TREE_EXECUTABLE_PATH,
        "mmay-definitely-missing-host-compiler", &selected, &output)
        == MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE);
    MMAY_EXPECT(output.data == NULL);
    output = mmay_host_bytes(mmay_host_valid);
    MMAY_EXPECT(mmay_native_host_toolchain_parse_macos(&output, "arm64", &selected));
    MMAY_EXPECT(mmay_native_host_toolchain_command(&selected, arguments, 7) == 0);
    MMAY_EXPECT(mmay_native_host_toolchain_command(&selected, arguments, 8) == 8);
    selected.path = "/untrusted/clang";
    MMAY_EXPECT(mmay_native_host_toolchain_command(&selected, arguments, 8) == 0);
}

#if defined(__APPLE__) && MMAY_PORTABLE_MACOS_TOOLCHAIN
static void mmay_host_real_macos_allocations(void) {
    MarimayFailAllocator control;
    MarimayNativeHostToolchain selected;
    MarimayOwnedBytes output = {0};
    size_t count = 0;
    size_t index = 0;
    mmay_fail_allocator_init(&control, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_native_host_toolchain_probe(mmay_fail_allocator(&control),
        "/installed/bin/mmay", NULL, &selected, &output) == MMAY_NATIVE_TOOLCHAIN_OK);
    MMAY_EXPECT(selected.portable_macos && strlen(selected.fingerprint) == 64);
    count = control.attempts;
    mmay_owned_bytes_dispose(&output);
    MMAY_EXPECT(control.live_allocations == 0 && control.live_bytes == 0);
    MMAY_EXPECT(count != 0);
    for (index = 0; index < count; index += 1) {
        MarimayFailAllocator state;
        MarimayNativeToolchainStatus status;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), index);
        status = mmay_native_host_toolchain_probe(mmay_fail_allocator(&state),
            "/installed/bin/mmay", NULL, &selected, &output);
        if (status == MMAY_NATIVE_TOOLCHAIN_OK) {
            MMAY_EXPECT(index >= state.attempts);
        } else {
            MMAY_EXPECT(status == MMAY_NATIVE_TOOLCHAIN_ALLOCATION_FAILED);
            MMAY_EXPECT(index < state.attempts && output.data == NULL);
            mmay_host_expect_empty(&selected);
        }
        mmay_owned_bytes_dispose(&output);
        MMAY_EXPECT(state.live_allocations == 0);
        MMAY_EXPECT(state.live_bytes == 0);
    }
    MMAY_EXPECT(mmay_native_host_toolchain_probe(mmay_system_allocator(), "/installed/bin/mmay",
        "/untrusted/clang", &selected, &output) == MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT);
    MMAY_EXPECT(output.data == NULL);
}
#endif

void mmay_test_native_host_toolchain(void) {
    mmay_host_parse_cases();
    mmay_host_transport_cases();
    mmay_host_selected_policy();
#if defined(__APPLE__) && MMAY_PORTABLE_MACOS_TOOLCHAIN
    mmay_host_real_macos_allocations();
#endif
}
