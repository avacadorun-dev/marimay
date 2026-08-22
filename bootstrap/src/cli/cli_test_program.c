#include "cli_compile_report.h"
#include "cli_internal.h"
#include "cli_resource_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmay/bytecode/mir_lower.h"
#include "mmay/bytecode/program.h"
#include "mmay/bytecode/vm.h"
#include "mmay/generated/diagnostic_codes.h"
#include "mmay/native/build.h"
#include "mmay/platform/process.h"
#include "mmay/source/source_manager.h"

enum { MMAY_CLI_TEST_CAPTURE_LIMIT = 1024 * 1024 };

typedef struct MarimayCliTestCase {
    const MarimayCheckedFunction *function;
    const MarimayCheckedSymbol *fact;
} MarimayCliTestCase;

typedef struct MarimayCliTestObservation {
    MarimayOwnedBytes standard_output;
    MarimayOwnedBytes standard_error;
    uint32_t exit_code;
    uint32_t signal_code;
    bool signaled;
} MarimayCliTestObservation;

typedef struct MarimayCliTestContext {
    MarimaySession *session;
    const MarimayCliOptions *options;
    const MarimayCliCheckTarget *target;
    const MarimayCompilerResult *compiler;
    const MarimayMirFunction **functions;
    size_t function_count;
} MarimayCliTestContext;

typedef struct MarimayCliTestCapture {
    MarimayVector bytes;
} MarimayCliTestCapture;

static int mmay_cli_test_bytes_compare(
    const MarimayOwnedBytes *left,
    const MarimayOwnedBytes *right
) {
    size_t common = left->length < right->length
        ? left->length : right->length;
    int compared = common != 0 ? memcmp(left->data, right->data, common) : 0;
    if (compared != 0) return compared;
    return left->length < right->length
        ? -1 : left->length > right->length ? 1 : 0;
}

static int mmay_cli_test_case_compare(const void *left, const void *right) {
    const MarimayCliTestCase *first = left;
    const MarimayCliTestCase *second = right;
    const char *first_path = mmay_source_path(first->fact->snapshot);
    const char *second_path = mmay_source_path(second->fact->snapshot);
    size_t first_path_length = mmay_source_path_length(first->fact->snapshot);
    size_t second_path_length = mmay_source_path_length(second->fact->snapshot);
    size_t path_common = first_path_length < second_path_length
        ? first_path_length : second_path_length;
    int compared = mmay_cli_test_bytes_compare(
        &first->fact->package_name, &second->fact->package_name
    );
    if (compared == 0 && path_common != 0) {
        compared = memcmp(first_path, second_path, path_common);
    }
    if (compared == 0 && first_path_length != second_path_length) {
        compared = first_path_length < second_path_length ? -1 : 1;
    }
    if (compared != 0) return compared;
    if (first->fact->start != second->fact->start) {
        return first->fact->start < second->fact->start ? -1 : 1;
    }
    return mmay_cli_test_bytes_compare(
        &first->function->name, &second->function->name
    );
}

static void mmay_cli_test_observation_dispose(
    MarimayCliTestObservation *observation
) {
    if (observation == NULL) return;
    mmay_owned_bytes_dispose(&observation->standard_output);
    mmay_owned_bytes_dispose(&observation->standard_error);
    memset(observation, 0, sizeof(*observation));
}

static bool mmay_cli_test_capture_publish(
    void *context,
    const unsigned char *bytes,
    size_t length
) {
    MarimayCliTestCapture *capture = context;
    return capture != NULL && capture->bytes.length <= MMAY_CLI_TEST_CAPTURE_LIMIT
        && length <= MMAY_CLI_TEST_CAPTURE_LIMIT - capture->bytes.length
        && mmay_vector_append(&capture->bytes, bytes, length);
}

static bool mmay_cli_test_capture_write(
    void *context,
    const char *bytes,
    size_t length
) {
    return mmay_cli_test_capture_publish(
        context, (const unsigned char *)bytes, length
    );
}

static bool mmay_cli_test_append(
    MarimayVector *bytes,
    const void *data,
    size_t length
) {
    return length == 0 || mmay_vector_append(bytes, data, length);
}

