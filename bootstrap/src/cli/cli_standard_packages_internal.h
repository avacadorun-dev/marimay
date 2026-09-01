#ifndef MMAY_CLI_STANDARD_PACKAGES_INTERNAL_H
#define MMAY_CLI_STANDARD_PACKAGES_INTERNAL_H

#include "cli_standard_packages.h"

typedef struct MarimayCliStandardSource {
    const char *import_path;
    const char *source_path;
    const unsigned char *source;
    size_t source_length;
    const char *tail_path;
    const unsigned char *tail;
    size_t tail_length;
    const char *third_path;
    const unsigned char *third;
    size_t third_length;
} MarimayCliStandardSource;

MarimayCliCheckTargetStatus mmay_cli_standard_package_add(
    MarimayCliCheckTarget *target,
    const MarimayCliStandardSource *standard
);
MarimayCliCheckTargetStatus mmay_cli_standard_context_packages_add(
    MarimayCliCheckTarget *target
);
MarimayCliCheckTargetStatus mmay_cli_standard_cli_package_add(
    MarimayCliCheckTarget *target
);
MarimayCliCheckTargetStatus mmay_cli_standard_json_package_add(
    MarimayCliCheckTarget *target
);

#endif
