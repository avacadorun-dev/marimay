#include "test_checker_imports_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

#include "mmay/bytecode/codec.h"
#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/program.h"
#include "mmay/bytecode/vm.h"
#include "mmay/check/interpret.h"
#include "mmay/check/entry.h"
#include "mmay/native/c_emitter.h"
#include "mmay/runtime/enum.h"

typedef struct MarimayCp32Input {
    const unsigned char *bytes;
    size_t length;
    size_t offset;
    size_t retries;
} MarimayCp32Input;

typedef struct MarimayCp32Output {
    unsigned char bytes[128];
    size_t length;
    size_t chunk;
    size_t retries;
    size_t flushes;
    bool fail_write;
    bool fail_flush;
} MarimayCp32Output;

typedef struct MarimayCp32Fixture {
    MarimayCheckerImportFixture source;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *main_function;
    const MarimayCheckedFunction *direct_error_function;
    const MarimayCheckedFunction *direct_stdout_error_function;
    const MarimayCheckedFunction *direct_stdin_error_function;
    const MarimayCheckedFunction *error_message_function;
    size_t main_index;
    const MarimayMirFunction *functions[24];
    size_t function_count;
} MarimayCp32Fixture;

static const char mmay_cp32_io_source[] =
    "public enum Error {\n"
    "  CapabilityDenied { capability: string, }, ReadFailed, WriteFailed,\n"
    "  FlushFailed, InvalidUtf8, LineTooLong { limit: int, },\n"
    "}\n"
    "public fn message(error: Error): string {\n"
    "  return match error {\n"
    "    CapabilityDenied { capability } => $\"capability {capability} is not granted\",\n"
    "    ReadFailed => \"standard input read failed\",\n"
    "    WriteFailed => \"standard stream write failed\",\n"
    "    FlushFailed => \"standard stream flush failed\",\n"
    "    InvalidUtf8 => \"standard input is not valid UTF-8\",\n"
    "    LineTooLong { limit } => $\"standard input line exceeds {limit}-byte limit\",\n"
    "  };\n"
    "}\n"
    "public extern \"c\" fn readLine(limit: int): Result<Option<string>, Error>;\n"
    "public extern \"c\" fn print(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn println(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn eprint(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn eprintln(text: string): Result<void, Error>;\n"
    "public extern \"c\" fn flushStdout(): Result<void, Error>;\n"
    "public extern \"c\" fn flushStderr(): Result<void, Error>;\n";

static const char mmay_cp32_application_source[] =
    "import { Error as IoError, eprint, eprintln, flushStderr, flushStdout, print, println, readLine } from \"std/io\";\n"
    "import * as io from \"std/io\";\n"
    "fn main(): Result<void, IoError> {\n"
    "  print(\"out\")?; println(\" line\")?;\n"
    "  eprint(\"err\")?; eprintln(\" line\")?;\n"
    "  const _ = readLine(8)?;\n"
    "  flushStdout()?; flushStderr()?; return Ok();\n"
    "}\n"
    "fn directError(): Result<void, IoError> { return eprint(\"direct\"); }\n"
    "fn directStdoutError(): Result<void, IoError> { return print(\"direct\"); }\n"
    "fn directStdinError(): Result<void, IoError> {\n"
    "  const _ = readLine(0)?; return Ok();\n"
    "}\n"
    "fn namespaceSurface(): void {\n"
    "  const _ = io.print(\"\"); const _ = io.println(\"\");\n"
    "  const _ = io.eprint(\"\"); const _ = io.eprintln(\"\");\n"
    "  const _ = io.readLine(0); const _ = io.flushStdout();\n"
    "  const _ = io.flushStderr(); return;\n"
    "}\n";

static MarimayIoTransferStatus mmay_cp32_read(
    void *context, unsigned char *destination, size_t capacity, size_t *count
) {
    MarimayCp32Input *input = context;
    if (input == NULL || destination == NULL || count == NULL || capacity == 0) {
        return MMAY_IO_TRANSFER_FAILED;
    }
    if (input->retries != 0) {
        input->retries -= 1;
        *count = 0;
        return MMAY_IO_TRANSFER_RETRY;
    }
    if (input->offset == input->length) {
        *count = 0;
        return MMAY_IO_TRANSFER_END;
    }
    destination[0] = input->bytes[input->offset++];
    *count = 1;
    return MMAY_IO_TRANSFER_OK;
}

