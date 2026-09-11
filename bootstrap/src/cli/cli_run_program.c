#include "cli_compile_report.h"
#include "cli_bytecode_report.h"
#include "cli_native_run.h"
#include "cli_program_arguments.h"
#include "cli_run_report.h"

#include <string.h>

#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/program.h"
#include "mmay/bytecode/vm_arguments.h"
#include "mmay/base/checked_i64.h"
#include "mmay/check/entry.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/runtime/enum.h"

typedef struct MarimayCliStdoutContext {
    MarimayWriter writer;
} MarimayCliStdoutContext;

static bool mmay_cli_stdout_publish(
    void *context,
    const unsigned char *bytes,
    size_t length
) {
    MarimayCliStdoutContext *stdout_context = context;
    return stdout_context != NULL
        && mmay_writer_write(
            stdout_context->writer, (const char *)bytes, length
        );
}

static MarimayIoTransferStatus mmay_cli_stdout_write(
    void *context,
    const unsigned char *bytes,
    size_t length,
    size_t *count
) {
    MarimayCliStdoutContext *stdout_context = context;
    if (count == NULL || stdout_context == NULL || (bytes == NULL && length != 0)
        || !mmay_writer_write(
            stdout_context->writer, (const char *)bytes, length
        )) {
        return MMAY_IO_TRANSFER_FAILED;
    }
    *count = length;
    return MMAY_IO_TRANSFER_OK;
}

static MarimayIoTransferStatus mmay_cli_stdout_flush(void *context) {
    MarimayCliStdoutContext *stdout_context = context;
    return stdout_context != NULL && mmay_writer_flush(stdout_context->writer)
        ? MMAY_IO_TRANSFER_OK : MMAY_IO_TRANSFER_FAILED;
}

static MarimayIoTransferStatus mmay_cli_stdin_read(
    void *context, unsigned char *destination, size_t capacity, size_t *count
) {
    MarimayReadStream *stream = context;
    MarimayReadStatus status = stream != NULL && stream->read != NULL
        ? stream->read(stream->context, destination, capacity, count)
        : MMAY_READ_ERROR;
    if (status == MMAY_READ_OK) return MMAY_IO_TRANSFER_OK;
    if (status == MMAY_READ_END) return MMAY_IO_TRANSFER_END;
    return MMAY_IO_TRANSFER_FAILED;
}

static const MarimayBytecodeEnumPayload *mmay_cli_run_error_payload(
    const MarimayBytecodeFunction *function
) {
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, function->result_layout
    );
    const MarimayBytecodeEnumVariant *variant = layout != NULL
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start + 1
        ) : NULL;
    if (layout == NULL || variant == NULL || variant->payload_count != 1) {
        return NULL;
    }
    return mmay_vector_at_const(
        &function->enum_payloads,
        layout->payload_start + variant->payload_start
    );
}

static const MarimayBytecodeEnumPayload *mmay_cli_run_ok_payload(
    const MarimayBytecodeFunction *function
) {
    const MarimayBytecodeEnumLayout *layout = mmay_bytecode_enum_layout(
        function, function->result_layout
    );
    const MarimayBytecodeEnumVariant *variant = layout != NULL
        ? mmay_vector_at_const(
            &function->enum_variants, layout->variant_start
        ) : NULL;
    return layout != NULL && variant != NULL && variant->payload_count == 1
        ? mmay_vector_at_const(
            &function->enum_payloads,
            layout->payload_start + variant->payload_start
        ) : NULL;
}

static int mmay_cli_run_selected_status(
    const MarimayCliOptions *options,
    int64_t status,
    MarimayWriter standard_error
) {
    if (status >= 0 && status <= 255) return (int)status;
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        MMAY_DIAGNOSTIC_PROGRAM_EXIT_STATUS,
        "program exit status is outside 0..255",
        MMAY_EXIT_SOURCE_FAILURE
    );
}

static bool mmay_cli_run_program_entry_index(
    const MarimayBytecodeProgram *program,
    MarimaySymbolId symbol,
    uint32_t *entry
) {
    size_t index = 0;
    for (index = 0; index < program->functions.length; index += 1) {
        const MarimayBytecodeProgramFunction *function =
            mmay_bytecode_program_function(program, index);
        if (function != NULL && mmay_symbol_id_equal(
                function->symbol, symbol
            )) {
            if (index > UINT32_MAX) return false;
            *entry = (uint32_t)index;
            return true;
        }
    }
    return false;
}