static bool mmay_cli_test_panic_bytes(
    MarimayAllocator allocator,
    const MarimayBytecodePanic *panic,
    MarimayOwnedBytes *output
) {
    static const char prefix[] = "panic[";
    static const char separator[] = "]: ";
    static const char newline[] = "\n";
    MarimayVector bytes;
    bool ready = false;
    memset(&bytes, 0, sizeof(bytes));
    if (panic == NULL || panic->code == NULL
        || !mmay_vector_init(&bytes, allocator, sizeof(unsigned char))) {
        return false;
    }
    ready = mmay_cli_test_append(&bytes, prefix, sizeof(prefix) - 1)
        && mmay_cli_test_append(&bytes, panic->code, strlen(panic->code))
        && mmay_cli_test_append(&bytes, separator, sizeof(separator) - 1)
        && mmay_cli_test_append(
            &bytes, panic->message.data, panic->message.length
        ) && mmay_cli_test_append(&bytes, newline, sizeof(newline) - 1)
        && mmay_owned_bytes_copy(
            output, allocator, bytes.data, bytes.length
        );
    mmay_vector_dispose(&bytes);
    return ready;
}

static bool mmay_cli_test_vm(
    const MarimayCliTestContext *context,
    const MarimayCliTestCase *test,
    MarimayCliTestObservation *observation
) {
    MarimayBytecodeProgram lowered;
    MarimayBytecodeProgram decoded;
    MarimayOwnedBytes encoded;
    MarimayBytecodeExecution execution;
    MarimayBytecodeExecutionOptions execution_options;
    MarimayBytecodePanic panic;
    MarimayBytecodeValue result;
    MarimayBytecodeRunOptions run_options;
    MarimayCliTestCapture capture;
    MarimayCliTestCapture error_capture;
    MarimayWriter error_writer;
    MarimayIoStdout sink;
    MarimayMirBytecodeLowerStatus lower_status;
    MarimayBytecodeCodecStatus codec_status;
    MarimayBytecodeRunStatus run_status;
    bool execution_ready = false;
    bool panic_ready = false;
    bool error_capture_ready = false;
    bool accepted = false;
    memset(&lowered, 0, sizeof(lowered));
    memset(&decoded, 0, sizeof(decoded));
    memset(&encoded, 0, sizeof(encoded));
    memset(&execution, 0, sizeof(execution));
    memset(&panic, 0, sizeof(panic));
    memset(&result, 0, sizeof(result));
    memset(&capture, 0, sizeof(capture));
    memset(&error_capture, 0, sizeof(error_capture));
    memset(observation, 0, sizeof(*observation));
    if (!mmay_vector_init(
            &capture.bytes, context->session->allocator,
            sizeof(unsigned char)
        )) goto cleanup;
    lower_status = mmay_mir_lower_bytecode_program(
        context->functions, context->function_count,
        test->function->mir.symbol, context->session->allocator,
        mmay_mir_bytecode_program_lower_default_options(), &lowered
    );
    if (lower_status != MMAY_MIR_BYTECODE_LOWER_OK) goto cleanup;
    codec_status = mmay_bytecode_encode_program(
        &lowered, context->session->allocator, &encoded
    );
    if (codec_status != MMAY_BYTECODE_CODEC_OK) goto cleanup;
    codec_status = mmay_bytecode_decode_program(
        encoded.data, encoded.length, context->session->allocator,
        mmay_bytecode_program_default_options(), &decoded
    );
    if (codec_status != MMAY_BYTECODE_CODEC_OK
        || mmay_bytecode_program_verify(&decoded)
            != MMAY_BYTECODE_PROGRAM_VERIFY_OK) goto cleanup;
    execution_options = mmay_bytecode_execution_default_options();
    execution_options.managed_heap.max_live_bytes =
        context->options->max_heap_bytes;
    if (!mmay_bytecode_execution_init(
            &execution, context->session->allocator, execution_options
        )) goto cleanup;
    execution_ready = true;
    if (!mmay_bytecode_panic_init(
            &panic, context->session->allocator
        )) goto cleanup;
    panic_ready = true;
    sink.capability_granted = true;
    sink.context = &capture;
    sink.publish = mmay_cli_test_capture_publish;
    run_options = mmay_bytecode_run_default_options();
    run_options.execution = &execution;
    run_options.stdout_sink = &sink;
    run_options.panic = &panic;
    run_status = mmay_bytecode_run_program(
        &decoded, NULL, 0, context->session->allocator,
        run_options, &result
    );
    if (run_status == MMAY_BYTECODE_RUN_OK
        && result.type == MMAY_BYTECODE_TYPE_VOID) {
        observation->exit_code = 0;
    } else if (run_status == MMAY_BYTECODE_RUN_PANIC) {
        observation->exit_code = 1;
        if (!mmay_cli_test_panic_bytes(
                context->session->allocator, &panic,
                &observation->standard_error
            )) goto cleanup;
    } else {
        bool classified = false;
        if (!mmay_vector_init(
                &error_capture.bytes, context->session->allocator,
                sizeof(unsigned char)
            )) goto cleanup;
        error_capture_ready = true;
        error_writer.context = &error_capture;
        error_writer.write = mmay_cli_test_capture_write;
        if (!mmay_cli_resource_render(
                MMAY_DIAGNOSTIC_HUMAN, run_status,
                &execution.managed_heap, error_writer, &classified
            ) || !classified
            || !mmay_owned_bytes_copy(
                &observation->standard_error,
                context->session->allocator,
                error_capture.bytes.data, error_capture.bytes.length
            )) goto cleanup;
        observation->exit_code = MMAY_EXIT_INTERNAL;
    }
    if (!mmay_owned_bytes_copy(
            &observation->standard_output, context->session->allocator,
            capture.bytes.data, capture.bytes.length
        )) goto cleanup;
    if (observation->exit_code == 0 && !mmay_owned_bytes_copy(
            &observation->standard_error, context->session->allocator,
            NULL, 0
        )) goto cleanup;
    accepted = true;
cleanup:
    if (!accepted) mmay_cli_test_observation_dispose(observation);
    if (panic_ready) mmay_bytecode_panic_dispose(&panic);
    if (execution_ready) mmay_bytecode_execution_dispose(&execution);
    if (error_capture_ready) mmay_vector_dispose(&error_capture.bytes);
    mmay_vector_dispose(&capture.bytes);
    mmay_bytecode_program_dispose(&decoded);
    mmay_owned_bytes_dispose(&encoded);
    mmay_bytecode_program_dispose(&lowered);
    return accepted;
}

