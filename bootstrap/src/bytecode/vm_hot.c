#include "vm_hot.h"

#include <limits.h>

#include "mmay/base/checked_i64_fast.h"
#include "mmay/runtime/array_scalar_view.h"

/* All storage is borrowed from a verified, rooted frame. No call made by this
 * owner may allocate, collect or invoke host code. A cold instruction expires
 * the entire cache before the ordinary dispatcher can reach a safepoint. */
#define MMAY_VM_HOT_ARRAYS 4

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

static MarimayManagedArrayView *mmay_bytecode_hot_array(
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

static bool mmay_bytecode_hot_array_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers, const MarimayBytecodeFunction *function,
    MarimayBytecodeExecution *execution, MarimayBytecodeHotArrays *cache
) {
    MarimayOpcode opcode = instruction->opcode;
    bool store = opcode == MMAY_OPCODE_ARRAY_SET
        || opcode == MMAY_OPCODE_ARRAY_SET_IN_BOUNDS;
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    size_t first = (size_t)instruction->operands[1].unsigned_value;
    size_t second = (size_t)instruction->operands[2].unsigned_value;
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
            ? (registers[second].as.boolean ? 1 : 0)
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

static bool mmay_bytecode_hot_scalar(
    const MarimayBytecodeInstruction *instruction, MarimayBytecodeValue *registers
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    size_t first = (size_t)instruction->operands[1].unsigned_value;
    size_t second = (size_t)instruction->operands[2].unsigned_value;
    switch (instruction->opcode) {
        case MMAY_OPCODE_CONST_I64:
            registers[target].as.i64 = instruction->operands[1].signed_value;
            return true;
        case MMAY_OPCODE_CONST_INTEGER:
        case MMAY_OPCODE_CONST_F64:
            registers[target].as.integer_bits = instruction->operands[1].unsigned_value;
            return true;
        case MMAY_OPCODE_CONST_BOOL:
            registers[target].as.boolean = instruction->operands[1].unsigned_value != 0;
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
    int64_t right = instruction->opcode == MMAY_OPCODE_I64_NEGATE_CHECKED
        ? 0 : registers[second].as.i64;
    int64_t value = 0;
    MarimayCheckedI64Status status = MMAY_CHECKED_I64_OK;
    switch (instruction->opcode) {
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

void mmay_bytecode_run_hot(const MarimayBytecodeFunction *function,
    MarimayBytecodeValue *registers, MarimayBytecodeExecution *execution,
    size_t *block_id, size_t *instruction_id, size_t *steps,
    MarimayBytecodeRunOptions options
) {
    MarimayBytecodeHotArrays cache;
    cache.count = 0;
    cache.next = 0;
    const MarimayBytecodeBlock *blocks =
        (const MarimayBytecodeBlock *)function->blocks.data;
    for (;;) {
        const MarimayBytecodeBlock *block = &blocks[*block_id];
        if (*instruction_id >= block->instructions.length) return;
        const MarimayBytecodeInstruction *instructions =
            (const MarimayBytecodeInstruction *)block->instructions.data;
        const MarimayBytecodeInstruction *instruction = &instructions[*instruction_id];
        if ((options.max_steps != SIZE_MAX && *steps >= options.max_steps)
            || (options.remaining_shared_steps != NULL
                && *options.remaining_shared_steps == 0)) return;
        size_t next_block = *block_id;
        size_t next_instruction = *instruction_id + 1;
        switch (instruction->opcode) {
            case MMAY_OPCODE_JUMP:
                next_block = (size_t)instruction->operands[0].unsigned_value;
                next_instruction = 0;
                break;
            case MMAY_OPCODE_BRANCH_BOOL: {
                size_t condition = (size_t)instruction->operands[0].unsigned_value;
                next_block = (size_t)instruction->operands[
                    registers[condition].as.boolean ? 1 : 2].unsigned_value;
                next_instruction = 0;
                break;
            }
            case MMAY_OPCODE_ARRAY_LENGTH:
            case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
            case MMAY_OPCODE_ARRAY_GET:
            case MMAY_OPCODE_ARRAY_SET:
            case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS:
            case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
            case MMAY_OPCODE_ARRAY_GENERATION:
            case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION:
                if (!mmay_bytecode_hot_array_instruction(instruction, registers,
                    function, execution, &cache)) return;
                break;
            default:
                if (!mmay_bytecode_hot_scalar(instruction, registers)) return;
                break;
        }
        if (options.max_steps != SIZE_MAX) *steps += 1;
        if (options.remaining_shared_steps != NULL) *options.remaining_shared_steps -= 1;
        *block_id = next_block;
        *instruction_id = next_instruction;
    }
}
