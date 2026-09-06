#include "cli_test_bytecode.h"

#include "cli_bytecode_report.h"

bool mmay_cli_test_bytecode_prepare(
    const MarimayMirFunction *const *functions,
    size_t function_count,
    MarimaySymbolId entry,
    MarimayAllocator allocator,
    MarimayBytecodeProgram *output,
    const char **failure_reason
) {
    MarimayBytecodeProgram lowered = {0};
    MarimayOwnedBytes encoded = {0};
    MarimayMirBytecodeLowerStatus lower_status;
    MarimayBytecodeCodecStatus codec_status;
    bool accepted = false;
    if (output == NULL || failure_reason == NULL) return false;
    mmay_bytecode_program_dispose(output);
    lower_status = mmay_mir_lower_bytecode_program(
        functions, function_count, entry, allocator,
        mmay_mir_bytecode_program_lower_default_options(), &lowered
    );
    if (lower_status != MMAY_MIR_BYTECODE_LOWER_OK) {
        *failure_reason = "test MIR could not lower to bytecode";
        goto cleanup;
    }
    codec_status = mmay_bytecode_encode_program(&lowered, allocator, &encoded);
    if (codec_status != MMAY_BYTECODE_CODEC_OK) {
        *failure_reason = mmay_cli_bytecode_codec_message(codec_status);
        goto cleanup;
    }
    codec_status = mmay_bytecode_decode_program(
        encoded.data, encoded.length, allocator,
        mmay_bytecode_program_default_options(), output
    );
    if (codec_status != MMAY_BYTECODE_CODEC_OK) {
        *failure_reason = mmay_cli_bytecode_codec_message(codec_status);
        goto cleanup;
    }
    accepted = true;
cleanup:
    if (!accepted) mmay_bytecode_program_dispose(output);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&lowered);
    return accepted;
}

bool mmay_cli_test_bytecode_select(
    MarimayBytecodeProgram *program,
    MarimaySymbolId entry,
    const char **failure_reason
) {
    uint32_t previous;
    if (program == NULL || failure_reason == NULL) return false;
    previous = program->entry_function;
    if (!mmay_bytecode_program_set_entry(program, entry)
        || mmay_bytecode_program_verify(program)
            != MMAY_BYTECODE_PROGRAM_VERIFY_OK) {
        program->entry_function = previous;
        *failure_reason = "test bytecode program verification failed";
        return false;
    }
    return true;
}
