#include "test_checker_imports_support.h"
#include "test_native_c_emitter_support.h"

#include <string.h>

#include "ruda/bytecode/mir_lower.h"
#include "ruda/check/entry.h"
#include "ruda/runtime/enum.h"

typedef struct RudaCp9IoSink {
    unsigned char bytes[64];
    size_t length;
    size_t calls;
    bool accept;
} RudaCp9IoSink;

typedef struct RudaCp9IoFixture {
    RudaCheckerImportFixture source;
    RudaCheckResult checked;
    const RudaCheckedFunction *main_function;
    const RudaCheckedFunction *native_entry;
    size_t main_index;
    size_t native_entry_index;
    const RudaMirFunction *functions[16];
    size_t function_count;
} RudaCp9IoFixture;

static const char ruda_cp9_io_package[] =
    "public enum Error {\n"
    "  CapabilityDenied { capability: string; },\n"
    "  WriteFailed,\n"
    "}\n"
    "public fn message(error: Error): string {\n"
    "  return match error {\n"
    "    case CapabilityDenied { capability } => \"capability io.stdout is not granted\";\n"
    "    case WriteFailed => \"stdout write failed\";\n"
    "  };\n"
    "}\n"
    "public extern \"c\" fn println(text: string): Result<void, Error>;\n";

static const char ruda_cp9_io_application[] =
    "import { Error as IoError, println } from \"std/io\";\n"
    "fn main(): Result<void, IoError> {\n"
    "  println(\"vm\\0payload\")?;\n"
    "  return Ok();\n"
    "}\n"
    "fn nativeEntry(): void {\n"
    "  const ignored = println(\"native\");\n"
    "  return;\n"
    "}\n";

static bool ruda_cp9_io_publish(
    void *context,
    const unsigned char *bytes,
    size_t length
) {
    RudaCp9IoSink *sink = context;
    if (sink == NULL || bytes == NULL || length > sizeof(sink->bytes)) {
        return false;
    }
    sink->calls += 1;
    if (!sink->accept) return false;
    memcpy(sink->bytes, bytes, length);
    sink->length = length;
    return true;
}

static RudaIoStdout ruda_cp9_io_stdout(
    RudaCp9IoSink *sink,
    bool granted
) {
    RudaIoStdout result;
    result.capability_granted = granted;
    result.context = sink;
    result.publish = ruda_cp9_io_publish;
    return result;
}

static bool ruda_cp9_io_fixture_init(RudaCp9IoFixture *fixture) {
    RudaTestImportSource sources[] = {
        {"test/cp9-io", "app/main.ruda", ruda_cp9_io_application},
        {"std/io", "std/io/io.ruda", ruda_cp9_io_package}
    };
    RudaCheckedEntry entry;
    size_t index = 0;
    memset(fixture, 0, sizeof(*fixture));
    memset(&entry, 0, sizeof(entry));
    fixture->main_index = SIZE_MAX;
    fixture->native_entry_index = SIZE_MAX;
    if (!ruda_checker_import_fixture_init(
            &fixture->source, sources,
            sizeof(sources) / sizeof(sources[0])
        ) || ruda_checker_import_fixture_check(
            &fixture->source, "test/cp9-io", ruda_system_allocator(),
            ruda_check_graph_default_options(), &fixture->checked
        ) != RUDA_CHECK_OK || fixture->checked.diagnostics.length != 0) {
        return false;
    }
    if (ruda_check_select_entry(
            &fixture->checked, "test/cp9-io",
            sizeof("test/cp9-io") - 1, &entry
        ) != RUDA_CHECK_ENTRY_OK || !entry.returns_result
        || entry.error_message == NULL) {
        return false;
    }
    fixture->main_function = ruda_checker_import_function(
        &fixture->checked, "main", &fixture->main_index
    );
    fixture->native_entry = ruda_checker_import_function(
        &fixture->checked, "nativeEntry", &fixture->native_entry_index
    );
    if (fixture->main_function == NULL || fixture->native_entry == NULL) {
        return false;
    }
    for (index = 0; index < fixture->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
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

static void ruda_cp9_io_fixture_dispose(RudaCp9IoFixture *fixture) {
    ruda_check_result_dispose(&fixture->checked);
    ruda_checker_import_fixture_dispose(&fixture->source);
    memset(fixture, 0, sizeof(*fixture));
}

static RudaMirInstruction *ruda_cp9_io_mir_instruction(
    const RudaMirFunction *function
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        RudaMirBlock *block = ruda_vector_at(
            (RudaVector *)&function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            RudaMirInstruction *instruction = ruda_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->kind == RUDA_MIR_IO_PRINTLN) {
                return instruction;
            }
        }
    }
    return NULL;
}

