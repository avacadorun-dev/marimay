#include "cli_standard_packages.h"

#include <string.h>

typedef struct MarimayCliStandardSource {
    const char *import_path;
    const char *source_path;
    const unsigned char *source;
    size_t source_length;
} MarimayCliStandardSource;

static const unsigned char mmay_cli_std_convert_source[] =
    "public enum ParseError {\n"
    "  Empty,\n"
    "  InvalidDigit { offset: int; },\n"
    "  OutOfRange,\n"
    "  InvalidRadix,\n"
    "}\n\n"
    "public extern \"c\" fn parseInt(text: string): Result<int, ParseError>;\n"
    "public extern \"c\" fn parseIntRadix(text: string, radix: int): Result<int, ParseError>;\n"
    "public extern \"c\" fn formatF32(value: f32): string;\n"
    "public extern \"c\" fn formatF64(value: f64): string;\n"
    "public extern \"c\" fn formatBool(value: bool): string;\n"
    "public extern \"c\" fn formatI8(value: i8): string;\n"
    "public extern \"c\" fn formatI16(value: i16): string;\n"
    "public extern \"c\" fn formatI32(value: i32): string;\n"
    "public extern \"c\" fn formatI64(value: i64): string;\n"
    "public extern \"c\" fn formatU8(value: u8): string;\n"
    "public extern \"c\" fn formatU16(value: u16): string;\n"
    "public extern \"c\" fn formatU32(value: u32): string;\n"
    "public extern \"c\" fn formatU64(value: u64): string;\n";

static const unsigned char mmay_cli_std_io_source[] =
    "public enum Error {\n"
    "  CapabilityDenied { capability: string; },\n"
    "  ReadFailed,\n"
    "  WriteFailed,\n"
    "  FlushFailed,\n"
    "  InvalidUtf8,\n"
    "  LineTooLong { limit: int; },\n"
    "}\n\n"
    "public fn message(error: Error): string {\n"
    "  return match error {\n"
    "    case CapabilityDenied { capability } => \"capability io.stdout is not granted\";\n"
    "    case ReadFailed => \"standard input read failed\";\n"
    "    case WriteFailed => \"stdout write failed\";\n"
    "    case FlushFailed => \"standard stream flush failed\";\n"
    "    case InvalidUtf8 => \"standard input is not valid UTF-8\";\n"
    "    case LineTooLong { limit } => \"standard input line exceeds limit\";\n"
    "  };\n"
    "}\n\n"
    "public extern \"c\" fn readLine(limit: int): Result<Option<string>, Error>;\n"
    "public extern \"c\" fn print(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn println(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn eprint(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn eprintln(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn flushStdout(): Result<void, Error>;\n"
    "public extern \"c\" fn flushStderr(): Result<void, Error>;\n";

static MarimayCliCheckTargetStatus mmay_cli_standard_package_add(
    MarimayCliCheckTarget *target,
    const MarimayCliStandardSource *standard
) {
    MarimayCliCheckPackage package;
    MarimayFileId file_id = 0;
    MarimayCompilerStatus compiler_status;
    memset(&package, 0, sizeof(package));
    compiler_status = mmay_compiler_service_add_document(
        &target->service,
        standard->source_path,
        strlen(standard->source_path),
        1,
        standard->source,
        standard->source_length,
        &file_id
    );
    if (compiler_status != MMAY_COMPILER_OK) {
        return compiler_status == MMAY_COMPILER_ALLOCATION_FAILED
            ? MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED
            : (compiler_status == MMAY_COMPILER_LIMIT_REACHED
                ? MMAY_CLI_CHECK_TARGET_LIMIT_REACHED
                : MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR);
    }
    if (!mmay_owned_bytes_copy(
            &package.import_path, target->allocator,
            standard->import_path, strlen(standard->import_path)
        ) || !mmay_owned_bytes_copy(
            &package.directory, target->allocator,
            standard->source_path, strlen(standard->source_path)
        ) || !mmay_vector_init(
            &package.file_ids, target->allocator, sizeof(MarimayFileId)
        ) || !mmay_vector_push(&package.file_ids, &file_id)
        || !mmay_vector_push(&target->packages, &package)) {
        mmay_vector_dispose(&package.file_ids);
        mmay_owned_bytes_dispose(&package.directory);
        mmay_owned_bytes_dispose(&package.import_path);
        return MMAY_CLI_CHECK_TARGET_ALLOCATION_FAILED;
    }
    memset(&package, 0, sizeof(package));
    return MMAY_CLI_CHECK_TARGET_OK;
}

MarimayCliCheckTargetStatus mmay_cli_standard_packages_add(
    MarimayCliCheckTarget *target
) {
    static const MarimayCliStandardSource sources[] = {
        {
            "std/convert", "std/convert/convert.my",
            mmay_cli_std_convert_source,
            sizeof(mmay_cli_std_convert_source) - 1
        },
        {
            "std/io", "std/io/io.my",
            mmay_cli_std_io_source,
            sizeof(mmay_cli_std_io_source) - 1
        }
    };
    size_t index = 0;
    if (target == NULL || !target->service_initialized
        || !target->packages_initialized) {
        return MMAY_CLI_CHECK_TARGET_INTERNAL_ERROR;
    }
    for (index = 0; index < sizeof(sources) / sizeof(sources[0]); index += 1) {
        MarimayCliCheckTargetStatus status = mmay_cli_standard_package_add(
            target, &sources[index]
        );
        if (status != MMAY_CLI_CHECK_TARGET_OK) {
            return status;
        }
    }
    return MMAY_CLI_CHECK_TARGET_OK;
}
