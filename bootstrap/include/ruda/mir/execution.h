#ifndef RUDA_MIR_EXECUTION_H
#define RUDA_MIR_EXECUTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"
#include "ruda/runtime/test_output.h"
#include "ruda/types/type.h"

/*
 * Context-bound handle into a RudaMirExecution object graph. Execution
 * provenance rejects foreign and stale handles; slot stores a kind-tagged
 * arena index and is never a host pointer.
 */
typedef struct RudaMirReference {
    uint64_t execution_provenance;
    uint32_t slot;
} RudaMirReference;

#define RUDA_MIR_INVALID_REFERENCE \
    ((RudaMirReference){UINT64_MAX, UINT32_MAX})

typedef struct RudaMirValue {
    RudaTypeId type;
    union {
        int64_t i64;
        uint64_t integer_bits;
        uint64_t f64_bits;
        bool boolean;
        RudaMirReference reference;
    } as;
} RudaMirValue;

typedef struct RudaMirExecutionOptions {
    size_t max_objects;
    size_t max_fields_per_object;
    size_t max_total_fields;
    size_t max_strings;
    size_t max_total_string_bytes;
    size_t max_arrays;
    size_t max_array_length;
    size_t max_total_array_elements;
} RudaMirExecutionOptions;

typedef struct RudaMirExecution {
    RudaAllocator allocator;
    /* Oracle-owned record descriptors, retained until explicit disposal. */
    RudaVector objects;
    /* Oracle-owned immutable enum descriptors, indexed separately by type. */
    RudaVector enums;
    /* Oracle-owned immutable UTF-8 strings. */
    RudaVector strings;
    /* Oracle-owned mutable arrays, indexed separately by exact type. */
    RudaVector arrays;
    /* Oracle-owned immutable interface payloads and selected method tables. */
    RudaVector interface_values;
    RudaTestOutput test_output;
    RudaMirExecutionOptions options;
    size_t total_fields;
    size_t total_string_bytes;
    size_t total_array_elements;
    uint64_t provenance;
    bool initialized;
} RudaMirExecution;

RudaMirExecutionOptions ruda_mir_execution_default_options(void);
bool ruda_mir_reference_is_invalid(RudaMirReference reference);
bool ruda_mir_reference_equal(
    RudaMirReference left,
    RudaMirReference right
);
bool ruda_mir_execution_init(
    RudaMirExecution *execution,
    RudaAllocator allocator,
    RudaMirExecutionOptions options
);
/* Invalidates every RudaMirReference owned by this execution. */
void ruda_mir_execution_dispose(RudaMirExecution *execution);
/* Internal oracle inspection; no corresponding safe-source tag operation. */
bool ruda_mir_execution_inspect_enum(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t *variant,
    size_t *payload_count
);
bool ruda_mir_execution_enum_payload(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t index,
    RudaMirValue *result
);
bool ruda_mir_execution_string_bytes(
    const RudaMirExecution *execution,
    RudaMirValue value,
    const unsigned char **bytes,
    size_t *length
);
bool ruda_mir_execution_array_length(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t *length
);
bool ruda_mir_execution_array_element(
    const RudaMirExecution *execution,
    RudaMirValue value,
    size_t index,
    RudaMirValue *result
);
/* Internal oracle inspection by allocation order. */
bool ruda_mir_execution_array_at(
    const RudaMirExecution *execution,
    size_t ordinal,
    RudaMirValue *result
);
/* Internal boundary validation for context-bound managed handles. */
bool ruda_mir_execution_owns_reference(
    const RudaMirExecution *execution,
    RudaMirValue value
);

#endif
