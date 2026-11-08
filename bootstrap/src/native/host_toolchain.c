#include "host_toolchain_internal.h"

#include <string.h>

#include "mmay/base/hash.h"
#include "mmay/build_config.h"

enum { MMAY_NATIVE_HOST_MAX_OUTPUT = 64 * 1024 };

bool mmay_native_host_toolchain_is_installed(const char *tool_executable_path) {
    return tool_executable_path != NULL && tool_executable_path[0] != 0
        && strcmp(tool_executable_path, MMAY_NATIVE_BUILD_TREE_EXECUTABLE_PATH) != 0;
}

static bool mmay_native_host_copy(char *destination, size_t capacity,
    const unsigned char *source, size_t length) {
    if (length >= capacity) return false;
    if (length != 0) memcpy(destination, source, length);
    destination[length] = 0;
    return true;
}

static bool mmay_native_host_architecture(const char *architecture) {
    return architecture != NULL && (strcmp(architecture, "arm64") == 0
        || strcmp(architecture, "x86_64") == 0);
}

static void mmay_native_host_macos_init(
    MarimayNativeHostToolchain *toolchain, const char *architecture
) {
    memset(toolchain, 0, sizeof(*toolchain));
    toolchain->path = "/usr/bin/clang";
    toolchain->id = "AppleClang";
    toolchain->policy = "macos-system-clang-v1";
    toolchain->portable_macos = true;
    if (mmay_native_host_architecture(architecture)) {
        (void)mmay_native_host_copy(toolchain->architecture,
            sizeof(toolchain->architecture),
            (const unsigned char *)architecture, strlen(architecture));
    }
}

bool mmay_native_host_toolchain_fingerprint(MarimayNativeHostToolchain *toolchain) {
    MarimaySha256 hash;
    unsigned char digest[32];
    const char *fields[] = {toolchain->policy, toolchain->id,
        toolchain->version, toolchain->build, toolchain->target,
        toolchain->architecture, toolchain->portable_macos
            ? MMAY_NATIVE_MACOS_DEPLOYMENT_TARGET : ""};
    size_t index = 0;
    mmay_sha256_init(&hash);
    for (index = 0; index < sizeof(fields) / sizeof(fields[0]); index += 1) {
        if (!mmay_sha256_update(&hash, fields[index], strlen(fields[index]) + 1)) {
            return false;
        }
    }
    mmay_sha256_final(&hash, digest);
    mmay_sha256_hex(digest, toolchain->fingerprint);
    return true;
}

static bool mmay_native_host_numeric(
    const unsigned char *text, size_t length
) {
    size_t index = 0;
    bool digit = false;
    for (index = 0; index < length; index += 1) {
        if (text[index] >= '0' && text[index] <= '9') digit = true;
        else if (text[index] == '.' && digit) digit = false;
        else return false;
    }
    return digit;
}

static bool mmay_native_host_version(
    const unsigned char *text, size_t length
) {
    size_t index = 0;
    size_t component = 0;
    uint32_t major = 0;
    for (component = 0; component < 3; component += 1) {
        uint32_t value = 0;
        size_t start = index;
        while (index < length && text[index] >= '0' && text[index] <= '9') {
            uint32_t digit = (uint32_t)(text[index] - '0');
            if (value > (UINT32_MAX - digit) / 10) return false;
            value = value * 10 + digit;
            index += 1;
        }
        if (index == start) return false;
        if (component == 0) major = value;
        if (component < 2) {
            if (index == length || text[index] != '.') return false;
            index += 1;
        }
    }
    return index == length && major >= 16;
}

