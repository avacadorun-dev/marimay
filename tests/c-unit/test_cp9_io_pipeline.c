#include "test_checker_imports_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

#include "mmay/bytecode/mir_lower.h"
#include "mmay/check/entry.h"
#include "mmay/runtime/enum.h"

typedef struct MarimayCp9IoSink {
    unsigned char bytes[64];
    size_t length;
    size_t calls;
    bool accept;
} MarimayCp9IoSink;

typedef struct MarimayCp9IoFixture {
    MarimayCheckerImportFixture source;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *main_function;
    const MarimayCheckedFunction *native_entry;
    size_t main_index;
    size_t native_entry_index;
    const MarimayMirFunction *functions[16];
    size_t function_count;
} MarimayCp9IoFixture;

static const char mmay_cp9_io_package[] =
    "public enum Error {\n"
    "  CapabilityDenied { capability: string, },\n"
    "  WriteFailed,\n"
    "}\n"
    "public fn message(error: Error): string {\n"
    "  return match error {\n"
    "    CapabilityDenied { capability } => \"capability io.stdout is not granted\",\n"
    "    WriteFailed => \"stdout write failed\",\n"
    "  };\n"
    "}\n"
    "public extern \"c\" fn println(text: string): Result<void, Error>;\n";

static const char mmay_cp9_io_application[] =
    "import { Error as IoError, println } from \"std/io\";\n"
    "fn main(): Result<void, IoError> {\n"
    "  println(\"vm\\0payload\")?;\n"
    "  return Ok();\n"
    "}\n"
    "fn nativeEntry(): void {\n"
    "  const ignored = println(\"native\");\n"
    "  return;\n"
    "}\n";

static bool mmay_cp9_io_publish(
    void *context,
    const unsigned char *bytes,
    size_t length
) {
    MarimayCp9IoSink *sink = context;
    if (sink == NULL || bytes == NULL || length > sizeof(sink->bytes)) {
        return false;
    }
    sink->calls += 1;
    if (!sink->accept) return false;
    memcpy(sink->bytes, bytes, length);
    sink->length = length;
    return true;
}

static MarimayIoStdout mmay_cp9_io_stdout(
    MarimayCp9IoSink *sink,
    bool granted
) {
    MarimayIoStdout result;
    result.capability_granted = granted;
    result.context = sink;
    result.publish = mmay_cp9_io_publish;
    return result;
}

static bool mmay_cp9_io_fixture_init(MarimayCp9IoFixture *fixture) {
    MarimayTestImportSource sources[] = {
        {"test/cp9-io", "app/main.my", mmay_cp9_io_application},
        {"std/io", "std/io/io.my", mmay_cp9_io_package}
    };
    MarimayCheckedEntry entry;
    size_t index = 0;
    memset(fixture, 0, sizeof(*fixture));
    memset(&entry, 0, sizeof(entry));
    fixture->main_index = SIZE_MAX;
    fixture->native_entry_index = SIZE_MAX;
    if (!mmay_checker_import_fixture_init(
            &fixture->source, sources,
            sizeof(sources) / sizeof(sources[0])
        ) || mmay_checker_import_fixture_check(
            &fixture->source, "test/cp9-io", mmay_system_allocator(),
            mmay_check_graph_default_options(), &fixture->checked
        ) != MMAY_CHECK_OK || fixture->checked.diagnostics.length != 0) {
        return false;
    }
    if (mmay_check_select_entry(
            &fixture->checked, "test/cp9-io",
            sizeof("test/cp9-io") - 1, &entry
        ) != MMAY_CHECK_ENTRY_OK || !entry.returns_result
        || entry.error_message == NULL) {
        return false;
    }
    fixture->main_function = mmay_checker_import_function(
        &fixture->checked, "main", &fixture->main_index
    );
    fixture->native_entry = mmay_checker_import_function(
        &fixture->checked, "nativeEntry", &fixture->native_entry_index
    );
    if (fixture->main_function == NULL || fixture->native_entry == NULL) {
        return false;
    }
    for (index = 0; index < fixture->checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &fixture->checked, index
        );
        if (function != NULL && !function->is_external) {
            if (fixture->function_count
                >= sizeof(fixture->functions) / sizeof(fixture->functions[0])) {
                return false;
            }
            fixture->functions[fixture->function_count++] = &function->mir;
        }
    }
    return true;
}