static RudaBytecodeInstruction *ruda_cp9_io_bytecode_instruction(
    RudaBytecodeFunction *function
) {
    size_t block_index = 0;
    for (block_index = 0; block_index < function->blocks.length;
        block_index += 1) {
        RudaBytecodeBlock *block = ruda_vector_at(
            &function->blocks, block_index
        );
        size_t instruction_index = 0;
        for (instruction_index = 0;
            block != NULL && instruction_index < block->instructions.length;
            instruction_index += 1) {
            RudaBytecodeInstruction *instruction = ruda_vector_at(
                &block->instructions, instruction_index
            );
            if (instruction != NULL
                && instruction->opcode == RUDA_OPCODE_IO_PRINTLN) {
                return instruction;
            }
        }
    }
    return NULL;
}

static void ruda_cp9_io_expect_mir_result(
    const RudaCp9IoFixture *fixture,
    bool granted,
    bool accept,
    size_t expected_variant,
    const unsigned char *expected,
    size_t expected_length
) {
    RudaMirExecution execution;
    RudaCheckInterpretOptions options;
    RudaMirValue result;
    RudaCp9IoSink sink;
    RudaIoStdout stdout_sink;
    size_t variant = SIZE_MAX;
    size_t payload_count = SIZE_MAX;
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    memset(&sink, 0, sizeof(sink));
    sink.accept = accept;
    stdout_sink = ruda_cp9_io_stdout(&sink, granted);
    RUDA_EXPECT(ruda_mir_execution_init(
        &execution, ruda_system_allocator(),
        ruda_mir_execution_default_options()
    ));
    options = ruda_check_interpret_default_options();
    options.execution = &execution;
    options.stdout_sink = &stdout_sink;
    RUDA_EXPECT(ruda_check_interpret_function(
        &fixture->checked, fixture->main_index, NULL, 0,
        ruda_system_allocator(), options, &result
    ) == RUDA_MIR_INTERPRET_OK);
    RUDA_EXPECT(ruda_mir_execution_inspect_enum(
        &execution, result, &variant, &payload_count
    ));
    RUDA_EXPECT(variant == expected_variant);
    RUDA_EXPECT(sink.calls == (granted ? 1U : 0U));
    RUDA_EXPECT(sink.length == expected_length);
    RUDA_EXPECT(expected_length == 0
        || memcmp(sink.bytes, expected, expected_length) == 0);
    ruda_mir_execution_dispose(&execution);
}

static bool ruda_cp9_io_program_function_index(
    const RudaBytecodeProgram *program,
    RudaSymbolId symbol,
    uint32_t *result
) {
    size_t index = 0;
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function =
            ruda_bytecode_program_function(program, index);
        if (function != NULL && ruda_symbol_id_equal(function->symbol, symbol)) {
            if (index > UINT32_MAX) return false;
            *result = (uint32_t)index;
            return true;
        }
    }
    return false;
}

