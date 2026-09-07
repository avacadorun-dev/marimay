#include "../test_support.h"

#include "../../../runtime/common/managed_reference_internal.h"

static void mmay_test_reference_object_kinds(void) {
    MarimayManagedArray array = {0};
    MarimayManagedString string = {0};
    MarimayManagedRecord record = {0};
    MarimayManagedEnum enumeration = {0};
    MarimayManagedInterface iface = {0};
    MarimayManagedMap map = {0};
    MarimayManagedSlot occupied[] = {
        {.kind = MMAY_MANAGED_OBJECT_ARRAY, .as.array = &array, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_STRING, .as.string = &string, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_RECORD, .as.record = &record, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_ENUM, .as.enumeration = &enumeration, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_INTERFACE, .as.iface = &iface, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_MAP, .as.map = &map, .generation = 31}
    };
    MarimayManagedSlot empty[] = {
        {.kind = MMAY_MANAGED_OBJECT_ARRAY, .as.array = NULL, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_STRING, .as.string = NULL, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_RECORD, .as.record = NULL, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_ENUM, .as.enumeration = NULL, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_INTERFACE, .as.iface = NULL, .generation = 31},
        {.kind = MMAY_MANAGED_OBJECT_MAP, .as.map = NULL, .generation = 31}
    };
    MarimayManagedHeap heap = {0};
    MarimayManagedReference reference = {7, 0, 31};
    size_t index = 0;
    heap.initialized = true;
    heap.provenance = reference.heap_provenance;
    heap.slot_count = sizeof(occupied) / sizeof(occupied[0]);
    MMAY_EXPECT(!mmay_managed_slot_occupied(NULL));
    for (index = 0; index < heap.slot_count; index += 1) {
        reference.slot = (uint32_t)index;
        heap.slots = occupied;
        MMAY_EXPECT(mmay_managed_slot_occupied(&occupied[index]));
        MMAY_EXPECT(mmay_managed_resolve_const(&heap, reference) == &occupied[index]);
        MMAY_EXPECT(mmay_managed_resolve(&heap, reference) == &occupied[index]);
        heap.slots = empty;
        MMAY_EXPECT(!mmay_managed_slot_occupied(&empty[index]));
        MMAY_EXPECT(mmay_managed_resolve_const(&heap, reference) == NULL);
        MMAY_EXPECT(mmay_managed_resolve(&heap, reference) == NULL);
    }
    /* Synthetic caller-owned metadata only; no object is allocated or freed. */
}

static void mmay_expect_reference_rejected(
    MarimayManagedHeap *heap, MarimayManagedReference reference
) {
    MMAY_EXPECT(mmay_managed_resolve_const(heap, reference) == NULL);
    MMAY_EXPECT(mmay_managed_resolve(heap, reference) == NULL);
}

static void mmay_test_reference_identity_boundaries(void) {
    MarimayManagedMap map = {0};
    MarimayManagedSlot slot = {
        .kind = MMAY_MANAGED_OBJECT_MAP, .as.map = &map, .generation = 19
    };
    MarimayManagedHeap heap = {0};
    MarimayManagedReference reference = {7, 0, 19};
    heap.initialized = true;
    heap.provenance = reference.heap_provenance;
    heap.slot_count = 1;
    heap.slots = &slot;
    MMAY_EXPECT(mmay_managed_heap_valid(&heap));
    MMAY_EXPECT(mmay_managed_resolve(&heap, reference) == &slot);
    MMAY_EXPECT(!mmay_managed_heap_valid(NULL));
    mmay_expect_reference_rejected(NULL, reference);
    heap.initialized = false;
    MMAY_EXPECT(!mmay_managed_heap_valid(&heap));
    mmay_expect_reference_rejected(&heap, reference);
    heap.initialized = true;
    heap.provenance = 0;
    MMAY_EXPECT(!mmay_managed_heap_valid(&heap));
    mmay_expect_reference_rejected(&heap, reference);
    heap.provenance = UINT64_MAX;
    MMAY_EXPECT(!mmay_managed_heap_valid(&heap));
    mmay_expect_reference_rejected(&heap, reference);
    heap.provenance = 8;
    mmay_expect_reference_rejected(&heap, reference);
    heap.provenance = reference.heap_provenance;
    reference.slot = 1;
    mmay_expect_reference_rejected(&heap, reference);
    reference.slot = UINT32_MAX;
    mmay_expect_reference_rejected(&heap, reference);
    reference.slot = 0;
    heap.slot_count = 0;
    mmay_expect_reference_rejected(&heap, reference);
    heap.slot_count = 1;
    reference.generation = 18;
    mmay_expect_reference_rejected(&heap, reference);
    reference.generation = 19;
    slot.kind = MMAY_MANAGED_OBJECT_INVALID;
    mmay_expect_reference_rejected(&heap, reference);
    slot.kind = (MarimayManagedObjectKind)99;
    mmay_expect_reference_rejected(&heap, reference);
    slot.kind = MMAY_MANAGED_OBJECT_MAP;
    slot.generation = UINT32_MAX;
    mmay_expect_reference_rejected(&heap, reference);
    reference.generation = UINT32_MAX;
    MMAY_EXPECT(mmay_managed_resolve(&heap, reference) == &slot);
    MMAY_EXPECT(mmay_managed_resolve_const(&heap, reference) == &slot);
}

void mmay_test_managed_reference_validation(void) {
    mmay_test_reference_object_kinds();
    mmay_test_reference_identity_boundaries();
}
