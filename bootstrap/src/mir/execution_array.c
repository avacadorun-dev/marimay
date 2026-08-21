#include "mir_internal.h"

#include <limits.h>
#include <string.h>

static size_t ruda_mir_array_append_size(
    unsigned char *buffer,
    size_t offset,
    size_t value
) {
    unsigned char digits[3 * sizeof(size_t)];
    size_t count = 0;
    do {
        digits[count] = (unsigned char)('0' + value % 10);
        count += 1;
        value /= 10;
    } while (value != 0);
    while (count != 0) {
        count -= 1;
        buffer[offset] = digits[count];
        offset += 1;
    }
    return offset;
}

static size_t ruda_mir_array_append_i64(
    unsigned char *buffer,
    size_t offset,
    int64_t value
) {
    uint64_t magnitude = (uint64_t)value;
    unsigned char digits[3 * sizeof(uint64_t)];
    size_t count = 0;
    if (value < 0) {
        buffer[offset] = '-';
        offset += 1;
        magnitude = UINT64_C(0) - magnitude;
    }
    do {
        digits[count] = (unsigned char)('0' + magnitude % UINT64_C(10));
        count += 1;
        magnitude /= UINT64_C(10);
    } while (magnitude != 0);
    while (count != 0) {
        count -= 1;
        buffer[offset] = digits[count];
        offset += 1;
    }
    return offset;
}

static RudaMirInterpretStatus ruda_mir_array_panic(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const char *code,
    const unsigned char *message,
    size_t message_length,
    const RudaAllocator *allocator,
    RudaMirPanic *panic
) {
    if (panic == NULL) {
        return RUDA_MIR_INTERPRET_PANIC;
    }
    if (!ruda_owned_bytes_copy(
            &panic->message, *allocator, message, message_length
        )) {
        return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
    }
    panic->code = code;
    panic->symbol = function->symbol;
    panic->span = instruction->span;
    return RUDA_MIR_INTERPRET_PANIC;
}

static RudaMirInterpretStatus ruda_mir_array_bounds_panic(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    int64_t index,
    size_t length,
    const RudaAllocator *allocator,
    RudaMirPanic *panic
) {
    static const unsigned char prefix[] = "array index ";
    static const unsigned char suffix[] = " is outside length ";
    unsigned char message[sizeof(prefix) + sizeof(suffix)
        + 3 * sizeof(uint64_t) + 3 * sizeof(size_t)];
    size_t message_length = 0;
    memcpy(message + message_length, prefix, sizeof(prefix) - 1);
    message_length += sizeof(prefix) - 1;
    message_length = ruda_mir_array_append_i64(
        message, message_length, index
    );
    memcpy(message + message_length, suffix, sizeof(suffix) - 1);
    message_length += sizeof(suffix) - 1;
    message_length = ruda_mir_array_append_size(
        message, message_length, length
    );
    return ruda_mir_array_panic(
        function, instruction, "G4002", message, message_length,
        allocator, panic
    );
}

static RudaMirInterpretStatus ruda_mir_array_generation_panic(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const RudaAllocator *allocator,
    RudaMirPanic *panic
) {
    static const unsigned char message[] =
        "array iterator invalidated by mutation";
    return ruda_mir_array_panic(
        function, instruction, "G4005", message, sizeof(message) - 1,
        allocator, panic
    );
}

