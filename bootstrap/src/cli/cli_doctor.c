#include "cli_doctor.h"

#include <string.h>

#include "mmay/build_config.h"
#include "mmay/native/runtime_artifact.h"
#include "mmay/native/host_toolchain.h"

typedef struct MarimayCliDoctorState {
    MarimayNativeRuntimeArtifact debug;
    MarimayNativeRuntimeArtifact release;
    MarimayNativeRuntimeArtifactStatus debug_status;
    MarimayNativeRuntimeArtifactStatus release_status;
    MarimayNativeToolchainStatus compiler_status;
    MarimayNativeHostToolchain compiler;
    MarimayOwnedBytes compiler_output;
    const char *layout;
    bool ok;
} MarimayCliDoctorState;

static const char *mmay_cli_doctor_runtime_status(
    MarimayNativeRuntimeArtifactStatus status
) {
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK) return "ok";
    if (status == MMAY_NATIVE_RUNTIME_ARTIFACT_MISSING) return "missing";
    return "incompatible";
}

static bool mmay_cli_doctor_internal_runtime_status(
    MarimayNativeRuntimeArtifactStatus status
) {
    return status == MMAY_NATIVE_RUNTIME_ARTIFACT_INVALID_ARGUMENT
        || status == MMAY_NATIVE_RUNTIME_ARTIFACT_ALLOCATION_FAILED
        || status == MMAY_NATIVE_RUNTIME_ARTIFACT_LIMIT_REACHED;
}

static bool mmay_cli_doctor_json_string(
    MarimayWriter writer,
    const char *text
) {
    const unsigned char *bytes = (const unsigned char *)text;
    size_t length = text != NULL ? strlen(text) : 0;
    size_t index = 0;
    if (text == NULL || !mmay_writer_write_cstr(writer, "\"")) return false;
    for (index = 0; index < length; index += 1) {
        char escaped[7];
        unsigned char byte = bytes[index];
        if (byte == '"' || byte == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)byte;
            if (!mmay_writer_write(writer, escaped, 2)) return false;
        } else if (byte <= 0x1fU) {
            static const char hex[] = "0123456789abcdef";
            escaped[0] = '\\'; escaped[1] = 'u'; escaped[2] = '0';
            escaped[3] = '0'; escaped[4] = hex[byte >> 4];
            escaped[5] = hex[byte & 0x0fU]; escaped[6] = 0;
            if (!mmay_writer_write(writer, escaped, 6)) return false;
        } else if (!mmay_writer_write(writer, (const char *)&bytes[index], 1)) {
            return false;
        }
    }
    return mmay_writer_write_cstr(writer, "\"");
}

static bool mmay_cli_doctor_json_runtime(
    MarimayWriter writer,
    MarimayNativeRuntimeArtifactStatus status,
    const MarimayNativeRuntimeArtifact *artifact
) {
    const char *path = status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK
        ? (const char *)artifact->archive.data : "";
    return mmay_writer_write_cstr(writer, "{\"status\":")
        && mmay_cli_doctor_json_string(
            writer, mmay_cli_doctor_runtime_status(status)
        ) && mmay_writer_write_cstr(writer, ",\"path\":")
        && mmay_cli_doctor_json_string(writer, path)
        && mmay_writer_write_cstr(writer, "}");
}

static bool mmay_cli_doctor_write_json(
    MarimayWriter writer,
    const MarimayCliDoctorState *state
) {
    return mmay_writer_write_cstr(writer, "{\"schema\":\"marimay.doctor/1\",\"ok\":")
        && mmay_writer_write_cstr(writer, state->ok ? "true" : "false")
        && mmay_writer_write_cstr(writer,
            ",\"toolchain\":{\"compiler\":\"" MMAY_COMPILER_VERSION
            "\",\"runtime\":\"" MMAY_RUNTIME_VERSION
            "\"},\"host\":{\"system\":\"" MMAY_HOST_SYSTEM
            "\",\"processor\":\"" MMAY_HOST_PROCESSOR
            "\"},\"layout\":")
        && mmay_cli_doctor_json_string(writer, state->layout)
        && mmay_writer_write_cstr(writer, ",\"compiler\":{\"path\":")
        && mmay_cli_doctor_json_string(writer, state->compiler.path)
        && mmay_writer_write_cstr(writer, ",\"id\":")
        && mmay_cli_doctor_json_string(writer, state->compiler.id)
        && mmay_writer_write_cstr(writer, ",\"version\":")
        && mmay_cli_doctor_json_string(writer, state->compiler.version)
        && mmay_writer_write_cstr(writer, ",\"available\":")
        && mmay_writer_write_cstr(
            writer, state->compiler_status == MMAY_NATIVE_TOOLCHAIN_OK
                ? "true" : "false"
        ) && mmay_writer_write_cstr(writer, ",\"policy\":")
        && mmay_cli_doctor_json_string(writer, state->compiler.policy)
        && mmay_writer_write_cstr(writer, ",\"build\":")
        && mmay_cli_doctor_json_string(writer, state->compiler.build)
        && mmay_writer_write_cstr(writer, ",\"target\":")
        && mmay_cli_doctor_json_string(writer, state->compiler.target)
        && mmay_writer_write_cstr(writer, "},\"runtimes\":{\"debug\":")
        && mmay_cli_doctor_json_runtime(
            writer, state->debug_status, &state->debug
        ) && mmay_writer_write_cstr(writer, ",\"release\":")
        && mmay_cli_doctor_json_runtime(
            writer, state->release_status, &state->release
        ) && mmay_writer_write_cstr(writer, "}}\n");
}

