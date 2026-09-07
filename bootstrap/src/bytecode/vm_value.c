#include "vm_internal.h"

#include <limits.h>
#include <string.h>

#include "mmay/base/checked_i64.h"
#include "mmay/base/checked_integer.h"
#include "mmay/base/f64.h"

MarimayBytecodeExecutionOptions mmay_bytecode_execution_default_options(void) {
    MarimayBytecodeExecutionOptions options;
    options.max_strings = 1000000;
    options.max_total_string_bytes = 64U * 1024U * 1024U;
    options.managed_heap = mmay_managed_heap_default_options();
    options.env = NULL;
    options.fs = NULL;
    options.process_context = NULL;
    return options;
}

bool mmay_bytecode_execution_init(
    MarimayBytecodeExecution *execution,
    MarimayAllocator allocator,
    MarimayBytecodeExecutionOptions options
) {
    if (execution == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL || options.max_strings == 0
        || options.max_strings > UINT32_MAX || options.max_total_string_bytes == 0) {
        return false;
    }
    memset(execution, 0, sizeof(*execution));
    execution->allocator = allocator;
    if (!mmay_vector_init(
            &execution->strings, allocator, sizeof(MarimayOwnedBytes)
        ) || !mmay_map_init(
            &execution->string_index, allocator, sizeof(MarimayBytecodeReference),
            UINT64_C(0x7275646162746573), UINT64_C(0x7472696e67696478)
        ) || !mmay_managed_heap_init(
            &execution->managed_heap, allocator, options.managed_heap
        ) || !mmay_bytecode_nominal_cache_init(execution, allocator)
        || !mmay_vector_init(&execution->owned_interface_tables, allocator, sizeof(void *))
        || !mmay_bytecode_array_cache_init(execution, allocator)
        || !mmay_bytecode_collection_cache_init(execution, allocator)
        || !mmay_env_init(&execution->env, allocator, options.env)
        || (options.fs != NULL
            && !mmay_fs_init(&execution->fs, allocator, options.fs))) {
        if (execution->fs.initialized) mmay_fs_dispose(&execution->fs);
        if (execution->env.initialized) mmay_env_dispose(&execution->env);
        mmay_managed_heap_dispose(&execution->managed_heap);
        mmay_map_dispose(&execution->string_index);
        mmay_vector_dispose(&execution->strings);
        mmay_bytecode_nominal_cache_dispose(execution);
        mmay_vector_dispose(&execution->owned_interface_tables);
        mmay_vector_dispose(&execution->array_descriptors);
        mmay_vector_dispose(&execution->collection_descriptors);
        return false;
    }
    execution->allocator = allocator;
    execution->options = options;
    execution->env_initialized = true;
    execution->fs_initialized = options.fs != NULL;
    execution->process_context = options.process_context != NULL
        ? *options.process_context : mmay_process_context_default_options();
    execution->initialized = true;
    return true;
}

void mmay_bytecode_execution_dispose(MarimayBytecodeExecution *execution) {
    size_t index = 0;
    if (execution == NULL) {
        return;
    }
    if (execution->fs_initialized) mmay_fs_dispose(&execution->fs);
    if (execution->env_initialized) mmay_env_dispose(&execution->env);
    for (index = 0; index < execution->strings.length; index += 1) {
        MarimayOwnedBytes *string = mmay_vector_at(&execution->strings, index);
        if (string != NULL) {
            mmay_owned_bytes_dispose(string);
        }
    }
    mmay_map_dispose(&execution->string_index);
    mmay_vector_dispose(&execution->strings);
    mmay_managed_heap_dispose(&execution->managed_heap);
    mmay_bytecode_nominal_cache_dispose(execution);
    mmay_bytecode_run_interface_tables_dispose(execution);
    mmay_bytecode_array_cache_dispose(execution);
    mmay_bytecode_collection_cache_dispose(execution);
    mmay_test_output_dispose(&execution->test_output);
    memset(execution, 0, sizeof(*execution));
}

bool mmay_bytecode_execution_string_bytes(
    const MarimayBytecodeExecution *execution,
    MarimayBytecodeValue value,
    const unsigned char **bytes,
    size_t *length
) {
    const MarimayOwnedBytes *string = execution != NULL && execution->initialized
            && value.type == MMAY_BYTECODE_TYPE_STRING
            && value.as.reference != MMAY_BYTECODE_INVALID_REFERENCE
        ? mmay_vector_at_const(&execution->strings, value.as.reference)
        : NULL;
    if (string == NULL || string->data == NULL || bytes == NULL || length == NULL) {
        return false;
    }
    *bytes = string->data;
    *length = string->length;
    return true;
}

bool mmay_bytecode_panic_init(MarimayBytecodePanic *panic, MarimayAllocator allocator) {
    if (panic == NULL || allocator.allocate == NULL || allocator.resize == NULL
        || allocator.deallocate == NULL) {
        return false;
    }
    memset(panic, 0, sizeof(*panic));
    if (!mmay_vector_init(
            &panic->frames, allocator, sizeof(MarimayBytecodePanicFrame)
        ) || !mmay_vector_init(
            &panic->suppressed, allocator,
            sizeof(MarimayBytecodeSuppressedPanic)
        )) {
        mmay_vector_dispose(&panic->frames);
        return false;
    }
    panic->initialized = true;
    return true;
}

void mmay_bytecode_panic_dispose(MarimayBytecodePanic *panic) {
    size_t index = 0;
    if (panic == NULL) {
        return;
    }
    mmay_owned_bytes_dispose(&panic->message);
    for (index = 0; index < panic->suppressed.length; index += 1) {
        MarimayBytecodeSuppressedPanic *suppressed = mmay_vector_at(
            &panic->suppressed, index
        );
        if (suppressed != NULL) {
            mmay_owned_bytes_dispose(&suppressed->message);
            mmay_vector_dispose(&suppressed->frames);
        }
    }
    mmay_vector_dispose(&panic->frames);
    mmay_vector_dispose(&panic->suppressed);
    memset(panic, 0, sizeof(*panic));
}

