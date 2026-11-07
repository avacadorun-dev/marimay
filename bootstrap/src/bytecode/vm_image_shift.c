#include "vm_image.h"
#include "vm_hot.h"

#include <string.h>

#include "mmay/base/checked_i64_fast.h"
#include "mmay/runtime/array_scalar_view.h"

typedef struct MarimayBytecodeShiftBudget {
    size_t steps;
    size_t remaining;
    size_t maximum;
    size_t *steps_output;
    size_t *remaining_output;
} MarimayBytecodeShiftBudget;

static MarimayBytecodeShiftBudget mmay_bytecode_shift_budget(
    size_t *steps, MarimayBytecodeRunOptions options
) {
    MarimayBytecodeShiftBudget budget = {*steps, SIZE_MAX,
        options.max_steps, steps, options.remaining_shared_steps};
    if (budget.remaining_output != NULL) {
        budget.remaining = *budget.remaining_output;
    }
    return budget;
}

static bool mmay_bytecode_shift_has_fuel(
    const MarimayBytecodeShiftBudget *budget, size_t required
) {
    return (budget->maximum == SIZE_MAX
            || (budget->steps <= budget->maximum
                && required <= budget->maximum - budget->steps))
        && budget->remaining >= required;
}

static void mmay_bytecode_shift_consume(
    MarimayBytecodeShiftBudget *budget, size_t count
) {
    if (budget->maximum != SIZE_MAX) budget->steps += count;
    if (budget->remaining_output != NULL) budget->remaining -= count;
}

static void mmay_bytecode_shift_commit(
    const MarimayBytecodeShiftBudget *budget
) {
    *budget->steps_output = budget->steps;
    if (budget->remaining_output != NULL) {
        *budget->remaining_output = budget->remaining;
    }
}

static bool mmay_bytecode_shift_compare(
    const MarimayBytecodeImageOperation *operation,
    const MarimayBytecodeValue *registers
) {
    int64_t left = registers[operation->second].as.i64;
    int64_t right = registers[operation->third].as.i64;
    if (operation->opcode == MMAY_OPCODE_I64_EQUAL) return left == right;
    if (operation->opcode == MMAY_OPCODE_I64_LESS_EQUAL) return left <= right;
    return left < right;
}

static bool mmay_bytecode_shift_same_reference(
    MarimayManagedReference left, MarimayManagedReference right
) {
    return left.heap_provenance == right.heap_provenance
        && left.slot == right.slot && left.generation == right.generation;
}

static bool mmay_bytecode_shift_get(const MarimayManagedArrayView *view,
    int64_t index, MarimayBytecodeValue *target
) {
    size_t length = 0;
    uint64_t bits = 0;
    MarimayManagedStatus status = view->descriptor->storage_kind
            == MMAY_MANAGED_ARRAY_STORAGE_RAW64
        ? mmay_managed_array_view_get_raw64_fast(view, index,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, &bits, &length)
        : mmay_managed_array_scalar_view_get_fast(view, index,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, &bits, &length);
    if (status != MMAY_MANAGED_OK) return false;
    if (target->type == MMAY_BYTECODE_TYPE_I64) {
        target->as.i64 = mmay_i64_from_twos_complement_fast(bits);
    } else if (target->type == MMAY_BYTECODE_TYPE_BOOL) {
        target->as.boolean = bits != 0;
    } else {
        target->as.integer_bits = bits;
    }
    return true;
}

static bool mmay_bytecode_shift_set(const MarimayManagedArrayView *view,
    int64_t index, const MarimayBytecodeValue *source
) {
    size_t length = 0;
    uint64_t bits = source->type == MMAY_BYTECODE_TYPE_I64
        ? (uint64_t)source->as.i64
        : source->type == MMAY_BYTECODE_TYPE_BOOL
            ? (source->as.boolean ? UINT64_C(1) : UINT64_C(0))
            : source->as.integer_bits;
    return (view->descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW64
        ? mmay_managed_array_view_set_raw64_fast(view, index,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, bits, &length)
        : mmay_managed_array_scalar_view_set_fast(view, index,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, bits, &length))
        == MMAY_MANAGED_OK;
}