static void mmay_cp9_io_fixture_dispose(MarimayCp9IoFixture *fixture) {
    mmay_check_result_dispose(&fixture->checked);
    mmay_checker_import_fixture_dispose(&fixture->source);
    memset(fixture, 0, sizeof(*fixture));
}

static MarimayMirInstruction *mmay_cp9_io_mir_instruction(
    const MarimayMirFunction *function
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        MarimayMirBlock *block = mmay_vector_at(
            (MarimayVector *)&function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayMirInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->kind == MMAY_MIR_IO_PRINTLN) {
                return instruction;
            }
        }
    }
    return NULL;
}

static MarimayBytecodeInstruction *mmay_cp9_io_bytecode_instruction(
    MarimayBytecodeFunction *function
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        MarimayBytecodeBlock *block = mmay_vector_at(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            MarimayBytecodeInstruction *instruction = mmay_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->opcode == MMAY_OPCODE_IO_PRINTLN) {
                return instruction;
            }
        }
    }
    return NULL;
}

static void mmay_cp9_io_expect_mir_result(
    const MarimayCp9IoFixture *fixture,
    bool granted,
    bool accept,
    size_t expected_variant,
    const unsigned char *expected,
    size_t expected_length
) {
    MarimayMirExecution execution;
    MarimayCheckInterpretOptions options;
    MarimayMirValue result;
    MarimayCp9IoSink sink;
    MarimayIoStdout stdout_sink;
    size_t variant = SIZE_MAX;
    size_t payload_count = SIZE_MAX;
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    memset(&sink, 0, sizeof(sink));
    sink.accept = accept;
    stdout_sink = mmay_cp9_io_stdout(&sink, granted);
    MMAY_EXPECT(mmay_mir_execution_init(
        &execution, mmay_system_allocator(),
        mmay_mir_execution_default_options()
    ));
    options = mmay_check_interpret_default_options();
    options.execution = &execution;
    options.stdout_sink = &stdout_sink;
    MMAY_EXPECT(mmay_check_interpret_function(
        &fixture->checked, fixture->main_index, NULL, 0,
        mmay_system_allocator(), options, &result
    ) == MMAY_MIR_INTERPRET_OK);
    MMAY_EXPECT(mmay_mir_execution_inspect_enum(
        &execution, result, &variant, &payload_count
    ));
    MMAY_EXPECT(variant == expected_variant);
    MMAY_EXPECT(sink.calls == (granted ? 1U : 0U));
    MMAY_EXPECT(sink.length == expected_length);
    MMAY_EXPECT(expected_length == 0
        || memcmp(sink.bytes, expected, expected_length) == 0);
    mmay_mir_execution_dispose(&execution);
}

static bool mmay_cp9_io_program_function_index(
    const MarimayBytecodeProgram *program,
    MarimaySymbolId symbol,
    uint32_t *result
) {
    size_t index = 0;
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function =
            mmay_bytecode_program_function(program, index);
        if (function != NULL && mmay_symbol_id_equal(function->symbol, symbol)) {
            if (index > UINT32_MAX) return false;
            *result = (uint32_t)index;
            return true;
        }
    }
    return false;
}

