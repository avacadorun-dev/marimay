#include "cli_standard_packages_internal.h"

#include "mmay_std_json_source.h"

MarimayCliCheckTargetStatus mmay_cli_standard_json_package_add(
    MarimayCliCheckTarget *target
) {
    const MarimayCliStandardSource source = {
        "std/json", "std/json/json.my",
        mmay_cli_std_json_model_source,
        sizeof(mmay_cli_std_json_model_source),
        "std/json/decode.my", mmay_cli_std_json_decode_source,
        sizeof(mmay_cli_std_json_decode_source),
        "std/json/encode.my", mmay_cli_std_json_encode_source,
        sizeof(mmay_cli_std_json_encode_source)
    };
    return mmay_cli_standard_package_add(target, &source);
}
