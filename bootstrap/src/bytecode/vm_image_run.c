#include "vm_image.h"
#include "vm_hot.h"

#include <limits.h>

#include "mmay/base/checked_i64_fast.h"
#include "mmay/runtime/array_scalar_view.h"

/* Predigested operations are owned by one verified execution image. Borrowed
 * array views never cross allocation, collection, host calls or cold exits. */
#define MMAY_VM_HOT_ARRAYS 4

#if defined(__clang__) || defined(__GNUC__)
#define MMAY_VM_HOT_INLINE static inline __attribute__((always_inline))
#else
#define MMAY_VM_HOT_INLINE static inline
#endif

typedef struct MarimayBytecodeHotArray {
    MarimayManagedArrayView view;
    MarimayBytecodeType type;
    uint32_t layout;
} MarimayBytecodeHotArray;

typedef struct MarimayBytecodeHotArrays {
    MarimayBytecodeHotArray entries[MMAY_VM_HOT_ARRAYS];
    size_t count;
    size_t next;
} MarimayBytecodeHotArrays;

static bool mmay_bytecode_hot_reference_same(
    MarimayManagedReference left, MarimayManagedReference right
) {
    return left.heap_provenance == right.heap_provenance
        && left.slot == right.slot && left.generation == right.generation;
}
static inline MarimayManagedArrayView *mmay_bytecode_hot_array(
    MarimayBytecodeHotArrays *cache, const MarimayBytecodeFunction *function,
    MarimayBytecodeExecution *execution, MarimayBytecodeValue value,
    size_t register_id
) {
    if (execution == NULL) return NULL;
    const uint32_t *layouts = (const uint32_t *)function->register_layouts.data;
    uint32_t layout = layouts[register_id];
    for (size_t index = 0; index < cache->count; index += 1) {
        MarimayBytecodeHotArray *entry = &cache->entries[index];
        if (entry->type == value.type && entry->layout == layout
            && mmay_bytecode_hot_reference_same(
                entry->view.reference, value.as.managed_reference)) {
            return &entry->view;
        }
    }
    const MarimayManagedArrayDescriptor *descriptor =
        mmay_bytecode_array_descriptor_cached(execution, function,
            value.type, layout);
    if (descriptor == NULL) return NULL;
    MarimayManagedArrayView view;
    if (mmay_managed_array_view_bind(&execution->managed_heap,
            value.as.managed_reference, descriptor, &view) != MMAY_MANAGED_OK) {
        return NULL;
    }
    size_t index = cache->count < MMAY_VM_HOT_ARRAYS
        ? cache->count++ : cache->next;
    cache->next = (index + 1) % MMAY_VM_HOT_ARRAYS;
    cache->entries[index].view = view;
    cache->entries[index].type = value.type;
    cache->entries[index].layout = layout;
    return &cache->entries[index].view;
}

