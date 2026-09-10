#include "vm_jit_internal.h"
#include "vm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(condition) do { if (!(condition)) { fprintf(stderr, "JIT retained view failed: %s at %d\n", #condition, __LINE__); exit(1); } } while (0)

typedef struct Fixture {
    MarimayFailAllocator failing;
    MarimayAllocator allocator;
    MarimayBytecodeExecution execution;
    MarimayBytecodeFunction functions[2];
    MarimayBytecodeValue value;
    MarimayManagedReference other;
    MarimayManagedReference *locations[2];
    MarimayManagedRootFrame roots;
    MarimayBytecodeJitBinding binding;
    MarimayBytecodeJitContext context;
    const MarimayManagedRecordDescriptor *record_descriptor;
    bool record;
} Fixture;

static void new_object(Fixture *f, MarimayManagedReference *reference) {
    MarimayManagedCell cell = {.f64_bits=42};
    if (f->record) {
        EXPECT(mmay_managed_record_new(&f->execution.managed_heap,
            f->record_descriptor, &cell, 1, reference) == MMAY_MANAGED_OK);
    } else {
        EXPECT(mmay_managed_array_new(&f->execution.managed_heap,
            &mmay_managed_array_f64_descriptor, &cell, 1, reference) == MMAY_MANAGED_OK);
    }
}

static void objects(Fixture *f) {
    EXPECT(mmay_bytecode_execution_init(&f->execution, f->allocator,
        mmay_bytecode_execution_default_options()));
    EXPECT(f->execution.managed_heap.view_epoch == 1);
    if (f->record) {
        f->record_descriptor = mmay_bytecode_nominal_reference_descriptor(
            &f->execution, &f->functions[0], MMAY_BYTECODE_TYPE_RECORD, 0);
        EXPECT(f->record_descriptor != NULL);
    }
    new_object(f, &f->value.as.managed_reference);
    new_object(f, &f->other);
    f->locations[0] = &f->value.as.managed_reference;
    f->locations[1] = &f->other;
    EXPECT(mmay_managed_root_push(&f->execution.managed_heap, &f->roots,
        f->locations, 2) == MMAY_MANAGED_OK);
}

static void init(Fixture *f, bool record) {
    memset(f, 0, sizeof(*f));
    f->record = record;
    mmay_fail_allocator_init(&f->failing, mmay_system_allocator(), SIZE_MAX);
    f->allocator = mmay_fail_allocator(&f->failing);
    f->value.type = record ? MMAY_BYTECODE_TYPE_RECORD : MMAY_BYTECODE_TYPE_ARRAY_F64;
    for (size_t i = 0; i < 2; ++i) {
        MarimayBytecodeFunction *function = &f->functions[i];
        uint32_t reg;
        EXPECT(mmay_bytecode_function_init(function, f->allocator,
            MMAY_BYTECODE_TYPE_VOID, 1, NULL, 0, mmay_bytecode_function_default_options()));
        EXPECT(mmay_bytecode_add_register(function, f->value.type, &reg) && reg == 0);
        if (record) {
            MarimayBytecodeRecordField field = {.type=MMAY_BYTECODE_TYPE_I64,
                .layout=MMAY_BYTECODE_NO_LAYOUT, .is_mutable=true};
            MarimaySymbolId symbol = {{0x10}};
            uint32_t layout;
            EXPECT(mmay_bytecode_add_record_layout(function, symbol, &field, 1, &layout));
            EXPECT(layout == 0 && mmay_bytecode_set_register_layout(function, reg, layout));
        }
    }
    objects(f);
    f->context = (MarimayBytecodeJitContext){.registers=&f->value,
        .bindings=&f->binding, .function=&f->functions[0], .execution=&f->execution};
}

