#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/bytecode/bytecode.h"
#include "mmay/bytecode/vm.h"

static bool mmay_fuzz_discard_write(
    void *context,
    const char *bytes,
    size_t length
) {
    (void)context;
    return bytes != NULL || length == 0;
}

static bool mmay_fuzz_run_status_expected(MarimayBytecodeRunStatus status) {
    return status == MMAY_BYTECODE_RUN_OK
        || status == MMAY_BYTECODE_RUN_STEP_LIMIT
        || status == MMAY_BYTECODE_RUN_OVERFLOW
        || status == MMAY_BYTECODE_RUN_DIVISION_BY_ZERO
        || status == MMAY_BYTECODE_RUN_INVALID_SHIFT
        || status == MMAY_BYTECODE_RUN_PANIC
        || status == MMAY_BYTECODE_RUN_INVALID_REFERENCE
        || status == MMAY_BYTECODE_RUN_UNSUPPORTED_HOST
        || status == MMAY_BYTECODE_RUN_LIMIT_REACHED
        || status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
}

static void mmay_fuzz_program(const uint8_t *data, size_t size) {
    MarimayBytecodeProgram program;
    MarimayBytecodeProgramOptions options = mmay_bytecode_program_default_options();
    MarimayBytecodeCodecStatus status;
    MarimayOwnedBytes encoded;
    MarimayBytecodeValue *arguments = NULL;
    size_t argument_count = 0;
    size_t argument_bytes = 0;
    size_t index = 0;
    memset(&program, 0, sizeof(program));
    memset(&encoded, 0, sizeof(encoded));
    options.max_functions = 256;
    options.max_total_registers = 4096;
    options.max_total_blocks = 4096;
    options.max_total_instructions = 65536;
    options.max_total_calls = 4096;
    options.max_total_call_arguments = 65536;
    options.max_total_string_constants = 4096;
    options.max_total_string_bytes = 1024U * 1024U;
    options.max_encoded_bytes = 1024U * 1024U;
    options.max_verify_work = 1000000;
    options.function_options.max_registers = 4096;
    options.function_options.max_blocks = 4096;
    options.function_options.max_instructions = 65536;
    options.function_options.max_calls = 4096;
    options.function_options.max_call_arguments = 65536;
    options.function_options.max_string_constants = 4096;
    options.function_options.max_string_bytes = 1024U * 1024U;
    options.function_options.max_source_files = 4096;
    options.function_options.max_encoded_bytes = 1024U * 1024U;
    options.function_options.max_verify_work = 1000000;
    status = mmay_bytecode_decode_program(
        data,
        size,
        mmay_system_allocator(),
        options,
        &program
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        if (status == MMAY_BYTECODE_CODEC_INVALID_ARGUMENT) {
            abort();
        }
        return;
    }
    if (mmay_bytecode_program_verify(&program)
            != MMAY_BYTECODE_PROGRAM_VERIFY_OK
        || mmay_bytecode_encode_program(
            &program,
            mmay_system_allocator(),
            &encoded
        ) != MMAY_BYTECODE_CODEC_OK
        || encoded.length != size
        || memcmp(encoded.data, data, size) != 0) {
        abort();
    }
    {
        const MarimayBytecodeProgramFunction *entry =
            mmay_bytecode_program_function(&program, program.entry_function);
        if (entry == NULL || !mmay_size_multiply(
                entry->body.parameter_count,
                sizeof(MarimayBytecodeValue),
                &argument_bytes
            )) {
            abort();
        }
        argument_count = entry->body.parameter_count;
        if (argument_bytes != 0) {
            arguments = mmay_allocate(&program.allocator, argument_bytes);
            if (arguments == NULL) {
                mmay_owned_bytes_dispose(&encoded);
                mmay_bytecode_program_dispose(&program);
                return;
            }
            memset(arguments, 0, argument_bytes);
            for (index = 0; index < argument_count; index += 1) {
                const MarimayBytecodeType *type = mmay_vector_at_const(
                    &entry->body.register_types,
                    index
                );
                arguments[index].type = *type;
            }
        }
    }
    {
        MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
        MarimayBytecodeExecution execution;
        MarimayBytecodeValue result;
        memset(&execution, 0, sizeof(execution));
        if (!mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
                mmay_bytecode_execution_default_options())) {
            abort();
        }
        run_options.max_steps = 1024;
        run_options.max_call_depth = 64;
        run_options.execution = &execution;
        memset(&result, 0, sizeof(result));
        if (!mmay_fuzz_run_status_expected(mmay_bytecode_run_program(
                &program,
                arguments,
                argument_count,
                mmay_system_allocator(),
                run_options,
                &result
            ))) {
            abort();
        }
        mmay_bytecode_execution_dispose(&execution);
    }
    mmay_deallocate(&program.allocator, arguments, argument_bytes);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&program);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    MarimayBytecodeFunction function;
    MarimayBytecodeFunction decoded_again;
    MarimayBytecodeFunctionOptions options = mmay_bytecode_function_default_options();
    MarimayBytecodeCodecStatus status;
    MarimayOwnedBytes encoded;
    MarimayWriter writer;
    MarimayBytecodeValue *arguments = NULL;
    size_t argument_bytes = 0;
    size_t argument_index = 0;
    if (size > 1024U * 1024U) {
        return 0;
    }
    mmay_fuzz_program(data, size);
    options.max_registers = 4096;
    options.max_blocks = 4096;
    options.max_instructions = 65536;
    options.max_calls = 4096;
    options.max_call_arguments = 65536;
    options.max_string_constants = 4096;
    options.max_string_bytes = 1024U * 1024U;
    options.max_source_files = 4096;
    options.max_encoded_bytes = 1024U * 1024U;
    options.max_verify_work = 1000000;
    memset(&function, 0, sizeof(function));
    memset(&decoded_again, 0, sizeof(decoded_again));
    memset(&encoded, 0, sizeof(encoded));
    status = mmay_bytecode_decode_function(
        data,
        size,
        mmay_system_allocator(),
        options,
        &function
    );
    if (status != MMAY_BYTECODE_CODEC_OK) {
        if (status == MMAY_BYTECODE_CODEC_INVALID_ARGUMENT) {
            abort();
        }
        return 0;
    }
    if (mmay_bytecode_verify(&function) != MMAY_BYTECODE_VERIFY_OK
        || mmay_bytecode_encode_function(
            &function,
            mmay_system_allocator(),
            &encoded
        ) != MMAY_BYTECODE_CODEC_OK
        || encoded.length != size
        || memcmp(encoded.data, data, size) != 0) {
        abort();
    }
    writer.context = NULL;
    writer.write = mmay_fuzz_discard_write;
    if (mmay_bytecode_disassemble(&function, writer)
        != MMAY_BYTECODE_DISASSEMBLE_OK) {
        abort();
    }
    if (!mmay_size_multiply(
            function.parameter_count,
            sizeof(MarimayBytecodeValue),
            &argument_bytes
        )) {
        abort();
    }
    if (argument_bytes != 0) {
        arguments = mmay_allocate(&function.allocator, argument_bytes);
        if (arguments == NULL) {
            mmay_owned_bytes_dispose(&encoded);
            mmay_bytecode_function_dispose(&function);
            return 0;
        }
        memset(arguments, 0, argument_bytes);
        for (argument_index = 0;
            argument_index < function.parameter_count;
            argument_index += 1) {
            const MarimayBytecodeType *type = mmay_vector_at_const(
                &function.register_types,
                argument_index
            );
            arguments[argument_index].type = *type;
        }
    }
    {
        MarimayBytecodeRunOptions run_options = mmay_bytecode_run_default_options();
        MarimayBytecodeExecution execution;
        MarimayBytecodeValue result;
        MarimayBytecodeRunStatus run_status;
        memset(&execution, 0, sizeof(execution));
        if (!mmay_bytecode_execution_init(&execution, mmay_system_allocator(),
                mmay_bytecode_execution_default_options())) {
            abort();
        }
        run_options.max_steps = 1024;
        run_options.execution = &execution;
        memset(&result, 0, sizeof(result));
        run_status = mmay_bytecode_run(
            &function,
            arguments,
            function.parameter_count,
            mmay_system_allocator(),
            run_options,
            &result
        );
        if (function.calls.length == 0
            && !mmay_fuzz_run_status_expected(run_status)) {
            abort();
        }
        if (function.calls.length != 0
            && run_status != MMAY_BYTECODE_RUN_INVALID_BYTECODE) {
            abort();
        }
        mmay_bytecode_execution_dispose(&execution);
    }
    if (mmay_bytecode_decode_function(
            encoded.data,
            encoded.length,
            mmay_system_allocator(),
            options,
            &decoded_again
        ) != MMAY_BYTECODE_CODEC_OK
        || mmay_bytecode_verify(&decoded_again) != MMAY_BYTECODE_VERIFY_OK) {
        abort();
    }
    mmay_bytecode_function_dispose(&decoded_again);
    mmay_deallocate(&function.allocator, arguments, argument_bytes);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_function_dispose(&function);
    return 0;
}