static MarimayIoTransferStatus mmay_cp32_write(
    void *context, const unsigned char *bytes, size_t length, size_t *count
) {
    MarimayCp32Output *output = context;
    size_t amount = length;
    if (output == NULL || count == NULL || (bytes == NULL && length != 0)
        || output->fail_write) return MMAY_IO_TRANSFER_FAILED;
    if (output->retries != 0) {
        output->retries -= 1;
        *count = 0;
        return MMAY_IO_TRANSFER_RETRY;
    }
    if (output->chunk != 0 && amount > output->chunk) amount = output->chunk;
    if (amount > sizeof(output->bytes) - output->length) {
        return MMAY_IO_TRANSFER_FAILED;
    }
    if (amount != 0) memcpy(output->bytes + output->length, bytes, amount);
    output->length += amount;
    *count = amount;
    return MMAY_IO_TRANSFER_OK;
}

static MarimayIoTransferStatus mmay_cp32_flush(void *context) {
    MarimayCp32Output *output = context;
    if (output == NULL || output->fail_flush) return MMAY_IO_TRANSFER_FAILED;
    output->flushes += 1;
    return MMAY_IO_TRANSFER_OK;
}

static bool mmay_cp32_publish(
    void *context, const unsigned char *bytes, size_t length
) {
    size_t count = 0;
    return mmay_cp32_write(context, bytes, length, &count)
            == MMAY_IO_TRANSFER_OK
        && count == length;
}

static MarimayIoStdout mmay_cp32_streams(
    MarimayCp32Input *input,
    MarimayCp32Output *output,
    MarimayCp32Output *error
) {
    MarimayIoStdout streams = {0};
    streams.capability_granted = true;
    streams.context = output;
    streams.publish = mmay_cp32_publish;
    streams.stderr_capability_granted = true;
    streams.stderr_context = error;
    streams.publish_stderr = mmay_cp32_publish;
    streams.flush_stdout = mmay_cp32_flush;
    streams.flush_stderr = mmay_cp32_flush;
    streams.stdin_capability_granted = true;
    streams.stdin_context = input;
    streams.read_stdin = mmay_cp32_read;
    streams.write_stdout = mmay_cp32_write;
    streams.write_stderr = mmay_cp32_write;
    return streams;
}

static bool mmay_cp32_fixture_init(MarimayCp32Fixture *fixture) {
    MarimayTestImportSource sources[] = {
        {"test/cp32", "app/main.my", mmay_cp32_application_source},
        {"std/io", "std/io/io.my", mmay_cp32_io_source}
    };
    MarimayCheckedEntry entry;
    size_t index = 0;
    memset(fixture, 0, sizeof(*fixture));
    memset(&entry, 0, sizeof(entry));
    fixture->main_index = SIZE_MAX;
    if (!mmay_checker_import_fixture_init(
            &fixture->source, sources,
            sizeof(sources) / sizeof(sources[0])
        ) || mmay_checker_import_fixture_check(
            &fixture->source, "test/cp32", mmay_system_allocator(),
            mmay_check_graph_default_options(), &fixture->checked
        ) != MMAY_CHECK_OK || fixture->checked.diagnostics.length != 0) {
        return false;
    }
    if (mmay_check_select_entry(
            &fixture->checked, "test/cp32", sizeof("test/cp32") - 1,
            &entry
        ) != MMAY_CHECK_ENTRY_OK || !entry.returns_result
        || entry.error_message == NULL) {
        return false;
    }
    fixture->error_message_function = entry.error_message;
    fixture->main_function = mmay_checker_import_function(
        &fixture->checked, "main", &fixture->main_index
    );
    fixture->direct_error_function = mmay_checker_import_function(
        &fixture->checked, "directError", NULL
    );
    fixture->direct_stdout_error_function = mmay_checker_import_function(
        &fixture->checked, "directStdoutError", NULL
    );
    fixture->direct_stdin_error_function = mmay_checker_import_function(
        &fixture->checked, "directStdinError", NULL
    );
    for (index = 0; index < fixture->checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &fixture->checked, index
        );
        if (function != NULL && !function->is_external
            && fixture->function_count
                < sizeof(fixture->functions) / sizeof(fixture->functions[0])) {
            fixture->functions[fixture->function_count++] = &function->mir;
        }
    }
    return fixture->main_function != NULL
        && fixture->direct_error_function != NULL
        && fixture->direct_stdout_error_function != NULL
        && fixture->direct_stdin_error_function != NULL
        && fixture->error_message_function != NULL;
}

