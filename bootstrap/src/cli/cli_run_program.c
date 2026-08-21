#include "cli_compile_report.h"
#include "cli_run_report.h"

#include <string.h>

#include "ruda/bytecode/mir_lower.h"
#include "ruda/bytecode/program.h"
#include "ruda/check/entry.h"
#include "ruda/runtime/enum.h"

typedef struct RudaCliStdoutContext {
    RudaWriter writer;
} RudaCliStdoutContext;

static bool ruda_cli_stdout_publish(
    void *context,
    const unsigned char *bytes,
    size_t length
) {
    RudaCliStdoutContext *stdout_context = context;
    return stdout_context != NULL
        && ruda_writer_write(
            stdout_context->writer, (const char *)bytes, length
        );
}

static const RudaBytecodeEnumPayload *ruda_cli_run_error_payload(
    const RudaBytecodeFunction *function
) {
    const RudaBytecodeEnumLayout *layout = ruda_bytecode_enum_layout(
        function, function->result_layout
    );
    const RudaBytecodeEnumVariant *variant = layout != NULL
        ? ruda_vector_at_const(
            &function->enum_variants, layout->variant_start + 1
        ) : NULL;
    if (layout == NULL || variant == NULL || variant->payload_count != 1) {
        return NULL;
    }
    return ruda_vector_at_const(
        &function->enum_payloads,
        layout->payload_start + variant->payload_start
    );
}

static bool ruda_cli_run_program_entry_index(
    const RudaBytecodeProgram *program,
    RudaSymbolId symbol,
    uint32_t *entry
) {
    size_t index = 0;
    for (index = 0; index < program->functions.length; index += 1) {
        const RudaBytecodeProgramFunction *function =
            ruda_bytecode_program_function(program, index);
        if (function != NULL && ruda_symbol_id_equal(
                function->symbol, symbol
            )) {
            if (index > UINT32_MAX) return false;
            *entry = (uint32_t)index;
            return true;
        }
    }
    return false;
}

static int ruda_cli_run_render_expected(
    RudaSession *session,
    const RudaCliOptions *options,
    const RudaCheckedEntry *entry,
    const RudaBytecodeProgram *program,
    RudaBytecodeExecution *execution,
    RudaBytecodeRunOptions run_options,
    RudaBytecodeValue result,
    RudaWriter standard_error
) {
    const RudaBytecodeProgramFunction *main_function =
        ruda_bytecode_program_function(program, program->entry_function);
    const RudaBytecodeEnumPayload *payload = main_function != NULL
        ? ruda_cli_run_error_payload(&main_function->body) : NULL;
    const RudaManagedEnumDescriptor *descriptor = NULL;
    size_t variant = 0;
    RudaManagedCell cell;
    RudaBytecodeValue error_value;
    RudaBytecodeValue message_value;
    RudaBytecodeProgram message_program;
    RudaManagedRootFrame roots;
    RudaManagedReference *locations[1];
    RudaBytecodeRunStatus status;
    const unsigned char *message = NULL;
    size_t message_length = 0;
    int exit_code = RUDA_EXIT_INTERNAL;
    memset(&cell, 0, sizeof(cell));
    memset(&error_value, 0, sizeof(error_value));
    memset(&message_value, 0, sizeof(message_value));
    memset(&roots, 0, sizeof(roots));
    if (payload == NULL || result.type != RUDA_BYTECODE_TYPE_ENUM
        || ruda_managed_enum_describe(
            &execution->managed_heap, result.as.managed_reference,
            &descriptor, &variant
        ) != RUDA_MANAGED_OK) {
        return ruda_cli_run_pipeline_error(
            options, "verified main returned an invalid Result value",
            standard_error
        );
    }
    if (variant == 0) return RUDA_EXIT_SUCCESS;
    if (variant != 1 || ruda_managed_enum_get_payload(
            &execution->managed_heap, result.as.managed_reference,
            1, 0, &cell
        ) != RUDA_MANAGED_OK) {
        return ruda_cli_run_pipeline_error(
            options, "verified main returned an invalid Result variant",
            standard_error
        );
    }
    error_value.type = payload->type;
    error_value.as.managed_reference = cell.reference;
    locations[0] = &error_value.as.managed_reference;
    if (ruda_managed_root_push(
            &execution->managed_heap, &roots, locations, 1
        ) != RUDA_MANAGED_OK) {
        return RUDA_EXIT_INTERNAL;
    }
    message_program = *program;
    if (!ruda_cli_run_program_entry_index(
            program, entry->error_message->mir.symbol,
            &message_program.entry_function
        )) goto cleanup_root;
    status = ruda_bytecode_run_program(
        &message_program, &error_value, 1, session->allocator,
        run_options, &message_value
    );
    if (status != RUDA_BYTECODE_RUN_OK) {
        exit_code = ruda_cli_run_runtime_error(
            options, status, execution, run_options.panic, standard_error
        );
        goto cleanup_root;
    }
    if (!ruda_bytecode_execution_string_bytes(
            execution, message_value, &message, &message_length
        )) {
        goto cleanup_root;
    }
    exit_code = ruda_cli_run_expected_error(
        options, message, message_length, standard_error
    );
cleanup_root:
    if (ruda_managed_root_pop(
            &execution->managed_heap, &roots
        ) != RUDA_MANAGED_OK) exit_code = RUDA_EXIT_INTERNAL;
    return exit_code;
}

