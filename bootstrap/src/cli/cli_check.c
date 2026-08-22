#include "cli_compile_report.h"

#include <string.h>

int mmay_cli_check(
    MarimaySession *session,
    const MarimayCliOptions *options,
    MarimayWriter standard_output,
    MarimayWriter standard_error
) {
    MarimayCliCheckTarget target;
    MarimayCompilerResult result;
    MarimayCliCheckTargetStatus target_status;
    MarimayCompilerStatus compiler_status;
    int exit_code = MMAY_EXIT_INTERNAL;
    (void)standard_output;
    memset(&target, 0, sizeof(target));
    memset(&result, 0, sizeof(result));
    target_status = mmay_cli_check_target_build(session, options, &target);
    if (target_status != MMAY_CLI_CHECK_TARGET_OK) {
        exit_code = mmay_cli_compile_target_error(
            options, &target, target_status, standard_error
        );
        goto cleanup;
    }
    compiler_status = target.module_wide
        ? mmay_compiler_service_check_module_graph(
            &target.service, target.compiler_packages.data,
            target.compiler_packages.length, &result
        ) : mmay_compiler_service_check_graph(
            &target.service, target.compiler_packages.data,
            target.compiler_packages.length,
            (const char *)target.selected_import_path.data,
            target.selected_import_path.length, &result
        );
    if (compiler_status != MMAY_COMPILER_OK) {
        exit_code = mmay_cli_compile_status_error(
            options, compiler_status, standard_error
        );
        goto cleanup;
    }
    exit_code = mmay_cli_compile_render_result(
        options, &result, standard_error
    );
cleanup:
    mmay_compiler_result_dispose(&result);
    mmay_cli_check_target_dispose(&target);
    return exit_code;
}
