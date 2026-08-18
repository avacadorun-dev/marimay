#include "cli_check_targets.h"

#include <string.h>

#include "ruda/generated/diagnostic_codes.h"

static bool ruda_cli_check_render(
    RudaWriter writer,
    RudaDiagnosticStyle style,
    const char *code,
    const char *message,
    const RudaSourceSnapshot *source,
    size_t start,
    size_t end
) {
    RudaDiagnostic diagnostic;
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.severity = RUDA_SEVERITY_ERROR;
    diagnostic.message = message;
    diagnostic.source = source;
    diagnostic.source_start = start;
    diagnostic.source_end = end;
    return ruda_diagnostic_render(writer, style, &diagnostic);
}

static int ruda_cli_check_target_error(
    const RudaCliOptions *options,
    const RudaCliCheckTarget *target,
    RudaCliCheckTargetStatus status,
    RudaWriter standard_error
) {
    size_t index = 0;
    if (status == RUDA_CLI_CHECK_TARGET_SOURCE_INVALID) {
        for (index = 0; index < target->manifest.diagnostics.length;
            index += 1) {
            const RudaManifestDiagnostic *diagnostic =
                ruda_manifest_diagnostic(&target->manifest, index);
            if (diagnostic == NULL || !ruda_cli_check_render(
                    standard_error,
                    options->diagnostic_style,
                    diagnostic->code,
                    diagnostic->message,
                    target->problem_source,
                    diagnostic->start,
                    diagnostic->end
                )) {
                return RUDA_EXIT_INTERNAL;
            }
        }
        if (target->problem_code != NULL && !ruda_cli_check_render(
                standard_error,
                options->diagnostic_style,
                target->problem_code,
                target->problem_message,
                target->problem_source,
                target->problem_start,
                target->problem_end
            )) {
            return RUDA_EXIT_INTERNAL;
        }
        return RUDA_EXIT_SOURCE_FAILURE;
    }
    if (status == RUDA_CLI_CHECK_TARGET_INPUT_ERROR
        || status == RUDA_CLI_CHECK_TARGET_LIMIT_REACHED) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_CHECK_INPUT_IO,
            status == RUDA_CLI_CHECK_TARGET_LIMIT_REACHED
                ? "checker target loading exhausted a defensive path, tree, or source limit"
                : "cannot discover or read the selected checker target",
            RUDA_EXIT_SOURCE_FAILURE
        );
    }
    return RUDA_EXIT_INTERNAL;
}

static bool ruda_cli_check_render_frontend(
    const RudaCompilerResult *result,
    RudaDiagnosticStyle style,
    RudaWriter standard_error
) {
    size_t unit_index = 0;
    for (unit_index = 0; unit_index < result->units.length;
        unit_index += 1) {
        const RudaCompilerUnit *unit = ruda_compiler_result_unit(
            result, unit_index
        );
        size_t diagnostic_index = 0;
        if (unit == NULL) {
            return false;
        }
        for (diagnostic_index = 0;
            diagnostic_index < unit->lexed.diagnostics.length;
            diagnostic_index += 1) {
            const RudaLexDiagnostic *diagnostic = ruda_lex_diagnostic(
                &unit->lexed, diagnostic_index
            );
            if (diagnostic == NULL || !ruda_cli_check_render(
                    standard_error, style, diagnostic->code,
                    diagnostic->message, unit->snapshot,
                    diagnostic->start, diagnostic->end
                )) {
                return false;
            }
        }
        for (diagnostic_index = 0;
            diagnostic_index < unit->parsed.diagnostics.length;
            diagnostic_index += 1) {
            const RudaParseDiagnostic *diagnostic = ruda_parse_diagnostic(
                &unit->parsed, diagnostic_index
            );
            if (diagnostic == NULL || !ruda_cli_check_render(
                    standard_error, style, diagnostic->code,
                    diagnostic->message, unit->snapshot,
                    diagnostic->start, diagnostic->end
                )) {
                return false;
            }
        }
    }
    return true;
}

static bool ruda_cli_check_render_semantics(
    const RudaCompilerResult *result,
    RudaDiagnosticStyle style,
    RudaWriter standard_error
) {
    size_t index = 0;
    if (!result->checked.initialized) {
        return true;
    }
    for (index = 0; index < result->checked.diagnostics.length; index += 1) {
        const RudaCheckDiagnostic *diagnostic = ruda_check_diagnostic(
            &result->checked, index
        );
        if (diagnostic == NULL || !ruda_cli_check_render(
                standard_error, style, diagnostic->code,
                diagnostic->message, diagnostic->snapshot,
                diagnostic->start, diagnostic->end
            )) {
            return false;
        }
    }
    return true;
}

static int ruda_cli_check_compiler_failure(
    const RudaCliOptions *options,
    RudaCompilerStatus status,
    RudaWriter standard_error
) {
    if (status == RUDA_COMPILER_INCOMPLETE_LANGUAGE) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_COMPILER_INCOMPLETE,
            "compiler does not implement an accepted language construct",
            RUDA_EXIT_INTERNAL
        );
    }
    if (status == RUDA_COMPILER_LIMIT_REACHED) {
        return ruda_cli_plain_error(
            standard_error,
            options->diagnostic_style,
            RUDA_DIAGNOSTIC_CHECK_INPUT_IO,
            "checker exhausted a defensive frontend or semantic limit",
            RUDA_EXIT_SOURCE_FAILURE
        );
    }
    if (status == RUDA_COMPILER_CANCELLED) {
        return RUDA_EXIT_INTERRUPTED;
    }
    return RUDA_EXIT_INTERNAL;
}

int ruda_cli_check(
    RudaSession *session,
    const RudaCliOptions *options,
    RudaWriter standard_output,
    RudaWriter standard_error
) {
    RudaCliCheckTarget target;
    RudaCompilerResult result;
    RudaCliCheckTargetStatus target_status;
    RudaCompilerStatus compiler_status;
    int exit_code = RUDA_EXIT_INTERNAL;
    (void)standard_output;
    memset(&target, 0, sizeof(target));
    memset(&result, 0, sizeof(result));
    target_status = ruda_cli_check_target_build(session, options, &target);
    if (target_status != RUDA_CLI_CHECK_TARGET_OK) {
        exit_code = ruda_cli_check_target_error(
            options, &target, target_status, standard_error
        );
        goto cleanup;
    }
    compiler_status = target.module_wide
        ? ruda_compiler_service_check_module_graph(
            &target.service,
            target.compiler_packages.data,
            target.compiler_packages.length,
            &result
        )
        : ruda_compiler_service_check_graph(
            &target.service,
            target.compiler_packages.data,
            target.compiler_packages.length,
            (const char *)target.selected_import_path.data,
            target.selected_import_path.length,
            &result
        );
    if (compiler_status != RUDA_COMPILER_OK) {
        exit_code = ruda_cli_check_compiler_failure(
            options, compiler_status, standard_error
        );
        goto cleanup;
    }
    if (!ruda_cli_check_render_frontend(
            &result, options->diagnostic_style, standard_error
        ) || !ruda_cli_check_render_semantics(
            &result, options->diagnostic_style, standard_error
        )) {
        exit_code = RUDA_EXIT_INTERNAL;
        goto cleanup;
    }
    exit_code = ruda_compiler_result_has_errors(&result)
        ? RUDA_EXIT_SOURCE_FAILURE
        : RUDA_EXIT_SUCCESS;
cleanup:
    ruda_compiler_result_dispose(&result);
    ruda_cli_check_target_dispose(&target);
    return exit_code;
}
