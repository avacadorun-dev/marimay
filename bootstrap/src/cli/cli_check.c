#include "cli_compile_report.h"

#include <string.h>

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
        exit_code = ruda_cli_compile_target_error(
            options, &target, target_status, standard_error
        );
        goto cleanup;
    }
    compiler_status = target.module_wide
        ? ruda_compiler_service_check_module_graph(
            &target.service, target.compiler_packages.data,
            target.compiler_packages.length, &result
        ) : ruda_compiler_service_check_graph(
            &target.service, target.compiler_packages.data,
            target.compiler_packages.length,
            (const char *)target.selected_import_path.data,
            target.selected_import_path.length, &result
        );
    if (compiler_status != RUDA_COMPILER_OK) {
        exit_code = ruda_cli_compile_status_error(
            options, compiler_status, standard_error
        );
        goto cleanup;
    }
    exit_code = ruda_cli_compile_render_result(
        options, &result, standard_error
    );
cleanup:
    ruda_compiler_result_dispose(&result);
    ruda_cli_check_target_dispose(&target);
    return exit_code;
}
