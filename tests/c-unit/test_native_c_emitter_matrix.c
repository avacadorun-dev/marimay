#include "test_native_c_emitter_support.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "mmay/ast/ast.h"
#include "mmay/check/checker.h"
#include "mmay/lex/lexer.h"
#include "mmay/parse/parser.h"
#include "mmay/source/source_manager.h"

static void mmay_test_native_source_matrix_differential(void) {
    static const char source[] =
        "fn main(): f64 {\n"
        "  let left = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]];\n"
        "  let right = [[7.0, 8.0], [9.0, 10.0], [11.0, 12.0]];\n"
        "  let result = [[0.0, 0.0], [0.0, 0.0]];\n"
        "  for row in 0..2 {\n"
        "    for column in 0..2 {\n"
        "      for inner in 0..3 {\n"
        "        result[row][column] += left[row][inner] * right[inner][column];\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "  let adjustment = 0.0;\n"
        "  for value in [1.0, 2.0, 3.0] { adjustment += value; }\n"
        "  return result[0][0] + result[0][1] + result[1][0] + result[1][1]\n"
        "    + adjustment;\n"
        "}\n"
        "fn bounds(): f64 { let values = [1.0]; values[3] = panic(\"rhs\"); }\n";
    static const uint64_t expected_bits = UINT64_C(0x407a500000000000);
    MarimaySourceManager sources;
    const MarimaySourceSnapshot *snapshot = NULL;
    MarimayLexResult lexed;
    MarimayParseResult parsed;
    MarimayAstTree ast;
    MarimayCheckResult checked;
    const MarimayCheckedFunction *entry = NULL;
    const MarimayCheckedFunction *bounds = NULL;
    const MarimayMirFunction *functions[2] = {0};
    MarimayBytecodeProgram program;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options;
    MarimayBytecodeRunOptions run_options;
    MarimayBytecodePanic vm_panic;
    MarimayBytecodeValue vm_result;
    MarimayNativeCProgram native_program;
    MarimayOwnedBytes generated;
#if !defined(_WIN32)
    char symbol_hex[sizeof(MarimaySymbolId) * 2 + 1];
    char frame_symbol_hex[sizeof(MarimaySymbolId) * 2 + 1];
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
    memset(&program, 0, sizeof(program));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&vm_panic, 0, sizeof(vm_panic));
    memset(&vm_result, 0, sizeof(vm_result));
    memset(&generated, 0, sizeof(generated));
    if (!mmay_source_manager_init(&sources, mmay_system_allocator())) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    sources_initialized = true;
    if (mmay_source_manager_add(
            &sources,
            "source-matrix.my",
            sizeof("source-matrix.my") - 1,
            source,
            sizeof(source) - 1,
            &snapshot
        ) != MMAY_SOURCE_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    if (mmay_lex_snapshot(snapshot, mmay_system_allocator(),
            mmay_lex_default_options(), &lexed) != MMAY_LEX_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    lexed_initialized = true;
    MMAY_EXPECT(lexed.diagnostics.length == 0);
    if (mmay_parse_file(&lexed, mmay_system_allocator(),
            mmay_parse_default_options(), &parsed) != MMAY_PARSE_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    parsed_initialized = true;
    MMAY_EXPECT(parsed.diagnostics.length == 0);
    if (mmay_ast_project(&parsed.tree, mmay_system_allocator(),
            mmay_ast_default_options(), &ast) != MMAY_AST_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    ast_initialized = true;
    if (mmay_check_ast(&ast, "test/source-matrix", mmay_system_allocator(),
            mmay_check_default_options(), &checked) != MMAY_CHECK_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    checked_initialized = true;
    MMAY_EXPECT(checked.diagnostics.length == 0);
    MMAY_EXPECT(checked.functions.length == 2);
    for (function_index = 0; function_index < checked.functions.length;
        function_index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &checked,
            function_index
        );
        if (function == NULL) {
            MMAY_EXPECT(false);
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
    MMAY_EXPECT(entry != NULL && bounds != NULL);
    if (entry == NULL || bounds == NULL) {
        goto cleanup;
    }
    if (mmay_mir_lower_bytecode_program(
            functions,
            2,
            entry->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(),
            &program
        ) != MMAY_MIR_BYTECODE_LOWER_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    program_initialized = true;
    if (mmay_bytecode_encode_program(
            &program,
            mmay_system_allocator(),
            &encoded
        ) != MMAY_BYTECODE_CODEC_OK
        || mmay_bytecode_decode_program(
            encoded.data,
            encoded.length,
            mmay_system_allocator(),
            mmay_bytecode_program_default_options(),
            &decoded
        ) != MMAY_BYTECODE_CODEC_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    decoded_initialized = true;
    execution_options = mmay_bytecode_execution_default_options();
    execution_options.managed_heap.collect_before_allocation = true;
    if (!mmay_bytecode_execution_init(
            &execution,
            mmay_system_allocator(),
            execution_options
        )) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    MMAY_EXPECT(mmay_bytecode_run_program(
        &decoded,
        NULL,
        0,
        mmay_system_allocator(),
        run_options,
        &vm_result
    ) == MMAY_BYTECODE_RUN_OK);
    MMAY_EXPECT(vm_result.type == MMAY_BYTECODE_TYPE_F64
        && vm_result.as.f64_bits == expected_bits);
    native_program.functions = functions;
    native_program.function_count = 2;
    native_program.entry_symbol = entry->mir.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program,
        mmay_system_allocator(),
        mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    if (generated.data != NULL) {
        mmay_test_native_compile_and_run(
            &generated,
            "ok:f64:407a500000000000\n",
            0
        );
    }
#endif

    mmay_owned_bytes_dispose(&generated);
    memset(&generated, 0, sizeof(generated));
    mmay_bytecode_execution_dispose(&execution);
    execution_initialized = false;
    memset(&execution, 0, sizeof(execution));
    mmay_bytecode_program_dispose(&decoded);
    decoded_initialized = false;
    memset(&decoded, 0, sizeof(decoded));
    mmay_owned_bytes_dispose(&encoded);
    memset(&encoded, 0, sizeof(encoded));
    mmay_bytecode_program_dispose(&program);
    program_initialized = false;
    memset(&program, 0, sizeof(program));

    if (mmay_mir_lower_bytecode_program(
            functions,
            2,
            bounds->mir.symbol,
            mmay_system_allocator(),
            mmay_mir_bytecode_program_lower_default_options(),
            &program
        ) != MMAY_MIR_BYTECODE_LOWER_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    program_initialized = true;
    if (mmay_bytecode_encode_program(
            &program,
            mmay_system_allocator(),
            &encoded
        ) != MMAY_BYTECODE_CODEC_OK
        || mmay_bytecode_decode_program(
            encoded.data,
            encoded.length,
            mmay_system_allocator(),
            mmay_bytecode_program_default_options(),
            &decoded
        ) != MMAY_BYTECODE_CODEC_OK) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    decoded_initialized = true;
    if (!mmay_bytecode_execution_init(
            &execution,
            mmay_system_allocator(),
            execution_options
        )) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    execution_initialized = true;
    if (!mmay_bytecode_panic_init(&vm_panic, mmay_system_allocator())) {
        MMAY_EXPECT(false);
        goto cleanup;
    }
    panic_initialized = true;
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.panic = &vm_panic;
    memset(&vm_result, 0, sizeof(vm_result));
    MMAY_EXPECT(mmay_bytecode_run_program(
        &decoded,
        NULL,
        0,
        mmay_system_allocator(),
        run_options,
        &vm_result
    ) == MMAY_BYTECODE_RUN_PANIC);
    MMAY_EXPECT(vm_panic.code != NULL && strcmp(vm_panic.code, "G4002") == 0);
    MMAY_EXPECT(vm_panic.message.length == 33 && vm_panic.frames.length == 1);
    MMAY_EXPECT(mmay_symbol_id_equal(vm_panic.symbol, bounds->mir.symbol));
    native_program.entry_symbol = bounds->mir.symbol;
    MMAY_EXPECT(mmay_native_c_emit_program(
        &native_program,
        mmay_system_allocator(),
        mmay_native_c_emit_default_options(),
        &generated
    ) == MMAY_NATIVE_C_EMIT_OK);
#if !defined(_WIN32)
    {
        const MarimayBytecodePanicFrame *frame = mmay_bytecode_panic_frame(
            &vm_panic,
            0
        );
        MMAY_EXPECT(frame != NULL);
        MMAY_EXPECT(mmay_test_native_hex_bytes(
            bounds->mir.symbol.bytes,
            sizeof(bounds->mir.symbol.bytes),
            symbol_hex,
            sizeof(symbol_hex)
        ));
        MMAY_EXPECT(mmay_test_native_hex_bytes(
            vm_panic.message.data,
            vm_panic.message.length,
            message_hex,
            sizeof(message_hex)
        ));
        if (frame != NULL) {
            MMAY_EXPECT(mmay_test_native_hex_bytes(
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
            MMAY_EXPECT(expected_written >= 0
                && (size_t)expected_written < sizeof(expected_panic));
            if (expected_written >= 0
                && (size_t)expected_written < sizeof(expected_panic)
                && generated.data != NULL) {
                mmay_test_native_compile_and_run(
                    &generated,
                    expected_panic,
                    1
                );
            }
        }
    }
#endif

cleanup:
    mmay_owned_bytes_dispose(&generated);
    if (panic_initialized) {
        mmay_bytecode_panic_dispose(&vm_panic);
    }
    if (execution_initialized) {
        mmay_bytecode_execution_dispose(&execution);
    }
    if (decoded_initialized) {
        mmay_bytecode_program_dispose(&decoded);
    }
    mmay_owned_bytes_dispose(&encoded);
    if (program_initialized) {
        mmay_bytecode_program_dispose(&program);
    }
    if (checked_initialized) {
        mmay_check_result_dispose(&checked);
    }
    if (ast_initialized) {
        mmay_ast_tree_dispose(&ast);
    }
    if (parsed_initialized) {
        mmay_parse_result_dispose(&parsed);
    }
    if (lexed_initialized) {
        mmay_lex_result_dispose(&lexed);
    }
    if (sources_initialized) {
        mmay_source_manager_dispose(&sources);
    }
}


void mmay_test_native_source_matrix(void) {
    mmay_test_native_source_matrix_differential();
}
