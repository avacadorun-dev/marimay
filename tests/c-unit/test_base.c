#include "test_support.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/arena.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/string_literal.h"
#include "ruda/base/vector.h"

typedef struct RudaAlignedFixture {
    max_align_t alignment;
    int value;
} RudaAlignedFixture;

void ruda_test_base(void) {
    RudaAllocator system = ruda_system_allocator();
    RudaVector vector;
    RudaOwnedBytes bytes;
    RudaArena arena;
    RudaFailAllocator fail_state;
    RudaAllocator failing;
    int value = 0;
    size_t index = 0;
    static const unsigned char payload[] = {'a', 0, 'b'};
    static const unsigned char ordinary_spelling[] =
        "\"line\\n\\u{26cf}\\0end\\'\"";
    static const unsigned char ordinary_value[] =
        "line\n\xe2\x9b\x8f\0end'";
    static const unsigned char raw_spelling[] = "r\"path\\\\raw\"";
    static const unsigned char raw_value[] = "path\\\\raw";
    RudaOwnedBytes decoded;

    RUDA_EXPECT(ruda_vector_init(&vector, system, sizeof(int)));
    for (index = 0; index < 100; index += 1) {
        value = (int)index;
        RUDA_EXPECT(ruda_vector_push(&vector, &value));
    }
    RUDA_EXPECT(vector.length == 100);
    RUDA_EXPECT(vector.capacity >= vector.length);
    for (index = 0; index < vector.length; index += 1) {
        const int *element = ruda_vector_at_const(&vector, index);
        RUDA_EXPECT(element != NULL);
        if (element != NULL) {
            RUDA_EXPECT(*element == (int)index);
        }
    }
    RUDA_EXPECT(ruda_vector_resize_zeroed(&vector, 110));
    for (index = 100; index < 110; index += 1) {
        const int *element = ruda_vector_at_const(&vector, index);
        RUDA_EXPECT(element != NULL && *element == 0);
    }
    RUDA_EXPECT(ruda_vector_resize_zeroed(&vector, 3));
    RUDA_EXPECT(vector.length == 3);
    RUDA_EXPECT(ruda_vector_at(&vector, 3) == NULL);
    ruda_vector_dispose(&vector);

    ruda_fail_allocator_init(&fail_state, system, 0);
    failing = ruda_fail_allocator(&fail_state);
    RUDA_EXPECT(ruda_vector_init(&vector, failing, sizeof(int)));
    value = 7;
    RUDA_EXPECT(!ruda_vector_push(&vector, &value));
    RUDA_EXPECT(vector.data == NULL);
    RUDA_EXPECT(vector.length == 0);
    ruda_vector_dispose(&vector);
    RUDA_EXPECT(fail_state.live_allocations == 0);

    RUDA_EXPECT(ruda_owned_bytes_copy(&bytes, system, payload, sizeof(payload)));
    RUDA_EXPECT(bytes.length == sizeof(payload));
    RUDA_EXPECT(bytes.data != NULL);
    if (bytes.data != NULL) {
        RUDA_EXPECT(bytes.data[0] == 'a');
        RUDA_EXPECT(bytes.data[1] == 0);
        RUDA_EXPECT(bytes.data[2] == 'b');
        RUDA_EXPECT(bytes.data[3] == 0);
    }
    ruda_owned_bytes_dispose(&bytes);

    RUDA_EXPECT(ruda_string_literal_decode(
        ordinary_spelling,
        sizeof(ordinary_spelling) - 1,
        false,
        system,
        &decoded
    ) == RUDA_STRING_LITERAL_OK);
    RUDA_EXPECT(decoded.length == sizeof(ordinary_value) - 1);
    RUDA_EXPECT(decoded.data != NULL && memcmp(
        decoded.data,
        ordinary_value,
        sizeof(ordinary_value) - 1
    ) == 0);
    ruda_owned_bytes_dispose(&decoded);

    RUDA_EXPECT(ruda_string_literal_decode(
        raw_spelling,
        sizeof(raw_spelling) - 1,
        true,
        system,
        &decoded
    ) == RUDA_STRING_LITERAL_OK);
    RUDA_EXPECT(decoded.length == sizeof(raw_value) - 1);
    RUDA_EXPECT(decoded.data != NULL && memcmp(
        decoded.data,
        raw_value,
        sizeof(raw_value) - 1
    ) == 0);
    ruda_owned_bytes_dispose(&decoded);

    RUDA_EXPECT(ruda_string_literal_decode(
        (const unsigned char *)"\"\\u{}\"",
        sizeof("\"\\u{}\"") - 1,
        false,
        system,
        &decoded
    ) == RUDA_STRING_LITERAL_INVALID);
    RUDA_EXPECT(decoded.data == NULL && decoded.length == 0);

    ruda_fail_allocator_init(&fail_state, system, 0);
    failing = ruda_fail_allocator(&fail_state);
    RUDA_EXPECT(ruda_string_literal_decode(
        ordinary_spelling,
        sizeof(ordinary_spelling) - 1,
        false,
        failing,
        &decoded
    ) == RUDA_STRING_LITERAL_ALLOCATION_FAILED);
    RUDA_EXPECT(decoded.data == NULL && decoded.length == 0);
    RUDA_EXPECT(fail_state.live_allocations == 0);

    ruda_arena_init(&arena, system, 32);
    for (index = 0; index < 20; index += 1) {
        RudaAlignedFixture *fixture = RUDA_ARENA_NEW(&arena, RudaAlignedFixture);
        RUDA_EXPECT(fixture != NULL);
        if (fixture != NULL) {
            RUDA_EXPECT((uintptr_t)fixture % _Alignof(RudaAlignedFixture) == 0);
            fixture->value = (int)index;
        }
    }
    RUDA_EXPECT(arena.blocks != NULL);
    ruda_arena_reset(&arena);
    RUDA_EXPECT(arena.blocks == NULL);

    ruda_fail_allocator_init(&fail_state, system, 0);
    failing = ruda_fail_allocator(&fail_state);
    ruda_arena_init(&arena, failing, 32);
    RUDA_EXPECT(RUDA_ARENA_NEW(&arena, RudaAlignedFixture) == NULL);
    ruda_arena_dispose(&arena);
    RUDA_EXPECT(fail_state.live_allocations == 0);
}
