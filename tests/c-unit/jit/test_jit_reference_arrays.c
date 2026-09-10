#include "vm_jit_internal.h"
#include "vm_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPECT(c) do { if (!(c)) { fprintf(stderr, "JIT reference array: %s at %d\n", #c, __LINE__); exit(1); } } while (0)

typedef struct Fixture {
    MarimayBytecodeProgram program;
    bool record, proven;
    size_t first_read, cold;
} Fixture;

static void append(MarimayBytecodeFunction *f, uint32_t block, MarimayOpcode op,
    uint64_t a, uint64_t b, uint64_t c) {
    MarimayBytecodeInstruction instruction = {.opcode=op,
        .span={.start=f->instruction_count + 100, .end=f->instruction_count + 101}};
    instruction.operands[0].unsigned_value = a;
    instruction.operands[1].unsigned_value = b;
    instruction.operands[2].unsigned_value = c;
    if (op == MMAY_OPCODE_CONST_I64) instruction.operands[1].signed_value = (int64_t)b;
    EXPECT(mmay_bytecode_append_instruction(f, block, &instruction));
}

static void read_element(Fixture *fixture, MarimayBytecodeFunction *f) {
    if (fixture->proven) append(f, 1, MMAY_OPCODE_ARRAY_REQUIRE_INDEX, 5, 0, 1);
    append(f, 1, fixture->proven ? MMAY_OPCODE_ARRAY_GET_IN_BOUNDS : MMAY_OPCODE_ARRAY_GET,
        2, fixture->proven ? 5 : 0, 1);
}

static void measure(Fixture *fixture, MarimayBytecodeFunction *f, uint32_t target) {
    append(f, 1, fixture->record ? MMAY_OPCODE_RECORD_GET : MMAY_OPCODE_ARRAY_LENGTH,
        target, 2, 0);
}

static void fixture_init(Fixture *fixture, bool record, bool proven) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->record = record; fixture->proven = proven;
    MarimayAllocator a = mmay_system_allocator();
    MarimayBytecodeType parent = record ? MMAY_BYTECODE_TYPE_ARRAY : MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64;
    MarimayBytecodeType element = record ? MMAY_BYTECODE_TYPE_RECORD : MMAY_BYTECODE_TYPE_ARRAY_F64;
    MarimayBytecodeType params[] = {parent, MMAY_BYTECODE_TYPE_I64};
    MarimayBytecodeType locals[] = {element, MMAY_BYTECODE_TYPE_I64, MMAY_BYTECODE_TYPE_BOOL,
        parent, MMAY_BYTECODE_TYPE_I64};
    MarimayBytecodeFunction f = {0};
    uint32_t added, parent_layout = MMAY_BYTECODE_NO_LAYOUT;
    EXPECT(mmay_bytecode_program_init(&fixture->program, a, 1, mmay_bytecode_program_default_options()));
    EXPECT(mmay_bytecode_function_init(&f, a, MMAY_BYTECODE_TYPE_I64, 1, params, 2,
        mmay_bytecode_function_default_options()));
    for (size_t i = 0; i < 5; ++i) EXPECT(mmay_bytecode_add_register(&f, locals[i], &added));
    if (record) {
        MarimayBytecodeRecordField field = {.type=MMAY_BYTECODE_TYPE_I64,
            .layout=MMAY_BYTECODE_NO_LAYOUT, .is_mutable=true};
        EXPECT(mmay_bytecode_add_record_layout(&f, (MarimaySymbolId){{0x21}}, &field, 1, &added));
        EXPECT(added == 0 && mmay_bytecode_set_register_layout(&f, 2, added));
        EXPECT(mmay_bytecode_add_array_type(&f, element, added, &parent_layout));
        EXPECT(mmay_bytecode_set_register_layout(&f, 0, parent_layout));
        EXPECT(mmay_bytecode_set_register_layout(&f, 5, parent_layout));
    }
    EXPECT(mmay_bytecode_add_block(&f, &added) && added == 0);
    EXPECT(mmay_bytecode_add_block(&f, &added) && added == 1);
    append(&f, 0, MMAY_OPCODE_CONST_BOOL, 4, 0, 0);
    append(&f, 0, MMAY_OPCODE_BRANCH_BOOL, 4, 0, 1);
    fixture->first_read = proven ? 3 : 2;
    read_element(fixture, &f);
    measure(fixture, &f, 3);
    append(&f, 1, MMAY_OPCODE_CONST_I64, 1, 1, 0);
    read_element(fixture, &f);
    measure(fixture, &f, 6);
    append(&f, 1, MMAY_OPCODE_I64_ADD_CHECKED, 3, 3, 6);
    fixture->cold = f.instruction_count - 2;
    MarimayBytecodeArrayConstructionId construction;
    EXPECT(mmay_bytecode_add_array_construction_with_layout(&f, parent, parent_layout,
        NULL, 0, &construction));
    append(&f, 1, MMAY_OPCODE_ARRAY_NEW, 0, construction, 0);
    append(&f, 1, MMAY_OPCODE_COPY, 5, 0, 0);
    /* The second allocation collects with the original parent no longer rooted.
     * The selected child survives solely through its exact VM register root. */
    append(&f, 1, MMAY_OPCODE_ARRAY_NEW, 0, construction, 0);
    measure(fixture, &f, 6);
    append(&f, 1, MMAY_OPCODE_I64_ADD_CHECKED, 3, 3, 6);
    append(&f, 1, MMAY_OPCODE_RETURN, 3, 0, 0);
    MarimaySymbolId symbol = {{0x22}};
    EXPECT(mmay_bytecode_program_take_function(&fixture->program, symbol, &f, &added));
    EXPECT(mmay_bytecode_program_set_entry(&fixture->program, symbol));
    EXPECT(mmay_bytecode_program_verify(&fixture->program) == MMAY_BYTECODE_PROGRAM_VERIFY_OK);
}

