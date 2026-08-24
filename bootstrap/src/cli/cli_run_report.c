#include "cli_run_report.h"
#include "cli_resource_report.h"

#include <string.h>

#include "mmay/generated/diagnostic_codes.h"

static bool mmay_cli_run_write_panic(
    MarimayWriter writer,
    MarimayDiagnosticStyle style,
    const char *code,
    const unsigned char *message,
    size_t message_length
) {
    if (style == MMAY_DIAGNOSTIC_JSON) {
        MarimayDiagnostic diagnostic;
        memset(&diagnostic, 0, sizeof(diagnostic));
        diagnostic.code = code;
        diagnostic.severity = MMAY_SEVERITY_ERROR;
        diagnostic.message = (const char *)message;
        diagnostic.message_length = message_length;
        diagnostic.message_is_counted = true;
        return mmay_diagnostic_render(writer, style, &diagnostic);
    }
    if (style == MMAY_DIAGNOSTIC_SHORT) {
        return mmay_writer_write_cstr(writer, code)
            && mmay_writer_write_cstr(writer, ":error:")
            && mmay_writer_write(writer, (const char *)message, message_length)
            && mmay_writer_write_cstr(writer, "\n");
    }
    return mmay_writer_write_cstr(writer, "panic[")
        && mmay_writer_write_cstr(writer, code)
        && mmay_writer_write_cstr(writer, "]: ")
        && mmay_writer_write(writer, (const char *)message, message_length)
        && mmay_writer_write_cstr(writer, "\n");
}

int mmay_cli_run_entry_error(
    const MarimayCliOptions *options,
    MarimayCheckEntryStatus status,
    MarimayWriter standard_error
) {
    const char *message = status == MMAY_CHECK_ENTRY_NOT_FOUND
        ? "executable root does not declare fn main()"
        : (status == MMAY_CHECK_ENTRY_ERROR_CONTRACT_MISSING
            ? "main error type does not provide message(error): string"
            : "main accepts zero parameters or []string and returns void, int, Result<void, E>, or Result<int, E>");
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        MMAY_DIAGNOSTIC_INVALID_ENTRY_POINT, message,
        MMAY_EXIT_SOURCE_FAILURE
    );
}

int mmay_cli_run_pipeline_error(
    const MarimayCliOptions *options,
    const char *message,
    MarimayWriter standard_error
) {
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        MMAY_DIAGNOSTIC_RUN_PIPELINE, message, MMAY_EXIT_INTERNAL
    );
}

int mmay_cli_run_runtime_error(
    const MarimayCliOptions *options,
    MarimayBytecodeRunStatus status,
    const MarimayBytecodeExecution *execution,
    const MarimayBytecodePanic *panic,
    MarimayWriter standard_error
) {
    static const unsigned char overflow[] = "integer overflow";
    static const unsigned char division[] = "integer division by zero";
    static const unsigned char shift[] = "integer shift count is outside width";
    const unsigned char *message = NULL;
    size_t message_length = 0;
    bool classified = false;
    if (status == MMAY_BYTECODE_RUN_PANIC && panic != NULL
        && panic->initialized && panic->code != NULL) {
        return mmay_cli_run_write_panic(
            standard_error, options->diagnostic_style,
            panic->code, panic->message.data,
            panic->message.length
        ) ? MMAY_EXIT_SOURCE_FAILURE : MMAY_EXIT_INTERNAL;
    }
    if (status == MMAY_BYTECODE_RUN_OVERFLOW) {
        message = overflow;
        message_length = sizeof(overflow) - 1;
    } else if (status == MMAY_BYTECODE_RUN_DIVISION_BY_ZERO) {
        message = division;
        message_length = sizeof(division) - 1;
    } else if (status == MMAY_BYTECODE_RUN_INVALID_SHIFT) {
        message = shift;
        message_length = sizeof(shift) - 1;
    }
    if (message != NULL) {
        return mmay_cli_run_write_panic(
            standard_error, options->diagnostic_style,
            MMAY_DIAGNOSTIC_ARITHMETIC_PANIC,
            message, message_length
        ) ? MMAY_EXIT_SOURCE_FAILURE : MMAY_EXIT_INTERNAL;
    }
    if (!mmay_cli_resource_render(
            options->diagnostic_style, status,
            execution == NULL ? NULL : &execution->managed_heap,
            standard_error, &classified
        )) {
        return MMAY_EXIT_INTERNAL;
    }
    if (classified) return MMAY_EXIT_INTERNAL;
    return mmay_cli_plain_error(
        standard_error, options->diagnostic_style,
        MMAY_DIAGNOSTIC_RUNTIME_FAILURE,
        "VM execution failed a resource, integrity, or host requirement",
        MMAY_EXIT_INTERNAL
    );
}

int mmay_cli_run_expected_error(
    const MarimayCliOptions *options,
    const unsigned char *message,
    size_t message_length,
    MarimayWriter standard_error
) {
    MarimayDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = MMAY_DIAGNOSTIC_PROGRAM_ERROR;
    diagnostic.severity = MMAY_SEVERITY_ERROR;
    diagnostic.message = (const char *)message;
    diagnostic.message_length = message_length;
    diagnostic.message_is_counted = true;
    return mmay_diagnostic_render(
        standard_error, options->diagnostic_style, &diagnostic
    ) ? MMAY_EXIT_SOURCE_FAILURE : MMAY_EXIT_INTERNAL;
}