static bool mmay_cli_doctor_human_runtime(
    MarimayWriter writer,
    const char *label,
    MarimayNativeRuntimeArtifactStatus status,
    const MarimayNativeRuntimeArtifact *artifact
) {
    return mmay_writer_write_cstr(writer, label)
        && mmay_writer_write_cstr(writer, ": ")
        && mmay_writer_write_cstr(
            writer, mmay_cli_doctor_runtime_status(status)
        ) && (status != MMAY_NATIVE_RUNTIME_ARTIFACT_OK
            || (mmay_writer_write_cstr(writer, " ")
                && mmay_writer_write(
                    writer, (const char *)artifact->archive.data,
                    artifact->archive.length
                ))) && mmay_writer_write_cstr(writer, "\n");
}

static bool mmay_cli_doctor_write_human(
    MarimayWriter writer,
    const MarimayCliDoctorState *state
) {
    return mmay_writer_write_cstr(
            writer, "toolchain: " MMAY_COMPILER_VERSION "\n"
            "host: " MMAY_HOST_SYSTEM "/" MMAY_HOST_PROCESSOR "\nlayout: "
        ) && mmay_writer_write_cstr(writer, state->layout)
        && mmay_writer_write_cstr(writer, "\ncompiler: ")
        && mmay_writer_write_cstr(
            writer, state->compiler_status == MMAY_NATIVE_TOOLCHAIN_OK
                ? "ok " : "missing "
        ) && mmay_writer_write_cstr(writer, state->compiler.path)
        && mmay_writer_write_cstr(writer, " (")
        && mmay_writer_write_cstr(writer, state->compiler.id)
        && mmay_writer_write_cstr(writer, " ")
        && mmay_writer_write_cstr(writer, state->compiler.version)
        && mmay_writer_write_cstr(writer, "; ")
        && mmay_writer_write_cstr(writer, state->compiler.policy)
        && mmay_writer_write_cstr(writer, state->compiler.portable_macos ? "; " : "")
        && mmay_writer_write_cstr(writer, state->compiler.build)
        && mmay_writer_write_cstr(writer, state->compiler.portable_macos ? "; " : "")
        && mmay_writer_write_cstr(writer, state->compiler.target)
        && mmay_writer_write_cstr(writer, ")\n")
        && mmay_cli_doctor_human_runtime(
            writer, "runtime-debug", state->debug_status, &state->debug
        ) && mmay_cli_doctor_human_runtime(
            writer, "runtime-release", state->release_status, &state->release
        ) && mmay_writer_write_cstr(
            writer, state->ok ? "doctor: ok\n" : "doctor: failed\n"
        );
}

int mmay_cli_doctor(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output
) {
    MarimayCliDoctorState state;
    bool written;
    memset(&state, 0, sizeof(state));
    state.layout = mmay_native_host_toolchain_is_installed(options->tool_executable_path)
        ? "installed" : "build-tree";
    state.compiler_status = mmay_native_host_toolchain_probe(
        session->allocator, options->tool_executable_path, NULL,
        &state.compiler, &state.compiler_output
    );
    state.debug_status = mmay_native_runtime_artifact_select(
        session->allocator, MMAY_NATIVE_RUNTIME_DEBUG,
        options->tool_executable_path, NULL, &state.debug
    );
    state.release_status = mmay_native_runtime_artifact_select(
        session->allocator, MMAY_NATIVE_RUNTIME_RELEASE,
        options->tool_executable_path, NULL, &state.release
    );
    if (state.compiler_status == MMAY_NATIVE_TOOLCHAIN_ALLOCATION_FAILED
        || mmay_cli_doctor_internal_runtime_status(state.debug_status)
        || mmay_cli_doctor_internal_runtime_status(state.release_status)) {
        mmay_native_runtime_artifact_dispose(&state.debug);
        mmay_native_runtime_artifact_dispose(&state.release);
        mmay_owned_bytes_dispose(&state.compiler_output);
        return MMAY_EXIT_INTERNAL;
    }
    state.ok = state.compiler_status == MMAY_NATIVE_TOOLCHAIN_OK
        && state.debug_status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK
        && state.release_status == MMAY_NATIVE_RUNTIME_ARTIFACT_OK;
    written = options->version_json
        ? mmay_cli_doctor_write_json(standard_output, &state)
        : mmay_cli_doctor_write_human(standard_output, &state);
    mmay_native_runtime_artifact_dispose(&state.debug);
    mmay_native_runtime_artifact_dispose(&state.release);
    mmay_owned_bytes_dispose(&state.compiler_output);
    if (!written) return MMAY_EXIT_INTERNAL;
    return state.ok ? MMAY_EXIT_SUCCESS : MMAY_EXIT_SOURCE_FAILURE;
}
