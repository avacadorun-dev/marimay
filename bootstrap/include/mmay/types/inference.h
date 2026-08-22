#ifndef MMAY_TYPES_INFERENCE_H
#define MMAY_TYPES_INFERENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mmay/base/allocator.h"
#include "mmay/base/vector.h"
#include "mmay/types/model.h"

typedef struct MarimayInferenceVariable {
    MarimayInferenceId parent;
    uint32_t rank;
    MarimayTypeId binding;
} MarimayInferenceVariable;

typedef struct MarimayInferenceContext {
    MarimayAllocator allocator;
    MarimayVector variables;
    size_t max_variables;
    bool initialized;
} MarimayInferenceContext;

bool mmay_inference_context_init(
    MarimayInferenceContext *context,
    MarimayAllocator allocator,
    size_t max_variables
);
void mmay_inference_context_dispose(MarimayInferenceContext *context);
bool mmay_inference_new(MarimayInferenceContext *context, MarimayInferenceId *result);
bool mmay_inference_unify_variables(
    MarimayInferenceContext *context,
    MarimayInferenceId left,
    MarimayInferenceId right
);
bool mmay_inference_bind(
    MarimayInferenceContext *context,
    MarimayInferenceId variable,
    MarimayTypeId type
);
MarimayTypeId mmay_inference_resolve(
    MarimayInferenceContext *context,
    MarimayInferenceId variable
);

#endif
