#ifndef RUDA_CHECK_SERVICE_CHECK_H
#define RUDA_CHECK_SERVICE_CHECK_H

#include "ruda/check/service_types.h"

/*
 * Compiles current snapshots in canonical exact-path order. Lexical, parse,
 * and type errors are owned diagnostics with RUDA_COMPILER_OK. A frontend
 * error suppresses AST publication and package checking. Unsupported accepted
 * language remains RUDA_COMPILER_INCOMPLETE_LANGUAGE. The result borrows
 * snapshots from service and must be disposed before service. result is
 * atomically replaced only on success.
 */
RudaCompilerStatus ruda_compiler_service_check(
    const RudaCompilerService *service,
    const char *package_name,
    RudaCompilerResult *result
);

/*
 * Request-local incremental check. previous is optional and may equal result.
 * Cancellation is polled between bounded phases; callbacks must not mutate
 * service/result. A phase failure takes precedence over later cancellation.
 * Stats are replaced only on success. The semantic package is fully rechecked
 * even when a file's parse tree is reused.
 */
RudaCompilerStatus ruda_compiler_service_check_request(
    const RudaCompilerService *service,
    const char *package_name,
    const RudaCompilerCheckRequest *request,
    RudaCompilerResult *result
);
bool ruda_compiler_result_is_current(
    const RudaCompilerService *service,
    const RudaCompilerResult *result
);
/* Borrowed until result is successfully replaced or disposed. */
const RudaCompilerUnit *ruda_compiler_result_unit(
    const RudaCompilerResult *result,
    size_t index
);
bool ruda_compiler_result_has_errors(const RudaCompilerResult *result);
void ruda_compiler_result_dispose(RudaCompilerResult *result);

#endif
