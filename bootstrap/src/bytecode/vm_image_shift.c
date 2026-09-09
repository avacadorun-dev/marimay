#include "vm_image.h"
#include "vm_hot.h"

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
            ? (source->as.boolean ? 1 : 0) : source->as.integer_bits;
    return (view->descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW64
        ? mmay_managed_array_view_set_raw64_fast(view, index,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, bits, &length)
        : mmay_managed_array_scalar_view_set_fast(view, index,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, bits, &length))
        == MMAY_MANAGED_OK;
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
    MarimayManagedArrayView view;
    bool view_ready = false;
    if (execution == NULL) {
        result = MMAY_BYTECODE_IMAGE_RUN_STOPPED;
        goto done;
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
