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

static bool ruda_fuzz_run_status_expected(RudaBytecodeRunStatus status) {
    return status == RUDA_BYTECODE_RUN_OK
        || status == RUDA_BYTECODE_RUN_STEP_LIMIT
        || status == RUDA_BYTECODE_RUN_OVERFLOW
        || status == RUDA_BYTECODE_RUN_DIVISION_BY_ZERO
        || status == RUDA_BYTECODE_RUN_INVALID_SHIFT
        || status == RUDA_BYTECODE_RUN_PANIC
        || status == RUDA_BYTECODE_RUN_INVALID_REFERENCE
        || status == RUDA_BYTECODE_RUN_LIMIT_REACHED
        || status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
}

static void ruda_fuzz_program(const uint8_t *data, size_t size) {
    RudaBytecodeProgram program;
    RudaBytecodeProgramOptions options = ruda_bytecode_program_default_options();
    RudaBytecodeCodecStatus status;
    RudaOwnedBytes encoded;
    RudaBytecodeValue *arguments = NULL;
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
    status = ruda_bytecode_decode_program(
        data,
        size,
        ruda_system_allocator(),
        options,
        &program
    );
    if (status != RUDA_BYTECODE_CODEC_OK) {
        if (status == RUDA_BYTECODE_CODEC_INVALID_ARGUMENT) {
            abort();
        }
        return;
    }
    if (ruda_bytecode_program_verify(&program)
            != RUDA_BYTECODE_PROGRAM_VERIFY_OK
        || ruda_bytecode_encode_program(
            &program,
            ruda_system_allocator(),
            &encoded
        ) != RUDA_BYTECODE_CODEC_OK
        || encoded.length != size
        || memcmp(encoded.data, data, size) != 0) {
        abort();
    }
    {
        const RudaBytecodeProgramFunction *entry =
            ruda_bytecode_program_function(&program, program.entry_function);
        if (entry == NULL || !ruda_size_multiply(
                entry->body.parameter_count,
                sizeof(RudaBytecodeValue),
                &argument_bytes
            )) {
            abort();
        }
        argument_count = entry->body.parameter_count;
        if (argument_bytes != 0) {
            arguments = ruda_allocate(&program.allocator, argument_bytes);
            if (arguments == NULL) {
                ruda_owned_bytes_dispose(&encoded);
                ruda_bytecode_program_dispose(&program);
                return;
            }
            memset(arguments, 0, argument_bytes);
            for (index = 0; index < argument_count; index += 1) {
                const RudaBytecodeType *type = ruda_vector_at_const(
                    &entry->body.register_types,
                    index
                );
                arguments[index].type = *type;
            }
        }
    }
    {
        RudaBytecodeRunOptions run_options = ruda_bytecode_run_default_options();
        RudaBytecodeExecution execution;
        RudaBytecodeValue result;
        memset(&execution, 0, sizeof(execution));
        if (!ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
                ruda_bytecode_execution_default_options())) {
            abort();
        }
        run_options.max_steps = 1024;
        run_options.max_call_depth = 64;
        run_options.execution = &execution;
        memset(&result, 0, sizeof(result));
        if (!ruda_fuzz_run_status_expected(ruda_bytecode_run_program(
                &program,
                arguments,
                argument_count,
                ruda_system_allocator(),
                run_options,
                &result
            ))) {
            abort();
        }
        ruda_bytecode_execution_dispose(&execution);
    }
    ruda_deallocate(&program.allocator, arguments, argument_bytes);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&program);
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
    ruda_fuzz_program(data, size);
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
        RudaBytecodeExecution execution;
        RudaBytecodeValue result;
        RudaBytecodeRunStatus run_status;
        memset(&execution, 0, sizeof(execution));
        if (!ruda_bytecode_execution_init(&execution, ruda_system_allocator(),
                ruda_bytecode_execution_default_options())) {
            abort();
        }
        run_options.max_steps = 1024;
        run_options.execution = &execution;
        memset(&result, 0, sizeof(result));
        run_status = ruda_bytecode_run(
            &function,
            arguments,
            function.parameter_count,
            ruda_system_allocator(),
            run_options,
            &result
        );
        if (function.calls.length == 0
            && !ruda_fuzz_run_status_expected(run_status)) {
            abort();
        }
        if (function.calls.length != 0
            && run_status != RUDA_BYTECODE_RUN_INVALID_BYTECODE) {
            abort();
        }
        ruda_bytecode_execution_dispose(&execution);
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
