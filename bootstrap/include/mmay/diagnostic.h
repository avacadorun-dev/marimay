#ifndef MMAY_DIAGNOSTIC_H
#define MMAY_DIAGNOSTIC_H

#include <stdbool.h>
#include <stddef.h>

#include "mmay/base/writer.h"
#include "mmay/source/source_manager.h"

typedef enum MarimayDiagnosticStyle {
    MMAY_DIAGNOSTIC_HUMAN,
    MMAY_DIAGNOSTIC_SHORT,
    MMAY_DIAGNOSTIC_JSON
} MarimayDiagnosticStyle;

typedef enum MarimaySeverity {
    MMAY_SEVERITY_ERROR,
    MMAY_SEVERITY_WARNING,
    MMAY_SEVERITY_INFORMATION,
    MMAY_SEVERITY_HELP
} MarimaySeverity;

typedef enum MarimayRuntimeResourceClass {
    MMAY_RUNTIME_RESOURCE_NONE,
    MMAY_RUNTIME_RESOURCE_MANAGED_HEAP_BUDGET,
    MMAY_RUNTIME_RESOURCE_HOST_ALLOCATION
} MarimayRuntimeResourceClass;

typedef struct MarimayDiagnostic {
    const char *code;
    MarimaySeverity severity;
    const char *message;
    size_t message_length;
    bool message_is_counted;
    bool has_command_argument;
    size_t command_argument;
    MarimayRuntimeResourceClass runtime_resource;
    size_t resource_live_bytes;
    size_t resource_requested_bytes;
    size_t resource_limit_bytes;
    const MarimaySourceSnapshot *source;
    size_t source_start;
    size_t source_end;
} MarimayDiagnostic;

/*
 * Diagnostic strings are borrowed for the duration of this call. The writer
 * owns its destination. False means the destination rejected a write.
 */
bool mmay_diagnostic_render(
    MarimayWriter writer,
    MarimayDiagnosticStyle style,
    const MarimayDiagnostic *diagnostic
);

#endif
