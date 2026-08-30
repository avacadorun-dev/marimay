#include "cli_standard_packages_internal.h"

#include "mmay_std_cli_source.h"

MarimayCliCheckTargetStatus mmay_cli_standard_cli_package_add(
    MarimayCliCheckTarget *target
) {
    const MarimayCliStandardSource source = {
        "std/cli", "std/cli/cli.my",
        mmay_cli_std_cli_model_source,
        sizeof(mmay_cli_std_cli_model_source),
        "std/cli/parse.my", mmay_cli_std_cli_parse_source,
        sizeof(mmay_cli_std_cli_parse_source),
        "std/cli/help.my", mmay_cli_std_cli_help_source,
        sizeof(mmay_cli_std_cli_help_source)
    };
    return mmay_cli_standard_package_add(target, &source);
}
