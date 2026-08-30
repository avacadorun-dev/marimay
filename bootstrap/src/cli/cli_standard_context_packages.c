#include "cli_standard_packages_internal.h"

static const unsigned char mmay_cli_std_env_source[] =
    "public enum Error {\n"
    "  InvalidName,\n"
    "  CapabilityDenied { name: string, },\n"
    "  InvalidUtf8 { offset: int, },\n"
    "  LimitExceeded { limit: int, },\n"
    "  ReadFailed,\n"
    "}\n\n"
    "public fn message(error: Error): string {\n"
    "  return match error {\n"
    "    InvalidName => \"invalid environment variable name\",\n"
    "    CapabilityDenied { name } => $\"environment variable {name} is not allowed\",\n"
    "    InvalidUtf8 { offset } => $\"environment value is not valid UTF-8 at byte {offset}\",\n"
    "    LimitExceeded { limit } => $\"environment value exceeds {limit}-byte limit\",\n"
    "    ReadFailed => \"environment variable read failed\",\n"
    "  };\n"
    "}\n\n"
    "extern \"c\" fn mmayEnvGet(name: string): Result<Option<string>, Error>;\n\n"
    "public fn get(name: string): Result<Option<string>, Error> {\n"
    "  return mmayEnvGet(name);\n"
    "}\n";

static const unsigned char mmay_cli_std_process_source[] =
    "import * as paths from \"std/path\";\n\n"
    "public enum ContextError {\n"
    "  CurrentDirectoryUnavailable,\n"
    "  ExecutablePathUnavailable,\n"
    "  InvalidUtf8 { offset: int, },\n"
    "  InvalidPath,\n"
    "}\n\n"
    "public fn message(error: ContextError): string {\n"
    "  return match error {\n"
    "    CurrentDirectoryUnavailable => \"current directory is unavailable\",\n"
    "    ExecutablePathUnavailable => \"executable path is unavailable\",\n"
    "    InvalidUtf8 { offset } => $\"process context is not valid UTF-8 at byte {offset}\",\n"
    "    InvalidPath => \"process context is not a valid absolute path\",\n"
    "  };\n"
    "}\n\n"
    "extern \"c\" fn mmayProcessCurrentDirectory(): Result<string, ContextError>;\n"
    "extern \"c\" fn mmayProcessExecutablePath(): Result<string, ContextError>;\n\n"
    "fn fromText(value: Result<string, ContextError>): Result<paths.Path, ContextError> {\n"
    "  return match value {\n"
    "    Ok(text) => match paths.fromString(text) {\n"
    "      Ok(path) => Ok(path),\n"
    "      Err(_) => Err(ContextError.InvalidPath),\n"
    "    },\n"
    "    Err(error) => Err(error),\n"
    "  };\n"
    "}\n\n"
    "public fn currentDirectory(): Result<paths.Path, ContextError> {\n"
    "  return fromText(mmayProcessCurrentDirectory());\n"
    "}\n\n"
    "public fn executablePath(): Result<paths.Path, ContextError> {\n"
    "  return fromText(mmayProcessExecutablePath());\n"
    "}\n";

MarimayCliCheckTargetStatus mmay_cli_standard_context_packages_add(
    MarimayCliCheckTarget *target
) {
    static const MarimayCliStandardSource sources[] = {
        {
            "std/env", "std/env/env.my",
            mmay_cli_std_env_source, sizeof(mmay_cli_std_env_source) - 1,
            NULL, NULL, 0,
            NULL, NULL, 0
        },
        {
            "std/process", "std/process/process.my",
            mmay_cli_std_process_source,
            sizeof(mmay_cli_std_process_source) - 1,
            NULL, NULL, 0,
            NULL, NULL, 0
        }
    };
    size_t index = 0;
    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); index += 1) {
        MarimayCliCheckTargetStatus status = mmay_cli_standard_package_add(
            target, &sources[index]
        );
        if (status != MMAY_CLI_CHECK_TARGET_OK) return status;
    }
    return MMAY_CLI_CHECK_TARGET_OK;
}
