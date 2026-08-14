#ifndef RUDA_TYPES_INFERENCE_H
#define RUDA_TYPES_INFERENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ruda/base/allocator.h"
#include "ruda/base/vector.h"
#include "ruda/types/model.h"

typedef struct RudaInferenceVariable {
    RudaInferenceId parent;
    uint32_t rank;
    RudaTypeId binding;
} RudaInferenceVariable;

typedef struct RudaInferenceContext {
    RudaAllocator allocator;
    RudaVector variables;
    size_t max_variables;
    bool initialized;
} RudaInferenceContext;

bool ruda_inference_context_init(
    RudaInferenceContext *context,
    RudaAllocator allocator,
    size_t max_variables
);
void ruda_inference_context_dispose(RudaInferenceContext *context);
bool ruda_inference_new(RudaInferenceContext *context, RudaInferenceId *result);
bool ruda_inference_unify_variables(
    RudaInferenceContext *context,
    RudaInferenceId left,
    RudaInferenceId right
);
bool ruda_inference_bind(
    RudaInferenceContext *context,
    RudaInferenceId variable,
    RudaTypeId type
);
RudaTypeId ruda_inference_resolve(
    RudaInferenceContext *context,
    RudaInferenceId variable
);

#endif