static void mmay_cp32_fixture_dispose(MarimayCp32Fixture *fixture) {
    mmay_check_result_dispose(&fixture->checked);
    mmay_checker_import_fixture_dispose(&fixture->source);
    memset(fixture, 0, sizeof(*fixture));
}

static size_t mmay_cp32_mir_kind_count(
    const MarimayMirFunction *function,
    MarimayMirInstructionKind kind
) {
    size_t block_index = 0;
    size_t count = 0;
    for (block_index = 0; block_index < function->blocks.length; block_index += 1) {
        const MarimayMirBlock *block = mmay_vector_at_const(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            const MarimayMirInstruction *instruction = mmay_vector_at_const(
                &block->instructions, instruction_index
            );
            if (instruction != NULL && instruction->kind == kind) count += 1;
        }
    }
    return count;
}

static MarimayMirInstruction *mmay_cp32_mir_instruction(
    const MarimayMirFunction *function,
    MarimayMirInstructionKind kind
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length; block_index += 1) {
        MarimayMirBlock *block = mmay_vector_at(
            (MarimayVector *)&function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayMirInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL && instruction->kind == kind) {
                return instruction;
            }
        }
    }
    return NULL;
}

static MarimayBytecodeInstruction *mmay_cp32_bytecode_instruction(
    MarimayBytecodeFunction *function,
    MarimayOpcode opcode
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length; block_index += 1) {
        MarimayBytecodeBlock *block = mmay_vector_at(&function->blocks, block_index);
        size_t instruction_index = 0;
        for (instruction_index = 0; block != NULL
            && instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayBytecodeInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL && instruction->opcode == opcode) {
                return instruction;
            }
        }
    }
    return NULL;
}

static bool mmay_cp32_program_entry(
    MarimayBytecodeProgram *program,
    MarimaySymbolId symbol
) {
    size_t index = 0;
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function =
            mmay_bytecode_program_function(program, index);
        if (function != NULL && mmay_symbol_id_equal(function->symbol, symbol)
            && index <= UINT32_MAX) {
            program->entry_function = (uint32_t)index;
            return true;
        }
    }
    return false;
}

