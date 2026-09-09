#include "vm_image.h"
#include "vm_hot.h"

#include "mmay/base/checked_i64_fast.h"
#include "mmay/runtime/array_scalar_view.h"

static bool mmay_bytecode_follow_has_fuel(size_t steps,
    MarimayBytecodeRunOptions options, size_t required
) {
    return (options.max_steps == SIZE_MAX
            || (steps <= options.max_steps
                && required <= options.max_steps - steps))
        && (options.remaining_shared_steps == NULL
            || *options.remaining_shared_steps >= required);
}

static void mmay_bytecode_follow_consume(size_t *steps,
    MarimayBytecodeRunOptions options, size_t count
) {
    if (options.max_steps != SIZE_MAX) *steps += count;
    if (options.remaining_shared_steps != NULL) {
        *options.remaining_shared_steps -= count;
    }
}

static bool mmay_bytecode_follow_same_reference(
    MarimayManagedReference left, MarimayManagedReference right
) {
    return left.heap_provenance == right.heap_provenance
        && left.slot == right.slot && left.generation == right.generation;
}

static bool mmay_bytecode_follow_record_get(
    const MarimayBytecodeImageOperation *operation,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    MarimayBytecodeImageRecordArrayCache *views
) {
    MarimayManagedReference record =
        registers[operation->second].as.managed_reference;
    if (views->record_ready && !mmay_bytecode_follow_same_reference(
            views->record.reference, record)) {
        views->record_ready = false;
        views->array_ready = false;
    }
    if (!views->record_ready) {
        const uint32_t *layouts = function->register_layouts.data;
        const MarimayManagedRecordDescriptor *descriptor =
            mmay_bytecode_record_descriptor_cached(
                execution, function, layouts[operation->second]);
        if (descriptor == NULL || mmay_managed_record_view_bind(
                &execution->managed_heap, record, descriptor,
                &views->record) != MMAY_MANAGED_OK) return false;
        views->record_ready = true;
    }
    if (operation->third >= views->record.descriptor->field_count) return false;
    MarimayManagedReference array =
        views->record.fields[operation->third].reference;
    registers[operation->first].as.managed_reference = array;
    if (views->array_ready && !mmay_bytecode_follow_same_reference(
            views->array.reference, array)) views->array_ready = false;
    return true;
}

static bool mmay_bytecode_follow_array(
    const MarimayBytecodeImageOperation *require,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    MarimayBytecodeImageRecordArrayCache *views
) {
    if (!views->array_ready) {
        const uint32_t *layouts = function->register_layouts.data;
        const MarimayManagedArrayDescriptor *descriptor =
            mmay_bytecode_array_descriptor_cached(execution, function,
                registers[require->second].type, layouts[require->second]);
        if (descriptor == NULL || mmay_managed_array_view_bind(
                &execution->managed_heap,
                registers[require->second].as.managed_reference, descriptor,
                &views->array) != MMAY_MANAGED_OK) return false;
        views->array_ready = true;
    }
    return true;
}

static bool mmay_bytecode_follow_get(const MarimayManagedArrayView *view,
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
    target->as.i64 = mmay_i64_from_twos_complement_fast(bits);
    return true;
}

static bool mmay_bytecode_follow_set(const MarimayManagedArrayView *view,
    int64_t index, int64_t value
) {
    size_t length = 0;
    return (view->descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW64
        ? mmay_managed_array_view_set_raw64_fast(view, index,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, (uint64_t)value, &length)
        : mmay_managed_array_scalar_view_set_fast(view, index,
            MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS, (uint64_t)value, &length))
        == MMAY_MANAGED_OK;
}

