#ifndef MMAY_RUNTIME_GENERATED_ARRAY_H
#define MMAY_RUNTIME_GENERATED_ARRAY_H

#include "mmay/runtime/array_access.h"

typedef struct MarimayManagedArrayGeneratedChildren
    MarimayManagedArrayGeneratedChildren;

/* Exact non-moving array object layout reserved for compiler-generated C. */
typedef struct MarimayManagedArray {
    const MarimayManagedArrayDescriptor *descriptor;
    size_t length;
    size_t capacity;
    uint64_t mutation_generation;
    unsigned char *storage;
    MarimayManagedArrayGeneratedChildren *generated_children;
} MarimayManagedArray;

typedef struct MarimayManagedArrayGeneratedChild {
    void *object;
    MarimayManagedArray array_snapshot;
} MarimayManagedArrayGeneratedChild;

struct MarimayManagedArrayGeneratedChildren {
    size_t capacity;
    size_t count;
    uint64_t generation;
    MarimayManagedArrayGeneratedChild entries[];
};

typedef struct MarimayManagedArrayGeneratedView {
    MarimayManagedArray *object;
} MarimayManagedArrayGeneratedView;

MarimayManagedStatus mmay_managed_array_generated_cache_children(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    MarimayManagedArrayGeneratedView *view
);

#if defined(__clang__) || defined(__GNUC__)
#define MMAY_MANAGED_G_INLINE static inline __attribute__((always_inline))
#else
#define MMAY_MANAGED_G_INLINE static inline
#endif

#include "mmay/runtime/generated_array_scalar.h"

MMAY_MANAGED_G_INLINE bool mmay_managed_generated_reference_equal(
    MarimayManagedReference left,
    MarimayManagedReference right
) {
    return left.heap_provenance == right.heap_provenance
        && left.slot == right.slot && left.generation == right.generation;
}

MMAY_MANAGED_G_INLINE MarimayManagedArrayView mmay_managed_array_generated_expand(
    const MarimayManagedArrayGeneratedView *view,
    MarimayManagedReference reference
) {
    MarimayManagedArrayView checked = {0};
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

MMAY_MANAGED_G_INLINE MarimayManagedStatus
mmay_managed_array_generated_push_raw64(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayGeneratedView *view,
    uint64_t bits
) {
    MarimayManagedArray *array = view != NULL ? view->object : NULL;
    if (array != NULL && array->length < array->capacity
        && array->mutation_generation != UINT64_MAX) {
        memcpy(array->storage + array->length * sizeof(bits),
            &bits, sizeof(bits));
        array->length += 1;
        array->mutation_generation += UINT64_C(1);
        return MMAY_MANAGED_OK;
    }
    {
        MarimayManagedArrayView checked =
            mmay_managed_array_generated_expand(view, reference);
        MarimayManagedCell cell;
        cell.f64_bits = bits;
        return mmay_managed_array_view_push(heap, &checked, cell);
    }
}

MMAY_MANAGED_G_INLINE MarimayManagedStatus
mmay_managed_array_generated_push_raw64_untracked(
    MarimayManagedHeap *heap,
    MarimayManagedReference reference,
    const MarimayManagedArrayGeneratedView *view,
    uint64_t bits
) {
    MarimayManagedArray *array = view != NULL ? view->object : NULL;
    if (array != NULL && array->length < array->capacity) {
        memcpy(array->storage + array->length * sizeof(bits),
            &bits, sizeof(bits));
        array->length += 1;
        return MMAY_MANAGED_OK;
    }
    return mmay_managed_array_generated_push_raw64(
        heap, reference, view, bits
    );
}

MMAY_MANAGED_G_INLINE MarimayManagedStatus
mmay_managed_array_generated_pop_raw64(
    const MarimayManagedArrayGeneratedView *view,
    MarimayManagedCell *result,
    bool *popped
) {
    MarimayManagedArray *array = view != NULL ? view->object : NULL;
    if (result != NULL) result->f64_bits = 0;
    if (popped != NULL) *popped = false;
    if (array == NULL || result == NULL || popped == NULL) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (array->length == 0) return MMAY_MANAGED_OK;
    if (array->mutation_generation == UINT64_MAX) {
        return MMAY_MANAGED_LIMIT_REACHED;
    }
    array->length -= 1;
    memcpy(&result->f64_bits,
        array->storage + array->length * sizeof(result->f64_bits),
        sizeof(result->f64_bits));
    memset(array->storage + array->length * sizeof(result->f64_bits), 0,
        sizeof(result->f64_bits));
    array->mutation_generation += UINT64_C(1);
    *popped = true;
    return MMAY_MANAGED_OK;
}

#undef MMAY_MANAGED_G_INLINE

#endif