static bool mmay_native_host_banner(
    const unsigned char *line, size_t length,
    MarimayNativeHostToolchain *toolchain
) {
    static const char prefix[] = "Apple clang version ";
    static const char build_prefix[] = "clang-";
    size_t start = sizeof(prefix) - 1;
    size_t end = start;
    size_t build_start = 0;
    size_t build_length = 0;
    while (end < length && ((line[end] >= '0' && line[end] <= '9')
            || line[end] == '.')) end += 1;
    if (!mmay_native_host_version(line + start, end - start)
        || length - end < 4 || line[end] != ' ' || line[end + 1] != '('
        || line[length - 1] != ')') return false;
    build_start = end + 2;
    build_length = length - build_start - 1;
    if (build_length <= sizeof(build_prefix) - 1
        || memcmp(line + build_start, build_prefix, sizeof(build_prefix) - 1) != 0
        || !mmay_native_host_numeric(line + build_start + sizeof(build_prefix) - 1,
            build_length - (sizeof(build_prefix) - 1))) return false;
    return mmay_native_host_copy(toolchain->version, sizeof(toolchain->version),
            line + start, end - start)
        && mmay_native_host_copy(toolchain->build, sizeof(toolchain->build),
            line + build_start, build_length);
}

static bool mmay_native_host_target(
    const unsigned char *target, size_t length,
    MarimayNativeHostToolchain *toolchain
) {
    static const char vendor[] = "-apple-";
    const char *systems[] = {"darwin", "macosx"};
    size_t arch_length = strlen(toolchain->architecture);
    size_t start = arch_length + sizeof(vendor) - 1;
    size_t index = 0;
    if (length <= start || memcmp(target, toolchain->architecture, arch_length) != 0
        || memcmp(target + arch_length, vendor, sizeof(vendor) - 1) != 0) return false;
    for (index = 0; index < sizeof(systems) / sizeof(systems[0]); index += 1) {
        size_t system_length = strlen(systems[index]);
        if (length > start + system_length
            && memcmp(target + start, systems[index], system_length) == 0
            && mmay_native_host_numeric(target + start + system_length,
                length - start - system_length)) {
            return mmay_native_host_copy(toolchain->target,
                sizeof(toolchain->target), target, length);
        }
    }
    return false;
}

bool mmay_native_host_toolchain_parse_macos(
    const MarimayOwnedBytes *output, const char *architecture,
    MarimayNativeHostToolchain *toolchain
) {
    static const char banner[] = "Apple clang version ";
    static const char target[] = "Target: ";
    MarimayNativeHostToolchain parsed;
    size_t index = 0;
    bool has_banner = false;
    bool has_target = false;
    if (toolchain == NULL) return false;
    mmay_native_host_macos_init(toolchain, architecture);
    if (!mmay_native_host_architecture(architecture) || output == NULL
        || output->data == NULL || output->length > MMAY_NATIVE_HOST_MAX_OUTPUT
        || memchr(output->data, 0, output->length) != NULL) return false;
    parsed = *toolchain;
    while (index < output->length) {
        size_t start = index;
        size_t length = 0;
        while (index < output->length && output->data[index] != '\n') index += 1;
        length = index - start;
        if (length != 0 && output->data[start + length - 1] == '\r') length -= 1;
        if (index < output->length) index += 1;
        if (length >= sizeof(banner) - 1
            && memcmp(output->data + start, banner, sizeof(banner) - 1) == 0) {
            if (has_banner || !mmay_native_host_banner(
                    output->data + start, length, &parsed)) return false;
            has_banner = true;
        } else if (length >= sizeof(target) - 1
            && memcmp(output->data + start, target, sizeof(target) - 1) == 0) {
            if (has_target || !mmay_native_host_target(
                    output->data + start + sizeof(target) - 1,
                    length - (sizeof(target) - 1), &parsed)) return false;
            has_target = true;
        }
    }
    if (!has_banner || !has_target || !mmay_native_host_toolchain_fingerprint(&parsed)) return false;
    *toolchain = parsed;
    return true;
}

size_t mmay_native_host_toolchain_command(
    const MarimayNativeHostToolchain *toolchain,
    const char **arguments, size_t capacity
) {
    if (toolchain == NULL || arguments == NULL || toolchain->path == NULL
        || toolchain->path[0] == 0) return 0;
    if (!toolchain->portable_macos) {
        if (capacity == 0) return 0;
        arguments[0] = toolchain->path;
        return 1;
    }
    if (capacity < 8 || !mmay_native_host_architecture(toolchain->architecture)
        || strcmp(toolchain->path, "/usr/bin/clang") != 0) return 0;
    arguments[0] = "/usr/bin/env";
    arguments[1] = "-i";
    arguments[2] = "PATH=/usr/bin:/bin:/usr/sbin:/sbin";
    arguments[3] = "LC_ALL=C";
    arguments[4] = "/usr/bin/clang";
    arguments[5] = "-arch";
    arguments[6] = toolchain->architecture;
    arguments[7] = "-mmacosx-version-min=" MMAY_NATIVE_MACOS_DEPLOYMENT_TARGET;
    return 8;
}