static MarimayBytecodeImageRunResult mmay_bytecode_follow_body(
    const MarimayBytecodeImageOperation *body, bool compress,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    MarimayBytecodeImageRecordArrayCache *views, size_t *pc, size_t *steps,
    MarimayBytecodeRunOptions options
) {
    size_t count = compress ? 11 : 5;
    if (!mmay_bytecode_follow_has_fuel(*steps, options, count)) {
        return MMAY_BYTECODE_IMAGE_RUN_CONTINUE;
    }
    if (!mmay_bytecode_follow_record_get(
            body, function, registers, execution, views)) {
        return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
    }
    int64_t index = registers[body[1].third].as.i64;
    size_t length = 0;
    if (!mmay_bytecode_follow_array(
            body + 1, function, registers, execution, views)
        || mmay_managed_array_view_require_fast(
            &views->array, index, &length) != MMAY_MANAGED_OK) {
        mmay_bytecode_follow_consume(steps, options, 1);
        *pc += 1;
        return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
    }
    registers[body[1].first] = registers[body[1].second];
    if (!mmay_bytecode_follow_get(
            &views->array, index, &registers[body[2].first])) {
        mmay_bytecode_follow_consume(steps, options, 2);
        *pc += 2;
        return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
    }
    registers[body[3].first] = registers[body[3].second];
    if (!compress) {
        mmay_bytecode_follow_consume(steps, options, count);
        *pc = body[4].first;
        return MMAY_BYTECODE_IMAGE_RUN_CONTINUE;
    }
    if (!mmay_bytecode_follow_record_get(
            body + 4, function, registers, execution, views)) {
        mmay_bytecode_follow_consume(steps, options, 4);
        *pc += 4;
        return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
    }
    registers[body[5].first] = registers[body[5].second];
    int64_t destination = registers[body[6].third].as.i64;
    if (!mmay_bytecode_follow_array(
            body + 6, function, registers, execution, views)
        || mmay_managed_array_view_require_fast(
            &views->array, destination, &length) != MMAY_MANAGED_OK) {
        mmay_bytecode_follow_consume(steps, options, 6);
        *pc += 6;
        return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
    }
    registers[body[6].first] = registers[body[6].second];
    int64_t value = registers[body[7].third].as.i64;
    if (!mmay_bytecode_follow_set(&views->array, destination, value)) {
        mmay_bytecode_follow_consume(steps, options, 7);
        *pc += 7;
        return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
    }
    registers[body[8].first] = registers[body[8].second];
    registers[body[9].first] = registers[body[9].second];
    mmay_bytecode_follow_consume(steps, options, count);
    *pc = body[10].first;
    return MMAY_BYTECODE_IMAGE_RUN_CONTINUE;
}

MarimayBytecodeImageRunResult mmay_bytecode_run_image_record_array_loop(
    const MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, size_t *pc, size_t *steps,
    MarimayBytecodeRunOptions options,
    MarimayBytecodeImageRecordArrayCache *views
) {
    if (execution == NULL || views == NULL) {
        return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
    }
    while (mmay_bytecode_follow_has_fuel(*steps, options, 5)) {
        size_t head_pc = *pc;
        const MarimayBytecodeImageOperation *head = &image->operations[*pc];
        bool compress = head->fusion
            == MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_COMPRESS_LOOP;
        bool inverted = head[4].opcode == MMAY_OPCODE_BOOL_NOT;
        size_t head_count = inverted ? 6 : 5;
        if (!mmay_bytecode_follow_has_fuel(
                *steps, options, head_count)) break;
        const MarimayBytecodeImageOperation *branch =
            head + (inverted ? 5 : 4);
        size_t body_pc = inverted ? branch->second : branch->third;
        if (!mmay_bytecode_follow_record_get(
                head, function, registers, execution, views)) {
            return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
        }
        int64_t index = registers[head[1].third].as.i64;
        size_t length = 0;
        if (!mmay_bytecode_follow_array(
                head + 1, function, registers, execution, views)
            || mmay_managed_array_view_require_fast(
                &views->array, index, &length) != MMAY_MANAGED_OK) {
            mmay_bytecode_follow_consume(steps, options, 1);
            *pc += 1;
            return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
        }
        registers[head[1].first] = registers[head[1].second];
        if (!mmay_bytecode_follow_get(
                &views->array, index, &registers[head[2].first])) {
            mmay_bytecode_follow_consume(steps, options, 2);
            *pc += 2;
            return MMAY_BYTECODE_IMAGE_RUN_STOPPED;
        }
        bool equal = registers[head[2].first].as.i64
            == registers[head[3].third].as.i64;
        registers[head[3].first].as.boolean = equal;
        if (inverted) registers[head[4].first].as.boolean = !equal;
        mmay_bytecode_follow_consume(steps, options, head_count);
        if (equal) {
            *pc = inverted ? branch->third : branch->second;
            return MMAY_BYTECODE_IMAGE_RUN_CONTINUE;
        }
        *pc = body_pc;
        MarimayBytecodeImageRunResult result = mmay_bytecode_follow_body(
            &image->operations[body_pc], compress, function, registers,
            execution, views, pc, steps, options);
        if (result == MMAY_BYTECODE_IMAGE_RUN_STOPPED || *pc != head_pc) {
            return result;
        }
    }
    return MMAY_BYTECODE_IMAGE_RUN_CONTINUE;
}
