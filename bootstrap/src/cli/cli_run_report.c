#include "cli_run_report.h"
#include "cli_resource_report.h"

#include <string.h>

#include "ruda/generated/diagnostic_codes.h"

static bool ruda_cli_run_write_panic(
    RudaWriter writer,
    RudaDiagnosticStyle style,
    const char *code,
    const unsigned char *message,
    size_t message_length
) {
    if (style == RUDA_DIAGNOSTIC_JSON) {
        RudaDiagnostic diagnostic;
        memset(&diagnostic, 0, sizeof(diagnostic));
        diagnostic.code = code;
        diagnostic.severity = RUDA_SEVERITY_ERROR;
        diagnostic.message = (const char *)message;
        diagnostic.message_length = message_length;
        diagnostic.message_is_counted = true;
        return ruda_diagnostic_render(writer, style, &diagnostic);
    }
    if (style == RUDA_DIAGNOSTIC_SHORT) {
        return ruda_writer_write_cstr(writer, code)
            && ruda_writer_write_cstr(writer, ":error:")
            && ruda_writer_write(writer, (const char *)message, message_length)
            && ruda_writer_write_cstr(writer, "\n");
    }
    return ruda_writer_write_cstr(writer, "panic[")
        && ruda_writer_write_cstr(writer, code)
        && ruda_writer_write_cstr(writer, "]: ")
        && ruda_writer_write(writer, (const char *)message, message_length)
        && ruda_writer_write_cstr(writer, "\n");
}

int ruda_cli_run_entry_error(
    const RudaCliOptions *options,
    RudaCheckEntryStatus status,
    RudaWriter standard_error
) {
    const char *message = status == RUDA_CHECK_ENTRY_NOT_FOUND
        ? "executable root does not declare fn main()"
        : (status == RUDA_CHECK_ENTRY_ERROR_CONTRACT_MISSING
            ? "main error type does not provide message(error): string"
            : "main must be fn main(): void or fn main(): Result<void, E>");
    return ruda_cli_plain_error(
        standard_error, options->diagnostic_style,
        RUDA_DIAGNOSTIC_INVALID_ENTRY_POINT, message,
        RUDA_EXIT_SOURCE_FAILURE
    );
}

int ruda_cli_run_pipeline_error(
    const RudaCliOptions *options,
    const char *message,
    RudaWriter standard_error
) {
    return ruda_cli_plain_error(
        standard_error, options->diagnostic_style,
        RUDA_DIAGNOSTIC_RUN_PIPELINE, message, RUDA_EXIT_INTERNAL
    );
}

int ruda_cli_run_runtime_error(
    const RudaCliOptions *options,
    RudaBytecodeRunStatus status,
    const RudaBytecodeExecution *execution,
    const RudaBytecodePanic *panic,
    RudaWriter standard_error
) {
    static const unsigned char overflow[] = "integer overflow";
    static const unsigned char division[] = "integer division by zero";
    static const unsigned char shift[] = "integer shift count is outside width";
    const unsigned char *message = NULL;
    size_t message_length = 0;
    bool classified = false;
    if (status == RUDA_BYTECODE_RUN_PANIC && panic != NULL
        && panic->initialized && panic->code != NULL) {
        return ruda_cli_run_write_panic(
            standard_error, options->diagnostic_style,
            panic->code, panic->message.data,
            panic->message.length
        ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
    }
    if (status == RUDA_BYTECODE_RUN_OVERFLOW) {
        message = overflow;
        message_length = sizeof(overflow) - 1;
    } else if (status == RUDA_BYTECODE_RUN_DIVISION_BY_ZERO) {
        message = division;
        message_length = sizeof(division) - 1;
    } else if (status == RUDA_BYTECODE_RUN_INVALID_SHIFT) {
        message = shift;
        message_length = sizeof(shift) - 1;
    }
    if (message != NULL) {
        return ruda_cli_run_write_panic(
            standard_error, options->diagnostic_style,
            RUDA_DIAGNOSTIC_ARITHMETIC_PANIC,
            message, message_length
        ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
    }
    if (!ruda_cli_resource_render(
            options->diagnostic_style, status,
            execution == NULL ? NULL : &execution->managed_heap,
            standard_error, &classified
        )) {
        return RUDA_EXIT_INTERNAL;
    }
    if (classified) return RUDA_EXIT_INTERNAL;
    return ruda_cli_plain_error(
        standard_error, options->diagnostic_style,
        RUDA_DIAGNOSTIC_RUNTIME_FAILURE,
        "VM execution failed a resource, integrity, or host requirement",
        RUDA_EXIT_INTERNAL
    );
}

int ruda_cli_run_expected_error(
    const RudaCliOptions *options,
    const unsigned char *message,
    size_t message_length,
    RudaWriter standard_error
) {
    RudaDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = RUDA_DIAGNOSTIC_PROGRAM_ERROR;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    diagnostic.message = (const char *)message;
    diagnostic.message_length = message_length;
    diagnostic.message_is_counted = true;
    return ruda_diagnostic_render(
        standard_error, options->diagnostic_style, &diagnostic
    ) ? RUDA_EXIT_SOURCE_FAILURE : RUDA_EXIT_INTERNAL;
}
