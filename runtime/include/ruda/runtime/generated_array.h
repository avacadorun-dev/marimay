#ifndef RUDA_RUNTIME_GENERATED_ARRAY_H
#define RUDA_RUNTIME_GENERATED_ARRAY_H

#include "ruda/runtime/array_access.h"

typedef struct RudaManagedArrayGeneratedChildren
    RudaManagedArrayGeneratedChildren;

/* Exact non-moving array object layout reserved for compiler-generated C. */
typedef struct RudaManagedArray {
    const RudaManagedArrayDescriptor *descriptor;
    size_t length;
    size_t capacity;
    uint64_t mutation_generation;
    unsigned char *storage;
    RudaManagedArrayGeneratedChildren *generated_children;
} RudaManagedArray;

typedef struct RudaManagedArrayGeneratedChild {
    void *object;
    RudaManagedArray array_snapshot;
} RudaManagedArrayGeneratedChild;

struct RudaManagedArrayGeneratedChildren {
    size_t capacity;
    size_t count;
    uint64_t generation;
    RudaManagedArrayGeneratedChild entries[];
};

typedef struct RudaManagedArrayGeneratedView {
    RudaManagedArray *object;
} RudaManagedArrayGeneratedView;

RudaManagedStatus ruda_managed_array_generated_cache_children(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    RudaManagedArrayGeneratedView *view
);

#if defined(__clang__) || defined(__GNUC__)
#define RUDA_MANAGED_G_INLINE static inline __attribute__((always_inline))
#else
#define RUDA_MANAGED_G_INLINE static inline
#endif

#include "ruda/runtime/generated_array_scalar.h"

RUDA_MANAGED_G_INLINE bool ruda_managed_generated_reference_equal(
    RudaManagedReference left,
    RudaManagedReference right
) {
    return left.heap_provenance == right.heap_provenance
        && left.slot == right.slot && left.generation == right.generation;
}

RUDA_MANAGED_G_INLINE RudaManagedArrayView ruda_managed_array_generated_expand(
    const RudaManagedArrayGeneratedView *view,
    RudaManagedReference reference
) {
    RudaManagedArrayView checked = {0};
    if (view != NULL && view->object != NULL) {
        checked.reference = reference;
        checked.descriptor = view->object->descriptor;
        checked.object = view->object;
        checked.length = &view->object->length;
        checked.capacity = &view->object->capacity;
        checked.mutation_generation = &view->object->mutation_generation;
        checked.storage = &view->object->storage;
    }
    return checked;
}

RUDA_MANAGED_G_INLINE RudaManagedStatus
ruda_managed_array_generated_push_raw64(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayGeneratedView *view,
    uint64_t bits
) {
    RudaManagedArray *array = view != NULL ? view->object : NULL;
    if (array != NULL && array->length < array->capacity
        && array->mutation_generation != UINT64_MAX) {
        memcpy(array->storage + array->length * sizeof(bits),
            &bits, sizeof(bits));
        array->length += 1;
        array->mutation_generation += UINT64_C(1);
        return RUDA_MANAGED_OK;
    }
    {
        RudaManagedArrayView checked =
            ruda_managed_array_generated_expand(view, reference);
        RudaManagedCell cell;
        cell.f64_bits = bits;
        return ruda_managed_array_view_push(heap, &checked, cell);
    }
}

RUDA_MANAGED_G_INLINE RudaManagedStatus
ruda_managed_array_generated_push_raw64_untracked(
    RudaManagedHeap *heap,
    RudaManagedReference reference,
    const RudaManagedArrayGeneratedView *view,
    uint64_t bits
) {
    RudaManagedArray *array = view != NULL ? view->object : NULL;
    if (array != NULL && array->length < array->capacity) {
        memcpy(array->storage + array->length * sizeof(bits),
            &bits, sizeof(bits));
        array->length += 1;
        return RUDA_MANAGED_OK;
    }
    return ruda_managed_array_generated_push_raw64(
        heap, reference, view, bits
    );
}

RUDA_MANAGED_G_INLINE RudaManagedStatus
ruda_managed_array_generated_pop_raw64(
    const RudaManagedArrayGeneratedView *view,
    RudaManagedCell *result,
    bool *popped
) {
    RudaManagedArray *array = view != NULL ? view->object : NULL;
    if (result != NULL) result->f64_bits = 0;
    if (popped != NULL) *popped = false;
    if (array == NULL || result == NULL || popped == NULL) {
        return RUDA_MANAGED_INVALID_ARGUMENT;
    }
    if (array->length == 0) return RUDA_MANAGED_OK;
    if (array->mutation_generation == UINT64_MAX) {
        return RUDA_MANAGED_LIMIT_REACHED;
    }
    array->length -= 1;
    memcpy(&result->f64_bits,
        array->storage + array->length * sizeof(result->f64_bits),
        sizeof(result->f64_bits));
    memset(array->storage + array->length * sizeof(result->f64_bits), 0,
        sizeof(result->f64_bits));
    array->mutation_generation += UINT64_C(1);
    *popped = true;
    return RUDA_MANAGED_OK;
}

#undef RUDA_MANAGED_G_INLINE

#endif