static void dispose(Fixture *f) {
    EXPECT(mmay_managed_root_pop(&f->execution.managed_heap, &f->roots) == MMAY_MANAGED_OK);
    mmay_bytecode_execution_dispose(&f->execution);
    for (size_t i = 0; i < 2; ++i) mmay_bytecode_function_dispose(&f->functions[i]);
    EXPECT(f->failing.live_allocations == 0 && f->failing.live_bytes == 0);
}

static MarimayManagedReference cached(Fixture *f) {
    return f->record ? f->execution.managed_heap.cached_record_reference
        : f->execution.managed_heap.cached_array_reference;
}

/* A persistent hit must leave the unrelated runtime cache entry untouched.
 * This proves the fast path without adding counters to production hot code. */
static void prime_other(Fixture *f) {
    if (f->record) {
        MarimayManagedRecordView view;
        EXPECT(mmay_managed_record_view_bind(&f->execution.managed_heap, f->other,
            f->record_descriptor, &view) == MMAY_MANAGED_OK);
    } else {
        MarimayManagedArrayView view;
        EXPECT(mmay_managed_array_view_bind(&f->execution.managed_heap, f->other,
            &mmay_managed_array_f64_descriptor, &view) == MMAY_MANAGED_OK);
    }
    EXPECT(mmay_managed_reference_equal(cached(f), f->other));
}

static void *bind(Fixture *f) {
    f->context.epoch += 1;
    return mmay_bytecode_jit_bind(&f->context, 0);
}

static void expect_binding(Fixture *f, bool reused) {
    prime_other(f);
    size_t attempts = f->failing.attempts;
    f->failing.failure_index = attempts;
    EXPECT(bind(f) == &f->binding.view);
    EXPECT(f->binding.epoch == f->context.epoch);
    EXPECT(mmay_managed_reference_equal(cached(f),
        reused ? f->other : f->value.as.managed_reference));
    EXPECT(f->failing.attempts == attempts);
    f->failing.failure_index = SIZE_MAX;
}

static void lifetime(bool record) {
    Fixture f;
    init(&f, record);
    expect_binding(&f, false);
    expect_binding(&f, true);
    MarimayManagedHeap *heap = &f.execution.managed_heap;
    MarimayManagedCell observed;
    if (record) {
        MarimayManagedCell changed = {.f64_bits=99};
        EXPECT(mmay_managed_record_set(heap, f.value.as.managed_reference,
            0, changed) == MMAY_MANAGED_OK);
        expect_binding(&f, true);
        EXPECT(f.binding.view.record.fields[0].f64_bits == 99);
    } else {
        uint64_t epoch = heap->view_epoch;
        EXPECT(mmay_managed_array_reserve(heap, f.value.as.managed_reference, 128) == MMAY_MANAGED_OK);
        EXPECT(mmay_managed_array_push(heap, f.value.as.managed_reference,
            (MarimayManagedCell){.f64_bits=99}) == MMAY_MANAGED_OK);
        EXPECT(heap->view_epoch == epoch);
        expect_binding(&f, true);
        EXPECT(*f.binding.view.array.length == 2 && *f.binding.view.array.capacity >= 129);
        size_t length;
        EXPECT(mmay_managed_array_view_get(&f.binding.view.array, 1,
            MMAY_MANAGED_ARRAY_ACCESS_CHECKED, &observed, &length) == MMAY_MANAGED_OK);
        EXPECT(observed.f64_bits == 99 && length == 2);
    }
    f.context.function = &f.functions[1];
    expect_binding(&f, false);
    expect_binding(&f, true);
    f.context.function = &f.functions[0];
    expect_binding(&f, false);

    /* A failed collection still expires the proof before tracing roots. */
    MarimayManagedReference saved = f.value.as.managed_reference;
    f.value.as.managed_reference.heap_provenance = heap->provenance == UINT64_MAX
        ? 1 : heap->provenance + 1;
    uint64_t epoch = heap->view_epoch;
    EXPECT(mmay_managed_heap_collect(heap) == MMAY_MANAGED_INVALID_REFERENCE);
    EXPECT(heap->view_epoch == epoch + 1 && !heap->collecting);
    f.value.as.managed_reference = saved;
    expect_binding(&f, false);
    expect_binding(&f, true);
    epoch = heap->view_epoch;
    EXPECT(mmay_managed_heap_collect(heap) == MMAY_MANAGED_OK);
    EXPECT(heap->view_epoch == epoch + 1);
    expect_binding(&f, false);

    /* A stale reference cannot alias the next occupant of the same slot. */
    saved = f.value.as.managed_reference;
    f.value.as.managed_reference = mmay_managed_invalid_reference();
    EXPECT(mmay_managed_heap_collect(heap) == MMAY_MANAGED_OK);
    f.value.as.managed_reference = saved;
    EXPECT(bind(&f) == NULL && f.binding.function == NULL && f.binding.epoch == 0);
    f.value.as.managed_reference = mmay_managed_invalid_reference();
    new_object(&f, &f.value.as.managed_reference);
    EXPECT(f.value.as.managed_reference.slot == saved.slot);
    EXPECT(f.value.as.managed_reference.generation != saved.generation);
    expect_binding(&f, false);

    /* A different value type cannot reuse an already checked object view. */
    MarimayBytecodeType type = f.value.type;
    f.value.type = record ? MMAY_BYTECODE_TYPE_ARRAY_F64 : MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64;
    EXPECT(bind(&f) == NULL && f.binding.function == NULL);
    f.value.type = type;
    expect_binding(&f, false);

    /* Saturation is sticky, and disables reuse even between ordinary entries. */
    heap->view_epoch = UINT64_MAX - 1;
    expect_binding(&f, false);
    EXPECT(mmay_managed_heap_collect(heap) == MMAY_MANAGED_OK && heap->view_epoch == UINT64_MAX);
    expect_binding(&f, false);
    expect_binding(&f, false);
    EXPECT(mmay_managed_heap_collect(heap) == MMAY_MANAGED_OK && heap->view_epoch == UINT64_MAX);
    expect_binding(&f, false);
    dispose(&f);
}

