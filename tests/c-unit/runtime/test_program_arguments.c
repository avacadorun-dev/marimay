#include "../test_support.h"

#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/runtime/managed.h"
#include "mmay/runtime/program_arguments.h"

static const MarimayManagedArrayDescriptor mmay_test_argument_strings = {
    MMAY_MANAGED_CELL_REFERENCE,
    MMAY_MANAGED_ARRAY_STORAGE_CELL,
    sizeof(MarimayManagedCell),
    MMAY_MANAGED_OBJECT_STRING,
    NULL
};

static MarimayProgramArgument mmay_test_argument(
    const unsigned char *data,
    size_t length
) {
    MarimayProgramArgument argument;
    argument.data = data;
    argument.length = length;
    return argument;
}

static bool mmay_test_argument_heap_init(
    MarimayManagedHeap *heap,
    MarimayAllocator allocator
) {
    MarimayManagedHeapOptions options = mmay_managed_heap_default_options();
    options.max_objects = 64;
    options.max_live_bytes = 2U * 1024U * 1024U;
    options.max_array_length = MMAY_PROGRAM_ARGUMENT_MAX_COUNT;
    return mmay_managed_heap_init(heap, allocator, options);
}

static void mmay_test_program_argument_validation(void) {
    static unsigned char maximum[MMAY_PROGRAM_ARGUMENT_MAX_SINGLE_BYTES];
    static const unsigned char utf8[] = "\xd0\x9c\xd0\xb8\xd1\x80";
    static const unsigned char invalid_utf8[] = {0xff};
    static const unsigned char nul[] = {'a', 0, 'b'};
    MarimayProgramArgument values[5];
    MarimayProgramArgument empty_values[MMAY_PROGRAM_ARGUMENT_MAX_COUNT];
    MarimayProgramArgumentList list;
    size_t index = 0;
    memset(maximum, 'a', sizeof(maximum));
    list.values = NULL;
    list.count = 0;
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_OK);
    values[0] = mmay_test_argument(NULL, 0);
    values[1] = mmay_test_argument(utf8, sizeof(utf8) - 1);
    list.values = values;
    list.count = 2;
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_OK);
    values[0] = mmay_test_argument(invalid_utf8, sizeof(invalid_utf8));
    list.count = 1;
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8);
    values[0] = mmay_test_argument(nul, sizeof(nul));
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_CONTAINS_NUL);
    values[0] = mmay_test_argument(maximum, sizeof(maximum) + 1);
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT);
    for (index = 0; index < 5; index += 1) {
        values[index] = mmay_test_argument(maximum, sizeof(maximum));
    }
    list.count = 4;
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_OK);
    list.count = 5;
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT);
    for (index = 0; index < MMAY_PROGRAM_ARGUMENT_MAX_COUNT; index += 1) {
        empty_values[index] = mmay_test_argument(NULL, 0);
    }
    list.values = empty_values;
    list.count = MMAY_PROGRAM_ARGUMENT_MAX_COUNT;
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_OK);
    list.values = NULL;
    list.count = 1;
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT);
    list.count = MMAY_PROGRAM_ARGUMENT_MAX_COUNT + 1U;
    MMAY_EXPECT(mmay_program_arguments_validate(list)
        == MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT);
}

static void mmay_test_host_argument_import(void) {
#if defined(_WIN32)
    static const wchar_t *const values[] = {
        L"", L"two words", L"--", L"\x041c\x0438\x0440"
    };
    static const wchar_t invalid_value[] = {(wchar_t)0xd800, 0};
    static const wchar_t *const invalid_values[] = {invalid_value};
#else
    static const char *const values[] = {
        "", "two words", "--", "\xd0\x9c\xd0\xb8\xd1\x80"
    };
    static const char invalid_value[] = {(char)0xff, 0};
    static const char *const invalid_values[] = {invalid_value};
#endif
    MarimayFailAllocator state;
    MarimayHostArguments imported;
    MarimayProgramArgumentsStatus status;
    memset(&imported, 0, sizeof(imported));
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    status = mmay_host_arguments_import(
        mmay_fail_allocator(&state), values, 4,
        MMAY_PROGRAM_ARGUMENT_MAX_COUNT, MMAY_PROGRAM_ARGUMENT_MAX_BYTES,
        &imported
    );
    MMAY_EXPECT(status == MMAY_PROGRAM_ARGUMENTS_OK);
    MMAY_EXPECT(imported.count == 4 && imported.values[4] == NULL);
    MMAY_EXPECT(imported.views[0].length == 0);
    MMAY_EXPECT(strcmp(imported.values[1], "two words") == 0);
    MMAY_EXPECT(strcmp(imported.values[2], "--") == 0);
    MMAY_EXPECT(imported.views[3].length == 6);
    MMAY_EXPECT(memcmp(
        imported.views[3].data,
        "\xd0\x9c\xd0\xb8\xd1\x80", 6
    ) == 0);
    mmay_host_arguments_dispose(&imported);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);

    mmay_fail_allocator_init(&state, mmay_system_allocator(), 0);
    status = mmay_host_arguments_import(
        mmay_fail_allocator(&state), values, 4,
        MMAY_PROGRAM_ARGUMENT_MAX_COUNT, MMAY_PROGRAM_ARGUMENT_MAX_BYTES,
        &imported
    );
    MMAY_EXPECT(status == MMAY_PROGRAM_ARGUMENTS_ALLOCATION_FAILED);
    mmay_host_arguments_dispose(&imported);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    MMAY_EXPECT(mmay_host_arguments_import(
        mmay_system_allocator(), values, 4, 3,
        MMAY_PROGRAM_ARGUMENT_MAX_BYTES, &imported
    ) == MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT);
    MMAY_EXPECT(mmay_host_arguments_import(
        mmay_system_allocator(), values, 4,
        MMAY_PROGRAM_ARGUMENT_MAX_COUNT, 3, &imported
    ) == MMAY_PROGRAM_ARGUMENTS_BYTE_LIMIT);
    MMAY_EXPECT(mmay_host_arguments_import(
        mmay_system_allocator(), NULL, 1,
        MMAY_PROGRAM_ARGUMENT_MAX_COUNT,
        MMAY_PROGRAM_ARGUMENT_MAX_BYTES, &imported
    ) == MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_host_arguments_import(
        mmay_system_allocator(), values, 4, 0,
        MMAY_PROGRAM_ARGUMENT_MAX_BYTES, &imported
    ) == MMAY_PROGRAM_ARGUMENTS_COUNT_LIMIT);
    MMAY_EXPECT(mmay_host_arguments_import(
        mmay_system_allocator(), values, 4,
        MMAY_PROGRAM_ARGUMENT_MAX_COUNT,
        MMAY_PROGRAM_ARGUMENT_MAX_BYTES, NULL
    ) == MMAY_PROGRAM_ARGUMENTS_INVALID_ARGUMENT);
    mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
    MMAY_EXPECT(mmay_host_arguments_import(
        mmay_fail_allocator(&state), invalid_values, 1,
        MMAY_PROGRAM_ARGUMENT_MAX_COUNT,
        MMAY_PROGRAM_ARGUMENT_MAX_BYTES, &imported
    ) == MMAY_PROGRAM_ARGUMENTS_INVALID_UTF8);
    mmay_host_arguments_dispose(&imported);
    MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    mmay_host_arguments_dispose(NULL);
}