static void mmay_cp32_expect_mir_success(const MarimayCp32Fixture *fixture) {
    static const unsigned char input_bytes[] = {'h', 'i', '\r', '\n'};
    MarimayCp32Input input = {input_bytes, sizeof(input_bytes), 0, 2};
    MarimayCp32Output output = {{0}, 0, 2, 1, 0, false, false};
    MarimayCp32Output error = {{0}, 0, 1, 1, 0, false, false};
    MarimayIoStdout streams = mmay_cp32_streams(&input, &output, &error);
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options = mmay_check_interpret_default_options();
    MarimayMirValue result = {0};
    size_t variant = SIZE_MAX;
    size_t payload_count = SIZE_MAX;
    memset(&execution, 0, sizeof(execution));
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    options.execution = &execution;
    options.stdout_sink = &streams;
    MMAY_EXPECT(mmay_check_interpret_function(
        &fixture->checked, fixture->main_index, NULL, 0,
        mmay_system_allocator(), options, &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mmay_mir_execution_inspect_enum(
        &execution, result, &variant, &payload_count
    ));
    MMAY_EXPECT(variant == 0 && payload_count == 0);
    MMAY_EXPECT(output.length == sizeof("out line\n") - 1
        && memcmp(output.bytes, "out line\n", output.length) == 0);
    MMAY_EXPECT(error.length == sizeof("err line\n") - 1
        && memcmp(error.bytes, "err line\n", error.length) == 0);
    MMAY_EXPECT(output.flushes == 1 && error.flushes == 1);
    mmay_mir_execution_dispose(&execution);
}

static void mmay_cp32_checker_and_mir(MarimayCp32Fixture *fixture) {
    MarimayMirInstruction *write = mmay_cp32_mir_instruction(
        &fixture->main_function->mir, MMAY_MIR_IO_WRITE
    );
    MarimayMirInstruction *read = mmay_cp32_mir_instruction(
        &fixture->main_function->mir, MMAY_MIR_IO_READ_LINE
    );
    MarimayMirIoOperation saved_operation;
    MarimayMirEnumId saved_option;
    MMAY_EXPECT(mmay_mir_verify(&fixture->main_function->mir)
        == MMAY_MIR_VERIFY_OK);
    MMAY_EXPECT(mmay_cp32_mir_kind_count(
        &fixture->main_function->mir, MMAY_MIR_IO_WRITE
    ) == 4);
    MMAY_EXPECT(mmay_cp32_mir_kind_count(
        &fixture->main_function->mir, MMAY_MIR_IO_PRINTLN
    ) == 0);
    MMAY_EXPECT(mmay_cp32_mir_kind_count(
        &fixture->main_function->mir, MMAY_MIR_IO_FLUSH
    ) == 2);
    MMAY_EXPECT(mmay_cp32_mir_kind_count(
        &fixture->main_function->mir, MMAY_MIR_IO_READ_LINE
    ) == 1);
    MMAY_EXPECT(write != NULL && read != NULL);
    if (write != NULL) {
        saved_operation = write->io_operation;
        write->io_operation = (MarimayMirIoOperation)99;
        MMAY_EXPECT(mmay_mir_verify(&fixture->main_function->mir)
            == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        write->io_operation = saved_operation;
    }
    if (read != NULL) {
        saved_option = read->option_enum_id;
        read->option_enum_id = MMAY_MIR_NO_ENUM;
        MMAY_EXPECT(mmay_mir_verify(&fixture->main_function->mir)
            == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        read->option_enum_id = saved_option;
    }
    MMAY_EXPECT(mmay_mir_verify(&fixture->main_function->mir)
        == MMAY_MIR_VERIFY_OK);
    mmay_cp32_expect_mir_success(fixture);
}

static void mmay_cp32_vm_allocation_sweep(
    const MarimayBytecodeProgram *program
) {
    static const unsigned char input_bytes[] = {'o', 'k', '\n'};
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 512 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution execution;
        MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
        MarimayBytecodeValue result = {0};
        MarimayCp32Input input = {input_bytes, sizeof(input_bytes), 0, 0};
        MarimayCp32Output output = {0};
        MarimayCp32Output error = {0};
        MarimayIoStdout streams = mmay_cp32_streams(
            &input, &output, &error
        );
        MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        if (mmay_bytecode_execution_init(
                &execution, mmay_fail_allocator(&state),
                mmay_bytecode_execution_default_options()
            )) {
            options.execution = &execution;
            options.stdout_sink = &streams;
            status = mmay_bytecode_run_program(
                program, NULL, 0, mmay_fail_allocator(&state),
                options, &result
            );
            reached_success = status == MMAY_BYTECODE_RUN_OK;
        }
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            || status == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_cp32_vm_error_case(
    const MarimayBytecodeProgram *program,
    const unsigned char *input_bytes,
    size_t input_length,
    bool stdin_granted,
    bool stdout_granted,
    bool stderr_granted,
    bool fail_stdout_write,
    bool fail_stdout_flush,
    bool fail_stderr_flush,
    size_t expected_variant,
    const char *expected_capability
) {
    MarimayCp32Input input = {input_bytes, input_length, 0, 0};
    MarimayCp32Output output = {0};
    MarimayCp32Output error = {0};
    MarimayIoStdout streams = mmay_cp32_streams(&input, &output, &error);
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result = {0};
    MarimayManagedCell cell = {0};
    MarimayManagedReference error_reference = mmay_managed_invalid_reference();
    const MarimayManagedEnumDescriptor *descriptor = NULL;
    const unsigned char *capability = NULL;
    size_t variant = SIZE_MAX;
    size_t capability_length = 0;
    memset(&execution, 0, sizeof(execution));
    streams.stdin_capability_granted = stdin_granted;
    streams.capability_granted = stdout_granted;
    streams.stderr_capability_granted = stderr_granted;
    output.fail_write = fail_stdout_write;
    output.fail_flush = fail_stdout_flush;
    error.fail_flush = fail_stderr_flush;
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    options.execution = &execution;
    options.stdout_sink = &streams;
    MMAY_EXPECT(mmay_bytecode_run_program(
        program, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(mmay_managed_enum_describe(
        &execution.managed_heap, result.as.managed_reference,
        &descriptor, &variant
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(variant == 1);
    MMAY_EXPECT(mmay_managed_enum_get_payload(
        &execution.managed_heap, result.as.managed_reference,
        1, 0, &cell
    ) == MMAY_MANAGED_OK);
    error_reference = cell.reference;
    MMAY_EXPECT(mmay_managed_enum_describe(
        &execution.managed_heap, error_reference, &descriptor, &variant
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(variant == expected_variant);
    if (expected_capability != NULL) {
        MMAY_EXPECT(mmay_managed_enum_get_payload(
            &execution.managed_heap, error_reference, 0, 0, &cell
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(mmay_managed_string_bytes(
            &execution.managed_heap,
            cell.reference,
            &capability,
            &capability_length
        ) == MMAY_MANAGED_OK);
        MMAY_EXPECT(capability_length == strlen(expected_capability)
            && memcmp(
                capability, expected_capability, capability_length
            ) == 0);
    }
    mmay_bytecode_execution_dispose(&execution);
}

static void mmay_cp32_vm_error_cases(
    const MarimayBytecodeProgram *program
) {
    static const unsigned char line[] = {'o', 'k', '\n'};
    static const unsigned char invalid_utf8[] = {0xff, '\n'};
    mmay_cp32_vm_error_case(
        program, line, sizeof(line), true, true, false,
        false, false, false, 0, "io.stderr"
    );
    mmay_cp32_vm_error_case(
        program, line, sizeof(line), false, true, true,
        false, false, false, 0, "io.stdin"
    );
    mmay_cp32_vm_error_case(
        program, line, sizeof(line), true, true, true,
        true, false, false, 2, NULL
    );
    mmay_cp32_vm_error_case(
        program, line, sizeof(line), true, true, true,
        false, true, false, 3, NULL
    );
    mmay_cp32_vm_error_case(
        program, invalid_utf8, sizeof(invalid_utf8), true, true, true,
        false, false, false, 4, NULL
    );
}

static void mmay_cp32_bytecode(MarimayCp32Fixture *fixture) {
    static const unsigned char input_bytes[] = {'h', 'i', '\n'};
    MarimayBytecodeProgram lowered;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeProgramFunction *main_function;
    MarimayBytecodeInstruction *write;
    MarimayBytecodeInstruction *read;
    MarimayCp32Input input = {input_bytes, sizeof(input_bytes), 0, 1};
    MarimayCp32Output output = {{0}, 0, 2, 1, 0, false, false};
    MarimayCp32Output error = {{0}, 0, 1, 1, 0, false, false};
    MarimayIoStdout streams = mmay_cp32_streams(&input, &output, &error);
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    MarimayBytecodeValue result = {0};
    const MarimayManagedEnumDescriptor *descriptor = NULL;
    size_t variant = SIZE_MAX;
    int64_t saved_operation;
    int64_t saved_option;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    MMAY_EXPECT(mmay_mir_lower_bytecode_program(
        fixture->functions, fixture->function_count,
        fixture->main_function->mir.symbol, mmay_system_allocator(),
        mmay_mir_bytecode_program_lower_default_options(), &lowered
    ) == MMAY_MIR_BYTECODE_LOWER_OK);
    MMAY_EXPECT(mmay_bytecode_program_verify(&lowered)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_encode_program(
        &lowered, mmay_system_allocator(), &encoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_decode_program(
        encoded.data, encoded.length, mmay_system_allocator(),
        mmay_bytecode_program_default_options(), &decoded
    ) == MMAY_BYTECODE_CODEC_OK);
    MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    main_function = (MarimayBytecodeProgramFunction *)
        mmay_bytecode_program_function_by_symbol(
            &decoded, fixture->main_function->mir.symbol
        );
    write = main_function != NULL ? mmay_cp32_bytecode_instruction(
        &main_function->body, MMAY_OPCODE_IO_WRITE
    ) : NULL;
    read = main_function != NULL ? mmay_cp32_bytecode_instruction(
        &main_function->body, MMAY_OPCODE_IO_READ_LINE
    ) : NULL;
    MMAY_EXPECT(write != NULL && read != NULL);
    if (write != NULL) {
        saved_operation = write->operands[2].signed_value;
        write->operands[2].signed_value = 9;
        MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
            == MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
        write->operands[2].signed_value = saved_operation;
    }
    if (read != NULL) {
        saved_option = read->operands[2].signed_value;
        read->operands[2].signed_value = -1;
        MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
            == MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
        read->operands[2].signed_value = saved_option;
    }
    MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
        == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    options.execution = &execution;
    options.stdout_sink = &streams;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &decoded, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(mmay_managed_enum_describe(
        &execution.managed_heap, result.as.managed_reference,
        &descriptor, &variant
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(descriptor != NULL && variant == 0);
    MMAY_EXPECT(output.length == sizeof("out line\n") - 1
        && memcmp(output.bytes, "out line\n", output.length) == 0);
    MMAY_EXPECT(error.length == sizeof("err line\n") - 1
        && memcmp(error.bytes, "err line\n", error.length) == 0);
    MMAY_EXPECT(output.flushes == 1 && error.flushes == 1);
    mmay_bytecode_execution_dispose(&execution);
    {
        MarimayBytecodeProgram direct = decoded;
        MMAY_EXPECT(mmay_cp32_program_entry(
            &direct, fixture->direct_error_function->mir.symbol
        ));
        mmay_cp32_vm_error_case(
            &direct, input_bytes, sizeof(input_bytes), true, true, false,
            false, false, false, 0, "io.stderr"
        );
    }
    mmay_cp32_vm_error_cases(&decoded);
    mmay_cp32_vm_allocation_sweep(&decoded);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&lowered);
}

static void mmay_cp32_native_capability_case(
    const MarimayCp32Fixture *fixture,
    const MarimayCheckedFunction *entry,
    const char *configuration,
    const char *expected
) {
    MarimayNativeCProgram program;
    MarimayNativeCEmitOptions options = mmay_native_c_emit_default_options();
    MarimayOwnedBytes generated = {0};
    memset(&program, 0, sizeof(program));
    program.functions = fixture->functions;
    program.function_count = fixture->function_count;
    program.entry_symbol = entry->mir.symbol;
    options.executable_entry = true;
    options.executable_returns_result = true;
    options.error_message_symbol = fixture->error_message_function->mir.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &program, mmay_system_allocator(), options, &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_native_contains(
        &generated, "signal(SIGPIPE, SIG_IGN)"
    ));
#if !defined(_WIN32)
    if (generated.data != NULL) {
        mmay_test_native_compile_and_run_configured(
            &generated, expected, 1, configuration
        );
    }
#else
    (void)configuration;
    (void)expected;
#endif
    mmay_owned_bytes_dispose(&generated);
}

static void mmay_cp32_native_capabilities(
    const MarimayCp32Fixture *fixture
) {
    mmay_cp32_native_capability_case(
        fixture,
        fixture->direct_stdin_error_function,
        "-DMMAY_G_STDIN_GRANTED=0",
        "error[G4003]: capability io.stdin is not granted\n"
    );
    mmay_cp32_native_capability_case(
        fixture,
        fixture->direct_stdout_error_function,
        "-DMMAY_G_STDOUT_GRANTED=0",
        "error[G4003]: capability io.stdout is not granted\n"
    );
    mmay_cp32_native_capability_case(
        fixture,
        fixture->direct_error_function,
        "-DMMAY_G_STDERR_GRANTED=0",
        "error[G4003]: capability io.stderr is not granted\n"
    );
}

static void mmay_cp32_checker_rejects(const char *body) {
    MarimayCheckerImportFixture source;
    MarimayCheckResult checked;
    MarimayTestImportSource sources[] = {
        {"test/cp32-negative", "app/main.my", body},
        {"std/io", "std/io/io.my", mmay_cp32_io_source}
    };
    memset(&source, 0, sizeof(source));
    memset(&checked, 0, sizeof(checked));
    MMAY_EXPECT(mmay_checker_import_fixture_init(
        &source, sources, sizeof(sources) / sizeof(sources[0])
    ));
    MMAY_EXPECT(mmay_checker_import_fixture_check(
        &source, "test/cp32-negative", mmay_system_allocator(),
        mmay_check_graph_default_options(), &checked
    ) == MMAY_CHECK_OK);
    MMAY_EXPECT(checked.diagnostics.length != 0);
    mmay_check_result_dispose(&checked);
    mmay_checker_import_fixture_dispose(&source);
}

void mmay_test_cp32_streams(void) {
    MarimayCp32Fixture fixture;
    memset(&fixture, 0, sizeof(fixture));
    MMAY_EXPECT(mmay_cp32_fixture_init(&fixture));
    if (fixture.checked.initialized && fixture.main_function != NULL) {
        mmay_cp32_checker_and_mir(&fixture);
        mmay_cp32_bytecode(&fixture);
        mmay_cp32_native_capabilities(&fixture);
    }
    mmay_cp32_fixture_dispose(&fixture);
    mmay_cp32_checker_rejects(
        "import { print } from \"std/io\"; fn bad(): void { const _ = print(1); }\n"
    );
    mmay_cp32_checker_rejects(
        "import { flushStdout } from \"std/io\"; fn bad(): void { const _ = flushStdout(1); }\n"
    );
    mmay_cp32_checker_rejects(
        "import { readLine } from \"std/io\"; fn bad(): void { const _ = readLine(\"x\"); }\n"
    );
    mmay_cp32_checker_rejects(
        "import { print } from \"std/io\"; fn bad(): void { const _ = print<int>(\"x\"); }\n"
    );
}