static RudaMirInterpretStatus ruda_mir_array_reserve_capacity(
    RudaMirExecution *execution,
    RudaMirValue array_value,
    size_t required_capacity
) {
    RudaMirManagedArray **slot = ruda_mir_execution_array_slot(
        execution, array_value
    );
    RudaMirManagedArray *array = slot != NULL ? *slot : NULL;
    size_t element_bytes = 0;
    size_t allocation_size = 0;
    if (array == NULL || array->type != array_value.type) {
        return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (required_capacity <= array->capacity) {
        return RUDA_MIR_INTERPRET_OK;
    }
    if (required_capacity > execution->options.max_array_length
        || (uintmax_t)required_capacity > (uintmax_t)INT64_MAX
        || !ruda_size_multiply(
            required_capacity, sizeof(RudaMirValue), &element_bytes
        ) || !ruda_size_add(
            offsetof(RudaMirManagedArray, elements), element_bytes,
            &allocation_size
        )) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    {
        RudaMirManagedArray *resized = ruda_resize(
            &execution->allocator, array, array->allocation_size,
            allocation_size
        );
        if (resized == NULL) {
            return RUDA_MIR_INTERPRET_ALLOCATION_FAILED;
        }
        resized->allocation_size = allocation_size;
        resized->capacity = required_capacity;
        *slot = resized;
    }
    return RUDA_MIR_INTERPRET_OK;
}

RudaMirInterpretStatus ruda_mir_execution_array_push(
    RudaMirExecution *execution,
    const RudaMirArray *layout,
    RudaMirValue array_value,
    RudaMirValue element
) {
    RudaMirManagedArray *array = NULL;
    size_t next_length = 0;
    size_t next_total = 0;
    RudaMirInterpretStatus status;
    if (execution == NULL || !execution->initialized || layout == NULL
        || element.type != layout->element_type) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    array = ruda_mir_execution_array_value_mutable(execution, array_value);
    if (array == NULL || !ruda_mir_managed_array_matches_layout(layout, array)) {
        return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (ruda_mir_type_is_managed(element.type)
        && !ruda_mir_execution_owns_reference(execution, element)) {
        return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (array->mutation_generation == UINT64_MAX
        || array->length >= execution->options.max_array_length
        || array->length >= (size_t)INT64_MAX
        || !ruda_size_add(array->length, 1, &next_length)
        || !ruda_size_add(execution->total_array_elements, 1, &next_total)
        || next_total > execution->options.max_total_array_elements) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    status = ruda_mir_array_reserve_capacity(
        execution, array_value, next_length
    );
    if (status != RUDA_MIR_INTERPRET_OK) {
        return status;
    }
    array = ruda_mir_execution_array_value_mutable(execution, array_value);
    array->elements[array->length] = element;
    array->length = next_length;
    array->mutation_generation += 1;
    execution->total_array_elements = next_total;
    return RUDA_MIR_INTERPRET_OK;
}

static RudaMirInterpretStatus ruda_mir_array_pop(
    RudaMirExecution *execution,
    RudaMirManagedArray *array,
    RudaMirValue *target
) {
    size_t removed = 0;
    if (array->length == 0) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    if (array->mutation_generation == UINT64_MAX) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    removed = array->length - 1;
    *target = array->elements[removed];
    memset(&array->elements[removed], 0, sizeof(array->elements[removed]));
    array->length = removed;
    array->mutation_generation += 1;
    execution->total_array_elements -= 1;
    return RUDA_MIR_INTERPRET_OK;
}

static RudaMirInterpretStatus ruda_mir_array_truncate(
    RudaMirExecution *execution,
    RudaMirManagedArray *array,
    int64_t requested
) {
    size_t next_length = 0;
    size_t removed = 0;
    if (requested < 0 || (uint64_t)requested > (uint64_t)array->length) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    next_length = (size_t)requested;
    if (next_length == array->length) {
        return RUDA_MIR_INTERPRET_OK;
    }
    if (array->mutation_generation == UINT64_MAX) {
        return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
    }
    removed = array->length - next_length;
    memset(
        array->elements + next_length, 0, removed * sizeof(RudaMirValue)
    );
    array->length = next_length;
    array->mutation_generation += 1;
    execution->total_array_elements -= removed;
    return RUDA_MIR_INTERPRET_OK;
}

static RudaMirInterpretStatus ruda_mir_array_reserve(
    RudaMirExecution *execution,
    RudaMirValue array_value,
    RudaMirManagedArray *array,
    int64_t additional
) {
    size_t required = 0;
    if (additional < 0
        || (uint64_t)additional > (uint64_t)INT64_MAX
            - (uint64_t)array->length
        || !ruda_size_add(array->length, (size_t)additional, &required)) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    return ruda_mir_array_reserve_capacity(execution, array_value, required);
}

RudaMirInterpretStatus ruda_mir_execute_array_instruction(
    const RudaMirFunction *function,
    const RudaMirInstruction *instruction,
    const RudaMirValue *first,
    const RudaMirValue *second,
    const RudaMirValue *third,
    RudaMirValue *target,
    const RudaAllocator *allocator,
    RudaMirInterpretOptions options
) {
    const RudaMirArray *layout = ruda_mir_array(function, instruction->array);
    RudaMirManagedArray *array = options.execution != NULL
        ? ruda_mir_execution_array_value_mutable(options.execution, *first)
        : NULL;
    RudaMirInterpretStatus status = RUDA_MIR_INTERPRET_OK;
    size_t index = 0;
    if (options.execution == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_ARGUMENT;
    }
    if (array == NULL) {
        return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
    }
    if (layout == NULL || !ruda_mir_managed_array_matches_layout(
            layout, array
        )) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    if (instruction->kind == RUDA_MIR_ARRAY_LENGTH) {
        target->as.i64 = (int64_t)array->length;
        return RUDA_MIR_INTERPRET_OK;
    }
    if (instruction->kind == RUDA_MIR_ARRAY_GENERATION) {
        target->as.integer_bits = array->mutation_generation;
        return RUDA_MIR_INTERPRET_OK;
    }
    if (instruction->kind == RUDA_MIR_ARRAY_REQUIRE_GENERATION) {
        if (array->mutation_generation != second->as.integer_bits) {
            return ruda_mir_array_generation_panic(
                function, instruction, allocator, options.panic
            );
        }
        *target = *first;
        return RUDA_MIR_INTERPRET_OK;
    }
    if (instruction->kind == RUDA_MIR_ARRAY_PUSH) {
        status = ruda_mir_execution_array_push(
            options.execution, layout, *first, *second
        );
        if (status == RUDA_MIR_INTERPRET_OK) {
            *target = *first;
        }
        return status;
    }
    if (instruction->kind == RUDA_MIR_ARRAY_POP) {
        return ruda_mir_array_pop(options.execution, array, target);
    }
    if (instruction->kind == RUDA_MIR_ARRAY_TRUNCATE
        || instruction->kind == RUDA_MIR_ARRAY_CLEAR
        || instruction->kind == RUDA_MIR_ARRAY_RESERVE) {
        status = instruction->kind == RUDA_MIR_ARRAY_RESERVE
            ? ruda_mir_array_reserve(
                options.execution, *first, array, second->as.i64
            )
            : ruda_mir_array_truncate(
                options.execution, array,
                instruction->kind == RUDA_MIR_ARRAY_CLEAR
                    ? INT64_C(0) : second->as.i64
            );
        if (status == RUDA_MIR_INTERPRET_OK) {
            *target = *first;
        }
        return status;
    }
    if (second->as.i64 < 0
        || (uint64_t)second->as.i64 >= (uint64_t)array->length) {
        return ruda_mir_array_bounds_panic(
            function, instruction, second->as.i64, array->length,
            allocator, options.panic
        );
    }
    index = (size_t)second->as.i64;
    if (instruction->kind == RUDA_MIR_ARRAY_REQUIRE_INDEX) {
        *target = *first;
        return RUDA_MIR_INTERPRET_OK;
    }
    if (instruction->kind == RUDA_MIR_ARRAY_SET) {
        if (array->mutation_generation == UINT64_MAX) {
            return RUDA_MIR_INTERPRET_OBJECT_LIMIT;
        }
        if (ruda_mir_type_is_managed(third->type)
            && !ruda_mir_execution_owns_reference(options.execution, *third)) {
            return RUDA_MIR_INTERPRET_INVALID_REFERENCE;
        }
        array->elements[index] = *third;
        array->mutation_generation += 1;
        *target = *third;
        return RUDA_MIR_INTERPRET_OK;
    }
    if (instruction->kind != RUDA_MIR_ARRAY_GET) {
        return RUDA_MIR_INTERPRET_INVALID_MIR;
    }
    *target = array->elements[index];
    return RUDA_MIR_INTERPRET_OK;
}
