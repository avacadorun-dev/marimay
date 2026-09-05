#include "cli_bytecode_report.h"

/* Owner: actionable bytecode pipeline failure reasons for CLI consumers. */
const char *mmay_cli_bytecode_codec_message(MarimayBytecodeCodecStatus status) {
    switch (status) {
    case MMAY_BYTECODE_CODEC_OK:
        return "bytecode codec succeeded";
    case MMAY_BYTECODE_CODEC_INVALID_ARGUMENT:
        return "bytecode codec failed: invalid internal argument";
    case MMAY_BYTECODE_CODEC_ALLOCATION_FAILED:
        return "bytecode codec failed: allocation failed";
    case MMAY_BYTECODE_CODEC_LIMIT_REACHED:
        return "bytecode codec failed: resource limit reached";
    case MMAY_BYTECODE_CODEC_TRUNCATED:
        return "bytecode codec failed: truncated encoding";
    case MMAY_BYTECODE_CODEC_INVALID_ENCODING:
        return "bytecode codec failed: invalid encoding";
    case MMAY_BYTECODE_CODEC_UNSUPPORTED_VERSION:
        return "bytecode codec failed: unsupported format version or feature flags";
    case MMAY_BYTECODE_CODEC_VERIFY_FAILED:
        return "bytecode codec failed: verification rejected decoded function";
    case MMAY_BYTECODE_CODEC_TRAILING_DATA:
        return "bytecode codec failed: trailing encoded data";
    }
    return "bytecode codec failed: unknown internal status";
}