static const MarimayBytecodeFunction *body(const Fixture *f) {
    return &mmay_bytecode_program_function(&f->program, 0)->body;
}

static void objects(const Fixture *f, MarimayBytecodeExecution *execution,
    MarimayAllocator a, MarimayManagedReference refs[3]) {
    EXPECT(mmay_bytecode_execution_init(execution, a, mmay_bytecode_execution_default_options()));
    MarimayManagedReference *locations[] = {&refs[0], &refs[1], &refs[2]};
    MarimayManagedRootFrame roots = {0};
    for (size_t i = 0; i < 3; ++i) refs[i] = mmay_managed_invalid_reference();
    EXPECT(mmay_managed_root_push(&execution->managed_heap, &roots, locations, 3) == MMAY_MANAGED_OK);
    for (size_t i = 0; i < 2; ++i) {
        MarimayManagedCell cells[2] = {{.f64_bits=(i + 1) * 10}, {.f64_bits=7}};
        if (f->record) {
            const void *d = mmay_bytecode_nominal_reference_descriptor(execution, body(f), MMAY_BYTECODE_TYPE_RECORD, 0);
            EXPECT(d != NULL && mmay_managed_record_new(&execution->managed_heap, d, cells, 1, &refs[i]) == MMAY_MANAGED_OK);
        } else EXPECT(mmay_managed_array_new(&execution->managed_heap,
            &mmay_managed_array_f64_descriptor, cells, i + 1, &refs[i]) == MMAY_MANAGED_OK);
    }
    const MarimayManagedArrayDescriptor *d = mmay_bytecode_array_descriptor_for(execution,
        body(f), f->record ? MMAY_BYTECODE_TYPE_ARRAY : MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64,
        f->record ? 0 : MMAY_BYTECODE_NO_LAYOUT);
    MarimayManagedCell cells[] = {{.reference=refs[0]}, {.reference=refs[1]}};
    EXPECT(d != NULL && mmay_managed_array_new(&execution->managed_heap, d, cells, 2, &refs[2]) == MMAY_MANAGED_OK);
    EXPECT(mmay_managed_root_pop(&execution->managed_heap, &roots) == MMAY_MANAGED_OK);
    execution->managed_heap.options.collect_before_allocation = true;
}

typedef struct Observation {
    MarimayBytecodeRunStatus status;
    MarimayBytecodeValue result;
    MarimayBytecodeTrap trap;
    MarimayBytecodeSpan panic_span;
    size_t remaining, attempts;
    bool bounds;
} Observation;

