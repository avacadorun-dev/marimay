#include "test_support.h"

#include <fenv.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/allocator.h"
#include "ruda/base/arena.h"
#include "ruda/base/f64.h"
#include "ruda/base/owned_bytes.h"
#include "ruda/base/string_literal.h"
#include "ruda/base/vector.h"

typedef struct RudaAlignedFixture {
    max_align_t alignment;
    int value;
} RudaAlignedFixture;

static void ruda_test_f64_decimal_parser(void) {
    static const struct {
        const char *source;
        uint64_t bits;
    } cases[] = {
        {"0.0", UINT64_C(0x0000000000000000)},
        {"1.0", UINT64_C(0x3ff0000000000000)},
        {"1_2.5_0", UINT64_C(0x4029000000000000)},
        {"6.02e23", UINT64_C(0x44dfde9f10a8d361)},
        {"9007199254740993.0", UINT64_C(0x4340000000000000)},
        {"5e-324", UINT64_C(0x0000000000000001)},
        {"2.4703282292062327e-324", UINT64_C(0x0000000000000000)},
        {"2.4703282292062328e-324", UINT64_C(0x0000000000000001)},
        {"2.2250738585072014e-308", UINT64_C(0x0010000000000000)},
        {"1.7976931348623157e308", UINT64_C(0x7fefffffffffffff)},
        {"1.7976931348623159e308", UINT64_C(0x7ff0000000000000)},
        {"1e999999", UINT64_C(0x7ff0000000000000)},
        {"1e-999999", UINT64_C(0x0000000000000000)}
    };
    static const char *const invalid[] = {
        "1", ".5", "1.", "1e", "1e+", "1__0.0", "1.0f64"
    };
    size_t index = 0;
    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index += 1) {
        uint64_t bits = UINT64_MAX;
        RUDA_EXPECT(ruda_f64_parse_decimal(
            (const unsigned char *)cases[index].source,
            strlen(cases[index].source),
            ruda_system_allocator(),
            4096,
            &bits
        ) == RUDA_F64_PARSE_OK);
        RUDA_EXPECT(bits == cases[index].bits);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        uint64_t bits = UINT64_MAX;
        RUDA_EXPECT(ruda_f64_parse_decimal(
            (const unsigned char *)invalid[index],
            strlen(invalid[index]),
            ruda_system_allocator(),
            4096,
            &bits
        ) == RUDA_F64_PARSE_INVALID_SYNTAX);
    }
    {
        RudaFailAllocator state;
        uint64_t bits = UINT64_MAX;
        ruda_fail_allocator_init(&state, ruda_system_allocator(), 0);
        RUDA_EXPECT(ruda_f64_parse_decimal(
            (const unsigned char *)"1.25", 4, ruda_fail_allocator(&state),
            4096, &bits
        ) == RUDA_F64_PARSE_ALLOCATION_FAILED);
        RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        RUDA_EXPECT(ruda_f64_parse_decimal(
            (const unsigned char *)"123.0", 5, ruda_system_allocator(),
            2, &bits
        ) == RUDA_F64_PARSE_LIMIT_REACHED);
    }
    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 256; failure_index += 1) {
            RudaFailAllocator state;
            RudaF64ParseStatus status;
            uint64_t bits = UINT64_MAX;
            ruda_fail_allocator_init(
                &state,
                ruda_system_allocator(),
                failure_index
            );
            status = ruda_f64_parse_decimal(
                (const unsigned char *)"1.7976931348623157e308",
                strlen("1.7976931348623157e308"),
                ruda_fail_allocator(&state),
                4096,
                &bits
            );
            RUDA_EXPECT(status == RUDA_F64_PARSE_ALLOCATION_FAILED
                || status == RUDA_F64_PARSE_OK);
            RUDA_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
            if (status == RUDA_F64_PARSE_OK) {
                RUDA_EXPECT(bits == UINT64_C(0x7fefffffffffffff));
                reached_success = true;
                break;
            }
        }
        RUDA_EXPECT(reached_success);
    }
}

static void ruda_test_f64_strict_arithmetic(void) {
    static const int rounding_modes[] = {
        FE_TONEAREST,
        FE_DOWNWARD,
        FE_UPWARD,
        FE_TOWARDZERO
    };
    int original = fegetround();
    size_t index = 0;
    RUDA_EXPECT(ruda_f64_binary64_supported());
    RUDA_EXPECT(original != -1);
    for (index = 0;
        index < sizeof(rounding_modes) / sizeof(rounding_modes[0]);
        index += 1) {
        if (fesetround(rounding_modes[index]) != 0) {
            continue;
        }
        RUDA_EXPECT(ruda_f64_add(
            UINT64_C(0x3ff0000000000000),
            UINT64_C(0x3ca8000000000000)
        ) == UINT64_C(0x3ff0000000000001));
        RUDA_EXPECT(fegetround() == rounding_modes[index]);
        RUDA_EXPECT(ruda_f64_divide(
            UINT64_C(0x0000000000000000),
            UINT64_C(0x0000000000000000)
        ) == UINT64_C(0x7ff8000000000000));
        RUDA_EXPECT(fegetround() == rounding_modes[index]);
    }
    if (original != -1) {
        RUDA_EXPECT(fesetround(original) == 0);
    }
    RUDA_EXPECT(ruda_f64_equal(
        UINT64_C(0x0000000000000000),
        UINT64_C(0x8000000000000000)
    ));
    RUDA_EXPECT(!ruda_f64_less(
        UINT64_C(0x8000000000000000),
        UINT64_C(0x0000000000000000)
    ));
    RUDA_EXPECT(ruda_f64_less(
        UINT64_C(0xbff0000000000000),
        UINT64_C(0x0000000000000000)
    ));
    RUDA_EXPECT(ruda_f64_less(
        UINT64_C(0x0000000000000000),
        UINT64_C(0x3ff0000000000000)
    ));
    RUDA_EXPECT(!ruda_f64_less_equal(
        UINT64_C(0x7ff8000000000001),
        UINT64_C(0x7ff8000000000001)
    ));
}

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

    ruda_test_f64_decimal_parser();
    ruda_test_f64_strict_arithmetic();

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
    RUDA_EXPECT(fail_state.live_bytes == 0);

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
    RUDA_EXPECT(fail_state.live_bytes == 0);

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
    RUDA_EXPECT(fail_state.live_bytes == 0);
}