static bool mmay_cli_test_native(
    const MarimayCliTestContext *context,
    const MarimayCliTestCase *test,
    MarimayCliTestObservation *observation,
    MarimayNativeBuildStatus *build_status,
    MarimayProcessStatus *process_status
) {
    MarimayNativeBuildRequest request;
    MarimayNativeBuildResult build;
    MarimayProcessCaptureResult capture;
    const char *arguments[2];
    bool accepted = false;
    memset(&request, 0, sizeof(request));
    memset(&build, 0, sizeof(build));
    memset(&capture, 0, sizeof(capture));
    memset(observation, 0, sizeof(*observation));
    request.functions = context->functions;
    request.function_count = context->function_count;
    request.entry_symbol = test->function->mir.symbol;
    request.root_directory = (const char *)context->target->root_directory.data;
    request.root_directory_length = context->target->root_directory.length;
    request.max_heap_bytes = context->options->max_heap_bytes;
    *build_status = mmay_native_build(
        context->session->allocator, &request, &build
    );
    if (*build_status != MMAY_NATIVE_BUILD_OK) goto cleanup;
    arguments[0] = (const char *)build.artifact_path.data;
    arguments[1] = NULL;
    *process_status = mmay_process_run_capture(
        context->session->allocator, arguments, 1,
        MMAY_CLI_TEST_CAPTURE_LIMIT, MMAY_CLI_TEST_CAPTURE_LIMIT,
        &capture
    );
    if (*process_status != MMAY_PROCESS_OK) goto cleanup;
    observation->standard_output = capture.standard_output;
    observation->standard_error = capture.standard_error;
    observation->exit_code = capture.exit_code;
    observation->signal_code = capture.signal_code;
    observation->signaled = capture.signaled;
    memset(&capture.standard_output, 0, sizeof(capture.standard_output));
    memset(&capture.standard_error, 0, sizeof(capture.standard_error));
    accepted = true;
cleanup:
    if (!accepted) mmay_cli_test_observation_dispose(observation);
    mmay_process_capture_result_dispose(&capture);
    mmay_native_build_result_dispose(&build);
    return accepted;
}