static int ruda_cli_run_execute(
    RudaSession *session,
    const RudaCliOptions *options,
    const RudaCompilerResult *compiler,
    const RudaCheckedEntry *entry,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    const RudaMirFunction **functions = NULL;
    size_t function_count = 0;
    size_t function_bytes = 0;
    size_t index = 0;
    RudaBytecodeProgram lowered;
    RudaBytecodeProgram decoded;
    RudaOwnedBytes encoded;
    RudaBytecodeExecution execution;
    RudaBytecodeExecutionOptions execution_options;
    RudaBytecodePanic panic;
    RudaBytecodeTrap trap;
    RudaBytecodeValue result;
    RudaBytecodeRunOptions run_options;
    RudaCliStdoutContext stdout_context;
    RudaIoStdout stdout_sink;
    RudaBytecodeRunStatus run_status;
    RudaMirBytecodeLowerStatus lower_status;
    RudaBytecodeCodecStatus codec_status;
    int exit_code = RUDA_EXIT_INTERNAL;
    bool execution_ready = false;
    bool panic_ready = false;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&trap, 0, sizeof(trap));
    memset(&result, 0, sizeof(result));
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &compiler->checked, index
        );
        if (function != NULL && !function->is_external) function_count += 1;
    }
    if (!ruda_size_multiply(
            function_count, sizeof(*functions), &function_bytes
        )) goto cleanup;
    functions = ruda_allocate(&session->allocator, function_bytes);
    if (functions == NULL) goto cleanup;
    function_count = 0;
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        const RudaCheckedFunction *function = ruda_checked_function(
            &compiler->checked, index
        );
        if (function != NULL && !function->is_external) {
            functions[function_count++] = &function->mir;
        }
    }
    lower_status = ruda_mir_lower_bytecode_program(
            functions, function_count, entry->function->mir.symbol,
            session->allocator,
            ruda_mir_bytecode_program_lower_default_options(), &lowered
        );
    if (lower_status != RUDA_MIR_BYTECODE_LOWER_OK) {
        exit_code = ruda_cli_run_pipeline_error(
            options, "canonical typed MIR did not lower to bytecode",
            standard_error
        );
        goto cleanup;
    }
    codec_status = ruda_bytecode_encode_program(
        &lowered, session->allocator, &encoded
    );
    if (codec_status != RUDA_BYTECODE_CODEC_OK) {
        exit_code = ruda_cli_run_pipeline_error(
            options, "verified bytecode did not encode", standard_error
        );
        goto cleanup;
    }
    codec_status = ruda_bytecode_decode_program(
        encoded.data, encoded.length, session->allocator,
        ruda_bytecode_program_default_options(), &decoded
    );
    if (codec_status != RUDA_BYTECODE_CODEC_OK
        || ruda_bytecode_program_verify(&decoded)
            != RUDA_BYTECODE_PROGRAM_VERIFY_OK) {
        exit_code = ruda_cli_run_pipeline_error(
            options, "encoded bytecode did not decode and verify",
            standard_error
        );
        goto cleanup;
    }
    execution_options = ruda_bytecode_execution_default_options();
    execution_options.managed_heap.max_live_bytes = options->max_heap_bytes;
    if (!ruda_bytecode_execution_init(
            &execution, session->allocator, execution_options
        )) goto cleanup;
    execution_ready = true;
    if (!ruda_bytecode_panic_init(&panic, session->allocator)) goto cleanup;
    panic_ready = true;
    stdout_context.writer = standard_output;
    stdout_sink.capability_granted = true;
    stdout_sink.context = &stdout_context;
    stdout_sink.publish = ruda_cli_stdout_publish;
    run_options = ruda_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.stdout_sink = &stdout_sink;
    run_options.panic = &panic;
    run_options.trap = &trap;
    run_status = ruda_bytecode_run_program(
        &decoded, NULL, 0, session->allocator, run_options, &result
    );
    if (run_status != RUDA_BYTECODE_RUN_OK) {
        exit_code = ruda_cli_run_runtime_error(
            options, run_status, &execution, &panic, standard_error
        );
    } else if (!entry->returns_result) {
        exit_code = result.type == RUDA_BYTECODE_TYPE_VOID
            ? RUDA_EXIT_SUCCESS : RUDA_EXIT_INTERNAL;
    } else {
        exit_code = ruda_cli_run_render_expected(
            session, options, entry, &decoded, &execution,
            run_options, result, standard_error
        );
    }