static bool mmay_bytecode_shift_raw64_eligible(
    const MarimayManagedArrayView *view,
    const MarimayBytecodeImageOperation *head,
    const MarimayBytecodeImageOperation *read,
    const MarimayBytecodeImageOperation *shift,
    const MarimayBytecodeValue *registers
) {
    uint32_t loop = shift[8].first;
    uint32_t invariant[] = {
        head->second, read->third, read[1].second, read[3].second,
        shift[2].third, shift[7].third
    };
    uint32_t written[] = {
        head->first, read->first, read[1].first, read[2].first,
        read[3].first, shift[0].first, shift[1].first, shift[2].first,
        shift[3].first, shift[4].first, shift[6].first, shift[7].first
    };
    bool stable = true;
    for (size_t source = 0;
        stable && source < sizeof(invariant) / sizeof(invariant[0]);
        source += 1) {
        stable = invariant[source] != loop;
        for (size_t target = 0;
            stable && target < sizeof(written) / sizeof(written[0]);
            target += 1) {
            stable = invariant[source] != written[target];
        }
    }
    for (size_t target = 0;
        stable && target < sizeof(written) / sizeof(written[0]);
        target += 1) {
        stable = written[target] != loop;
        for (size_t other = target + 1;
            stable && other < sizeof(written) / sizeof(written[0]);
            other += 1) {
            stable = written[target] != written[other];
        }
    }
    return stable && view != NULL && view->object != NULL
        && view->descriptor != NULL
        && view->length != NULL && view->mutation_generation != NULL
        && view->storage != NULL && *view->storage != NULL
        && view->descriptor->storage_kind
            == MMAY_MANAGED_ARRAY_STORAGE_RAW64
        && view->descriptor->storage_stride == sizeof(uint64_t)
        && head->opcode == MMAY_OPCODE_I64_LESS && head->third == loop
        && read->opcode == MMAY_OPCODE_I64_SUBTRACT_PROVEN
        && read->second == loop && read[1].third == read->first
        && read[2].second == read[1].first
        && read[2].third == read[1].third
        && read[3].opcode == MMAY_OPCODE_I64_LESS
        && read[3].third == read[2].first
        && read[1].second == shift[1].second
        && read[1].second == shift[3].second
        && shift[0].second == loop
        && shift[1].third == shift[0].first
        && shift[2].second == loop
        && shift[3].third == shift[2].first
        && shift[4].second == shift[3].first
        && shift[4].third == shift[3].third
        && shift[5].first == shift[1].first
        && shift[5].second == shift[1].third
        && shift[5].third == shift[4].first
        && shift[6].second == shift[4].first
        && shift[7].second == loop
        && shift[8].second == shift[7].first
        && registers[loop].type == MMAY_BYTECODE_TYPE_I64
        && registers[read[2].first].type == MMAY_BYTECODE_TYPE_I64
        && registers[shift[4].first].type == MMAY_BYTECODE_TYPE_I64
        && registers[shift[5].third].type == MMAY_BYTECODE_TYPE_I64;
}

typedef struct MarimayBytecodeShiftRaw64Iteration {
    int64_t loop_before;
    int64_t read_index;
    int64_t read_value;
    int64_t source_index;
    int64_t source_value;
    int64_t loop_after;
    bool complete;
} MarimayBytecodeShiftRaw64Iteration;

static void mmay_bytecode_shift_raw64_publish(
    const MarimayBytecodeImageOperation *head,
    const MarimayBytecodeImageOperation *read,
    const MarimayBytecodeImageOperation *shift,
    MarimayBytecodeValue *registers,
    const MarimayBytecodeShiftRaw64Iteration *iteration
) {
    if (!iteration->complete) return;
    registers[head->first].as.boolean = true;
    registers[read->first].as.i64 = iteration->read_index;
    registers[read[1].first] = registers[read[1].second];
    registers[read[2].first].as.i64 = iteration->read_value;
    registers[read[3].first].as.boolean = true;
    registers[shift[0].first].as.i64 = iteration->loop_before;
    registers[shift[1].first] = registers[shift[1].second];
    registers[shift[2].first].as.i64 = iteration->source_index;
    registers[shift[3].first] = registers[shift[3].second];
    registers[shift[4].first].as.i64 = iteration->source_value;
    registers[shift[6].first].as.i64 = iteration->source_value;
    registers[shift[7].first].as.i64 = iteration->loop_after;
    registers[shift[8].first].as.i64 = iteration->loop_after;
}