static Observation run(const Fixture *f, bool jit, int64_t index, size_t fuel,
    bool shared, size_t failure) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator a = mmay_fail_allocator(&failing);
    MarimayBytecodeExecution execution = {0};
    MarimayManagedReference refs[3];
    objects(f, &execution, a, refs);
    MarimayBytecodeValue args[] = {{.type=f->record ? MMAY_BYTECODE_TYPE_ARRAY : MMAY_BYTECODE_TYPE_ARRAY_ARRAY_F64,
        .as.managed_reference=refs[2]}, {.type=MMAY_BYTECODE_TYPE_I64, .as.i64=index}};
    MarimayBytecodePanic panic = {0};
    EXPECT(mmay_bytecode_panic_init(&panic, a));
    Observation o = {.result={.type=MMAY_BYTECODE_TYPE_I64, .as.i64=1234}, .remaining=fuel};
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.execution = &execution; options.trap = &o.trap; options.panic = &panic;
    options.max_steps = shared ? SIZE_MAX : fuel;
    options.remaining_shared_steps = shared ? &o.remaining : NULL;
    failing.attempts = 0; failing.failure_index = failure;
    o.status = jit ? mmay_bytecode_run_program(&f->program, args, 2, a, options, &o.result)
        : mmay_bytecode_run(body(f), args, 2, a, options, &o.result);
    o.panic_span = panic.span;
    o.bounds = panic.code != NULL && strcmp(panic.code, "G4002") == 0;
    o.attempts = failing.attempts;
    if (failure < failing.attempts) EXPECT(o.status == MMAY_BYTECODE_RUN_ALLOCATION_FAILED && o.result.as.i64 == 1234);
    EXPECT(execution.managed_heap.roots == NULL);
    mmay_bytecode_panic_dispose(&panic);
    mmay_bytecode_execution_dispose(&execution);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    return o;
}

static void compare(const Fixture *f, int64_t index, size_t fuel, bool shared) {
    Observation a = run(f, false, index, fuel, shared, SIZE_MAX);
    Observation b = run(f, true, index, fuel, shared, SIZE_MAX);
    EXPECT(a.status == b.status && a.result.as.i64 == b.result.as.i64 && a.remaining == b.remaining);
    EXPECT(a.trap.has_span == b.trap.has_span && a.bounds == b.bounds);
    if (a.trap.has_span) EXPECT(a.trap.span.start == b.trap.span.start && a.trap.span.end == b.trap.span.end);
    if (a.bounds) EXPECT(a.panic_span.start == b.panic_span.start && a.panic_span.end == b.panic_span.end);
    if (a.status == MMAY_BYTECODE_RUN_OK)
        EXPECT(a.result.as.i64 == (index == 0 ? 5 : 6) * (f->record ? 10 : 1));
}