const MarimayBytecodePanicFrame *mmay_bytecode_panic_frame(
    const MarimayBytecodePanic *panic,
    size_t index
) {
    return panic != NULL && panic->initialized
        ? mmay_vector_at_const(&panic->frames, index)
        : NULL;
}

const MarimayBytecodeSuppressedPanic *mmay_bytecode_panic_suppressed(
    const MarimayBytecodePanic *panic,
    size_t index
) {
    return panic != NULL && panic->initialized
        ? mmay_vector_at_const(&panic->suppressed, index)
        : NULL;
}

MarimayBytecodeRunStatus mmay_bytecode_execution_intern(
    MarimayBytecodeExecution *execution,
    const unsigned char *bytes,
    size_t length,
    MarimayBytecodeReference *result
) {
    const MarimayBytecodeReference *found = NULL;
    MarimayOwnedBytes owned;
    MarimayBytecodeReference reference;
    size_t next_bytes = 0;
    if (execution == NULL || !execution->initialized || result == NULL
        || (bytes == NULL && length != 0)) {
        return MMAY_BYTECODE_RUN_INVALID_ARGUMENT;
    }
    found = mmay_map_get_const(&execution->string_index, bytes, length);
    if (found != NULL) {
        *result = *found;
        return MMAY_BYTECODE_RUN_OK;
    }
    if (execution->strings.length >= execution->options.max_strings
        || execution->strings.length >= UINT32_MAX
        || !mmay_size_add(execution->total_string_bytes, length, &next_bytes)
        || next_bytes > execution->options.max_total_string_bytes) {
        return MMAY_BYTECODE_RUN_LIMIT_REACHED;
    }
    memset(&owned, 0, sizeof(owned));
    if (!mmay_owned_bytes_copy(&owned, execution->allocator, bytes, length)) {
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    reference = (MarimayBytecodeReference)execution->strings.length;
    if (!mmay_vector_push(&execution->strings, &owned)
        || !mmay_map_put(&execution->string_index, bytes, length, &reference)) {
        if (execution->strings.length != 0
            && reference == execution->strings.length - 1) {
            execution->strings.length -= 1;
        }
        mmay_owned_bytes_dispose(&owned);
        return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
    }
    execution->total_string_bytes = next_bytes;
    *result = reference;
    return MMAY_BYTECODE_RUN_OK;
}

MarimayBytecodeRunOptions mmay_bytecode_run_default_options(void) {
    MarimayBytecodeRunOptions options;
    options.max_steps = 10000000;
    options.max_call_depth = 1024;
    options.remaining_shared_steps = NULL;
    options.execution = NULL;
    options.stdout_sink = NULL;
    options.panic = NULL;
    options.trap = NULL;
    return options;
}

static MarimayBytecodeRunStatus mmay_bytecode_run_checked_status(
    MarimayCheckedI64Status status
) {
    if (status == MMAY_CHECKED_I64_OK) {
        return MMAY_BYTECODE_RUN_OK;
    }
    if (status == MMAY_CHECKED_I64_DIVISION_BY_ZERO) {
        return MMAY_BYTECODE_RUN_DIVISION_BY_ZERO;
    }
    if (status == MMAY_CHECKED_I64_INVALID_SHIFT) {
        return MMAY_BYTECODE_RUN_INVALID_SHIFT;
    }
    return MMAY_BYTECODE_RUN_OVERFLOW;
}

static bool mmay_bytecode_integer_kind(
    MarimayBytecodeType type,
    MarimayIntegerKind *result
) {
    if (result == NULL) {
        return false;
    }
    switch (type) {
        case MMAY_BYTECODE_TYPE_I8: *result = MMAY_INTEGER_I8; return true;
        case MMAY_BYTECODE_TYPE_I16: *result = MMAY_INTEGER_I16; return true;
        case MMAY_BYTECODE_TYPE_I32: *result = MMAY_INTEGER_I32; return true;
        case MMAY_BYTECODE_TYPE_I64: *result = MMAY_INTEGER_I64; return true;
        case MMAY_BYTECODE_TYPE_U8: *result = MMAY_INTEGER_U8; return true;
        case MMAY_BYTECODE_TYPE_U16: *result = MMAY_INTEGER_U16; return true;
        case MMAY_BYTECODE_TYPE_U32: *result = MMAY_INTEGER_U32; return true;
        case MMAY_BYTECODE_TYPE_U64: *result = MMAY_INTEGER_U64; return true;
        default: return false;
    }
}

static uint64_t mmay_bytecode_integer_mask(MarimayIntegerKind kind) {
    unsigned int width = mmay_integer_bit_width(kind);
    return width == 64U ? UINT64_MAX : (UINT64_C(1) << width) - UINT64_C(1);
}

static MarimayIntegerValue mmay_bytecode_integer_value(
    const MarimayBytecodeValue *value
) {
    MarimayIntegerKind kind = MMAY_INTEGER_I64;
    MarimayIntegerValue result;
    (void)mmay_bytecode_integer_kind(value->type, &kind);
    result.kind = kind;
    result.bits = value->as.integer_bits & mmay_bytecode_integer_mask(kind);
    return result;
}

static MarimayBytecodeRunStatus mmay_bytecode_integer_status(
    MarimayCheckedIntegerStatus status
) {
    switch (status) {
        case MMAY_CHECKED_INTEGER_OK: return MMAY_BYTECODE_RUN_OK;
        case MMAY_CHECKED_INTEGER_OVERFLOW: return MMAY_BYTECODE_RUN_OVERFLOW;
        case MMAY_CHECKED_INTEGER_DIVISION_BY_ZERO:
            return MMAY_BYTECODE_RUN_DIVISION_BY_ZERO;
        case MMAY_CHECKED_INTEGER_INVALID_SHIFT:
            return MMAY_BYTECODE_RUN_INVALID_SHIFT;
    }
    return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
}

static MarimayBytecodeRunStatus mmay_bytecode_integer_binary(
    MarimayOpcode opcode,
    const MarimayBytecodeValue *first,
    const MarimayBytecodeValue *second,
    MarimayBytecodeValue *target
) {
    MarimayIntegerValue left = mmay_bytecode_integer_value(first);
    MarimayIntegerValue right = mmay_bytecode_integer_value(second);
    MarimayIntegerValue output = {left.kind, 0};
    MarimayCheckedIntegerStatus status = MMAY_CHECKED_INTEGER_OVERFLOW;
    switch (opcode) {
        case MMAY_OPCODE_I64_ADD_CHECKED:
            status = mmay_checked_integer_add(left, right, &output); break;
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED:
            status = mmay_checked_integer_subtract(left, right, &output); break;
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED:
            status = mmay_checked_integer_multiply(left, right, &output); break;
        case MMAY_OPCODE_I64_DIVIDE_CHECKED:
            status = mmay_checked_integer_divide(left, right, &output); break;
        case MMAY_OPCODE_I64_REMAINDER_CHECKED:
            status = mmay_checked_integer_remainder(left, right, &output); break;
        case MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED:
            status = mmay_checked_integer_shift_left(left, right, &output); break;
        case MMAY_OPCODE_I64_SHIFT_RIGHT:
            status = mmay_checked_integer_shift_right(left, right, &output); break;
        default:
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    if (status == MMAY_CHECKED_INTEGER_OK) {
        target->as.integer_bits = output.bits;
    }
    return mmay_bytecode_integer_status(status);
}

static bool mmay_bytecode_integer_less(
    const MarimayBytecodeValue *first,
    const MarimayBytecodeValue *second,
    bool equal_allowed
) {
    MarimayIntegerValue left = mmay_bytecode_integer_value(first);
    MarimayIntegerValue right = mmay_bytecode_integer_value(second);
    if (mmay_integer_kind_is_signed(left.kind)) {
        int64_t left_value = 0;
        int64_t right_value = 0;
        (void)mmay_integer_to_i64(left, &left_value);
        (void)mmay_integer_to_i64(right, &right_value);
        return equal_allowed ? left_value <= right_value
            : left_value < right_value;
    }
    {
        uint64_t left_value = 0;
        uint64_t right_value = 0;
        (void)mmay_integer_to_u64(left, &left_value);
        (void)mmay_integer_to_u64(right, &right_value);
        return equal_allowed ? left_value <= right_value
            : left_value < right_value;
    }
}

MarimayBytecodeRunStatus mmay_bytecode_run_managed_status(
    MarimayManagedStatus status
) {
    switch (status) {
        case MMAY_MANAGED_OK:
            return MMAY_BYTECODE_RUN_OK;
        case MMAY_MANAGED_ALLOCATION_FAILED:
            return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
        case MMAY_MANAGED_LIMIT_REACHED:
            return MMAY_BYTECODE_RUN_LIMIT_REACHED;
        case MMAY_MANAGED_LENGTH_LIMIT:
            /* ARRAY_NEW cannot reach this after verified bounded decoding. */
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        case MMAY_MANAGED_INVALID_REFERENCE:
            return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
        case MMAY_MANAGED_BOUNDS:
            return MMAY_BYTECODE_RUN_PANIC;
        case MMAY_MANAGED_TRUNCATE_INVALID_LENGTH:
        case MMAY_MANAGED_NEGATIVE_ADDITIONAL:
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        case MMAY_MANAGED_ITERATOR_INVALIDATED:
            return MMAY_BYTECODE_RUN_PANIC;
        case MMAY_MANAGED_INVALID_ARGUMENT:
        case MMAY_MANAGED_TYPE_MISMATCH:
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
}

MarimayBytecodeRunStatus mmay_bytecode_run_value_instruction(
    const MarimayBytecodeInstruction *instruction,
    MarimayBytecodeValue *registers,
    MarimayBytecodeExecution *execution,
    const MarimayBytecodeFunction *function,
    MarimayAllocator allocator,
    const MarimayIoStdout *stdout_sink,
    MarimayBytecodePanic *panic,
    MarimaySymbolId symbol,
    int64_t *bounds_index,
    size_t *bounds_length
) {
    size_t target = (size_t)instruction->operands[0].unsigned_value;
    /* Dispatch once; scalar instructions must not probe unrelated owners. */
    switch (instruction->opcode) {
        case MMAY_OPCODE_CONST_I64:
            registers[target].as.i64 = instruction->operands[1].signed_value;
            return MMAY_BYTECODE_RUN_OK;
        case MMAY_OPCODE_CONST_INTEGER:
            registers[target].as.integer_bits =
                instruction->operands[1].unsigned_value;
            return MMAY_BYTECODE_RUN_OK;
        case MMAY_OPCODE_CONST_F64:
            registers[target].as.f64_bits = instruction->operands[1].unsigned_value;
            return MMAY_BYTECODE_RUN_OK;
        case MMAY_OPCODE_CONST_BOOL:
            registers[target].as.boolean = instruction->operands[1].unsigned_value
                != 0;
            return MMAY_BYTECODE_RUN_OK;
        case MMAY_OPCODE_CONST_STRING: {
            const MarimayBytecodeStringConstant *constant =
                mmay_bytecode_string_constant(
                    function,
                    (MarimayBytecodeStringId)instruction->operands[1].unsigned_value
                );
            MarimayBytecodeReference reference = MMAY_BYTECODE_INVALID_REFERENCE;
            if (constant == NULL || execution == NULL || !execution->initialized) {
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            }
            MarimayBytecodeRunStatus status = mmay_bytecode_execution_intern(
                    execution,
                    constant->value.data,
                    constant->value.length,
                    &reference
                );
            if (status != MMAY_BYTECODE_RUN_OK) {
                return status;
            }
            registers[target].as.reference = reference;
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_COPY:
            registers[target] = registers[
                (size_t)instruction->operands[1].unsigned_value
            ];
            return MMAY_BYTECODE_RUN_OK;
        case MMAY_OPCODE_ARRAY_NEW: {
            const MarimayBytecodeArrayConstruction *construction =
                mmay_bytecode_array_construction(
                    function,
                    (MarimayBytecodeArrayConstructionId)
                        instruction->operands[1].unsigned_value
                );
            const MarimayManagedArrayDescriptor *descriptor = construction != NULL
                ? mmay_bytecode_array_descriptor_for(
                    execution, function, construction->array_type,
                    construction->array_layout
                )
                : NULL;
            MarimayManagedCell *cells = NULL;
            MarimayManagedReference **cell_locations = NULL;
            MarimayManagedRootFrame cell_roots;
            size_t cell_bytes = 0;
            size_t location_bytes = 0;
            size_t index = 0;
            MarimayManagedStatus managed_status;
            bool cells_rooted = false;
            memset(&cell_roots, 0, sizeof(cell_roots));
            if (execution == NULL || descriptor == NULL
                || construction->argument_count
                    > execution->options.managed_heap.max_array_length
                || !mmay_size_multiply(
                    construction->argument_count, sizeof(MarimayManagedCell),
                    &cell_bytes
                )) {
                return descriptor == NULL
                    ? (execution != NULL
                        && execution->array_descriptor_allocation_failed
                        ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
                        : MMAY_BYTECODE_RUN_INVALID_BYTECODE)
                    : MMAY_BYTECODE_RUN_LIMIT_REACHED;
            }
            if (cell_bytes != 0) {
                cells = mmay_allocate(&allocator, cell_bytes);
                if (cells == NULL) {
                    return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
                }
                memset(cells, 0, cell_bytes);
            }
            if (construction->argument_count != 0 && cells == NULL) {
                return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
            }
            if (descriptor->cell_kind == MMAY_MANAGED_CELL_REFERENCE
                && construction->argument_count != 0) {
                if (!mmay_size_multiply(
                        construction->argument_count,
                        sizeof(*cell_locations),
                        &location_bytes
                    )) {
                    mmay_deallocate(&allocator, cells, cell_bytes);
                    return MMAY_BYTECODE_RUN_LIMIT_REACHED;
                }
                cell_locations = mmay_allocate(&allocator, location_bytes);
                if (cell_locations == NULL) {
                    mmay_deallocate(&allocator, cells, cell_bytes);
                    return MMAY_BYTECODE_RUN_ALLOCATION_FAILED;
                }
                for (index = 0; index < construction->argument_count;
                    index += 1) {
                    cells[index].reference = mmay_managed_invalid_reference();
                    cell_locations[index] = &cells[index].reference;
                }
                managed_status = mmay_managed_root_push(
                    &execution->managed_heap,
                    &cell_roots,
                    cell_locations,
                    construction->argument_count
                );
                if (managed_status != MMAY_MANAGED_OK) {
                    mmay_deallocate(
                        &allocator, cell_locations, location_bytes
                    );
                    mmay_deallocate(&allocator, cells, cell_bytes);
                    return mmay_bytecode_run_managed_status(managed_status);
                }
                cells_rooted = true;
            }
            for (index = 0; index < construction->argument_count; index += 1) {
                const MarimayBytecodeArrayArgument *argument =
                    mmay_bytecode_array_argument(
                        function, construction->argument_start + index
                    );
                if (argument == NULL) {
                    if (cells_rooted) (void)mmay_managed_root_pop(
                        &execution->managed_heap, &cell_roots
                    );
                    mmay_deallocate(
                        &allocator, cell_locations, location_bytes
                    );
                    mmay_deallocate(&allocator, cells, cell_bytes);
                    return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
                }
                {
                    MarimayBytecodeRunStatus cell_status =
                        mmay_bytecode_value_to_managed_cell(
                            execution,
                            &registers[argument->register_id],
                            &cells[index]
                        );
                    if (cell_status != MMAY_BYTECODE_RUN_OK) {
                        if (cells_rooted) (void)mmay_managed_root_pop(
                            &execution->managed_heap, &cell_roots
                        );
                        mmay_deallocate(
                            &allocator, cell_locations, location_bytes
                        );
                        mmay_deallocate(&allocator, cells, cell_bytes);
                        return cell_status;
                    }
                }
            }
            managed_status = mmay_managed_array_new(
                &execution->managed_heap, descriptor, cells,
                construction->argument_count,
                &registers[target].as.managed_reference
            );
            if (cells_rooted) {
                MarimayManagedStatus pop_status = mmay_managed_root_pop(
                    &execution->managed_heap, &cell_roots
                );
                if (managed_status == MMAY_MANAGED_OK) {
                    managed_status = pop_status;
                }
            }
            mmay_deallocate(&allocator, cell_locations, location_bytes);
            mmay_deallocate(&allocator, cells, cell_bytes);
            return mmay_bytecode_run_managed_status(managed_status);
        }
        case MMAY_OPCODE_ARRAY_PUSH: {
            size_t array = (size_t)instruction->operands[0].unsigned_value;
            size_t value = (size_t)instruction->operands[1].unsigned_value;
            const MarimayManagedArrayDescriptor *descriptor =
                mmay_bytecode_array_descriptor_for(
                    execution, function, registers[array].type,
                    mmay_bytecode_register_layout(function, (uint32_t)array)
                );
            MarimayManagedCell cell;
            MarimayManagedStatus managed_status;
            if (descriptor == NULL
                || !mmay_bytecode_managed_value_valid_for(
                    execution, function, registers[array],
                    mmay_bytecode_register_layout(function, (uint32_t)array)
                )) {
                return descriptor == NULL && execution != NULL
                        && execution->array_descriptor_allocation_failed
                    ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
                    : MMAY_BYTECODE_RUN_INVALID_REFERENCE;
            }
            {
                MarimayBytecodeRunStatus cell_status =
                    mmay_bytecode_value_to_managed_cell(
                        execution, &registers[value], &cell
                    );
                if (cell_status != MMAY_BYTECODE_RUN_OK) return cell_status;
            }
            managed_status = mmay_managed_array_push(
                &execution->managed_heap,
                registers[array].as.managed_reference,
                cell
            );
            return mmay_bytecode_run_managed_status(managed_status);
        }
        case MMAY_OPCODE_ARRAY_LENGTH: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t length = 0;
            if (!mmay_bytecode_managed_value_valid_for(
                    execution, function, registers[first],
                    mmay_bytecode_register_layout(function, (uint32_t)first)
                )) {
                return execution != NULL
                        && execution->array_descriptor_allocation_failed
                    ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
                    : MMAY_BYTECODE_RUN_INVALID_REFERENCE;
            }
            MarimayManagedStatus managed_status = execution != NULL
                ? mmay_managed_array_length(
                    &execution->managed_heap,
                    registers[first].as.managed_reference,
                    &length
                )
                : MMAY_MANAGED_INVALID_ARGUMENT;
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_run_managed_status(managed_status);
            }
            if ((uintmax_t)length > (uintmax_t)INT64_MAX) {
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            }
            registers[target].as.i64 = (int64_t)length;
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_ARRAY_GET_IN_BOUNDS:
        case MMAY_OPCODE_ARRAY_SET_IN_BOUNDS:
            return mmay_bytecode_run_range_instruction(
                instruction, registers, execution, function
            );
        case MMAY_OPCODE_ARRAY_REQUIRE_INDEX:
        case MMAY_OPCODE_ARRAY_GET: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            MarimayManagedCell cell;
            size_t length = 0;
            int64_t index = registers[second].as.i64;
            if (!mmay_bytecode_managed_value_valid_for(
                    execution, function, registers[first],
                    mmay_bytecode_register_layout(function, (uint32_t)first)
                )) {
                return execution != NULL
                        && execution->array_descriptor_allocation_failed
                    ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
                    : MMAY_BYTECODE_RUN_INVALID_REFERENCE;
            }
            MarimayManagedStatus managed_status = execution != NULL
                ? mmay_managed_array_get(
                    &execution->managed_heap,
                    registers[first].as.managed_reference,
                    index,
                    &cell,
                    &length
                )
                : MMAY_MANAGED_INVALID_ARGUMENT;
            if (managed_status == MMAY_MANAGED_BOUNDS) {
                if (bounds_index != NULL) {
                    *bounds_index = index;
                }
                if (bounds_length != NULL) {
                    *bounds_length = length;
                }
                return MMAY_BYTECODE_RUN_PANIC;
            }
            if (managed_status != MMAY_MANAGED_OK) {
                return mmay_bytecode_run_managed_status(managed_status);
            }
            if (instruction->opcode == MMAY_OPCODE_ARRAY_REQUIRE_INDEX) {
                registers[target] = registers[first];
            } else {
                MarimayBytecodeRunStatus cell_status =
                    mmay_bytecode_managed_cell_to_value(
                        execution, cell, &registers[target]
                    );
                if (cell_status != MMAY_BYTECODE_RUN_OK) return cell_status;
            }
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_ARRAY_SET: {
            size_t array = (size_t)instruction->operands[0].unsigned_value;
            size_t index_register =
                (size_t)instruction->operands[1].unsigned_value;
            size_t value = (size_t)instruction->operands[2].unsigned_value;
            MarimayManagedCell cell;
            size_t length = 0;
            int64_t index = registers[index_register].as.i64;
            MarimayManagedStatus managed_status;
            memset(&cell, 0, sizeof(cell));
            const MarimayManagedArrayDescriptor *descriptor =
                mmay_bytecode_array_descriptor_for(
                    execution, function, registers[array].type,
                    mmay_bytecode_register_layout(function, (uint32_t)array)
                );
            if (!mmay_bytecode_managed_value_valid_for(
                    execution, function, registers[array],
                    mmay_bytecode_register_layout(function, (uint32_t)array)
                )) {
                return execution != NULL
                        && execution->array_descriptor_allocation_failed
                    ? MMAY_BYTECODE_RUN_ALLOCATION_FAILED
                    : MMAY_BYTECODE_RUN_INVALID_REFERENCE;
            }
            if (descriptor == NULL) {
                return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
            }
            {
                MarimayBytecodeRunStatus cell_status =
                    mmay_bytecode_value_to_managed_cell(
                        execution, &registers[value], &cell
                    );
                if (cell_status != MMAY_BYTECODE_RUN_OK) return cell_status;
            }
            managed_status = execution != NULL
                ? mmay_managed_array_set(
                    &execution->managed_heap,
                    registers[array].as.managed_reference,
                    index,
                    cell,
                    &length
                )
                : MMAY_MANAGED_INVALID_ARGUMENT;
            if (managed_status == MMAY_MANAGED_BOUNDS) {
                if (bounds_index != NULL) {
                    *bounds_index = index;
                }
                if (bounds_length != NULL) {
                    *bounds_length = length;
                }
                return MMAY_BYTECODE_RUN_PANIC;
            }
            return mmay_bytecode_run_managed_status(managed_status);
        }
        case MMAY_OPCODE_I64_ADD_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                return mmay_bytecode_integer_binary(
                    instruction->opcode, &registers[first],
                    &registers[second], &registers[target]
                );
            }
            return mmay_bytecode_run_checked_status(mmay_checked_i64_add(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case MMAY_OPCODE_I64_ADD_PROVEN: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.i64 = mmay_i64_add_proven(
                registers[first].as.i64, registers[second].as.i64
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_SUBTRACT_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                return mmay_bytecode_integer_binary(
                    instruction->opcode, &registers[first],
                    &registers[second], &registers[target]
                );
            }
            return mmay_bytecode_run_checked_status(mmay_checked_i64_subtract(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case MMAY_OPCODE_I64_SUBTRACT_PROVEN: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.i64 = mmay_i64_subtract_proven(
                registers[first].as.i64, registers[second].as.i64
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_MULTIPLY_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                return mmay_bytecode_integer_binary(
                    instruction->opcode, &registers[first],
                    &registers[second], &registers[target]
                );
            }
            return mmay_bytecode_run_checked_status(mmay_checked_i64_multiply(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case MMAY_OPCODE_I64_MULTIPLY_PROVEN: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.i64 = mmay_i64_multiply_proven(
                registers[first].as.i64, registers[second].as.i64
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_DIVIDE_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                return mmay_bytecode_integer_binary(
                    instruction->opcode, &registers[first],
                    &registers[second], &registers[target]
                );
            }
            return mmay_bytecode_run_checked_status(mmay_checked_i64_divide(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case MMAY_OPCODE_I64_REMAINDER_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                return mmay_bytecode_integer_binary(
                    instruction->opcode, &registers[first],
                    &registers[second], &registers[target]
                );
            }
            return mmay_bytecode_run_checked_status(mmay_checked_i64_remainder(
                registers[first].as.i64,
                registers[second].as.i64,
                &registers[target].as.i64
            ));
        }
        case MMAY_OPCODE_I64_NEGATE_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                MarimayIntegerValue input = mmay_bytecode_integer_value(
                    &registers[first]
                );
                MarimayIntegerValue output = {input.kind, 0};
                MarimayCheckedIntegerStatus status = mmay_checked_integer_negate(
                    input, &output
                );
                if (status == MMAY_CHECKED_INTEGER_OK) {
                    registers[target].as.integer_bits = output.bits;
                }
                return mmay_bytecode_integer_status(status);
            }
            return mmay_bytecode_run_checked_status(mmay_checked_i64_negate(
                registers[first].as.i64,
                &registers[target].as.i64
            ));
        }
        case MMAY_OPCODE_F64_NEGATE: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            registers[target].as.f64_bits = mmay_f64_negate(
                registers[first].as.f64_bits
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_BOOL_NOT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            registers[target].as.boolean = !registers[first].as.boolean;
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_BITWISE_NOT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                MarimayIntegerValue value = mmay_bytecode_integer_value(
                    &registers[first]
                );
                registers[target].as.integer_bits =
                    (~value.bits) & mmay_bytecode_integer_mask(value.kind);
                return MMAY_BYTECODE_RUN_OK;
            }
            registers[target].as.i64 = mmay_i64_bitwise_not(
                registers[first].as.i64
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_BITWISE_AND: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                MarimayIntegerValue left = mmay_bytecode_integer_value(
                    &registers[first]
                );
                MarimayIntegerValue right = mmay_bytecode_integer_value(
                    &registers[second]
                );
                registers[target].as.integer_bits = left.bits & right.bits;
                return MMAY_BYTECODE_RUN_OK;
            }
            registers[target].as.i64 = mmay_i64_bitwise_and(
                registers[first].as.i64,
                registers[second].as.i64
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_BITWISE_XOR: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                MarimayIntegerValue left = mmay_bytecode_integer_value(
                    &registers[first]
                );
                MarimayIntegerValue right = mmay_bytecode_integer_value(
                    &registers[second]
                );
                registers[target].as.integer_bits = left.bits ^ right.bits;
                return MMAY_BYTECODE_RUN_OK;
            }
            registers[target].as.i64 = mmay_i64_bitwise_xor(
                registers[first].as.i64,
                registers[second].as.i64
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_BITWISE_OR: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                MarimayIntegerValue left = mmay_bytecode_integer_value(
                    &registers[first]
                );
                MarimayIntegerValue right = mmay_bytecode_integer_value(
                    &registers[second]
                );
                registers[target].as.integer_bits = left.bits | right.bits;
                return MMAY_BYTECODE_RUN_OK;
            }
            registers[target].as.i64 = mmay_i64_bitwise_or(
                registers[first].as.i64,
                registers[second].as.i64
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_SHIFT_LEFT_CHECKED: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                return mmay_bytecode_integer_binary(
                    instruction->opcode, &registers[first],
                    &registers[second], &registers[target]
                );
            }
            return mmay_bytecode_run_checked_status(
                mmay_checked_i64_shift_left(
                    registers[first].as.i64,
                    registers[second].as.i64,
                    &registers[target].as.i64
                )
            );
        }
        case MMAY_OPCODE_I64_SHIFT_RIGHT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                return mmay_bytecode_integer_binary(
                    instruction->opcode, &registers[first],
                    &registers[second], &registers[target]
                );
            }
            return mmay_bytecode_run_checked_status(
                mmay_checked_i64_shift_right(
                    registers[first].as.i64,
                    registers[second].as.i64,
                    &registers[target].as.i64
                )
            );
        }
        case MMAY_OPCODE_I64_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            if (registers[first].type == MMAY_BYTECODE_TYPE_RUNE) {
                registers[target].as.boolean = registers[first].as.integer_bits
                    == registers[second].as.integer_bits;
            } else if (registers[first].type != MMAY_BYTECODE_TYPE_I64) {
                registers[target].as.boolean =
                    !mmay_bytecode_integer_less(
                        &registers[first], &registers[second], false
                    ) && !mmay_bytecode_integer_less(
                        &registers[second], &registers[first], false
                    );
            } else {
                registers[target].as.boolean = registers[first].as.i64
                    == registers[second].as.i64;
            }
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_F64_ADD: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.f64_bits = mmay_f64_add(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_F64_SUBTRACT: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.f64_bits = mmay_f64_subtract(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_F64_MULTIPLY: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.f64_bits = mmay_f64_multiply(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_F64_DIVIDE: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.f64_bits = mmay_f64_divide(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_F64_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = mmay_f64_equal(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_F64_LESS: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = mmay_f64_less(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_F64_LESS_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = mmay_f64_less_equal(
                registers[first].as.f64_bits, registers[second].as.f64_bits
            );
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_BOOL_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].as.boolean
                == registers[second].as.boolean;
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_STRING_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            const unsigned char *left = NULL;
            const unsigned char *right = NULL;
            size_t left_length = 0;
            size_t right_length = 0;
            if (!mmay_bytecode_execution_string_bytes(
                    execution, registers[first], &left, &left_length
                ) || !mmay_bytecode_execution_string_bytes(
                    execution, registers[second], &right, &right_length
                )) {
                return MMAY_BYTECODE_RUN_INVALID_REFERENCE;
            }
            registers[target].as.boolean = left_length == right_length
                && (left_length == 0 || memcmp(left, right, left_length) == 0);
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_LESS: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].type
                    != MMAY_BYTECODE_TYPE_I64
                ? mmay_bytecode_integer_less(
                    &registers[first], &registers[second], false
                )
                : registers[first].as.i64 < registers[second].as.i64;
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_I64_LESS_EQUAL: {
            size_t first = (size_t)instruction->operands[1].unsigned_value;
            size_t second = (size_t)instruction->operands[2].unsigned_value;
            registers[target].as.boolean = registers[first].type
                    != MMAY_BYTECODE_TYPE_I64
                ? mmay_bytecode_integer_less(
                    &registers[first], &registers[second], true
                )
                : registers[first].as.i64 <= registers[second].as.i64;
            return MMAY_BYTECODE_RUN_OK;
        }
        case MMAY_OPCODE_INVALID:
        case MMAY_OPCODE_JUMP:
        case MMAY_OPCODE_BRANCH_BOOL:
        case MMAY_OPCODE_RETURN:
        case MMAY_OPCODE_RETURN_VOID:
        case MMAY_OPCODE_CALL_DIRECT:
        case MMAY_OPCODE_CALL_DIRECT_VOID:
        case MMAY_OPCODE_CALL_DIRECT_NEVER:
        case MMAY_OPCODE_PANIC_STRING:
        case MMAY_OPCODE_DEFER_REGISTER:
        case MMAY_OPCODE_DEFER_CALL:
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        case MMAY_OPCODE_ARRAY_POP:
        case MMAY_OPCODE_ARRAY_TRUNCATE:
        case MMAY_OPCODE_ARRAY_CLEAR:
        case MMAY_OPCODE_ARRAY_RESERVE:
        case MMAY_OPCODE_ARRAY_GENERATION:
        case MMAY_OPCODE_ARRAY_REQUIRE_GENERATION: {
            bool handled = false;
            MarimayBytecodeRunStatus status =
                mmay_bytecode_run_array_mutation_instruction(
                    instruction, registers, execution, function, allocator,
                    panic, symbol, &handled
                );
            return handled ? status : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        case MMAY_OPCODE_RECORD_NEW:
        case MMAY_OPCODE_RECORD_GET:
        case MMAY_OPCODE_RECORD_SET:
        case MMAY_OPCODE_RECORD_IDENTITY_SAME:
        case MMAY_OPCODE_ENUM_NEW:
        case MMAY_OPCODE_ENUM_IS_VARIANT:
        case MMAY_OPCODE_ENUM_REQUIRE_VARIANT:
        case MMAY_OPCODE_ENUM_GET_PAYLOAD:
            return mmay_bytecode_run_nominal_instruction(
                instruction, registers, execution, function, allocator
            );
        case MMAY_OPCODE_INTERFACE_NEW:
            return mmay_bytecode_run_interface_new(
                instruction, registers, execution, function
            );
        case MMAY_OPCODE_CALL_INTERFACE:
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        case MMAY_OPCODE_INTEGER_CHECKED_ADD:
        case MMAY_OPCODE_INTEGER_CHECKED_SUBTRACT:
        case MMAY_OPCODE_INTEGER_CHECKED_MULTIPLY:
        case MMAY_OPCODE_INTEGER_CHECKED_DIVIDE:
        case MMAY_OPCODE_INTEGER_CHECKED_REMAINDER:
        case MMAY_OPCODE_INTEGER_CHECKED_NEGATE:
        case MMAY_OPCODE_INTEGER_CHECKED_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_CHECKED_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_WRAPPING_ADD:
        case MMAY_OPCODE_INTEGER_WRAPPING_SUBTRACT:
        case MMAY_OPCODE_INTEGER_WRAPPING_MULTIPLY:
        case MMAY_OPCODE_INTEGER_WRAPPING_DIVIDE:
        case MMAY_OPCODE_INTEGER_WRAPPING_REMAINDER:
        case MMAY_OPCODE_INTEGER_WRAPPING_NEGATE:
        case MMAY_OPCODE_INTEGER_WRAPPING_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_WRAPPING_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_SATURATING_ADD:
        case MMAY_OPCODE_INTEGER_SATURATING_SUBTRACT:
        case MMAY_OPCODE_INTEGER_SATURATING_MULTIPLY:
        case MMAY_OPCODE_INTEGER_SATURATING_DIVIDE:
        case MMAY_OPCODE_INTEGER_SATURATING_REMAINDER:
        case MMAY_OPCODE_INTEGER_SATURATING_NEGATE:
        case MMAY_OPCODE_INTEGER_SATURATING_SHIFT_LEFT:
        case MMAY_OPCODE_INTEGER_SATURATING_SHIFT_RIGHT:
        case MMAY_OPCODE_INTEGER_CONVERT_TOTAL:
        case MMAY_OPCODE_INTEGER_CONVERT_OPTION:
            return mmay_bytecode_run_numeric_instruction(
                instruction, registers, execution, function
            );
        case MMAY_OPCODE_FORMAT_F32:
        case MMAY_OPCODE_FORMAT_F64:
        case MMAY_OPCODE_FORMAT_BOOL:
        case MMAY_OPCODE_FORMAT_INTEGER:
            return mmay_bytecode_run_format_instruction(
                instruction, registers, execution
            );
        case MMAY_OPCODE_STRING_CONCAT:
            return mmay_bytecode_run_string_concat_instruction(
                instruction, registers, execution
            );
        case MMAY_OPCODE_PARSE_INTEGER:
            return mmay_bytecode_run_parse_instruction(
                instruction, registers, execution, function
            );
        case MMAY_OPCODE_TEXT_BYTE_LENGTH:
        case MMAY_OPCODE_TEXT_RUNE_COUNT:
        case MMAY_OPCODE_TEXT_GRAPHEME_COUNT:
        case MMAY_OPCODE_TEXT_IS_RUNE_BOUNDARY:
        case MMAY_OPCODE_TEXT_DROP_PREFIX:
        case MMAY_OPCODE_TEXT_TAKE_PREFIX:
        case MMAY_OPCODE_TEXT_CONTAINS:
        case MMAY_OPCODE_TEXT_STARTS_WITH:
        case MMAY_OPCODE_TEXT_ENDS_WITH:
        case MMAY_OPCODE_TEXT_TRIM_ASCII:
        case MMAY_OPCODE_TEXT_SPLIT_COUNT:
        case MMAY_OPCODE_TEXT_SPLIT:
        case MMAY_OPCODE_TEXT_JOIN:
        case MMAY_OPCODE_TEXT_RUNES:
        case MMAY_OPCODE_TEXT_GRAPHEMES:
        case MMAY_OPCODE_TEXT_ENCODE_UTF8:
        case MMAY_OPCODE_TEXT_INVALID_UTF8_OFFSET:
        case MMAY_OPCODE_TEXT_DECODE_UTF8:
            return mmay_bytecode_run_text_instruction(
                instruction, registers, execution, function, allocator,
                stdout_sink
            );
        case MMAY_OPCODE_PATH_NORMALIZE:
        case MMAY_OPCODE_PATH_IS_ABSOLUTE:
        case MMAY_OPCODE_FS_OPEN_READ:
        case MMAY_OPCODE_FS_READ:
        case MMAY_OPCODE_FS_CLOSE:
        case MMAY_OPCODE_FS_CLOSE_BEST_EFFORT:
        case MMAY_OPCODE_FS_READ_ALL:
        case MMAY_OPCODE_FS_WRITE_ATOMIC:
        case MMAY_OPCODE_FS_METADATA:
        case MMAY_OPCODE_FS_READ_DIRECTORY:
            return mmay_bytecode_run_fs_instruction(
                instruction, registers, execution, function
            );
        case MMAY_OPCODE_ENV_GET:
        case MMAY_OPCODE_PROCESS_CURRENT_DIRECTORY:
        case MMAY_OPCODE_PROCESS_EXECUTABLE_PATH:
            return mmay_bytecode_run_context_instruction(
                instruction, registers, execution, function
            );
        case MMAY_OPCODE_COLLECTION_NEW:
        case MMAY_OPCODE_COLLECTION_LENGTH:
        case MMAY_OPCODE_COLLECTION_CONTAINS:
        case MMAY_OPCODE_COLLECTION_GET:
        case MMAY_OPCODE_COLLECTION_PUT:
        case MMAY_OPCODE_COLLECTION_ADD:
        case MMAY_OPCODE_COLLECTION_REMOVE:
        case MMAY_OPCODE_COLLECTION_CLEAR:
        case MMAY_OPCODE_COLLECTION_GENERATION:
        case MMAY_OPCODE_COLLECTION_REQUIRE_GENERATION:
        case MMAY_OPCODE_COLLECTION_ENTRY_KEY:
        case MMAY_OPCODE_COLLECTION_ENTRY_VALUE:
        case MMAY_OPCODE_COLLECTION_NEXT:
        case MMAY_OPCODE_COLLECTION_CURSOR_KEY:
        case MMAY_OPCODE_COLLECTION_CURSOR_VALUE:
        case MMAY_OPCODE_COLLECTION_GET_OPTION:
        case MMAY_OPCODE_COLLECTION_REMOVE_OPTION:
        case MMAY_OPCODE_COLLECTION_PUT_RESULT:
        case MMAY_OPCODE_COLLECTION_ADD_RESULT: {
            bool handled = false;
            MarimayBytecodeRunStatus status =
                mmay_bytecode_run_collection_instruction(
                    instruction, registers, execution, function, allocator,
                    panic, symbol, &handled
                );
            return handled ? status : MMAY_BYTECODE_RUN_INVALID_BYTECODE;
        }
        case MMAY_OPCODE_IO_PRINTLN:
        case MMAY_OPCODE_IO_WRITE:
        case MMAY_OPCODE_IO_FLUSH:
        case MMAY_OPCODE_IO_READ_LINE:
        case MMAY_OPCODE_IO_READ_BYTES:
        case MMAY_OPCODE_IO_WRITE_BYTES:
            return mmay_bytecode_run_io_instruction(
                instruction, registers, execution, function,
                allocator, stdout_sink
            );
        case MMAY_OPCODE_TEST_ASSERT:
        case MMAY_OPCODE_TEST_OUTPUT_BEGIN:
        case MMAY_OPCODE_TEST_OUTPUT_END:
            return mmay_bytecode_run_test_output_instruction(
                instruction, registers, execution, function,
                allocator, panic, symbol
            );
        default:
            return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
    }
    return MMAY_BYTECODE_RUN_INVALID_BYTECODE;
}