static void mmay_bytecode_shift_raw64_loop(
    const MarimayBytecodeImageOperation *head,
    const MarimayBytecodeImageOperation *read,
    const MarimayBytecodeImageOperation *shift,
    MarimayBytecodeValue *registers, const MarimayManagedArrayView *view,
    MarimayBytecodeShiftBudget *budget
) {
    MarimayBytecodeShiftRaw64Iteration iteration = {0};
    size_t length = *view->length;
    unsigned char *storage = *view->storage;
    uint64_t generation = *view->mutation_generation;
    int64_t loop = registers[head->third].as.i64;
    int64_t bound = registers[head->second].as.i64;
    int64_t read_delta = registers[read->third].as.i64;
    int64_t key = registers[read[3].second].as.i64;
    int64_t source_delta = registers[shift[2].third].as.i64;
    int64_t loop_delta = registers[shift[7].third].as.i64;
    while (mmay_bytecode_shift_has_fuel(budget, 17) && bound < loop) {
        int64_t read_index = mmay_i64_subtract_proven_fast(
            loop, read_delta);
        if (read_index < 0 || (uint64_t)read_index >= (uint64_t)length) break;
        uint64_t bits = 0;
        memcpy(&bits, storage + (size_t)read_index * sizeof(bits),
            sizeof(bits));
        int64_t read_value = mmay_i64_from_twos_complement_fast(bits);
        if (!(key < read_value)) break;
        int64_t destination = loop;
        int64_t source = mmay_i64_subtract_proven_fast(loop, source_delta);
        if (destination < 0 || (uint64_t)destination >= (uint64_t)length
            || source < 0 || (uint64_t)source >= (uint64_t)length
            || generation == UINT64_MAX) break;
        memcpy(&bits, storage + (size_t)source * sizeof(bits), sizeof(bits));
        int64_t source_value = mmay_i64_from_twos_complement_fast(bits);
        memcpy(storage + (size_t)destination * sizeof(bits), &bits,
            sizeof(bits));
        generation += UINT64_C(1);
        int64_t loop_after = mmay_i64_subtract_proven_fast(loop, loop_delta);
        iteration = (MarimayBytecodeShiftRaw64Iteration){
            loop, read_index, read_value, source, source_value,
            loop_after, true
        };
        loop = loop_after;
        mmay_bytecode_shift_consume(budget, 17);
    }
    if (iteration.complete) *view->mutation_generation = generation;
    mmay_bytecode_shift_raw64_publish(
        head, read, shift, registers, &iteration);
}