static MarimayProcessStatus mmay_native_host_run(
    void *context, MarimayAllocator allocator, const char *const arguments[],
    size_t count, size_t limit, MarimayProcessResult *result
) {
    (void)context;
    return mmay_process_run(allocator, arguments, count, limit, result);
}

MarimayNativeToolchainStatus mmay_native_host_toolchain_probe_macos(
    MarimayAllocator allocator, const char *architecture,
    MarimayNativeHostProbeRunner runner, void *context,
    MarimayNativeHostToolchain *toolchain, MarimayOwnedBytes *identity_output
) {
    const char *arguments[10];
    MarimayProcessResult process = {0};
    MarimayProcessStatus status;
    size_t count = 0;
    if (toolchain == NULL || identity_output == NULL) {
        return MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT;
    }
    memset(identity_output, 0, sizeof(*identity_output));
    mmay_native_host_macos_init(toolchain, architecture);
    count = mmay_native_host_toolchain_command(toolchain, arguments, 8);
    if (count == 0) return MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT;
    arguments[count++] = "--version";
    arguments[count] = NULL;
    status = (runner != NULL ? runner : mmay_native_host_run)(
        context, allocator, arguments, count, MMAY_NATIVE_HOST_MAX_OUTPUT, &process
    );
    if (status != MMAY_PROCESS_OK || process.signaled || process.exit_code != 0
        || !mmay_native_host_toolchain_parse_macos(
            &process.output, architecture, toolchain)) {
        mmay_process_result_dispose(&process);
        return status == MMAY_PROCESS_ALLOCATION_FAILED
            ? MMAY_NATIVE_TOOLCHAIN_ALLOCATION_FAILED
            : MMAY_NATIVE_TOOLCHAIN_UNAVAILABLE;
    }
    *identity_output = process.output;
    memset(&process.output, 0, sizeof(process.output));
    mmay_process_result_dispose(&process);
    return MMAY_NATIVE_TOOLCHAIN_OK;
}

MarimayNativeToolchainStatus mmay_native_host_toolchain_probe(
    MarimayAllocator allocator, const char *tool_executable_path,
    const char *compiler_override,
    MarimayNativeHostToolchain *toolchain, MarimayOwnedBytes *identity_output
) {
    if (toolchain == NULL || identity_output == NULL) {
        return MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT;
    }
    memset(toolchain, 0, sizeof(*toolchain));
    memset(identity_output, 0, sizeof(*identity_output));
#if defined(__APPLE__) && MMAY_PORTABLE_MACOS_TOOLCHAIN
    if (mmay_native_host_toolchain_is_installed(tool_executable_path)) {
        mmay_native_host_macos_init(toolchain, MMAY_HOST_PROCESSOR);
        if (compiler_override != NULL && strcmp(compiler_override, toolchain->path) != 0) {
            return MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT;
        }
        return mmay_native_host_toolchain_probe_macos(allocator,
            MMAY_HOST_PROCESSOR, NULL, NULL, toolchain, identity_output);
    }
#else
    (void)tool_executable_path;
#endif
    toolchain->path = compiler_override != NULL ? compiler_override : MMAY_NATIVE_C_COMPILER;
    toolchain->id = MMAY_NATIVE_C_COMPILER_ID;
    toolchain->policy = "pinned";
    if (!mmay_native_host_copy(toolchain->version, sizeof(toolchain->version),
            (const unsigned char *)MMAY_NATIVE_C_COMPILER_VERSION,
            strlen(MMAY_NATIVE_C_COMPILER_VERSION))
        || !mmay_native_host_toolchain_fingerprint(toolchain)) {
        return MMAY_NATIVE_TOOLCHAIN_INVALID_ARGUMENT;
    }
    return mmay_native_toolchain_probe(allocator, toolchain->path,
        MMAY_NATIVE_C_COMPILER_VERSION, identity_output);
}