static int mmay_cli_run_render_expected(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const MarimayCheckedEntry *entry,
    const MarimayBytecodeProgram *program,
    MarimayBytecodeExecution *execution,
    MarimayBytecodeRunOptions run_options,
    MarimayBytecodeValue result,
    MarimayWriter standard_error
) {
    const MarimayBytecodeProgramFunction *main_function =
        mmay_bytecode_program_function(program, program->entry_function);
    const MarimayBytecodeEnumPayload *payload = main_function != NULL
        ? mmay_cli_run_error_payload(&main_function->body) : NULL;
    const MarimayManagedEnumDescriptor *descriptor = NULL;
    size_t variant = 0;
    MarimayManagedCell cell;
    MarimayBytecodeValue error_value;
    MarimayBytecodeValue message_value;
    MarimayBytecodeProgram message_program;
    MarimayManagedRootFrame roots;
    MarimayManagedReference *locations[1];
    MarimayBytecodeRunStatus status;
    const unsigned char *message = NULL;
    size_t message_length = 0;
    int exit_code = MMAY_EXIT_INTERNAL;
    memset(&cell, 0, sizeof(cell));
    memset(&error_value, 0, sizeof(error_value));
    memset(&message_value, 0, sizeof(message_value));
    memset(&roots, 0, sizeof(roots));
    if (payload == NULL || result.type != MMAY_BYTECODE_TYPE_ENUM
        || mmay_managed_enum_describe(
            &execution->managed_heap, result.as.managed_reference,
            &descriptor, &variant
        ) != MMAY_MANAGED_OK) {
        return mmay_cli_run_pipeline_error(
            options, "verified main returned an invalid Result value",
            standard_error
        );
    }
    if (variant == 0) {
        const MarimayBytecodeEnumPayload *ok_payload =
            entry->returns_status ? mmay_cli_run_ok_payload(
                &main_function->body
            ) : NULL;
        if (!entry->returns_status) return MMAY_EXIT_SUCCESS;
        if (ok_payload == NULL || ok_payload->type != MMAY_BYTECODE_TYPE_I64
            || mmay_managed_enum_get_payload(
                &execution->managed_heap, result.as.managed_reference,
                0, 0, &cell
            ) != MMAY_MANAGED_OK) {
            return mmay_cli_run_pipeline_error(
                options, "verified main returned an invalid Result status",
                standard_error
            );
        }
        return mmay_cli_run_selected_status(
            options, mmay_i64_from_twos_complement(cell.f64_bits),
            standard_error
        );
    }
    if (variant != 1 || mmay_managed_enum_get_payload(
            &execution->managed_heap, result.as.managed_reference,
            1, 0, &cell
        ) != MMAY_MANAGED_OK) {
        return mmay_cli_run_pipeline_error(
            options, "verified main returned an invalid Result variant",
            standard_error
        );
    }
    error_value.type = payload->type;
    error_value.as.managed_reference = cell.reference;
    locations[0] = &error_value.as.managed_reference;
    if (mmay_managed_root_push(
            &execution->managed_heap, &roots, locations, 1
        ) != MMAY_MANAGED_OK) {
        return MMAY_EXIT_INTERNAL;
    }
    message_program = *program;
    if (!mmay_cli_run_program_entry_index(
            program, entry->error_message->mir.symbol,
            &message_program.entry_function
        )) goto cleanup_root;
    status = mmay_bytecode_run_program(
        &message_program, &error_value, 1, session->allocator,
        run_options, &message_value
    );
    if (status != MMAY_BYTECODE_RUN_OK) {
        exit_code = mmay_cli_run_runtime_error(
            options, status, execution, run_options.panic, standard_error
        );
        goto cleanup_root;
    }
    if (!mmay_bytecode_execution_string_bytes(
            execution, message_value, &message, &message_length
        )) {
        goto cleanup_root;
    }
    exit_code = mmay_cli_run_expected_error(
        options, message, message_length, standard_error
    );
cleanup_root:
    if (mmay_managed_root_pop(
            &execution->managed_heap, &roots
        ) != MMAY_MANAGED_OK) exit_code = MMAY_EXIT_INTERNAL;
    return exit_code;
}