static bool mmay_cli_test_observations_equal(
    const MarimayCliTestObservation *left,
    const MarimayCliTestObservation *right
) {
    return left->exit_code == right->exit_code
        && left->signaled == right->signaled
        && left->signal_code == right->signal_code
        && mmay_cli_test_bytes_compare(
            &left->standard_output, &right->standard_output
        ) == 0 && mmay_cli_test_bytes_compare(
            &left->standard_error, &right->standard_error
        ) == 0;
}

static bool mmay_cli_test_resource_failure(
    const MarimayCliTestObservation *observation
) {
    static const unsigned char host[] =
        "error[I0002]: runtime host allocation failed\n";
    static const unsigned char budget[] =
        "error[I0002]: managed heap budget exceeded: live=";
    return observation != NULL
        && observation->exit_code == MMAY_EXIT_INTERNAL
        && !observation->signaled
        && observation->standard_output.length == 0
        && ((observation->standard_error.length == sizeof(host) - 1
                && memcmp(
                    observation->standard_error.data, host, sizeof(host) - 1
                ) == 0)
            || (observation->standard_error.length > sizeof(budget) - 1
                && memcmp(
                    observation->standard_error.data,
                    budget,
                    sizeof(budget) - 1
                ) == 0));
}

static bool mmay_cli_test_write_name(
    MarimayWriter writer,
    const MarimayCliTestCase *test
) {
    return mmay_writer_write(
            writer, (const char *)test->fact->package_name.data,
            test->fact->package_name.length
        ) && mmay_writer_write_cstr(writer, ".")
        && mmay_writer_write(
            writer, (const char *)test->function->name.data,
            test->function->name.length
        );
}

static bool mmay_cli_test_report_observation(
    MarimayWriter writer,
    const MarimayCliTestObservation *observation
) {
    if (observation->standard_output.length != 0
        && (!mmay_writer_write_cstr(writer, "  stdout:\n")
            || !mmay_writer_write(
                writer, (const char *)observation->standard_output.data,
                observation->standard_output.length
            ))) return false;
    if (observation->standard_error.length != 0
        && (!mmay_writer_write_cstr(writer, "  stderr:\n")
            || !mmay_writer_write(
                writer, (const char *)observation->standard_error.data,
                observation->standard_error.length
            ))) return false;
    return true;
}

static int mmay_cli_test_pipeline_error(
    const MarimayCliOptions *options,
    MarimayWriter standard_error,
    const char *message
) {
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        MMAY_DIAGNOSTIC_RUNTIME_FAILURE, message, MMAY_EXIT_INTERNAL
    );
}

static int mmay_cli_test_build_error(
    const MarimayCliOptions *options,
    MarimayWriter standard_error,
    MarimayNativeBuildStatus status
) {
    const char *code = MMAY_DIAGNOSTIC_NATIVE_EMIT_FAILED;
    const char *message = "native test build failed inside the compiler";
    int exit_code = MMAY_EXIT_INTERNAL;
    if (status == MMAY_NATIVE_BUILD_TOOLCHAIN_MISSING) {
        code = MMAY_DIAGNOSTIC_NATIVE_TOOLCHAIN_MISSING;
        message = "supported host C toolchain is unavailable for native tests";
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    } else if (status == MMAY_NATIVE_BUILD_IO_FAILED
        || status == MMAY_NATIVE_BUILD_LIMIT_REACHED) {
        code = MMAY_DIAGNOSTIC_BUILD_OUTPUT_IO;
        message = "cannot write the native test build cache";
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    } else if (status == MMAY_NATIVE_BUILD_COMPILE_FAILED) {
        code = MMAY_DIAGNOSTIC_NATIVE_COMPILE_FAILED;
        message = "the supported host compiler rejected generated test C";
    } else if (status == MMAY_NATIVE_BUILD_UNSUPPORTED_HOST) {
        code = MMAY_DIAGNOSTIC_NATIVE_HOST_UNSUPPORTED;
        message = "the selected host cannot preserve native test semantics";
        exit_code = MMAY_EXIT_SOURCE_FAILURE;
    }
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        code, message, exit_code
    );
}