MMAY_VM_HOT_INLINE bool mmay_bytecode_hot_array_instruction(
    MarimayOpcode opcode, uint32_t target_operand, uint32_t first_operand,
    uint32_t second_operand,
    MarimayBytecodeValue *registers, const MarimayBytecodeFunction *function,
    MarimayBytecodeExecution *execution, MarimayBytecodeHotArrays *cache
) {
    bool store = opcode == MMAY_OPCODE_ARRAY_SET
        || opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS;
    size_t target = (size_t)target_operand;
    size_t first = (size_t)first_operand;
    size_t second = (size_t)second_operand;
    size_t source = store ? target : first;
    MarimayManagedArrayView *view = mmay_bytecode_hot_array(
        cache, function, execution, registers[source], source);
    if (view == NULL) return false;
    size_t length = 0;
    if (opcode == MMAY_OPCODE_ARRAY_LENGTH) {
        if (mmay_managed_array_view_length_fast(view, &length)
                != MMAY_MANAGED_OK || (uintmax_t)length > (uintmax_t)INT64_MAX) {
            return false;
        }
        registers[target].as.i64 = (int64_t)length;
        return true;
    }
    if (opcode == MMAY_OPCODE_ARRAY_GENERATION) {
        registers[target].as.integer_bits = *view->mutation_generation;
        return true;
    }
    if (opcode == MMAY_OPCODE_ARRAY_REQUIRE_GENERATION) {
        if (*view->mutation_generation != registers[second].as.integer_bits) {
            return false;
        }
        registers[target] = registers[first];
        return true;
    }
    int64_t index = registers[store ? first : second].as.i64;
    if (opcode == MMAY_OPCODE_ARRAY_REQUIRE_INDEX) {
        if (mmay_managed_array_view_require_fast(view, index, &length)
            != MMAY_MANAGED_OK) return false;
        registers[target] = registers[first];
        return true;
    }
    MarimayManagedArrayAccessMode mode =
        opcode == MMAY_OPCODE_ARRAY_GET_IN_BOUNDS
            || opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS
        ? MMAY_MANAGED_ARRAY_ACCESS_IN_BOUNDS : MMAY_MANAGED_ARRAY_ACCESS_CHECKED;
    uint64_t bits = 0;
    if (store) {
        bits = registers[second].type == MMAY_BYTECODE_TYPE_I64
            ? (uint64_t)registers[second].as.i64
            : registers[second].type == MMAY_BYTECODE_TYPE_BOOL
            ? (registers[second].as.boolean ? UINT64_C(1) : UINT64_C(0))
            : registers[second].as.integer_bits;
        return (view->descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW64
            ? mmay_managed_array_view_set_raw64_fast(view, index, mode, bits, &length)
            : mmay_managed_array_scalar_view_set_fast(view, index, mode, bits, &length))
            == MMAY_MANAGED_OK;
    }
    if ((view->descriptor->storage_kind == MMAY_MANAGED_ARRAY_STORAGE_RAW64
            ? mmay_managed_array_view_get_raw64_fast(view, index, mode, &bits, &length)
            : mmay_managed_array_scalar_view_get_fast(view, index, mode, &bits, &length))
        != MMAY_MANAGED_OK) return false;
    if (registers[target].type == MMAY_BYTECODE_TYPE_I64) {
        registers[target].as.i64 = mmay_i64_from_twos_complement_fast(bits);
    } else if (registers[target].type == MMAY_BYTECODE_TYPE_BOOL) {
        registers[target].as.boolean = bits != 0;
    } else {
        registers[target].as.integer_bits = bits;
    }
    return true;
}