static int mmay_cli_run_execute(
    MarimaySession *session,
    const MarimayCliOptions *options,
    const MarimayCompilerResult *compiler,
    const MarimayCheckedEntry *entry,
    MarimayProgramArgumentList program_arguments,
    MarimayReadStream standard_input,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    const MarimayMirFunction **functions = NULL;
    size_t function_count = 0;
    size_t function_bytes = 0;
    size_t index = 0;
    MarimayBytecodeProgram lowered;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options;
    MarimayCliFsCapabilities fs_capabilities;
    MarimayCliEnvCapabilities env_capabilities;
    MarimayBytecodePanic panic;
    MarimayBytecodeTrap trap;
    MarimayBytecodeValue result;
    MarimayBytecodeRunOptions run_options;
    MarimayCliStdoutContext stdout_context;
    MarimayCliStdoutContext stderr_context;
    MarimayIoStdout stdout_sink;
    MarimayBytecodeRunStatus run_status;
    MarimayMirBytecodeLowerStatus lower_status;
    MarimayBytecodeCodecStatus codec_status;
    int exit_code = MMAY_EXIT_INTERNAL;
    bool execution_ready = false;
    bool panic_ready = false;
    MarimayBytecodeValue entry_arguments[1];
    const MarimayBytecodeProgramFunction *entry_function = NULL;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&fs_capabilities, 0, sizeof(fs_capabilities));
    memset(&env_capabilities, 0, sizeof(env_capabilities));
    memset(&panic, 0, sizeof(panic));
    memset(&trap, 0, sizeof(trap));
    memset(&result, 0, sizeof(result));
    memset(entry_arguments, 0, sizeof(entry_arguments));
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &compiler->checked, index
        );
        if (function != NULL && !function->is_external) function_count += 1;
    }
    if (!mmay_size_multiply(
            function_count, sizeof(*functions), &function_bytes
        )) goto cleanup;
    functions = mmay_allocate(&session->allocator, function_bytes);
    if (functions == NULL) goto cleanup;
    function_count = 0;
    for (index = 0; index < compiler->checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &compiler->checked, index
        );
        if (function != NULL && !function->is_external) {
            functions[function_count++] = &function->mir;
        }
    }
    lower_status = mmay_mir_lower_bytecode_program(
            functions, function_count, entry->function->mir.symbol,
            session->allocator,
            mmay_mir_bytecode_program_lower_default_options(), &lowered
        );
    if (lower_status != MMAY_MIR_BYTECODE_LOWER_OK) {
        exit_code = mmay_cli_run_pipeline_error(
            options, "canonical typed MIR did not lower to bytecode",
            standard_error
        );
        goto cleanup;
    }
    codec_status = mmay_bytecode_encode_program(
        &lowered, session->allocator, &encoded
    );
    if (codec_status != MMAY_BYTECODE_CODEC_OK) {
        exit_code = mmay_cli_run_pipeline_error(
            options, mmay_cli_bytecode_codec_message(codec_status), standard_error
        );
        goto cleanup;
    }
    codec_status = mmay_bytecode_decode_program(
        encoded.data, encoded.length, session->allocator,
        mmay_bytecode_program_default_options(), &decoded
    );
    if (codec_status != MMAY_BYTECODE_CODEC_OK) {
        exit_code = mmay_cli_run_pipeline_error(
            options, mmay_cli_bytecode_codec_message(codec_status), standard_error
        );
        goto cleanup;
    }
    /* Successful program decoding already includes complete verification.
     * Keep that D049 boundary and the independent VM admission check. */
    execution_options = mmay_bytecode_execution_default_options();
    execution_options.managed_heap.max_live_bytes = options->max_heap_bytes;
    if (!mmay_cli_fs_capabilities(options, &fs_capabilities)) goto cleanup;
    if (!mmay_cli_env_capabilities(options, &env_capabilities)) goto cleanup;
    execution_options.fs = &fs_capabilities.options;
    execution_options.env = &env_capabilities.options;
    if (!mmay_bytecode_execution_init(
            &execution, session->allocator, execution_options
        )) goto cleanup;
    execution_ready = true;
    if (!mmay_bytecode_panic_init(&panic, session->allocator)) goto cleanup;
    panic_ready = true;
    stdout_context.writer = standard_output;
    stderr_context.writer = standard_error;
    stdout_sink.capability_granted = true;
    stdout_sink.context = &stdout_context;
    stdout_sink.publish = mmay_cli_stdout_publish;
    stdout_sink.stderr_capability_granted = true;
    stdout_sink.stderr_context = &stderr_context;
    stdout_sink.publish_stderr = mmay_cli_stdout_publish;
    stdout_sink.flush_stdout = mmay_cli_stdout_flush;
    stdout_sink.flush_stderr = mmay_cli_stdout_flush;
    stdout_sink.stdin_capability_granted = standard_input.read != NULL;
    stdout_sink.stdin_context = &standard_input;
    stdout_sink.read_stdin = mmay_cli_stdin_read;
    stdout_sink.write_stdout = mmay_cli_stdout_write;
    stdout_sink.write_stderr = mmay_cli_stdout_write;
    run_options = mmay_bytecode_run_default_options();
    /* D087: local commands are not instruction-fuel sandboxes. */
    run_options.max_steps = SIZE_MAX;
    run_options.execution = &execution;
    run_options.stdout_sink = &stdout_sink;
    run_options.panic = &panic;
    run_options.trap = &trap;
    entry_function = mmay_bytecode_program_function(
        &decoded, decoded.entry_function
    );
    if (entry_function == NULL) goto cleanup;
    if (entry->takes_arguments) {
        run_status = mmay_bytecode_execution_program_arguments(
            &execution, &entry_function->body,
            program_arguments, &entry_arguments[0]
        );
        if (run_status != MMAY_BYTECODE_RUN_OK) {
            exit_code = mmay_cli_run_runtime_error(
                options, run_status, &execution, &panic, standard_error
            );
            goto cleanup;
        }
    }
    run_status = mmay_bytecode_run_program(
        &decoded,
        entry->takes_arguments ? entry_arguments : NULL,
        entry->takes_arguments ? 1 : 0,
        session->allocator, run_options, &result
    );
    if (run_status != MMAY_BYTECODE_RUN_OK) {
        exit_code = mmay_cli_run_runtime_error(
            options, run_status, &execution, &panic, standard_error
        );
    } else if (!entry->returns_result) {
        if (!entry->returns_status) {
            exit_code = result.type == MMAY_BYTECODE_TYPE_VOID
                ? MMAY_EXIT_SUCCESS : MMAY_EXIT_INTERNAL;
        } else if (result.type == MMAY_BYTECODE_TYPE_I64) {
            exit_code = mmay_cli_run_selected_status(
                options, result.as.i64, standard_error
            );
        }
    } else {
        exit_code = mmay_cli_run_render_expected(
            session, options, entry, &decoded, &execution,
            run_options, result, standard_error
        );
    }