MarimayBytecodeImageRunResult mmay_bytecode_run_image_array_shift_loop(
    const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, size_t *pc, size_t *steps,
    MarimayBytecodeRunOptions options
) {
    MarimayBytecodeImageRunResult result =
        MMAY_BYTECODE_IMAGE_RUN_CONTINUE;
    MarimayBytecodeShiftBudget budget =
        mmay_bytecode_shift_budget(steps, options);
    const MarimayBytecodeImageOperation *head = &image->operations[*pc];
    const MarimayBytecodeImageOperation *head_branch = head + 1;
    const MarimayBytecodeImageOperation *read =
        &image->operations[head_branch->second];
    const MarimayBytecodeImageOperation *read_branch = read + 4;
    const MarimayBytecodeImageOperation *shift =
        &image->operations[read_branch->second];
    MarimayManagedArrayView view = {0};
    bool view_ready = false;
    if (execution == NULL) {
        result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
        goto done;
    }
    if (mmay_bytecode_shift_has_fuel(&budget, 17)
        && mmay_bytecode_shift_compare(head, registers)) {
        const MarimayBytecodeImageOperation *require = read + 1;
        int64_t read_index = mmay_i64_subtract_proven_fast(
            registers[read->second].as.i64,
            registers[read->third].as.i64);
        uint32_t layout = ((const uint32_t *)
            function->register_layouts.data)[require->second];
        const MarimayManagedArrayDescriptor *descriptor =
            mmay_bytecode_array_descriptor_cached(execution, function,
                registers[require->second].type, layout);
        size_t length = 0;
        if (descriptor != NULL && mmay_managed_array_view_bind(
                &execution->managed_heap,
                registers[require->second].as.managed_reference,
                descriptor, &view) == MMAY_MANAGED_OK) {
            view_ready = true;
            if (mmay_managed_array_view_require_fast(
                    &view, read_index, &length) == MMAY_MANAGED_OK
                && mmay_bytecode_shift_raw64_eligible(
                    &view, head, read, shift, registers)) {
                mmay_bytecode_shift_raw64_loop(
                    head, read, shift, registers, &view, &budget);
            }
        }
    }
    while (mmay_bytecode_shift_has_fuel(&budget, 17)) {
        bool condition = mmay_bytecode_shift_compare(head, registers);
        registers[head->first].as.boolean = condition;
        if (!condition) {
            mmay_bytecode_shift_consume(&budget, 2);
            *pc = head_branch->third;
            goto done;
        }
        registers[read->first].as.i64 = mmay_i64_subtract_proven_fast(
            registers[read->second].as.i64,
            registers[read->third].as.i64);
        const MarimayBytecodeImageOperation *require = read + 1;
        int64_t read_index = registers[require->third].as.i64;
        if (view_ready && !mmay_bytecode_shift_same_reference(view.reference,
                registers[require->second].as.managed_reference)) {
            view_ready = false;
        }
        if (!view_ready) {
            uint32_t layout = ((const uint32_t *)
                function->register_layouts.data)[require->second];
            const MarimayManagedArrayDescriptor *descriptor =
                mmay_bytecode_array_descriptor_cached(execution, function,
                    registers[require->second].type, layout);
            if (descriptor == NULL || mmay_managed_array_view_bind(
                    &execution->managed_heap,
                    registers[require->second].as.managed_reference,
                    descriptor, &view) != MMAY_MANAGED_OK) {
                mmay_bytecode_shift_consume(&budget, 3);
                *pc = head_branch->second + 1;
                result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
                goto done;
            }
            view_ready = true;
        }
        size_t length = 0;
        if (mmay_managed_array_view_require_fast(&view, read_index, &length)
            != MMAY_MANAGED_OK) {
            mmay_bytecode_shift_consume(&budget, 3);
            *pc = head_branch->second + 1;
            result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
            goto done;
        }
        registers[require->first] = registers[require->second];
        const MarimayBytecodeImageOperation *get = read + 2;
        if (!mmay_bytecode_shift_get(&view, read_index,
                &registers[get->first])) {
            mmay_bytecode_shift_consume(&budget, 4);
            *pc = head_branch->second + 2;
            result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
            goto done;
        }
        condition = mmay_bytecode_shift_compare(read + 3, registers);
        registers[read[3].first].as.boolean = condition;
        if (!condition) {
            mmay_bytecode_shift_consume(&budget, 7);
            *pc = read_branch->third;
            goto done;
        }
        registers[shift[0].first] = registers[shift[0].second];
        int64_t destination = registers[shift[1].third].as.i64;
        if (mmay_managed_array_view_require_fast(&view, destination, &length)
            != MMAY_MANAGED_OK) {
            mmay_bytecode_shift_consume(&budget, 8);
            *pc = read_branch->second + 1;
            result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
            goto done;
        }
        registers[shift[1].first] = registers[shift[1].second];
        registers[shift[2].first].as.i64 = mmay_i64_subtract_proven_fast(
            registers[shift[2].second].as.i64,
            registers[shift[2].third].as.i64);
        int64_t source_index = registers[shift[3].third].as.i64;
        if (mmay_managed_array_view_require_fast(&view, source_index, &length)
            != MMAY_MANAGED_OK) {
            mmay_bytecode_shift_consume(&budget, 10);
            *pc = read_branch->second + 3;
            result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
            goto done;
        }
        registers[shift[3].first] = registers[shift[3].second];
        if (!mmay_bytecode_shift_get(&view, source_index,
                &registers[shift[4].first])) {
            mmay_bytecode_shift_consume(&budget, 11);
            *pc = read_branch->second + 4;
            result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
            goto done;
        }
        if (!mmay_bytecode_shift_set(&view, destination,
                &registers[shift[5].third])) {
            mmay_bytecode_shift_consume(&budget, 12);
            *pc = read_branch->second + 5;
            result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
            goto done;
        }
        registers[shift[6].first] = registers[shift[6].second];
        registers[shift[7].first].as.i64 = mmay_i64_subtract_proven_fast(
            registers[shift[7].second].as.i64,
            registers[shift[7].third].as.i64);
        registers[shift[8].first] = registers[shift[8].second];
        mmay_bytecode_shift_consume(&budget, 17);
    }
done:
    mmay_bytecode_shift_commit(&budget);
    return result;
}
