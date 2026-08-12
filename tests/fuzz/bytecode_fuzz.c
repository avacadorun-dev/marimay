#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/bytecode/bytecode.h"
#include "ruda/bytecode/vm.h"

static bool ruda_fuzz_discard_write(
    void *context,
    const char *bytes,
    size_t length
) {
    (void)context;
    return bytes != NULL || length == 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    RudaBytecodeFunction function;
    RudaBytecodeFunction decoded_again;
    RudaBytecodeFunctionOptions options = ruda_bytecode_function_default_options();
    RudaBytecodeCodecStatus status;
    RudaOwnedBytes encoded;
    RudaWriter writer;
    RudaBytecodeValue *arguments = NULL;
    size_t argument_bytes = 0;
    size_t argument_index = 0;
    if (size > 1024U * 1024U) {
        return 0;
    }
    options.max_registers = 4096;
    options.max_blocks = 4096;
    options.max_instructions = 65536;
    options.max_source_files = 4096;
    options.max_encoded_bytes = 1024U * 1024U;
    options.max_verify_work = 1000000;
    memset(&function, 0, sizeof(function));
    memset(&decoded_again, 0, sizeof(decoded_again));
    memset(&encoded, 0, sizeof(encoded));
    status = ruda_bytecode_decode_function(
        data,
        size,
        ruda_system_allocator(),
        options,
        &function
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        if (status == RUDA_BYTECODE_CODEC_INVALID_ARGUMENT) {
            abort();
        }
        return 0;
    }
    if (ruda_bytecode_verify(&function) != RUDA_BYTECODE_VERIFY_OK
        || ruda_bytecode_encode_function(
            &function,
            ruda_system_allocator(),
            &encoded
        ) != RUDA_BYTECODE_CODEC_OK
        || encoded.length != size
        || memcmp(encoded.data, data, size) != 0) {
        abort();
    }
    writer.context = NULL;
    writer.write = ruda_fuzz_discard_write;
    if (ruda_bytecode_disassemble(&function, writer)
        != RUDA_BYTECODE_DISASSEMBLE_OK) {
        abort();
    }
    if (!ruda_size_multiply(
            function.parameter_count,
            sizeof(RudaBytecodeValue),
            &argument_bytes
        )) {
        abort();
    }
    if (argument_bytes != 0) {
        arguments = ruda_allocate(&function.allocator, argument_bytes);
        if (arguments == NULL) {
            ruda_owned_bytes_dispose(&encoded);
            ruda_bytecode_function_dispose(&function);
            return 0;
        }
        memset(arguments, 0, argument_bytes);
        for (argument_index = 0;
            argument_index < function.parameter_count;
            argument_index += 1) {
            const RudaBytecodeType *type = ruda_vector_at_const(
                &function.register_types,
                argument_index
            );
            arguments[argument_index].type = *type;
        }
    }
    {
        RudaBytecodeRunOptions run_options = ruda_bytecode_run_default_options();
        RudaBytecodeValue result;
        RudaBytecodeRunStatus run_status;
        run_options.max_steps = 1024;
        memset(&result, 0, sizeof(result));
        run_status = ruda_bytecode_run(
            &function,
            arguments,
            function.parameter_count,
            ruda_system_allocator(),
            run_options,
            &result
        );
        if (run_status != RUDA_BYTECODE_RUN_OK
            && run_status != RUDA_BYTECODE_RUN_STEP_LIMIT
            && run_status != RUDA_BYTECODE_RUN_OVERFLOW
            && run_status != RUDA_BYTECODE_RUN_DIVISION_BY_ZERO
            && run_status != RUDA_BYTECODE_RUN_LIMIT_REACHED
            && run_status != RUDA_BYTECODE_RUN_ALLOCATION_FAILED) {
            abort();
        }
    }
    if (ruda_bytecode_decode_function(
            encoded.data,
            encoded.length,
            ruda_system_allocator(),
            options,
            &decoded_again
        ) != RUDA_BYTECODE_CODEC_OK
        || ruda_bytecode_verify(&decoded_again) != RUDA_BYTECODE_VERIFY_OK) {
        abort();
    }
    ruda_bytecode_function_dispose(&decoded_again);
    ruda_deallocate(&function.allocator, arguments, argument_bytes);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_function_dispose(&function);
    return 0;
}