cleanup:
    if (panic_ready) mmay_bytecode_panic_dispose(&panic);
    if (execution_ready) mmay_bytecode_execution_dispose(&execution);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&lowered);
    mmay_deallocate(&session->allocator, functions, function_bytes);
    return exit_code;
}

int mmay_cli_run_program(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayReadStream standard_input,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayCliCheckTarget target;
    MarimayCompilerResult compiler;
    MarimayCheckedEntry entry;
    MarimayCliCheckTargetStatus target_status;
    MarimayCompilerStatus compiler_status;
    MarimayCheckEntryStatus entry_status;
    MarimayCliProgramArguments program_arguments;
    MarimayProgramArgumentsStatus argument_status;
    int exit_code = MMAY_EXIT_INTERNAL;
    memset(&target, 0, sizeof(target));
    memset(&compiler, 0, sizeof(compiler));
    memset(&entry, 0, sizeof(entry));
    memset(&program_arguments, 0, sizeof(program_arguments));
    argument_status = mmay_cli_program_arguments_build(
        session, options, &program_arguments
    );
    if (argument_status != MMAY_PROGRAM_ARGUMENTS_OK) {
        return mmay_cli_program_arguments_error(
            options, argument_status, standard_error
        );
    }
    target_status = mmay_cli_check_target_build(session, options, &target);
    if (target_status != MMAY_CLI_CHECK_TARGET_OK) {
        exit_code = mmay_cli_compile_target_error(
            options, &target, target_status, standard_error
        );
        goto cleanup;
    }
    compiler_status = mmay_compiler_service_check_graph(
        &target.service, target.compiler_packages.data,
        target.compiler_packages.length,
        (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &compiler
    );
    if (compiler_status != MMAY_COMPILER_OK) {
        exit_code = mmay_cli_compile_status_error(
            options, compiler_status, &compiler, standard_error
        );
        goto cleanup;
    }
    exit_code = mmay_cli_compile_render_result(
        options, &compiler, standard_error
    );
    if (exit_code != MMAY_EXIT_SUCCESS) goto cleanup;
    if (!mmay_cli_optimize_checked_mir(&compiler)) {
        exit_code = mmay_cli_run_pipeline_error(
            options, "canonical typed MIR range proof was invalid",
            standard_error
        );
        goto cleanup;
    }
    entry_status = mmay_check_select_entry(
        &compiler.checked,
        (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &entry
    );
    if (entry_status != MMAY_CHECK_ENTRY_OK) {
        exit_code = mmay_cli_run_entry_error(
            options, entry_status, standard_error
        );
        goto cleanup;
    }
    if (strcmp(options->run_engine, "native") == 0) {
        exit_code = mmay_cli_native_run_checked(
            session, options, &target, &compiler, &entry,
            standard_input, standard_output, standard_error
        );
    } else {
        exit_code = mmay_cli_run_execute(
            session, options, &compiler, &entry,
            program_arguments.list, standard_input,
            standard_output, standard_error
        );
    }
cleanup:
    mmay_cli_program_arguments_dispose(&program_arguments);
    mmay_compiler_result_dispose(&compiler);
    mmay_cli_check_target_dispose(&target);
    return exit_code;
}