MMAY_VM_HOT_INLINE bool mmay_bytecode_hot_scalar(
    MarimayOpcode opcode, uint32_t target_operand, uint32_t first_operand,
    uint32_t second_operand, uint64_t immediate,
    MarimayBytecodeValue *registers
) {
    size_t target = (size_t)target_operand;
    size_t first = (size_t)first_operand;
    size_t second = (size_t)second_operand;
    switch (opcode) {
        case MMAY_OPCODE_CONST_I64:
            registers[target].as.i64 = (int64_t)immediate;
            return true;
        case MMAY_OPCODE_CONST_INTEGER:
        case MMAY_OPCODE_CONST_F64:
            registers[target].as.integer_bits = immediate;
            return true;
        case MMAY_OPCODE_CONST_BOOL:
            registers[target].as.boolean = immediate != 0;
            return true;
        case MMAY_OPCODE_COPY:
            registers[target] = registers[first];
            return true;
        case MMAY_OPCODE_BOOL_NOT:
            registers[target].as.boolean = !registers[first].as.boolean;
            return true;
        case MMAY_OPCODE_BOOL_EQUAL:
            registers[target].as.boolean = registers[first].as.boolean == registers[second].as.boolean;
            return true;
        case MMAY_OPCODE_I64_ADD_CHECKED:
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED:
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED:
        case MMAY_OPCODE_I64_DIVIDE_CHECKED:
        case MMAY_OPCODE_I64_REMAINDER_CHECKED:
        case MMAY_OPCODE_I64_NEGATE_CHECKED:
        case MMAY_OPCODE_I64_ADD_PROVEN:
        case MMAY_OPCODE_I64_SUBTRACT_PROVEN:
        case MMAY_OPCODE_I64_MULTIPLY_PROVEN:
        case MMAY_OPCODE_I64_EQUAL:
        case MMAY_OPCODE_I64_LESS:
        case MMAY_OPCODE_I64_LESS_EQUAL:
            break;
        default: return false;
    }
    if (registers[first].type != MMAY_BYTECODE_TYPE_I64) return false;
    int64_t left = registers[first].as.i64;
    /* Unary instructions have no second register operand. */
    int64_t right = opcode == MMAY_OPCODE_I64_NEGATE_CHECKED
        ? 0 : registers[second].as.i64;
    int64_t value = 0;
    MarimayCheckedI64Status status = MMAY_CHECKED_I64_OK;
    switch (opcode) {
        case MMAY_OPCODE_I64_ADD_CHECKED:
            status = mmay_checked_i64_add_fast(left, right, &value); break;
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED:
            status = mmay_checked_i64_subtract_fast(left, right, &value); break;
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED:
            status = mmay_checked_i64_multiply_fast(left, right, &value); break;
        case MMAY_OPCODE_I64_DIVIDE_CHECKED:
            status = mmay_checked_i64_divide_fast(left, right, &value); break;
        case MMAY_OPCODE_I64_REMAINDER_CHECKED:
            status = mmay_checked_i64_remainder_fast(left, right, &value); break;
        case MMAY_OPCODE_I64_NEGATE_CHECKED:
            status = mmay_checked_i64_negate_fast(left, &value); break;
        case MMAY_OPCODE_I64_ADD_PROVEN:
            value = mmay_i64_add_proven_fast(left, right); break;
        case MMAY_OPCODE_I64_SUBTRACT_PROVEN:
            value = mmay_i64_subtract_proven_fast(left, right); break;
        case MMAY_OPCODE_I64_MULTIPLY_PROVEN:
            value = mmay_i64_multiply_proven_fast(left, right); break;
        case MMAY_OPCODE_I64_EQUAL:
            registers[target].as.boolean = left == right; return true;
        case MMAY_OPCODE_I64_LESS:
            registers[target].as.boolean = left < right; return true;
        case MMAY_OPCODE_I64_LESS_EQUAL:
            registers[target].as.boolean = left <= right; return true;
        default: return false;
    }
    if (status != MMAY_CHECKED_I64_OK) return false;
    registers[target].as.i64 = value;
    return true;
}

static inline bool mmay_bytecode_image_has_fuel(size_t steps,
    MarimayBytecodeRunOptions options, size_t required
) {
    return (options.max_steps == SIZE_MAX
            || (steps <= options.max_steps
                && required <= options.max_steps - steps))
        && (options.remaining_shared_steps == NULL
            || *options.remaining_shared_steps >= required);
}

static inline void mmay_bytecode_image_consume(size_t *steps,
    MarimayBytecodeRunOptions options, size_t count
) {
    if (options.max_steps != SIZE_MAX) *steps += count;
    if (options.remaining_shared_steps != NULL) {
        *options.remaining_shared_steps -= count;
    }
}
static inline int64_t mmay_bytecode_image_proven(
    const MarimayBytecodeImageOperation *operation,
    const MarimayBytecodeValue *registers
) {
    int64_t left = registers[operation->second].as.i64;
    int64_t right = registers[operation->third].as.i64;
    if (operation->opcode == MMAY_OPCODE_I64_ADD_PROVEN) {
        return mmay_i64_add_proven_fast(left, right);
    }
    if (operation->opcode == MMAY_OPCODE_I64_MULTIPLY_PROVEN) {
        return mmay_i64_multiply_proven_fast(left, right);
    }
    return mmay_i64_subtract_proven_fast(left, right);
}

static inline bool mmay_bytecode_image_compare(
    const MarimayBytecodeImageOperation *operation,
    const MarimayBytecodeValue *registers
) {
    if (operation->opcode == MMAY_OPCODE_BOOL_EQUAL) {
        return registers[operation->second].as.boolean
            == registers[operation->third].as.boolean;
    }
    int64_t left = registers[operation->second].as.i64;
    int64_t right = registers[operation->third].as.i64;
    if (operation->opcode == MMAY_OPCODE_I64_EQUAL) return left == right;
    if (operation->opcode == MMAY_OPCODE_I64_LESS_EQUAL) return left <= right;
    return left < right;
}

