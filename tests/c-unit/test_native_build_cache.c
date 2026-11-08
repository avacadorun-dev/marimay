#include "test_support.h"

#include <string.h>

#include "build_internal.h"
#include "host_toolchain_internal.h"

void mmay_test_native_build_cache(void) {
    static const char *const changed[] = {
        "Apple clang version 17.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.7)\nTarget: arm64-apple-darwin24.0.0\n",
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\nTarget: arm64-apple-darwin24.1.0\n"
    };
    static const char banner[] =
        "Apple clang version 16.0.0 (clang-1600.0.26.6)\n"
        "Target: arm64-apple-darwin24.0.0\n";
    MarimayNativeHostToolchain control;
    MarimayNativeHostToolchain selected;
    MarimayNativeBuildRequest request = {0};
    MarimayNativeRuntimeArtifact runtime = {0};
    MarimayOwnedBytes source = {0};
    MarimayOwnedBytes output = {0};
    char expected[65];
    char actual[65];
    size_t index = 0;
    source.data = (unsigned char *)"same-generated-c";
    source.length = strlen((const char *)source.data);
    memset(runtime.content_hash, 'a', 64);
    runtime.content_hash[64] = 0;
    output.data = (unsigned char *)banner;
    output.length = sizeof(banner) - 1;
    MMAY_EXPECT(mmay_native_host_toolchain_parse_macos(&output, "arm64", &control));
    MMAY_EXPECT(mmay_native_build_hash(&source, &request, &runtime, &control, expected));
    for (index = 0; index < sizeof(changed) / sizeof(changed[0]); index += 1) {
        output.data = (unsigned char *)changed[index];
        output.length = strlen(changed[index]);
        MMAY_EXPECT(mmay_native_host_toolchain_parse_macos(&output, "arm64", &selected));
        MMAY_EXPECT(mmay_native_build_hash(&source, &request, &runtime, &selected, actual));
        MMAY_EXPECT(strcmp(expected, actual) != 0);
    }
    selected = control;
    selected.policy = "another-policy";
    MMAY_EXPECT(mmay_native_host_toolchain_fingerprint(&selected));
    MMAY_EXPECT(mmay_native_build_hash(&source, &request, &runtime, &selected, actual));
    MMAY_EXPECT(strcmp(expected, actual) != 0);
    selected = control;
    selected.id = "another-family";
    MMAY_EXPECT(mmay_native_host_toolchain_fingerprint(&selected));
    MMAY_EXPECT(mmay_native_build_hash(&source, &request, &runtime, &selected, actual));
    MMAY_EXPECT(strcmp(expected, actual) != 0);
    /* Complete-prefix movement cannot change the same content identity. */
    runtime.archive.data = (unsigned char *)"/moved/runtime.a";
    runtime.archive.length = strlen((const char *)runtime.archive.data);
    MMAY_EXPECT(mmay_native_build_hash(&source, &request, &runtime, &control, actual));
    MMAY_EXPECT(strcmp(expected, actual) == 0);
    runtime.content_hash[0] = 'b';
    MMAY_EXPECT(mmay_native_build_hash(&source, &request, &runtime, &control, actual));
    MMAY_EXPECT(strcmp(expected, actual) != 0);
}