static void native(const Fixture *f) {
    MarimayFailAllocator failing;
    mmay_fail_allocator_init(&failing, mmay_system_allocator(), SIZE_MAX);
    MarimayAllocator a = mmay_fail_allocator(&failing);
    MarimayCodeMemoryFault pages = {.failure_index=SIZE_MAX};
    MarimayBytecodeExecution execution = {0};
    MarimayManagedReference refs[3];
    objects(f, &execution, a, refs);
    MarimayBytecodeExecutionImage image = {0};
    MarimayBytecodeJit jit = {0};
    EXPECT(mmay_bytecode_execution_image_init(&image, &f->program, a) == MMAY_BYTECODE_IMAGE_OK);
    EXPECT(mmay_bytecode_jit_init(&jit, &f->program, &image, a, &pages, true) == MMAY_BYTECODE_RUN_OK);
    EXPECT(jit.ready && jit.functions[0].code != NULL && jit.functions[0].plan[f->first_read].supported);
    MarimayBytecodeValue regs[7] = {0};
    const MarimayBytecodeType *types = body(f)->register_types.data;
    for (size_t i = 0; i < 7; ++i) {
        regs[i].type = types[i];
        if (mmay_bytecode_type_is_managed(types[i])) regs[i].as.managed_reference = mmay_managed_invalid_reference();
    }
    regs[0].as.managed_reference = refs[2];
    MarimayManagedReference *locations[] = {&regs[0].as.managed_reference,
        &regs[2].as.managed_reference, &regs[5].as.managed_reference};
    MarimayManagedRootFrame roots = {0};
    EXPECT(mmay_managed_root_push(&execution.managed_heap, &roots, locations, 3) == MMAY_MANAGED_OK);
    MarimayBytecodeRunOptions options = mmay_bytecode_run_default_options();
    options.max_steps = SIZE_MAX; options.execution = &execution;
    for (size_t trial = 0; trial < 4; ++trial) {
        size_t block = 1, pc = 0, steps = 0;
        regs[1].as.i64 = 0;
        if (trial == 1) {
            EXPECT(mmay_managed_array_reserve(&execution.managed_heap, refs[2], 64) == MMAY_MANAGED_OK);
            size_t length;
            EXPECT(mmay_managed_array_set(&execution.managed_heap, refs[2], 1,
                (MarimayManagedCell){.reference=refs[0]}, &length) == MMAY_MANAGED_OK);
        }
        if (trial == 2) regs[0].as.managed_reference.heap_provenance += 1;
        if (trial == 3) { regs[0].as.managed_reference = refs[2]; regs[0].as.managed_reference.generation += 1; }
        size_t attempts = failing.attempts;
        failing.failure_index = attempts;
        EXPECT(mmay_bytecode_jit_run(&jit, 0, &image.functions[0], body(f), regs,
            &execution, &block, &pc, &steps, options));
        EXPECT(failing.attempts == attempts && block == 1);
        failing.failure_index = SIZE_MAX;
        EXPECT(pc == (trial < 2 ? f->cold : 0));
        if (!mmay_managed_reference_equal(regs[2].as.managed_reference, refs[trial == 0 ? 1 : 0]))
            fprintf(stderr, "record=%d proven=%d trial=%zu pc=%zu got=%llu/%u/%u expected=%llu/%u/%u\n",
                f->record, f->proven, trial, pc,
                (unsigned long long)regs[2].as.managed_reference.heap_provenance,
                regs[2].as.managed_reference.slot, regs[2].as.managed_reference.generation,
                (unsigned long long)refs[trial == 0 ? 1 : 0].heap_provenance,
                refs[trial == 0 ? 1 : 0].slot, refs[trial == 0 ? 1 : 0].generation);
        EXPECT(mmay_managed_reference_equal(regs[2].as.managed_reference, refs[trial == 0 ? 1 : 0]));
    }
    regs[0].as.managed_reference = mmay_managed_invalid_reference();
    regs[5].as.managed_reference = mmay_managed_invalid_reference();
    EXPECT(mmay_managed_heap_collect(&execution.managed_heap) == MMAY_MANAGED_OK);
    MarimayManagedObjectKind kind;
    EXPECT(mmay_managed_object_kind(&execution.managed_heap, refs[0], &kind) == MMAY_MANAGED_OK);
    EXPECT(mmay_managed_object_kind(&execution.managed_heap, refs[2], &kind) == MMAY_MANAGED_INVALID_REFERENCE);
    EXPECT(mmay_managed_root_pop(&execution.managed_heap, &roots) == MMAY_MANAGED_OK);
    mmay_bytecode_jit_dispose(&jit);
    mmay_bytecode_execution_image_dispose(&image);
    mmay_bytecode_execution_dispose(&execution);
    EXPECT(failing.live_allocations == 0 && failing.live_bytes == 0);
    EXPECT(pages.live_mappings == 0 && pages.live_bytes == 0);
}

int main(void) {
    if (!mmay_code_memory_supported()) return 77;
    for (size_t kind = 0; kind < 2; ++kind) for (size_t proven = 0; proven < 2; ++proven) {
        Fixture f;
        fixture_init(&f, kind != 0, proven != 0);
        native(&f);
        const int64_t indices[] = {INT64_MIN, -1, 0, 1, 2, INT64_MAX};
        for (size_t i = 0; i < 6; ++i) for (size_t shared = 0; shared < 2; ++shared) {
            compare(&f, indices[i], SIZE_MAX, shared != 0);
            for (size_t fuel = 1; fuel <= 20; ++fuel) compare(&f, indices[i], fuel, shared != 0);
        }
        for (size_t meter = 0; meter < 2; ++meter) for (size_t bounds = 0; bounds < 2; ++bounds) {
            size_t count = run(&f, true, bounds ? -1 : 0, meter ? 40 : SIZE_MAX, false, SIZE_MAX).attempts;
            for (size_t i = 0; i <= count; ++i) run(&f, true, bounds ? -1 : 0, meter ? 40 : SIZE_MAX, false, i);
        }
        mmay_bytecode_program_dispose(&f.program);
    }
    puts("JIT reference arrays: native checked/proven reads, full identities, rebinding, growth, GC roots, stale/foreign handles, cold fuel/panic differentials and all-index failures: passed");
    return 0;
}