static inline bool mmay_bytecode_image_array_get(
    const MarimayManagedArrayView *view, int64_t index,
    MarimayBytecodeValue *target
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

static inline bool mmay_bytecode_image_array_set(
    const MarimayManagedArrayView *view, int64_t index,
    const MarimayBytecodeValue *source
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

void mmay_bytecode_run_image_hot(MarimayBytecodeImageFunction *image,
    const MarimayBytecodeFunction *function, MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution, size_t *block_id,
    size_t *instruction_id, size_t *steps, MarimayBytecodeRunOptions options
) {
    MarimayBytecodeHotArrays cache;
    cache.count = 0;
    cache.next = 0;
    if (image == NULL || function == NULL || registers == NULL
        || block_id == NULL || instruction_id == NULL || steps == NULL
        || *block_id >= image->block_count
        || *instruction_id > UINT32_MAX) return;
    size_t pc = (size_t)image->block_starts[*block_id] + *instruction_id;
    for (;;) {
        if (pc >= image->operation_count) return;
        const MarimayBytecodeImageOperation *operation = &image->operations[pc];
        if ((options.max_steps != SIZE_MAX && *steps >= options.max_steps)
            || (options.remaining_shared_steps != NULL
                && *options.remaining_shared_steps == 0)) break;
        if (operation->fusion == MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT_LOOP
            && mmay_bytecode_image_has_fuel(*steps, options, 17)) {
            MarimayBytecodeImageRunResult result =
                mmay_bytecode_run_image_array_shift_loop(
                    image, function, registers, execution, &pc, steps,
                    options);
            if (result == MMAY_BYTECODE_IMAGE_RUN_STOPPED) goto stopped;
            continue;
        }
        if ((operation->fusion
                == MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_FOLLOW_LOOP
                || operation->fusion
                    == MMAY_BYTECODE_IMAGE_FUSION_RECORD_ARRAY_COMPRESS_LOOP)
            && mmay_bytecode_image_has_fuel(*steps, options,
                image->operations[pc + 4].opcode == MMAY_OPCODE_BOOL_NOT
                    ? 6 : 5)) {
            MarimayBytecodeImageRunResult result =
                mmay_bytecode_run_image_record_array_loop(
                    image, function, registers, execution, &pc, steps,
                    options, &image->record_array_cache);
            if (result == MMAY_BYTECODE_IMAGE_RUN_STOPPED) goto stopped;
            continue;
        }
        if (operation->fusion == MMAY_BYTECODE_IMAGE_FUSION_COMPARE_BRANCH
            && mmay_bytecode_image_has_fuel(*steps, options, 2)) {
            const MarimayBytecodeImageOperation *branch = operation + 1;
            bool condition = mmay_bytecode_image_compare(operation, registers);
            registers[operation->first].as.boolean = condition;
            mmay_bytecode_image_consume(steps, options, 2);
            pc = condition ? branch->second : branch->third;
            continue;
        }
        if (operation->fusion == MMAY_BYTECODE_IMAGE_FUSION_INDUCTION
            && mmay_bytecode_image_has_fuel(*steps, options, 3)) {
            const MarimayBytecodeImageOperation *copy = operation + 1;
            const MarimayBytecodeImageOperation *jump = operation + 2;
            registers[operation->first].as.i64 =
                mmay_bytecode_image_proven(operation, registers);
            registers[copy->first] = registers[copy->second];
            mmay_bytecode_image_consume(steps, options, 3);
            pc = jump->first;
            continue;
        }
        if (operation->fusion == MMAY_BYTECODE_IMAGE_FUSION_ARRAY_READ_BRANCH
            && mmay_bytecode_image_has_fuel(*steps, options, 5)) {
            const MarimayBytecodeImageOperation *require = operation + 1;
            const MarimayBytecodeImageOperation *get = operation + 2;
            const MarimayBytecodeImageOperation *compare = operation + 3;
            const MarimayBytecodeImageOperation *branch = operation + 4;
            registers[operation->first].as.i64 =
                mmay_bytecode_image_proven(operation, registers);
            MarimayManagedArrayView *view = mmay_bytecode_hot_array(
                &cache, function, execution, registers[require->second],
                require->second);
            size_t length = 0;
            int64_t index = registers[require->third].as.i64;
            if (view == NULL || mmay_managed_array_view_require_fast(
                    view, index, &length) != MMAY_MANAGED_OK) {
                mmay_bytecode_image_consume(steps, options, 1);
                pc += 1;
                goto stopped;
            }
            registers[require->first] = registers[require->second];
            if (!mmay_bytecode_image_array_get(
                    view, index, &registers[get->first])) {
                mmay_bytecode_image_consume(steps, options, 2);
                pc += 2;
                goto stopped;
            }
            bool condition = mmay_bytecode_image_compare(compare, registers);
            registers[compare->first].as.boolean = condition;
            mmay_bytecode_image_consume(steps, options, 5);
            pc = condition ? branch->second : branch->third;
            continue;
        }
        if (operation->fusion == MMAY_BYTECODE_IMAGE_FUSION_ARRAY_SHIFT
            && mmay_bytecode_image_has_fuel(*steps, options, 10)) {
            const MarimayBytecodeImageOperation *items = operation;
            registers[items[0].first] = registers[items[0].second];
            MarimayManagedArrayView *view = mmay_bytecode_hot_array(
                &cache, function, execution, registers[items[1].second],
                items[1].second);
            size_t length = 0;
            int64_t first_index = registers[items[1].third].as.i64;
            if (view == NULL || mmay_managed_array_view_require_fast(
                    view, first_index, &length) != MMAY_MANAGED_OK) {
                mmay_bytecode_image_consume(steps, options, 1);
                pc += 1;
                goto stopped;
            }
            registers[items[1].first] = registers[items[1].second];
            registers[items[2].first].as.i64 =
                mmay_bytecode_image_proven(&items[2], registers);
            int64_t second_index = registers[items[3].third].as.i64;
            if (mmay_managed_array_view_require_fast(
                    view, second_index, &length) != MMAY_MANAGED_OK) {
                mmay_bytecode_image_consume(steps, options, 3);
                pc += 3;
                goto stopped;
            }
            registers[items[3].first] = registers[items[3].second];
            if (!mmay_bytecode_image_array_get(
                    view, second_index, &registers[items[4].first])) {
                mmay_bytecode_image_consume(steps, options, 4);
                pc += 4;
                goto stopped;
            }
            if (!mmay_bytecode_image_array_set(
                    view, first_index, &registers[items[5].third])) {
                mmay_bytecode_image_consume(steps, options, 5);
                pc += 5;
                goto stopped;
            }
            registers[items[6].first] = registers[items[6].second];
            registers[items[7].first].as.i64 =
                mmay_bytecode_image_proven(&items[7], registers);
            registers[items[8].first] = registers[items[8].second];
            mmay_bytecode_image_consume(steps, options, 10);
            pc = items[9].first;
            continue;
        }
        size_t next = pc + 1;
        switch ((MarimayOpcode)operation->opcode) {
            case MMAY_OPCODE_JUMP:
                next = operation->first;
                break;
            case MMAY_OPCODE_BRANCH_BOOL:
                next = registers[operation->first].as.boolean
                    ? operation->second : operation->third;
                break;
            case MMAY_OPCODE_ARRAY_LENGTH:
            case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
            case MMAY_OPCODE_ARRAY_GET:
            case MMAY_OPCODE_ARRAY_SET:
            case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS:
            case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
            case MMAY_OPCODE_ARRAY_GENERATION:
            case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION:
                if (!mmay_bytecode_hot_array_instruction(
                    (MarimayOpcode)operation->opcode, operation->first,
                    operation->second, operation->third, registers, function,
                    execution, &cache)) goto stopped;
                break;
            default:
                if (!mmay_bytecode_hot_scalar(
                    (MarimayOpcode)operation->opcode, operation->first,
                    operation->second, operation->third, operation->immediate,
                    registers)) goto stopped;
                break;
        }
        if (options.max_steps != SIZE_MAX) *steps += 1;
        if (options.remaining_shared_steps != NULL) {
            *options.remaining_shared_steps -= 1;
        }
        pc = next;
    }
stopped:
    if (pc < image->operation_count) {
        *block_id = image->operations[pc].source_block;
        *instruction_id = image->operations[pc].source_instruction;
    }
}
