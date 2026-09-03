#include "cli_compile_report.h"
#include "cli_native_build.h"
#include "cli_run_report.h"

#include <string.h>

#include "mmay/check/entry.h"
#include "mmay/generated/diagnostic_codes.h"

int mmay_cli_build_program(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayCliCheckTarget target;
    MarimayCompilerResult compiler;
    MarimayCheckedEntry entry;
    MarimayCliCheckTargetStatus target_status;
    MarimayCompilerStatus compiler_status;
    MarimayCheckEntryStatus entry_status;
    MarimayNativeBuildResult build;
    int exit_code = MMAY_EXIT_INTERNAL;
    memset(&target, 0, sizeof(target));
    memset(&compiler, 0, sizeof(compiler));
    memset(&entry, 0, sizeof(entry));
    memset(&build, 0, sizeof(build));
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
        exit_code = mmay_cli_plain_error(
            standard_error, options->diagnostic_style,
            MMAY_DIAGNOSTIC_NATIVE_EMIT_FAILED,
            "canonical typed MIR range proof was invalid",
            MMAY_EXIT_INTERNAL
        );
        goto cleanup;
    }
    entry_status = mmay_check_select_entry(
        &compiler.checked, (const char *)target.selected_import_path.data,
        target.selected_import_path.length, &entry
    );
    if (entry_status != MMAY_CHECK_ENTRY_OK) {
        exit_code = mmay_cli_run_entry_error(
            options, entry_status, standard_error
        );
        goto cleanup;
    }
    exit_code = mmay_cli_native_build_checked(
        session, options, &target, &compiler, &entry,
        true, standard_output, standard_error, &build
    );
cleanup:
    mmay_native_build_result_dispose(&build);
    mmay_compiler_result_dispose(&compiler);
    mmay_cli_check_target_dispose(&target);
    return exit_code;
}