static void mmay_test_managed_program_arguments(void) {
    static const unsigned char first[] = "";
    static const unsigned char second[] = "two words";
    static const unsigned char third[] = "\xd0\x9c\xd0\xb8\xd1\x80";
    MarimayProgramArgument values[] = {
        {first, 0}, {second, sizeof(second) - 1}, {third, sizeof(third) - 1}
    };
    MarimayProgramArgumentList list = {values, 3};
    MarimayManagedHeap heap;
    MarimayManagedReference array = mmay_managed_invalid_reference();
    MarimayManagedCell cell;
    const unsigned char *bytes = NULL;
    size_t length = 0;
    size_t array_length = 0;
    MarimayManagedArrayDescriptor invalid_descriptor =
        mmay_test_argument_strings;
    memset(&heap, 0, sizeof(heap));
    memset(&cell, 0, sizeof(cell));
    MMAY_EXPECT(mmay_test_argument_heap_init(
        &heap, mmay_system_allocator()
    ));
    MMAY_EXPECT(mmay_program_arguments_managed(
        NULL, &mmay_test_argument_strings, list, &array
    ) == MMAY_MANAGED_INVALID_ARGUMENT);
    invalid_descriptor.reference_kind = MMAY_MANAGED_OBJECT_ARRAY;
    MMAY_EXPECT(mmay_program_arguments_managed(
        &heap, &invalid_descriptor, list, &array
    ) == MMAY_MANAGED_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_program_arguments_managed(
        &heap, &mmay_test_argument_strings, list, &array
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_array_length(
        &heap, array, &array_length
    ) == MMAY_MANAGED_OK && array_length == 3);
    MMAY_EXPECT(mmay_managed_array_get(
        &heap, array, 1, &cell, &array_length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(mmay_managed_string_bytes(
        &heap, cell.reference, &bytes, &length
    ) == MMAY_MANAGED_OK);
    MMAY_EXPECT(length == sizeof(second) - 1
        && memcmp(bytes, second, length) == 0);
    mmay_managed_heap_dispose(&heap);
}

static void mmay_test_managed_program_argument_failures(void) {
    static const unsigned char first[] = "alpha";
    static const unsigned char second[] = "two words";
    MarimayProgramArgument values[] = {
        {first, sizeof(first) - 1}, {second, sizeof(second) - 1}
    };
    MarimayProgramArgumentList list = {values, 2};
    size_t offset = 0;
    bool reached_success = false;
    for (offset = 0; offset < 32 && !reached_success; offset += 1) {
        MarimayFailAllocator state;
        MarimayManagedHeap heap;
        MarimayManagedReference result = mmay_managed_invalid_reference();
        MarimayManagedStatus status;
        memset(&heap, 0, sizeof(heap));
        mmay_fail_allocator_init(&state, mmay_system_allocator(), SIZE_MAX);
        MMAY_EXPECT(mmay_test_argument_heap_init(
            &heap, mmay_fail_allocator(&state)
        ));
        state.failure_index = state.attempts + offset;
        status = mmay_program_arguments_managed(
            &heap, &mmay_test_argument_strings, list, &result
        );
        if (status == MMAY_MANAGED_OK) reached_success = true;
        else MMAY_EXPECT(status == MMAY_MANAGED_ALLOCATION_FAILED);
        mmay_managed_heap_dispose(&heap);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
    }
    MMAY_EXPECT(reached_success);
}

void mmay_test_program_arguments(void) {
    mmay_test_program_argument_validation();
    mmay_test_host_argument_import();
    mmay_test_managed_program_arguments();
    mmay_test_managed_program_argument_failures();
}
