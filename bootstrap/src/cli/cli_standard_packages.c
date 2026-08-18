#include "cli_standard_packages.h"

#include <string.h>

typedef struct RudaCliStandardSource {
    const char *import_path;
    const char *source_path;
    const unsigned char *source;
    size_t source_length;
} RudaCliStandardSource;

static const unsigned char ruda_cli_std_convert_source[] =
    "public extern \"c\" fn formatF32(value: f32): string;\n"
    "public extern \"c\" fn formatF64(value: f64): string;\n";

static const unsigned char ruda_cli_std_io_source[] =
    "public enum Error {\n"
    "  CapabilityDenied { capability: string; },\n"
    "  WriteFailed,\n"
    "}\n\n"
    "public fn message(error: Error): string {\n"
    "  return match error {\n"
    "    case CapabilityDenied { capability } => \"capability io.stdout is not granted\";\n"
    "    case WriteFailed => \"stdout write failed\";\n"
    "  };\n"
    "}\n\n"
    "public extern \"c\" fn println(text: string): Result<void, Error>;\n";

static RudaCliCheckTargetStatus ruda_cli_standard_package_add(
    RudaCliCheckTarget *target,
    const RudaCliStandardSource *standard
) {
    RudaCliCheckPackage package;
    RudaFileId file_id = 0;
    RudaCompilerStatus compiler_status;
    memset(&package, 0, sizeof(package));
    compiler_status = ruda_compiler_service_add_document(
        &target->service,
        standard->source_path,
        strlen(standard->source_path),
        1,
        standard->source,
        standard->source_length,
        &file_id
    );
    if (compiler_status != RUDA_COMPILER_OK) {
        return compiler_status == RUDA_COMPILER_ALLOCATION_FAILED
            ? RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED
            : (compiler_status == RUDA_COMPILER_LIMIT_REACHED
                ? RUDA_CLI_CHECK_TARGET_LIMIT_REACHED
                : RUDA_CLI_CHECK_TARGET_INTERNAL_ERROR);
    }
    if (!ruda_owned_bytes_copy(
            &package.import_path, target->allocator,
            standard->import_path, strlen(standard->import_path)
        ) || !ruda_owned_bytes_copy(
            &package.directory, target->allocator,
            standard->source_path, strlen(standard->source_path)
        ) || !ruda_vector_init(
            &package.file_ids, target->allocator, sizeof(RudaFileId)
        ) || !ruda_vector_push(&package.file_ids, &file_id)
        || !ruda_vector_push(&target->packages, &package)) {
        ruda_vector_dispose(&package.file_ids);
        ruda_owned_bytes_dispose(&package.directory);
        ruda_owned_bytes_dispose(&package.import_path);
        return RUDA_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    memset(&package, 0, sizeof(package));
    return RUDA_CLI_CHECK_TARGET_OK;
}

RudaCliCheckTargetStatus ruda_cli_standard_packages_add(
    RudaCliCheckTarget *target
) {
    static const RudaCliStandardSource sources[] = {
        {
            "std/convert", "std/convert/convert.ruda",
            ruda_cli_std_convert_source,
            sizeof(ruda_cli_std_convert_source) - 1
        },
        {
            "std/io", "std/io/io.ruda",
            ruda_cli_std_io_source,
            sizeof(ruda_cli_std_io_source) - 1
        }
    };
    size_t index = 0;
    if (target == NULL || !target->service_initialized
        || !target->packages_initialized) {
        return RUDA_CLI_CHECK_TARGET_INTERNAL_ERROR;
    }
    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); index += 1) {
        RudaCliCheckTargetStatus status = ruda_cli_standard_package_add(
            target, &sources[index]
        );
        if (status != RUDA_CLI_CHECK_TARGET_OK) {
            return status;
        }
    }
    return RUDA_CLI_CHECK_TARGET_OK;
}