static int mmay_cli_test_execute(
    MarimayCliTestContext *context,
    MarimayVector *tests,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    size_t index = 0;
    size_t passed = 0;
    size_t failed = 0;
    bool use_vm = strcmp(context->options->run_engine, "native") != 0;
    bool use_native = strcmp(context->options->run_engine, "vm") != 0;
    bool both = use_vm && use_native;
    for (index = 0; index < tests->length; index += 1) {
        const MarimayCliTestCase *test = mmay_vector_at_const(tests, index);
        MarimayCliTestObservation vm;
        MarimayCliTestObservation native;
        const MarimayCliTestObservation *observed = NULL;
        MarimayNativeBuildStatus build_status = MMAY_NATIVE_BUILD_OK;
        MarimayProcessStatus process_status = MMAY_PROCESS_OK;
        bool ready = true;
        memset(&vm, 0, sizeof(vm));
        memset(&native, 0, sizeof(native));
        if (use_vm) ready = mmay_cli_test_vm(context, test, &vm);
        if (ready && use_native) ready = mmay_cli_test_native(
            context, test, &native, &build_status, &process_status
        );
        if (!ready) {
            mmay_cli_test_observation_dispose(&native);
            mmay_cli_test_observation_dispose(&vm);
            if (build_status != MMAY_NATIVE_BUILD_OK) {
                return mmay_cli_test_build_error(
                    context->options, standard_error, build_status
                );
            }
            if (process_status != MMAY_PROCESS_OK) {
                return mmay_cli_test_pipeline_error(
                    context->options, standard_error,
                    "native test process could not be captured"
                );
            }
            return mmay_cli_test_pipeline_error(
                context->options, standard_error,
                "test execution pipeline failed"
            );
        }
        if (use_native && native.exit_code == MMAY_EXIT_INTERNAL
            && !mmay_cli_test_resource_failure(&native)) {
            mmay_cli_test_observation_dispose(&native);
            mmay_cli_test_observation_dispose(&vm);
            return mmay_cli_test_pipeline_error(
                context->options, standard_error,
                "native test runtime failed internally"
            );
        }
        if (both && !mmay_cli_test_observations_equal(&vm, &native)) {
            (void)mmay_writer_write_cstr(standard_error, "divergent test ");
            (void)mmay_cli_test_write_name(standard_error, test);
            (void)mmay_writer_write_cstr(standard_error, "\nVM observation:\n");
            (void)mmay_cli_test_report_observation(standard_error, &vm);
            (void)mmay_writer_write_cstr(standard_error, "native observation:\n");
            (void)mmay_cli_test_report_observation(standard_error, &native);
            mmay_cli_test_observation_dispose(&native);
            mmay_cli_test_observation_dispose(&vm);
            return mmay_cli_plain_error(
                standard_error, context->options->diagnostic_style,
                MMAY_DIAGNOSTIC_ENGINE_DIVERGENCE,
                "VM and native result/output/panic observations differ",
                MMAY_EXIT_DIVERGENCE
            );
        }
        observed = use_vm ? &vm : &native;
        if (observed->exit_code == MMAY_EXIT_INTERNAL) {
            bool wrote = mmay_writer_write(
                standard_error, (const char *)observed->standard_error.data,
                observed->standard_error.length
            );
            mmay_cli_test_observation_dispose(&native);
            mmay_cli_test_observation_dispose(&vm);
            if (!wrote) return MMAY_EXIT_INTERNAL;
            return MMAY_EXIT_INTERNAL;
        }
        if (!context->options->quiet) {
            if (!mmay_writer_write_cstr(standard_output, "test ")
                || !mmay_cli_test_write_name(standard_output, test)
                || !mmay_writer_write_cstr(
                    standard_output,
                    observed->exit_code == 0 ? " ... ok\n" : " ... FAILED\n"
                )) {
                mmay_cli_test_observation_dispose(&native);
                mmay_cli_test_observation_dispose(&vm);
                return MMAY_EXIT_INTERNAL;
            }
        }
        if (observed->exit_code == 0) {
            passed += 1;
        } else {
            failed += 1;
            if (!mmay_cli_test_report_observation(
                    standard_output, observed
                )) {
                mmay_cli_test_observation_dispose(&native);
                mmay_cli_test_observation_dispose(&vm);
                return MMAY_EXIT_INTERNAL;
            }
        }
        mmay_cli_test_observation_dispose(&native);
        mmay_cli_test_observation_dispose(&vm);
    }
    if (!context->options->quiet) {
        char summary[160];
        int length = snprintf(
            summary, sizeof(summary),
            "test result: %s. %zu passed; %zu failed\n",
            failed == 0 ? "ok" : "FAILED", passed, failed
        );
        if (length < 0 || (size_t)length >= sizeof(summary)
            || !mmay_writer_write(
                standard_output, summary, (size_t)length
            )) return MMAY_EXIT_INTERNAL;
    }
    return failed == 0 ? MMAY_EXIT_SUCCESS : MMAY_EXIT_SOURCE_FAILURE;
}

