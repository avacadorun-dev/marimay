#include "cli_compile_report.h"

#include <string.h>

#include "mmay/generated/diagnostic_codes.h"

static bool mmay_cli_compile_render_semantics(
    const MarimayCompilerResult *result,
    MarimayDiagnosticStyle style,
    MarimayWriter standard_error
);

static bool mmay_cli_compile_render(
    MarimayWriter writer,
    MarimayDiagnosticStyle style,
    MarimaySeverity severity,
    const char *code,
    const char *message,
    const MarimaySourceSnapshot *source,
    size_t start,
    size_t end
) {
    MarimayDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = severity;
    diagnostic.message = message;
    diagnostic.source = source;
    diagnostic.source_start = start;
    diagnostic.source_end = end;
    return mmay_diagnostic_render(writer, style, &diagnostic);
}

int mmay_cli_compile_target_error(
    const MarimayCliOptions *options,
    const MarimayCliCheckTarget *target,
    MarimayCliCheckTargetStatus status,
    MarimayWriter standard_error
) {
    size_t index = 0;
    if (status == MMAY_CLI_CHECK_TARGET_SOURCE_INVALID) {
        for (index = 0; index < target->manifest.diagnostics.length;
            index += 1) {
            const MarimayManifestDiagnostic *diagnostic =
                mmay_manifest_diagnostic(&target->manifest, index);
            if (diagnostic == NULL || !mmay_cli_compile_render(
                    standard_error, options->diagnostic_style, MMAY_SEVERITY_ERROR,
                    diagnostic->code, diagnostic->message,
                    target->problem_source, diagnostic->start,
                    diagnostic->end
                )) return MMAY_EXIT_INTERNAL;
        }
        if (target->problem_code != NULL && !mmay_cli_compile_render(
                standard_error, options->diagnostic_style, MMAY_SEVERITY_ERROR,
                target->problem_code, target->problem_message,
                target->problem_source, target->problem_start,
                target->problem_end
            )) return MMAY_EXIT_INTERNAL;
        return MMAY_EXIT_SOURCE_FAILURE;
    }
    if (status == MMAY_CLI_CHECK_TARGET_INPUT_ERROR
        || status == MMAY_CLI_CHECK_TARGET_LIMIT_REACHED) {
        return mmay_cli_plain_error(
            standard_error, options->diagnostic_style,
            MMAY_DIAGNOSTIC_CHECK_INPUT_IO,
            status == MMAY_CLI_CHECK_TARGET_LIMIT_REACHED
                ? "checker target loading exhausted a defensive path, tree, or source limit"
                : "cannot discover or read the selected checker target",
            MMAY_EXIT_SOURCE_FAILURE
        );
    }
    return MMAY_EXIT_INTERNAL;
}

int mmay_cli_compile_status_error(
    const MarimayCliOptions *options,
    MarimayCompilerStatus status,
    const MarimayCompilerResult *result,
    MarimayWriter standard_error
) {
    if (status == MMAY_COMPILER_INCOMPLETE_LANGUAGE) {
        if (result != NULL && result->initialized
            && result->checked.initialized
            && result->checked.error_count != 0) {
            (void)mmay_cli_compile_render_semantics(
                result, options->diagnostic_style, standard_error
            );
            return MMAY_EXIT_INTERNAL;
        }
        return mmay_cli_plain_error(
            standard_error, options->diagnostic_style,
            MMAY_DIAGNOSTIC_COMPILER_INCOMPLETE,
            "compiler does not implement an accepted language construct",
            MMAY_EXIT_INTERNAL
        );
    }
    if (status == MMAY_COMPILER_LIMIT_REACHED) {
        return mmay_cli_plain_error(
            standard_error, options->diagnostic_style,
            MMAY_DIAGNOSTIC_CHECK_INPUT_IO,
            "checker exhausted a defensive frontend or semantic limit",
            MMAY_EXIT_SOURCE_FAILURE
        );
    }
    if (status == MMAY_COMPILER_CANCELLED) return MMAY_EXIT_INTERRUPTED;
    return MMAY_EXIT_INTERNAL;
}

static bool mmay_cli_compile_render_frontend(
    const MarimayCompilerResult *result,
    MarimayDiagnosticStyle style,
    MarimayWriter standard_error
) {
    size_t unit_index = 0;
    for (unit_index = 0; unit_index < result->units.length;
        unit_index += 1) {
        const MarimayCompilerUnit *unit = mmay_compiler_result_unit(
            result, unit_index
        );
        size_t diagnostic_index = 0;
        if (unit == NULL) return false;
        for (diagnostic_index = 0;
            diagnostic_index < unit->lexed.diagnostics.length;
            diagnostic_index += 1) {
            const MarimayLexDiagnostic *diagnostic = mmay_lex_diagnostic(
                &unit->lexed, diagnostic_index
            );
            if (diagnostic == NULL || !mmay_cli_compile_render(
                    standard_error, style, MMAY_SEVERITY_ERROR, diagnostic->code,
                    diagnostic->message, unit->snapshot,
                    diagnostic->start, diagnostic->end
                )) return false;
        }
        for (diagnostic_index = 0;
            diagnostic_index < unit->parsed.diagnostics.length;
            diagnostic_index += 1) {
            const MarimayParseDiagnostic *diagnostic = mmay_parse_diagnostic(
                &unit->parsed, diagnostic_index
            );
            if (diagnostic == NULL || !mmay_cli_compile_render(
                    standard_error, style, MMAY_SEVERITY_ERROR, diagnostic->code,
                    diagnostic->message, unit->snapshot,
                    diagnostic->start, diagnostic->end
                )) return false;
        }
    }
    return true;
}

static bool mmay_cli_compile_render_semantics(
    const MarimayCompilerResult *result,
    MarimayDiagnosticStyle style,
    MarimayWriter standard_error
) {
    size_t index = 0;
    if (!result->checked.initialized) return true;
    for (index = 0; index < result->checked.diagnostics.length; index += 1) {
        const MarimayCheckDiagnostic *diagnostic = mmay_check_diagnostic(
            &result->checked, index
        );
        if (diagnostic == NULL || !mmay_cli_compile_render(
                standard_error, style, diagnostic->severity, diagnostic->code,
                diagnostic->message, diagnostic->snapshot,
                diagnostic->start, diagnostic->end
            )) return false;
    }
    return true;
}

int mmay_cli_compile_render_result(
    const MarimayCliOptions *options,
    const MarimayCompilerResult *result,
    MarimayWriter standard_error
) {
    if (!mmay_cli_compile_render_frontend(
            result, options->diagnostic_style, standard_error
        ) || !mmay_cli_compile_render_semantics(
            result, options->diagnostic_style, standard_error
        )) return MMAY_EXIT_INTERNAL;
    return mmay_compiler_result_has_errors(result)
        ? MMAY_EXIT_SOURCE_FAILURE : MMAY_EXIT_SUCCESS;
}
