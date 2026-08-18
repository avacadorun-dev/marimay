#include "test_native_c_emitter_support.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "ruda/ast/ast.h"
#include "ruda/check/checker.h"
#include "ruda/lex/lexer.h"
#include "ruda/parse/parser.h"
#include "ruda/source/source_manager.h"

static void ruda_test_native_source_matrix_differential(void) {
    static const char source[] =
        "fn main(): f64 {\n"
        "  let left = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]];\n"
        "  let right = [[7.0, 8.0], [9.0, 10.0], [11.0, 12.0]];\n"
        "  let result = [[0.0, 0.0], [0.0, 0.0]];\n"
        "  for let row = 0; row < 2; row += 1 {\n"
        "    for let column = 0; column < 2; column += 1 {\n"
        "      for let inner = 0; inner < 3; inner += 1 {\n"
        "        result[row][column] += left[row][inner] * right[inner][column];\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "  let adjustment = 0.0;\n"
        "  for const value of [1.0, 2.0, 3.0] { adjustment += value; }\n"
        "  return result[0][0] + result[0][1] + result[1][0] + result[1][1]\n"
        "    + adjustment;\n"
        "}\n"
        "fn bounds(): f64 { let values = [1.0]; values[3] = panic(\"rhs\"); }\n";
    static const uint64_t expected_bits = UINT64_C(0x407a500000000000);
    RudaSourceManager sources;
    const RudaSourceSnapshot *snapshot = NULL;
    RudaLexResult lexed;
    RudaParseResult parsed;
    RudaAstTree ast;
    RudaCheckResult checked;
    const RudaCheckedFunction *entry = NULL;
    const RudaCheckedFunction *bounds = NULL;
    const RudaMirFunction *functions[2];
    RudaBytecodeProgram program;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options;
    RudaBytecodeRunOptions run_options;
    RudaBytecodePanic vm_panic;
    RudaBytecodeValue vm_result;
    RudaNativeCProgram native_program;
    RudaOwnedBytes generated;
#if !defined(_WIN32)
    char symbol_hex[sizeof(RudaSymbolId) * 2 + 1];
    char frame_symbol_hex[sizeof(RudaSymbolId) * 2 + 1];
    char message_hex[256];
    char expected_panic[1024];
#endif
    size_t function_index = 0;
#if !defined(_WIN32)
    int expected_written = 0;
#endif
    bool sources_initialized = false;
    bool lexed_initialized = false;
    bool parsed_initialized = false;
    bool ast_initialized = false;
    bool checked_initialized = false;
    bool program_initialized = false;
    bool decoded_initialized = false;
    bool execution_initialized = false;
    bool panic_initialized = false;
    memset(&sources, 0, sizeof(sources));
    memset(&lexed, 0, sizeof(lexed));
    memset(&parsed, 0, sizeof(parsed));
    memset(&ast, 0, sizeof(ast));
    memset(&checked, 0, sizeof(checked));
    memset(functions, 0, sizeof(functions));
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&vm_panic, 0, sizeof(vm_panic));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&generated, 0, sizeof(generated));
    if (!ruda_source_manager_init(&sources, ruda_system_allocator())) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    sources_initialized = true;
    if (ruda_source_manager_add(
            &sources,
            "source-matrix.ruda",
            sizeof("source-matrix.ruda") - 1,
            source,
            sizeof(source) - 1,
            &snapshot
        ) != RUDA_SOURCE_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    if (ruda_lex_snapshot(snapshot, ruda_system_allocator(),
            ruda_lex_default_options(), &lexed) != RUDA_LEX_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    lexed_initialized = true;
    RUDA_EXPECT(lexed.diagnostics.length == 0);
    if (ruda_parse_file(&lexed, ruda_system_allocator(),
            ruda_parse_default_options(), &parsed) != RUDA_PARSE_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    parsed_initialized = true;
    RUDA_EXPECT(parsed.diagnostics.length == 0);
    if (ruda_ast_project(&parsed.tree, ruda_system_allocator(),
            ruda_ast_default_options(), &ast) != RUDA_AST_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    ast_initialized = true;
    if (ruda_check_ast(&ast, "test/source-matrix", ruda_system_allocator(),
            ruda_check_default_options(), &checked) != RUDA_CHECK_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    checked_initialized = true;
    RUDA_EXPECT(checked.diagnostics.length == 0);
    RUDA_EXPECT(checked.functions.length == 2);
    for (function_index = 0; function_index < checked.functions.length;
        function_index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &checked,
            function_index
        );
        if (function == NULL) {
            RUDA_EXPECT(false);
            goto cleanup;
        }
        functions[function_index] = &function->mir;
        if (function->name.length == sizeof("main") - 1
            && memcmp(function->name.data, "main", sizeof("main") - 1) == 0) {
            entry = function;
        } else if (function->name.length == sizeof("bounds") - 1
            && memcmp(
                function->name.data,
                "bounds",
                sizeof("bounds") - 1
            ) == 0) {
            bounds = function;
        }
    }
    RUDA_EXPECT(entry != NULL && bounds != NULL);
    if (entry == NULL || bounds == NULL) {
        goto cleanup;
    }
    if (ruda_mir_lower_bytecode_program(
            functions,
            2,
            entry->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(),
            &program
        ) != RUDA_MIR_BYTECODE_LOWER_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    program_initialized = true;
    if (ruda_bytecode_encode_program(
            &program,
            ruda_system_allocator(),
            &encoded
        ) != RUDA_BYTECODE_CODEC_OK
        || ruda_bytecode_decode_program(
            encoded.data,
            encoded.length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &decoded
        ) != RUDA_BYTECODE_CODEC_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    decoded_initialized = true;
    execution_options = ruda_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    if (!ruda_bytecode_execution_init(
            &execution,
            ruda_system_allocator(),
            execution_options
        )) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    RUDA_EXPECT(ruda_bytecode_run_program(
        &decoded,
        NULL,
        0,
        ruda_system_allocator(),
        run_options,
        &vm_result
    ) == RUDA_BYTECODE_RUN_OK);
    RUDA_EXPECT(vm_result.type == RUDA_BYTECODE_TYPE_F64
        && vm_result.as.f64_bits == expected_bits);
    native_program.functions = functions;
    native_program.function_count = 2;
    native_program.entry_symbol = entry->mir.symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program,
        ruda_system_allocator(),
        ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        ruda_test_native_compile_and_run(
            &generated,
            "ok:f64:407a500000000000\n",
            0
        );
    }
#endif

    ruda_owned_bytes_dispose(&generated);
    memset(&generated, 0, sizeof(generated));
    ruda_bytecode_execution_dispose(&execution);
    execution_initialized = false;
    memset(&execution, 0, sizeof(execution));
    ruda_bytecode_program_dispose(&decoded);
    decoded_initialized = false;
    memset(&decoded, 0, sizeof(decoded));
    ruda_owned_bytes_dispose(&encoded);
    memset(&encoded, 0, sizeof(encoded));
    ruda_bytecode_program_dispose(&program);
    program_initialized = false;
    memset(&program, 0, sizeof(program));

    if (ruda_mir_lower_bytecode_program(
            functions,
            2,
            bounds->mir.symbol,
            ruda_system_allocator(),
            ruda_mir_bytecode_program_lower_default_options(),
            &program
        ) != RUDA_MIR_BYTECODE_LOWER_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    program_initialized = true;
    if (ruda_bytecode_encode_program(
            &program,
            ruda_system_allocator(),
            &encoded
        ) != RUDA_BYTECODE_CODEC_OK
        || ruda_bytecode_decode_program(
            encoded.data,
            encoded.length,
            ruda_system_allocator(),
            ruda_bytecode_program_default_options(),
            &decoded
        ) != RUDA_BYTECODE_CODEC_OK) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    decoded_initialized = true;
    if (!ruda_bytecode_execution_init(
            &execution,
            ruda_system_allocator(),
            execution_options
        )) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    execution_initialized = true;
    if (!ruda_bytecode_panic_init(&vm_panic, ruda_system_allocator())) {
        RUDA_EXPECT(false);
        goto cleanup;
    }
    panic_initialized = true;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &vm_panic;
    memset(&vm_result, 0, sizeof(vm_result));
    RUDA_EXPECT(ruda_bytecode_run_program(
        &decoded,
        NULL,
        0,
        ruda_system_allocator(),
        run_options,
        &vm_result
    ) == RUDA_BYTECODE_RUN_PANIC);
    RUDA_EXPECT(vm_panic.code != NULL && strcmp(vm_panic.code, "G4002") == 0);
    RUDA_EXPECT(vm_panic.message.length == 33 && vm_panic.frames.length == 1);
    RUDA_EXPECT(ruda_symbol_id_equal(vm_panic.symbol, bounds->mir.symbol));
    native_program.entry_symbol = bounds->mir.symbol;
    RUDA_EXPECT(ruda_native_c_emit_program(
        &native_program,
        ruda_system_allocator(),
        ruda_native_c_emit_default_options(),
        &generated
    ) == RUDA_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    {
        const RudaBytecodePanicFrame *frame = ruda_bytecode_panic_frame(
            &vm_panic,
            0
        );
        RUDA_EXPECT(frame != NULL);
        RUDA_EXPECT(ruda_test_native_hex_bytes(
            bounds->mir.symbol.bytes,
            sizeof(bounds->mir.symbol.bytes),
            symbol_hex,
            sizeof(symbol_hex)
        ));
        RUDA_EXPECT(ruda_test_native_hex_bytes(
            vm_panic.message.data,
            vm_panic.message.length,
            message_hex,
            sizeof(message_hex)
        ));
        if (frame != NULL) {
            RUDA_EXPECT(ruda_test_native_hex_bytes(
                frame->symbol.bytes,
                sizeof(frame->symbol.bytes),
                frame_symbol_hex,
                sizeof(frame_symbol_hex)
            ));
            expected_written = snprintf(
                expected_panic,
                sizeof(expected_panic),
                "panic:G4002:%s:%" PRIu64 ":%" PRIu64
                ":%zu:%s:%zu:%s:%" PRIu64 ":%" PRIu64 "\n",
                symbol_hex,
                vm_panic.span.start,
                vm_panic.span.end,
                vm_panic.message.length,
                message_hex,
                vm_panic.frames.length,
                frame_symbol_hex,
                frame->call_span.start,
                frame->call_span.end
            );
            RUDA_EXPECT(expected_written >= 0
                && (size_t)expected_written < sizeof(expected_panic));
            if (expected_written >= 0
                && (size_t)expected_written < sizeof(expected_panic)
                && generated.data != NULL) {
                ruda_test_native_compile_and_run(
                    &generated,
                    expected_panic,
                    1
                );
            }
        }
    }
#endif

cleanup:
    ruda_owned_bytes_dispose(&generated);
    if (panic_initialized) {
        ruda_bytecode_panic_dispose(&vm_panic);
    }
    if (execution_initialized) {
        ruda_bytecode_execution_dispose(&execution);
    }
    if (decoded_initialized) {
        ruda_bytecode_program_dispose(&decoded);
    }
    ruda_owned_bytes_dispose(&encoded);
    if (program_initialized) {
        ruda_bytecode_program_dispose(&program);
    }
    if (checked_initialized) {
        ruda_check_result_dispose(&checked);
    }
    if (ast_initialized) {
        ruda_ast_tree_dispose(&ast);
    }
    if (parsed_initialized) {
        ruda_parse_result_dispose(&parsed);
    }
    if (lexed_initialized) {
        ruda_lex_result_dispose(&lexed);
    }
    if (sources_initialized) {
        ruda_source_manager_dispose(&sources);
    }
}


void ruda_test_native_source_matrix(void) {
    ruda_test_native_source_matrix_differential();
}