int mmay_cli_test_program(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayCliOptions target_options = *options;
    MarimayCliCheckTarget target;
    MarimayCompilerResult compiler;
    MarimayVector tests;
    MarimayCliTestContext context;
    MarimayCliCheckTargetStatus target_status;
    MarimayCompilerStatus compiler_status;
    size_t index = 0;
    size_t function_bytes = 0;
    int exit_code = MMAY_EXIT_INTERNAL;
    memset(&target, 0, sizeof(target));
    memset(&compiler, 0, sizeof(compiler));
    memset(&tests, 0, sizeof(tests));
    memset(&context, 0, sizeof(context));
    target_options.check_all_targets = true;
    target_status = mmay_cli_check_target_build(
        session, &target_options, &target
    );
    if (target_status != MMAY_CLI_CHECK_TARGET_OK) {
        exit_code = mmay_cli_compile_target_error(
            options, &target, target_status, standard_error
        );
        goto cleanup;
    }
    compiler_status = mmay_compiler_service_check_module_graph(
        &target.service, target.compiler_packages.data,
        target.compiler_packages.length, &compiler
    );
    if (compiler_status != MMAY_COMPILER_OK) {
        exit_code = mmay_cli_compile_status_error(
            options, compiler_status, standard_error
        );
        goto cleanup;
    }
    exit_code = mmay_cli_compile_render_result(
        options, &compiler, standard_error
    );
    if (exit_code != MMAY_EXIT_SUCCESS) goto cleanup;
    if (!mmay_cli_optimize_checked_mir(&compiler)) goto cleanup;
    if (!mmay_vector_init(
            &tests, session->allocator, sizeof(MarimayCliTestCase)
        )) goto cleanup;
    for (index = 0; index < compiler.checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &compiler.checked, index
        );
        if (function != NULL && function->is_test) {
            MarimayCliTestCase test;
            test.function = function;
            test.fact = mmay_checked_symbol_by_id(
                &compiler.checked, function->mir.symbol
            );
            if (test.fact == NULL || !mmay_vector_push(&tests, &test)) {
                goto cleanup;
            }
        }
    }
    if (tests.length > 1) qsort(
        tests.data, tests.length, tests.element_size,
        mmay_cli_test_case_compare
    );
    for (index = 0; index < compiler.checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &compiler.checked, index
        );
        if (function != NULL && !function->is_external) {
            context.function_count += 1;
        }
    }
    if (!mmay_size_multiply(
            context.function_count, sizeof(*context.functions),
            &function_bytes
        )) goto cleanup;
    if (context.function_count != 0) {
        context.functions = mmay_allocate(
            &session->allocator, function_bytes
        );
        if (context.functions == NULL) goto cleanup;
    }
    context.function_count = 0;
    for (index = 0; index < compiler.checked.functions.length; index += 1) {
        const MarimayCheckedFunction *function = mmay_checked_function(
            &compiler.checked, index
        );
        if (function != NULL && !function->is_external) {
            context.functions[context.function_count++] = &function->mir;
        }
    }
    context.session = session;
    context.options = options;
    context.target = &target;
    context.compiler = &compiler;
    exit_code = mmay_cli_test_execute(
        &context, &tests, standard_output, standard_error
    );
cleanup:
    mmay_deallocate(&session->allocator, context.functions, function_bytes);
    mmay_vector_dispose(&tests);
    mmay_compiler_result_dispose(&compiler);
    mmay_cli_check_target_dispose(&target);
    return exit_code;
}
