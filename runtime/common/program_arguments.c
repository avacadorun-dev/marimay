#include "mmay/runtime/program_arguments.h"

#include <string.h>

#include "mmay/base/utf8.h"
#include "mmay/runtime/string.h"

MarimayProgramArgumentsStatus mmay_program_arguments_validate(
    MarimayProgramArgumentList arguments
) {
    size_t total = 0;
    size_t index = 0;
    if (arguments.count > MMAY_PROGRAM_ARGUMENT_MAX_COUNT) {
        return MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT;
    }
    if (arguments.values == NULL && arguments.count != 0) {
        return MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT;
    }
    for (index = 0; index < arguments.count; index += 1) {
        const MarimayProgramArgument *argument = &arguments.values[index];
        if (argument->data == NULL && argument->length != 0) {
            return MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT;
        }
        if (argument->length > MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES
            || !mmay_size_add(total, argument->length, &total)
            || total > MMAY_PROGRAM_ARGUMENT_MAX_BYTES) {
            return MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT;
        }
        if (argument->length != 0
            && memchr(argument->data, 0, argument->length) != NULL) {
            return MMAY_PROGRAM_ARGUMENTS_CONTAINS_NUL;
        }
        if (!mmay_utf8_validate(
                (const char *)argument->data, argument->length, NULL
            )) {
            return MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8;
        }
    }
    return MMAY_PROGRAM_ARGUMENTS_OK;
}

MarimayManagedStatus mmay_program_arguments_managed(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayDescriptor *descriptor,
    MarimayProgramArgumentList arguments,
    MarimayManagedReference *result
) {
    MarimayManagedCell *cells = NULL;
    MarimayManagedReference **locations = NULL;
    MarimayManagedRootFrame roots;
    MarimayManagedStatus status = MMAY_MANAGED_OK;
    size_t cell_bytes = 0;
    size_t location_bytes = 0;
    size_t index = 0;
    bool rooted = false;
    if (result != NULL) *result = mmay_managed_invalid_reference();
    memset(&roots, 0, sizeof(roots));
    if (heap == NULL || !heap->initialized || result == NULL
        || descriptor == NULL
        || descriptor->cell_kind != MMAY_MANAGED_CELL_REFERENCE
        || descriptor->storage_kind != MMAY_MANAGED_ARRAY_STORAGE_CELL
        || descriptor->reference_kind != MMAY_MANAGED_OBJECT_STRING
        || descriptor->reference_descriptor != NULL
        || mmay_program_arguments_validate(arguments)
            != MMAY_PROGRAM_ARGUMENTS_OK
        || !mmay_size_multiply(
            arguments.count, sizeof(*cells), &cell_bytes
        ) || !mmay_size_multiply(
            arguments.count, sizeof(*locations), &location_bytes
        )) {
        return MMAY_MANAGED_INVALID_ARGUMENT;
    }
    if (arguments.count != 0) {
        cells = mmay_allocate(&heap->allocator, cell_bytes);
        locations = mmay_allocate(&heap->allocator, location_bytes);
        if (cells == NULL || locations == NULL) {
            status = MMAY_MANAGED_ALLOCATION_FAILED;
            goto cleanup;
        }
        memset(cells, 0, cell_bytes);
        for (index = 0; index < arguments.count; index += 1) {
            cells[index].reference = mmay_managed_invalid_reference();
            locations[index] = &cells[index].reference;
        }
        status = mmay_managed_root_push(
            heap, &roots, locations, arguments.count
        );
        if (status != MMAY_MANAGED_OK) goto cleanup;
        rooted = true;
        for (index = 0; index < arguments.count; index += 1) {
            status = mmay_managed_string_new(
                heap, arguments.values[index].data,
                arguments.values[index].length,
                &cells[index].reference
            );
            if (status != MMAY_MANAGED_OK) goto cleanup;
        }
    }
    status = mmay_managed_array_new(
        heap, descriptor, cells, arguments.count, result
    );
cleanup:
    if (rooted) {
        MarimayManagedStatus pop_status = mmay_managed_root_pop(heap, &roots);
        if (status == MMAY_MANAGED_OK) status = pop_status;
    }
    mmay_deallocate(&heap->allocator, locations, location_bytes);
    mmay_deallocate(&heap->allocator, cells, cell_bytes);
    if (status != MMAY_MANAGED_OK) {
        *result = mmay_managed_invalid_reference();
    }
    return status;
}
