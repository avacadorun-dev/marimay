#ifndef MMAY_RUNTIME_PROGRAM_ARGUMENTS_H
#define MMAY_RUNTIME_PROGRAM_ARGUMENTS_H

#include <stddef.h>

#if defined(_WIN32)
#include <wchar.h>
typedef wchar_t MarimayHostArgumentChar;
#else
typedef char MarimayHostArgumentChar;
#endif

#include "mmay/base/allocator.h"
#include "mmay/runtime/array.h"

enum {
    MMAY_PROGRAM_ARGUMENT_MAX_COUNT = 4096,
    MMAY_PROGRAM_ARGUMENT_MAX_BYTES = 1024 * 1024,
    MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES = 256 * 1024
};

typedef struct MarimayProgramArgument {
    const unsigned char *data;
    size_t length;
} MarimayProgramArgument;

typedef struct MarimayProgramArgumentList {
    const MarimayProgramArgument *values;
    size_t count;
} MarimayProgramArgumentList;

typedef enum MarimayProgramArgumentsStatus {
    MMAY_PROGRAM_ARGUMENTS_OK,
    MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT,
    MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8,
    MMAY_PROGRAM_ARGUMENTS_CONTAINS_NUL,
    MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT,
    MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT,
    MMAY_PROGRAM_ARGUMENTS_ALLOCATION_FAILED
} MarimayProgramArgumentsStatus;

typedef struct MarimayHostArguments {
    MarimayAllocator allocator;
    char **values;
    MarimayProgramArgument *views;
    size_t count;
    size_t allocation_size;
} MarimayHostArguments;

MarimayProgramArgumentsStatus mmay_program_arguments_validate(
    MarimayProgramArgumentList arguments
);

/* Imports one host argv slice as owned, NUL-terminated UTF-8 strings. */
MarimayProgramArgumentsStatus mmay_host_arguments_import(
    MarimayAllocator allocator,
    const MarimayHostArgumentChar *const values[],
    size_t count,
    size_t max_count,
    size_t max_total_bytes,
    MarimayHostArguments *result
);
void mmay_host_arguments_dispose(MarimayHostArguments *arguments);

/* Constructs the exact managed []string value described by descriptor. */
MarimayManagedStatus mmay_program_arguments_managed(
    MarimayManagedHeap *heap,
    const MarimayManagedArrayDescriptor *descriptor,
    MarimayProgramArgumentList arguments,
    MarimayManagedReference *result
);

#endif
