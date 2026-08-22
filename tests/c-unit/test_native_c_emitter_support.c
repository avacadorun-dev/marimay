#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#endif

#include "test_native_c_emitter_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RUDA_TEST_C_COMPILER
#define RUDA_TEST_C_COMPILER "cc"
#endif

#ifndef RUDA_TEST_SOURCE_DIR
#define RUDA_TEST_SOURCE_DIR "."
#endif

#if !defined(_WIN32)
#include <unistd.h>

#include "ruda/platform/process.h"
#endif

bool ruda_test_native_contains(
    const RudaOwnedBytes *bytes,
    const char *needle
) {
    size_t needle_length = needle != NULL ? strlen(needle) : 0;
    size_t index = 0;
    if (bytes == NULL || needle == NULL || needle_length > bytes->length) {
        return false;
    }
    for (index = 0; index + needle_length <= bytes->length; index += 1) {
        if (memcmp(bytes->data + index, needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

#if !defined(_WIN32)
bool ruda_test_native_hex_bytes(
    const unsigned char *bytes,
    size_t length,
    char *output,
    size_t capacity
) {
    static const char digits[] = "0123456789abcdef";
    size_t index = 0;
    if ((bytes == NULL && length != 0) || output == NULL || capacity == 0
        || length > (capacity - 1) / 2) {
        return false;
    }
    for (index = 0; index < length; index += 1) {
        output[index * 2] = digits[bytes[index] >> 4];
        output[index * 2 + 1] = digits[bytes[index] & 0x0f];
    }
    output[length * 2] = '\0';
    return true;
}
#endif

RudaMirInstruction ruda_test_native_instruction(
    RudaMirInstructionKind kind,
    RudaMirRegister target,
    RudaMirRegister first,
    RudaMirRegister second,
    int64_t integer,
    bool boolean,
    size_t start
) {
    RudaMirInstruction instruction;
    memset(&instruction, 0, sizeof(instruction));
    instruction.kind = kind;
    instruction.target = target;
    instruction.first = first;
    instruction.second = second;
    instruction.integer = integer;
    instruction.boolean = boolean;
    instruction.span.start = start;
    instruction.span.end = start + 1;
    return instruction;
}

RudaMirTerminator ruda_test_native_terminator(
    RudaMirTerminatorKind kind,
    RudaMirRegister value,
    RudaMirBlockId target,
    RudaMirBlockId otherwise,
    size_t start
) {
    RudaMirTerminator terminator;
    memset(&terminator, 0, sizeof(terminator));
    terminator.kind = kind;
    terminator.value = value;
    terminator.target = target;
    terminator.otherwise = otherwise;
    terminator.span.start = start;
    terminator.span.end = start + 1;
    return terminator;
}


#if !defined(_WIN32)
/* Local child-compiler evidence only; Windows validates emitter-only tests. */
void ruda_test_native_compile_and_run_configured(
    const RudaOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit,
    const char *configuration
) {
    static const char *const optimizations[] = {"-O0", "-O2"};
    char directory[] = "/tmp/ruda-native-c-XXXXXX";
    char source_path[512];
    char executable_path[512];
    char include_argument[1024];
    char runtime_include_argument[1024];
    char checked_source_path[1024];
    char checked_integer_source_path[1024];
    char number_format_source_path[1024];
    char f64_source_path[1024];
    char allocator_source_path[1024];
    char provenance_source_path[1024];
    char managed_source_path[1024];
    char managed_internal_source_path[1024];
    char managed_string_source_path[1024];
    char managed_record_source_path[1024];
    char managed_enum_source_path[1024];
    char managed_interface_source_path[1024];
    const char *run_arguments[2];
    RudaProcessResult process;
    FILE *file = NULL;
    int written = 0;
    size_t optimization_index = 0;
    bool source_created = false;
    if (source == NULL || source->data == NULL || mkdtemp(directory) == NULL) {
        RUDA_EXPECT(false);
        return;
    }
    written = snprintf(source_path, sizeof(source_path), "%s/program.c", directory);
    if (written < 0 || (size_t)written >= sizeof(source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        include_argument,
        sizeof(include_argument),
        "-I%s/bootstrap/include",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(include_argument)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        runtime_include_argument,
        sizeof(runtime_include_argument),
        "-I%s/runtime/include",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(runtime_include_argument)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        checked_source_path,
        sizeof(checked_source_path),
        "%s/bootstrap/src/base/checked_i64.c",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(checked_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        checked_integer_source_path,
        sizeof(checked_integer_source_path),
        "%s/bootstrap/src/base/checked_integer.c",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0
        || (size_t)written >= sizeof(checked_integer_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        number_format_source_path,
        sizeof(number_format_source_path),
        "%s/bootstrap/src/base/number_format.c",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0
        || (size_t)written >= sizeof(number_format_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        f64_source_path,
        sizeof(f64_source_path),
        "%s/bootstrap/src/base/f64_runtime.c",
        RUDA_TEST_SOURCE_DIR
    );
    if (written < 0 || (size_t)written >= sizeof(f64_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(allocator_source_path, sizeof(allocator_source_path),
        "%s/bootstrap/src/base/allocator.c", RUDA_TEST_SOURCE_DIR);
    if (written < 0 || (size_t)written >= sizeof(allocator_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(provenance_source_path, sizeof(provenance_source_path),
        "%s/bootstrap/src/base/provenance.c", RUDA_TEST_SOURCE_DIR);
    if (written < 0 || (size_t)written >= sizeof(provenance_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(managed_source_path, sizeof(managed_source_path),
        "%s/runtime/common/managed.c", RUDA_TEST_SOURCE_DIR);
    if (written < 0 || (size_t)written >= sizeof(managed_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        managed_internal_source_path, sizeof(managed_internal_source_path),
        "%s/runtime/common/managed_internal.c", RUDA_TEST_SOURCE_DIR
    );
    if (written < 0
        || (size_t)written >= sizeof(managed_internal_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        managed_string_source_path, sizeof(managed_string_source_path),
        "%s/runtime/common/managed_string.c", RUDA_TEST_SOURCE_DIR
    );
    if (written < 0
        || (size_t)written >= sizeof(managed_string_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        managed_record_source_path, sizeof(managed_record_source_path),
        "%s/runtime/common/managed_record.c", RUDA_TEST_SOURCE_DIR
    );
    if (written < 0
        || (size_t)written >= sizeof(managed_record_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        managed_enum_source_path, sizeof(managed_enum_source_path),
        "%s/runtime/common/managed_enum.c", RUDA_TEST_SOURCE_DIR
    );
    if (written < 0
        || (size_t)written >= sizeof(managed_enum_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    written = snprintf(
        managed_interface_source_path, sizeof(managed_interface_source_path),
        "%s/runtime/common/managed_interface.c", RUDA_TEST_SOURCE_DIR
    );
    if (written < 0
        || (size_t)written >= sizeof(managed_interface_source_path)) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    file = fopen(source_path, "wb");
    if (file == NULL) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    source_created = true;
    if (fwrite(source->data, 1, source->length, file) != source->length
        || fclose(file) != 0) {
        file = NULL;
        RUDA_EXPECT(false);
        goto cleanup;
    }
    file = NULL;
    for (optimization_index = 0;
        optimization_index < sizeof(optimizations) / sizeof(optimizations[0]);
        optimization_index += 1) {
        const char *arguments[40];
        size_t argument_count = 0;
        written = snprintf(
            executable_path,
            sizeof(executable_path),
            "%s/program-%zu",
            directory,
            optimization_index
        );
        if (written < 0 || (size_t)written >= sizeof(executable_path)) {
            RUDA_EXPECT(false);
            goto cleanup;
        }
        arguments[argument_count++] = RUDA_TEST_C_COMPILER;
        arguments[argument_count++] = "-std=c17";
        arguments[argument_count++] = optimizations[optimization_index];
#if defined(RUDA_TEST_C_SANITIZER)
        arguments[argument_count++] = RUDA_TEST_C_SANITIZER;
#endif
        arguments[argument_count++] = "-Wall";
        arguments[argument_count++] = "-Wextra";
        arguments[argument_count++] = "-Wpedantic";
        arguments[argument_count++] = "-Wconversion";
        arguments[argument_count++] = "-Wshadow";
        arguments[argument_count++] = "-Werror";
        arguments[argument_count++] = "-fno-fast-math";
        arguments[argument_count++] = "-frounding-math";
        arguments[argument_count++] = "-ffp-contract=off";
        arguments[argument_count++] = "-fexcess-precision=standard";
        arguments[argument_count++] = "-DRUDA_G_COLLECT_BEFORE_ALLOCATION=1";
        if (configuration != NULL) {
            arguments[argument_count++] = configuration;
        }
        arguments[argument_count++] = include_argument;
        arguments[argument_count++] = runtime_include_argument;
        arguments[argument_count++] = source_path;
        arguments[argument_count++] = checked_source_path;
        arguments[argument_count++] = checked_integer_source_path;
        arguments[argument_count++] = number_format_source_path;
        arguments[argument_count++] = f64_source_path;
        arguments[argument_count++] = allocator_source_path;
        arguments[argument_count++] = provenance_source_path;
        arguments[argument_count++] = managed_source_path;
        arguments[argument_count++] = managed_internal_source_path;
        arguments[argument_count++] = managed_string_source_path;
        arguments[argument_count++] = managed_record_source_path;
        arguments[argument_count++] = managed_enum_source_path;
        arguments[argument_count++] = managed_interface_source_path;
        arguments[argument_count++] = RUDA_TEST_NATIVE_RUNTIME_ARCHIVE;
        arguments[argument_count++] = "-lm";
        arguments[argument_count++] = "-o";
        arguments[argument_count++] = executable_path;
        arguments[argument_count] = NULL;
        memset(&process, 0, sizeof(process));
        if (ruda_process_run(
                ruda_system_allocator(), arguments, argument_count, 16384, &process
            ) != RUDA_PROCESS_OK
            || process.exit_code != 0 || process.signaled) {
            if (process.output.data != NULL) {
                (void)fwrite(process.output.data, 1, process.output.length, stderr);
            }
            RUDA_EXPECT(false);
            ruda_process_result_dispose(&process);
            goto cleanup;
        }
        ruda_process_result_dispose(&process);

        run_arguments[0] = executable_path;
        run_arguments[1] = NULL;
        memset(&process, 0, sizeof(process));
        RUDA_EXPECT(ruda_process_run(
            ruda_system_allocator(), run_arguments, 1, 16384, &process
        ) == RUDA_PROCESS_OK);
        RUDA_EXPECT(process.exit_code == expected_exit && !process.signaled);
        RUDA_EXPECT(process.output.length == strlen(expected_output)
            && memcmp(
                process.output.data,
                expected_output,
                process.output.length
            ) == 0);
        if (process.exit_code != expected_exit || process.signaled
            || process.output.length != strlen(expected_output)
            || memcmp(
                process.output.data,
                expected_output,
                process.output.length
            ) != 0) {
            (void)fprintf(
                stderr,
                "native generated program mismatch at %s: exit=%u signal=%d output-bytes=%zu expected-bytes=%zu\n",
                optimizations[optimization_index],
                process.exit_code,
                process.signaled ? 1 : 0,
                process.output.length,
                strlen(expected_output)
            );
            (void)fwrite(process.output.data, 1, process.output.length, stderr);
        }
        ruda_process_result_dispose(&process);
        (void)remove(executable_path);
    }

cleanup:
    if (file != NULL) {
        (void)fclose(file);
    }
    if (source_created) {
        (void)remove(source_path);
    }
    RUDA_EXPECT(rmdir(directory) == 0);
}

void ruda_test_native_compile_and_run(
    const RudaOwnedBytes *source,
    const char *expected_output,
    uint32_t expected_exit
) {
    ruda_test_native_compile_and_run_configured(
        source, expected_output, expected_exit, NULL
    );
}
#endif

void ruda_test_native_emit(
    const RudaMirFunction *const *functions,
    size_t count,
    RudaSymbolId entry,
    RudaNativeCEmitOptions options,
    RudaNativeCEmitStatus expected
) {
    RudaNativeCProgram program;
    RudaOwnedBytes output;
    memset(&program, 0, sizeof(program));
    memset(&output, 0, sizeof(output));
    program.functions = functions;
    program.function_count = count;
    program.entry_symbol = entry;
    RUDA_EXPECT(ruda_native_c_emit_program(&program, ruda_system_allocator(),
        options, &output) == expected);
    if (expected != RUDA_NATIVE_C_EMIT_OK) {
        RUDA_EXPECT(output.data == NULL && output.length == 0);
    }
    ruda_owned_bytes_dispose(&output);
}

void ruda_test_native_vm_program(
    const RudaMirFunction *const *functions,
    RudaSymbolId entry_symbol,
    int64_t expected
) {
    RudaBytecodeProgram program;
    RudaBytecodeValue result;
    memset(&program, 0, sizeof(program));
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = INT64_MIN;
    RUDA_EXPECT(ruda_mir_lower_bytecode_program(functions, 2, entry_symbol,
        ruda_system_allocator(), ruda_mir_bytecode_program_lower_default_options(),
        &program) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_run_program(&program, NULL, 0,
        ruda_system_allocator(), ruda_bytecode_run_default_options(), &result)
        == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == expected);
    ruda_bytecode_program_dispose(&program);
}

void ruda_test_native_vm_overflow(const RudaMirFunction *input) {
    RudaBytecodeFunction function;
    RudaBytecodeValue result;
    RudaBytecodeTrap trap;
    RudaBytecodeRunOptions options = ruda_bytecode_run_default_options();
    memset(&function, 0, sizeof(function));
    memset(&trap, 0, sizeof(trap));
    result.type = RUDA_BYTECODE_TYPE_I64;
    result.as.i64 = 1234;
    options.trap = &trap;
    RUDA_EXPECT(ruda_mir_lower_bytecode_function(input, ruda_system_allocator(),
        ruda_mir_bytecode_lower_default_options(), &function)
        == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_run(&function, NULL, 0, ruda_system_allocator(),
        options, &result) == RUDA_BYTECODE_RUN_OVERFLOW);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_I64 && result.as.i64 == 1234);
    RUDA_EXPECT(trap.has_span && trap.span.start == 32 && trap.span.end == 33);
    ruda_bytecode_function_dispose(&function);
}

typedef enum RudaTestNativeScalarOutcome {
    RUDA_TEST_NATIVE_SCALAR_I64,
    RUDA_TEST_NATIVE_SCALAR_BOOL,
    RUDA_TEST_NATIVE_SCALAR_OVERFLOW,
    RUDA_TEST_NATIVE_SCALAR_DIVISION_BY_ZERO,
    RUDA_TEST_NATIVE_SCALAR_INVALID_SHIFT
} RudaTestNativeScalarOutcome;

typedef struct RudaTestNativeScalarCase {
    RudaMirInstructionKind kind;
    int64_t left;
    int64_t right;
    bool left_boolean;
    bool right_boolean;
    RudaTestNativeScalarOutcome outcome;
    int64_t expected_i64;
    bool expected_boolean;
    bool native;
} RudaTestNativeScalarCase;
