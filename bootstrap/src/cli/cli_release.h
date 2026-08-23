#ifndef MMAY_CLI_RELEASE_H
#define MMAY_CLI_RELEASE_H

#include <stdbool.h>

#include "mmay/base/writer.h"

typedef struct MarimayCliTaskHelp {
    const char *name;
    const char *summary;
    const char *usage;
    const char *example;
    const char *owner;
    bool implemented;
} MarimayCliTaskHelp;

const MarimayCliTaskHelp *mmay_cli_release_find_task(const char *name);
const char *mmay_cli_release_suggest_task(const char *name);
bool mmay_cli_release_write_top_help(MarimayWriter writer);
bool mmay_cli_release_write_task_help(
    MarimayWriter writer,
    const MarimayCliTaskHelp *task
);
bool mmay_cli_release_write_version(MarimayWriter writer, bool json);

#endif