static void mmay_cp9_io_expect_vm_result(
    const MarimayBytecodeProgram *program,
    bool granted,
    bool accept,
    size_t expected_variant,
    const unsigned char *expected,
    size_t expected_length
) {
    MarimayBytecodeExecution execution;
    MarimayBytecodeRunOptions options;
    MarimayBytecodeValue result;
    MarimayCp9IoSink sink;
    MarimayIoStdout stdout_sink;
    const MarimayManagedEnumDescriptor *descriptor = NULL;
    size_t variant = SIZE_MAX;
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    memset(&sink, 0, sizeof(sink));
    sink.accept = accept;
    stdout_sink = mmay_cp9_io_stdout(&sink, granted);
    MMAY_EXPECT(mmay_bytecode_execution_init(
        &execution, mmay_system_allocator(),
        mmay_bytecode_execution_default_options()
    ));
    options = mmay_bytecode_run_default_options();
    options.execution = &execution;
    options.stdout_sink = &stdout_sink;
    MMAY_EXPECT(mmay_bytecode_run_program(
        program, NULL, 0, mmay_system_allocator(), options, &result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(result.type == MMAY_BYTECODE_TYPE_ENUM);
    MMAY_EXPECT(mmay_managed_enum_describe(
        &execution.managed_heap, result.as.managed_reference,
        &descriptor, &variant
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(descriptor != NULL && variant == expected_variant);
    MMAY_EXPECT(sink.calls == (granted ? 1U : 0U));
    MMAY_EXPECT(sink.length == expected_length);
    MMAY_EXPECT(expected_length == 0
        || memcmp(sink.bytes, expected, expected_length) == 0);
    mmay_bytecode_execution_dispose(&execution);
}

static void mmay_cp9_io_allocation_sweep_mir(
    const MarimayCp9IoFixture *fixture
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 256 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayMirExecution execution;
        MarimayCheckInterpretOptions options;
        MarimayMirValue result;
        MarimayCp9IoSink sink;
        MarimayIoStdout stdout_sink;
        MarimayMirInterpretStatus status = MMAY_MIR_INTERPRET_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        memset(&sink, 0, sizeof(sink));
        sink.accept = true;
        stdout_sink = mmay_cp9_io_stdout(&sink, true);
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        if (mmay_mir_execution_init(
                &execution, mmay_fail_allocator(&state),
                mmay_mir_execution_default_options()
            )) {
            options = mmay_check_interpret_default_options();
            options.execution = &execution;
            options.stdout_sink = &stdout_sink;
            status = mmay_check_interpret_function(
                &fixture->checked, fixture->native_entry_index, NULL, 0,
                mmay_fail_allocator(&state), options, &result
            );
            reached_success = status == MMAY_MIR_INTERPRET_OK;
        }
        mmay_mir_execution_dispose(&execution);
        MMAY_EXPECT(status == MMAY_MIR_INTERPRET_ALLOCATION_FAILED
            || status == MMAY_MIR_INTERPRET_OK);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (!reached_success) MMAY_EXPECT(sink.length == 0);
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_cp9_io_allocation_sweep_vm(
    const MarimayBytecodeProgram *decoded,
    MarimaySymbolId native_symbol
) {
    MarimayBytecodeProgram program = *decoded;
    size_t failure_index = 0;
    bool reached_success = false;
    MMAY_EXPECT(mmay_cp9_io_program_function_index(
        decoded, native_symbol, &program.entry_function
    ));
    for (failure_index = 0; failure_index < 512 && !reached_success;
        failure_index += 1) {
        MarimayFailAllocator state;
        MarimayBytecodeExecution execution;
        MarimayBytecodeRunOptions options;
        MarimayBytecodeValue result;
        MarimayCp9IoSink sink;
        MarimayIoStdout stdout_sink;
        MarimayBytecodeRunStatus status = MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        memset(&sink, 0, sizeof(sink));
        sink.accept = true;
        stdout_sink = mmay_cp9_io_stdout(&sink, true);
        mmay_fail_allocator_init(
            &state, mmay_system_allocator(), failure_index
        );
        if (mmay_bytecode_execution_init(
                &execution, mmay_fail_allocator(&state),
                mmay_bytecode_execution_default_options()
            )) {
            options = mmay_bytecode_run_default_options();
            options.execution = &execution;
            options.stdout_sink = &stdout_sink;
            status = mmay_bytecode_run_program(
                &program, NULL, 0, mmay_fail_allocator(&state),
                options, &result
            );
            reached_success = status == MMAY_BYTECODE_RUN_OK;
        }
        mmay_bytecode_execution_dispose(&execution);
        MMAY_EXPECT(status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED
            || status == MMAY_BYTECODE_RUN_OK);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (!reached_success) MMAY_EXPECT(sink.length == 0);
    }
    MMAY_EXPECT(reached_success);
}

static void mmay_cp9_io_test_pipeline(MarimayCp9IoFixture *fixture) {
    static const unsigned char vm_line[] = {
        'v', 'm', '\0', 'p', 'a', 'y', 'l', 'o', 'a', 'd', '\n'
    };
    MarimayMirInstruction *mir_io = mmay_cp9_io_mir_instruction(
        &fixture->main_function->mir
    );
    MarimayBytecodeProgram lowered;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeProgramFunction *main_bytecode = NULL;
    MarimayBytecodeInstruction *bytecode_io = NULL;
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes native_source;
    MarimayMirRegister original_mir_source = MMAY_MIR_NO_REGISTER;
    uint64_t original_bytecode_source = 0;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&native_program, 0, sizeof(native_program));
    memset(&native_source, 0, sizeof(native_source));

    MMAY_EXPECT(mir_io != NULL);
    MMAY_EXPECT(mmay_mir_verify(&fixture->main_function->mir)
        == MMAY_MIR_VERIFY_OK);
    if (mir_io != NULL) {
        original_mir_source = mir_io->first;
        mir_io->first = mir_io->target;
        MMAY_EXPECT(mmay_mir_verify(&fixture->main_function->mir)
            == MMAY_MIR_VERIFY_TYPE_MISMATCH);
        mir_io->first = original_mir_source;
        MMAY_EXPECT(mmay_mir_verify(&fixture->main_function->mir)
            == MMAY_MIR_VERIFY_OK);
    }
    mmay_cp9_io_expect_mir_result(
        fixture, true, true, 0, vm_line, sizeof(vm_line)
    );
    mmay_cp9_io_expect_mir_result(
        fixture, false, true, 1, NULL, 0
    );
    mmay_cp9_io_expect_mir_result(
        fixture, true, false, 1, NULL, 0
    );
    mmay_cp9_io_allocation_sweep_mir(fixture);

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
    main_bytecode = (MarimayBytecodeProgramFunction *)
        mmay_bytecode_program_function_by_symbol(
            &decoded, fixture->main_function->mir.symbol
        );
    bytecode_io = main_bytecode != NULL
        ? mmay_cp9_io_bytecode_instruction(&main_bytecode->body) : NULL;
    MMAY_EXPECT(bytecode_io != NULL);
    if (bytecode_io != NULL) {
        original_bytecode_source = bytecode_io->operands[1].unsigned_value;
        bytecode_io->operands[1] = bytecode_io->operands[0];
        MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
            == MMAY_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
        bytecode_io->operands[1].unsigned_value = original_bytecode_source;
        MMAY_EXPECT(mmay_bytecode_program_verify(&decoded)
            == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
    }
    mmay_cp9_io_expect_vm_result(
        &decoded, true, true, 0, vm_line, sizeof(vm_line)
    );
    mmay_cp9_io_expect_vm_result(
        &decoded, false, true, 1, NULL, 0
    );
    mmay_cp9_io_expect_vm_result(
        &decoded, true, false, 1, NULL, 0
    );
    mmay_cp9_io_allocation_sweep_vm(
        &decoded, fixture->native_entry->mir.symbol
    );

    native_program.functions = fixture->functions;
    native_program.function_count = fixture->function_count;
    native_program.entry_symbol = fixture->native_entry->mir.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program, mmay_system_allocator(),
        mmay_native_c_emit_default_options(), &native_source
    ) == MMAY_NATIVE_C_EMIT_OK);
    MMAY_EXPECT(mmay_test_native_contains(
        &native_source, "mmay_g_io_println"
    ));
#if !defined(_WIN32)
    if (native_source.data != NULL) {
        mmay_test_native_compile_and_run(
            &native_source, "native\nok:void\n", 0
        );
        mmay_test_native_compile_and_run_configured(
            &native_source, "ok:void\n", 0,
            "-DMMAY_G_STDOUT_GRANTED=0"
        );
    }
#endif
    mmay_owned_bytes_dispose(&native_source);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&lowered);
}

void mmay_test_cp9_io_pipeline(void) {
    MarimayCp9IoFixture fixture;
    memset(&fixture, 0, sizeof(fixture));
    MMAY_EXPECT(mmay_cp9_io_fixture_init(&fixture));
    if (fixture.checked.initialized && fixture.main_function != NULL) {
        mmay_cp9_io_test_pipeline(&fixture);
    }
    mmay_cp9_io_fixture_dispose(&fixture);
}