cleanup:
    if (panic_ready) ruda_bytecode_panic_dispose(&panic);
    if (execution_ready) ruda_bytecode_execution_dispose(&execution);
    ruda_bytecode_program_dispose(&decoded);
    ruda_owned_bytes_dispose(&encoded);
    ruda_bytecode_program_dispose(&lowered);
    ruda_deallocate(&session->allocator, functions, function_bytes);
    return exit_code;
}

int ruda_cli_run_program(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaCliCheckTarget target;
    RudaCompilerResult compiler;
    RudaCheckedEntry entry;
    RudaCliCheckTargetStatus target_status;
    RudaCompilerStatus compiler_status;
    RudaCheckEntryStatus entry_status;
    int exit_code = RUDA_EXIT_INTERNAL;
    memset(&target, 0, sizeof(target));
    memset(&compiler, 0, sizeof(compiler));
    memset(&entry, 0, sizeof(entry));
    (void)options->program_argument_start;
    (void)options->program_argument_count;
    target_status = ruda_cli_check_target_build(session, options, &target);
    if (target_status != RUDA_CLI_CHECK_TARGET_OK) {
        exit_code = ruda_cli_compile_target_error(
            options, &target, target_status, standard_error
        );
        goto cleanup;
    }
    compiler_status = ruda_compiler_service_check_graph(
        &target.service, target.compiler_packages.data,
        target.compiler_packages.length,
        (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &compiler
    );
    if (compiler_status != RUDA_COMPILER_OK) {
        exit_code = ruda_cli_compile_status_error(
            options, compiler_status, standard_error
        );
        goto cleanup;
    }
    exit_code = ruda_cli_compile_render_result(
        options, &compiler, standard_error
    );
    if (exit_code != RUDA_EXIT_SUCCESS) goto cleanup;
    entry_status = ruda_check_select_entry(
        &compiler.checked,
        (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &entry
    );
    if (entry_status != RUDA_CHECK_ENTRY_OK) {
        exit_code = ruda_cli_run_entry_error(
            options, entry_status, standard_error
        );
        goto cleanup;
    }
    exit_code = ruda_cli_run_execute(
        session, options, &compiler, &entry,
        standard_output, standard_error
    );
cleanup:
    ruda_compiler_result_dispose(&compiler);
    ruda_cli_check_target_dispose(&target);
    return exit_code;
}
