#ifndef MMAY_CHECK_SERVICE_CHECK_H
#define MMAY_CHECK_SERVICE_CHECK_H

#include "mmay/check/service_types.h"

/*
 * Compiles current snapshots in canonical exact-path order. Lexical, parse,
 * and type errors are owned diagnostics with MMAY_COMPILER_OK. A frontend
 * error suppresses AST publication and package checking. Unsupported accepted
 * language remains MMAY_COMPILER_INCOMPLETE_LANGUAGE and atomically publishes
 * a diagnostic-only result containing the exact I0001 source span and
 * construct. The result borrows snapshots from service and must be disposed
 * before service. Other failures leave result unchanged.
 */
MarimayCompilerStatus mmay_compiler_service_check(
    const MarimayCompilerService *service,
    const char *package_name,
    MarimayCompilerResult *result
);

/*
 * Request-local incremental check. previous is optional and may equal result.
 * Cancellation is polled between bounded phases; callbacks must not mutate
 * service/result. A phase failure takes precedence over later cancellation.
 * Stats are replaced only on success. The semantic package is fully rechecked
 * even when a file's parse tree is reused.
 */
MarimayCompilerStatus mmay_compiler_service_check_request(
    const MarimayCompilerService *service,
    const char *package_name,
    const MarimayCompilerCheckRequest *request,
    MarimayCompilerResult *result
);

/*
 * Checks an exact caller-supplied local package graph over current service
 * documents. Every document must appear exactly once. Import paths are copied
 * into result facts by the checker; this API performs no filesystem access.
 */
MarimayCompilerStatus mmay_compiler_service_check_graph_request(
    const MarimayCompilerService *service,
    const MarimayCompilerPackage *packages,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    const MarimayCompilerCheckRequest *request,
    MarimayCompilerResult *result
);
MarimayCompilerStatus mmay_compiler_service_check_graph(
    const MarimayCompilerService *service,
    const MarimayCompilerPackage *packages,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    MarimayCompilerResult *result
);
/* Checks all packages in one exact module graph, including disconnected roots. */
MarimayCompilerStatus mmay_compiler_service_check_module_graph(
    const MarimayCompilerService *service,
    const MarimayCompilerPackage *packages,
    size_t package_count,
    MarimayCompilerResult *result
);
bool mmay_compiler_result_is_current(
    const MarimayCompilerService *service,
    const MarimayCompilerResult *result
);
/* Borrowed until result is successfully replaced or disposed. */
const MarimayCompilerUnit *mmay_compiler_result_unit(
    const MarimayCompilerResult *result,
    size_t index
);
bool mmay_compiler_result_has_errors(const MarimayCompilerResult *result);
void mmay_compiler_result_dispose(MarimayCompilerResult *result);

#endif