static void ruda_cp9_io_expect_vm_result(
    const RudaBytecodeProgram *program,
    bool granted,
    bool accept,
    size_t expected_variant,
    const unsigned char *expected,
    size_t expected_length
) {
    RudaBytecodeExecution execution;
    RudaBytecodeRunOptions options;
    RudaBytecodeValue result;
    RudaCp9IoSink sink;
    RudaIoStdout stdout_sink;
    const RudaManagedEnumDescriptor *descriptor = NULL;
    size_t variant = SIZE_MAX;
    memset(&execution, 0, sizeof(execution));
    memset(&result, 0, sizeof(result));
    memset(&sink, 0, sizeof(sink));
    sink.accept = accept;
    stdout_sink = ruda_cp9_io_stdout(&sink, granted);
    RUDA_EXPECT(ruda_bytecode_execution_init(
        &execution, ruda_system_allocator(),
        ruda_bytecode_execution_default_options()
    ));
    options = ruda_bytecode_run_default_options();
    options.execution = &execution;
    options.stdout_sink = &stdout_sink;
    RUDA_EXPECT(ruda_bytecode_run_program(
        program, NULL, 0, ruda_system_allocator(), options, &result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(result.type == RUDA_BYTECODE_TYPE_ENUM);
    RUDA_EXPECT(ruda_managed_enum_describe(
        &execution.managed_heap, result.as.managed_reference,
        &descriptor, &variant
    ) == RUDA_MANAGED_OK);
    RUDA_EXPECT(descriptor != NULL && variant == expected_variant);
    RUDA_EXPECT(sink.calls == (granted ? 1U : 0U));
    RUDA_EXPECT(sink.length == expected_length);
    RUDA_EXPECT(expected_length == 0
        || memcmp(sink.bytes, expected, expected_length) == 0);
    ruda_bytecode_execution_dispose(&execution);
}

static void ruda_cp9_io_allocation_sweep_mir(
    const RudaCp9IoFixture *fixture
) {
    size_t failure_index = 0;
    bool reached_success = false;
    for (failure_index = 0; failure_index < 256 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaMirExecution execution;
        RudaCheckInterpretOptions options;
        RudaMirValue result;
        RudaCp9IoSink sink;
        RudaIoStdout stdout_sink;
        RudaMirInterpretStatus status = RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        memset(&sink, 0, sizeof(sink));
        sink.accept = true;
        stdout_sink = ruda_cp9_io_stdout(&sink, true);
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        if (ruda_mir_execution_init(
                &execution, ruda_fail_allocator(&state),
                ruda_mir_execution_default_options()
            )) {
            options = ruda_check_interpret_default_options();
            options.execution = &execution;
            options.stdout_sink = &stdout_sink;
            status = ruda_check_interpret_function(
                &fixture->checked, fixture->native_entry_index, NULL, 0,
                ruda_fail_allocator(&state), options, &result
            );
            reached_success = status == RUDA_MIR_INTERPRET_OK;
        }
        ruda_mir_execution_dispose(&execution);
        RUDA_EXPECT(status == RUDA_MIR_INTERPRET_ALLOCATION_FAILED
            || status == RUDA_MIR_INTERPRET_OK);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (!reached_success) RUDA_EXPECT(sink.length == 0);
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_cp9_io_allocation_sweep_vm(
    const RudaBytecodeProgram *decoded,
    RudaSymbolId native_symbol
) {
    RudaBytecodeProgram program = *decoded;
    size_t failure_index = 0;
    bool reached_success = false;
    RUDA_EXPECT(ruda_cp9_io_program_function_index(
        decoded, native_symbol, &program.entry_function
    ));
    for (failure_index = 0; failure_index < 512 && !reached_success;
        failure_index += 1) {
        RudaFailAllocator state;
        RudaBytecodeExecution execution;
        RudaBytecodeRunOptions options;
        RudaBytecodeValue result;
        RudaCp9IoSink sink;
        RudaIoStdout stdout_sink;
        RudaBytecodeRunStatus status = RUDA_BYTECODE_RUN_ALLOCATION_FAILED;
        memset(&execution, 0, sizeof(execution));
        memset(&result, 0, sizeof(result));
        memset(&sink, 0, sizeof(sink));
        sink.accept = true;
        stdout_sink = ruda_cp9_io_stdout(&sink, true);
        ruda_fail_allocator_init(
            &state, ruda_system_allocator(), failure_index
        );
        if (ruda_bytecode_execution_init(
                &execution, ruda_fail_allocator(&state),
                ruda_bytecode_execution_default_options()
            )) {
            options = ruda_bytecode_run_default_options();
            options.execution = &execution;
            options.stdout_sink = &stdout_sink;
            status = ruda_bytecode_run_program(
                &program, NULL, 0, ruda_fail_allocator(&state),
                options, &result
            );
            reached_success = status == RUDA_BYTECODE_RUN_OK;
        }
        ruda_bytecode_execution_dispose(&execution);
        RUDA_EXPECT(status == RUDA_BYTECODE_RUN_ALLOCATION_FAILED
            || status == RUDA_BYTECODE_RUN_OK);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        if (!reached_success) RUDA_EXPECT(sink.length == 0);
    }
    RUDA_EXPECT(reached_success);
}

static void ruda_cp9_io_test_pipeline(RudaCp9IoFixture *fixture) {
    static const unsigned char vm_line[] = {
        'v', 'm', '\0', 'p', 'a', 'y', 'l', 'o', 'a', 'd', '\n'
    };
    RudaMirInstruction *mir_io = ruda_cp9_io_mir_instruction(
        &fixture->main_function->mir
    );
    RudaBytecodeProgram lowered;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeProgramFunction *main_bytecode = NULL;
    RudaBytecodeInstruction *bytecode_io = NULL;
    RudaNativeCProgram native_program;
    RudaOwnedBytes native_source;
    RudaMirRegister original_mir_source = RUDA_MIR_NO_REGISTER;
    uint64_t original_bytecode_source = 0;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&native_program, 0, sizeof(native_program));
    memset(&native_source, 0, sizeof(native_source));

    RUDA_EXPECT(mir_io != NULL);
    RUDA_EXPECT(ruda_mir_verify(&fixture->main_function->mir)
        == RUDA_MIR_VERIFY_OK);
    if (mir_io != NULL) {
        original_mir_source = mir_io->first;
        mir_io->first = mir_io->target;
        RUDA_EXPECT(ruda_mir_verify(&fixture->main_function->mir)
            == RUDA_MIR_VERIFY_TYPE_MISMATCH);
        mir_io->first = original_mir_source;
        RUDA_EXPECT(ruda_mir_verify(&fixture->main_function->mir)
            == RUDA_MIR_VERIFY_OK);
    }
    ruda_cp9_io_expect_mir_result(
        fixture, true, true, 0, vm_line, sizeof(vm_line)
    );
    ruda_cp9_io_expect_mir_result(
        fixture, false, true, 1, NULL, 0
    );
    ruda_cp9_io_expect_mir_result(
        fixture, true, false, 1, NULL, 0
    );
    ruda_cp9_io_allocation_sweep_mir(fixture);

    RUDA_EXPECT(ruda_mir_lower_bytecode_program(
        fixture->functions, fixture->function_count,
        fixture->main_function->mir.symbol, ruda_system_allocator(),
        ruda_mir_bytecode_program_lower_default_options(), &lowered
    ) == RUDA_MIR_BYTECODE_LOWER_OK);
    RUDA_EXPECT(ruda_bytecode_program_verify(&lowered)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    RUDA_EXPECT(ruda_bytecode_encode_program(
        &lowered, ruda_system_allocator(), &encoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_decode_program(
        encoded.data, encoded.length, ruda_system_allocator(),
        ruda_bytecode_program_default_options(), &decoded
    ) == RUDA_BYTECODE_CODEC_OK);
    RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
        == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    main_bytecode = (RudaBytecodeProgramFunction *)
        ruda_bytecode_program_function_by_symbol(
            &decoded, fixture->main_function->mir.symbol
        );
    bytecode_io = main_bytecode != NULL
        ? ruda_cp9_io_bytecode_instruction(&main_bytecode->body) : NULL;
    RUDA_EXPECT(bytecode_io != NULL);
    if (bytecode_io != NULL) {
        original_bytecode_source = bytecode_io->operands[1].unsigned_value;
        bytecode_io->operands[1] = bytecode_io->operands[0];
        RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
            == RUDA_BYTECODE_PROGRAM_VERIFY_INVALID_FUNCTION);
        bytecode_io->operands[1].unsigned_value = original_bytecode_source;
        RUDA_EXPECT(ruda_bytecode_program_verify(&decoded)
            == RUDA_BYTECODE_PROGRAM_VERIFY_OK);
    }
    ruda_cp9_io_expect_vm_result(
        &decoded, true, true, 0, vm_line, sizeof(vm_line)
    );
    ruda_cp9_io_expect_vm_result(
        &decoded, false, true, 1, NULL, 0
    );
    ruda_cp9_io_expect_vm_result(
        &decoded, true, false, 1, NULL, 0
    );
    ruda_cp9_io_allocation_sweep_vm(
        &decoded, fixture->native_entry->mir.symbol
    );

    native_program.functions = fixture->functions;
    native_program.function_count = fixture->function_count;
    native_program.entry_symbol = fixture->native_entry->mir.symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program, ruda_system_allocator(),
        ruda_native_c_emit_default_options(), &native_source
    ) == RUDA_NATIVE_C_EMIT_OK);
    RUDA_EXPECT(ruda_test_native_contains(
        &native_source, "ruda_g_io_println"
    ));
#if !defined(_WIN32)
    if (native_source.data != NULL) {
        ruda_test_native_compile_and_run(
            &native_source, "native\nok:void\n", 0
        );
        ruda_test_native_compile_and_run_configured(
            &native_source, "ok:void\n", 0,
            "-DRUDA_G_STDOUT_GRANTED=0"
        );
    }
#endif
    ruda_owned_bytes_dispose(&native_source);
    ruda_bytecode_program_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&lowered);
}

void ruda_test_cp9_io_pipeline(void) {
    RudaCp9IoFixture fixture;
    memset(&fixture, 0, sizeof(fixture));
    RUDA_EXPECT(ruda_cp9_io_fixture_init(&fixture));
    if (fixture.checked.initialized && fixture.main_function != NULL) {
        ruda_cp9_io_test_pipeline(&fixture);
    }
    ruda_cp9_io_fixture_dispose(&fixture);
}
