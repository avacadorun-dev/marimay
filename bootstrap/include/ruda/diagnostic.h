#ifndef RUDA_DIAGNOSTIC_H
#define RUDA_DIAGNOSTIC_H

#include <stdbool.h>
#include <stddef.h>

#include "ruda/base/writer.h"
#include "ruda/source/source_manager.h"

typedef enum RudaDiagnosticStyle {
    RUDA_DIAGNOSTIC_HUMAN,
    RUDA_DIAGNOSTIC_SHORT,
    RUDA_DIAGNOSTIC_JSON
} RudaDiagnosticStyle;

typedef enum RudaSeverity {
    RUDA_SEVERITY_ERROR,
    RUDA_SEVERITY_WARNING,
    RUDA_SEVERITY_INFORMATION,
    RUDA_SEVERITY_HELP
} RudaSeverity;

typedef enum RudaRuntimeResourceClass {
    RUDA_RUNTIME_RESOURCE_NONE,
    RUDA_RUNTIME_RESOURCE_MANAGED_HEAP_BUDGET,
    RUDA_RUNTIME_RESOURCE_HOST_ALLOCATION
} RudaRuntimeResourceClass;

typedef struct RudaDiagnostic {
    const char *code;
    RudaSeverity severity;
    const char *message;
    size_t message_length;
    bool message_is_counted;
    bool has_command_argument;
    size_t command_argument;
    RudaRuntimeResourceClass runtime_resource;
    size_t resource_live_bytes;
    size_t resource_requested_bytes;
    size_t resource_limit_bytes;
    const RudaSourceSnapshot *source;
    size_t source_start;
    size_t source_end;
} RudaDiagnostic;

/*
 * Diagnostic strings are borrowed for the duration of this call. The writer
 * owns its destination. False means the destination rejected a write.
 */
bool ruda_diagnostic_render(
    RudaWriter writer,
    RudaDiagnosticStyle style,
    const RudaDiagnostic *diagnostic
);

#endif