static void provenance(bool record) {
    Fixture f, other;
    init(&f, record);
    init(&other, record);
    expect_binding(&f, false);
    MarimayManagedReference saved = f.value.as.managed_reference;
    f.context.execution = &other.execution;
    EXPECT(bind(&f) == NULL && f.binding.function == NULL);
    f.context.execution = &f.execution;
    expect_binding(&f, false);
    dispose(&other);

    /* Same execution address, function/layout, epoch, slot and generation;
     * only fresh heap provenance distinguishes the new lifetime. */
    uint64_t old_epoch = f.execution.managed_heap.view_epoch;
    EXPECT(mmay_managed_root_pop(&f.execution.managed_heap, &f.roots) == MMAY_MANAGED_OK);
    mmay_bytecode_execution_dispose(&f.execution);
    objects(&f);
    EXPECT(f.execution.managed_heap.view_epoch == old_epoch);
    EXPECT(f.value.as.managed_reference.slot == saved.slot);
    EXPECT(f.value.as.managed_reference.generation == saved.generation);
    EXPECT(f.value.as.managed_reference.heap_provenance != saved.heap_provenance);
    expect_binding(&f, false);
    expect_binding(&f, true);
    MarimayManagedReference current = f.value.as.managed_reference;
    f.value.as.managed_reference = saved;
    EXPECT(bind(&f) == NULL && f.binding.function == NULL);
    f.value.as.managed_reference = current;
    dispose(&f);
}

int main(void) {
    for (size_t kind = 0; kind < 2; ++kind) {
        lifetime(kind != 0);
        provenance(kind != 0);
    }
    puts("JIT retained array/record views: proved cache hits, mutation/growth, successful/failed GC, callee changes, stale slot reuse, wrong heap, fresh provenance, sticky saturation and zero allocation counters: passed");
    return 0;
}
