#include "test_support.h"

#include <fenv.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/allocator.h"
#include "mmay/base/arena.h"
#include "mmay/base/f64.h"
#include "mmay/base/owned_bytes.h"
#include "mmay/base/string_literal.h"
#include "mmay/base/vector.h"

typedef struct MarimayAlignedFixture {
    max_align_t alignment;
    int value;
} MarimayAlignedFixture;

static void mmay_test_f64_decimal_parser(void) {
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
        MMAY_EXPECT(mmay_f64_parse_decimal(
            (const unsigned char *)cases[index].source,
            strlen(cases[index].source),
            mmay_system_allocator(),
            4096,
            &bits
        ) == MMAY_F64_PARSE_OK);
        MMAY_EXPECT(bits == cases[index].bits);
    }
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index += 1) {
        uint64_t bits = UINT64_MAX;
        MMAY_EXPECT(mmay_f64_parse_decimal(
            (const unsigned char *)invalid[index],
            strlen(invalid[index]),
            mmay_system_allocator(),
            4096,
            &bits
        ) == MMAY_F64_PARSE_INVALID_SYNTAX);
    }
    {
        MarimayFailAllocator state;
        uint64_t bits = UINT64_MAX;
        mmay_fail_allocator_init(&state, mmay_system_allocator(), 0);
        MMAY_EXPECT(mmay_f64_parse_decimal(
            (const unsigned char *)"1.25", 4, mmay_fail_allocator(&state),
            4096, &bits
        ) == MMAY_F64_PARSE_ALLOCATION_FAILED);
        MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
        MMAY_EXPECT(mmay_f64_parse_decimal(
            (const unsigned char *)"123.0", 5, mmay_system_allocator(),
            2, &bits
        ) == MMAY_F64_PARSE_LIMIT_REACHED);
    }
    {
        size_t failure_index = 0;
        bool reached_success = false;
        for (failure_index = 0; failure_index < 256; failure_index += 1) {
            MarimayFailAllocator state;
            MarimayF64ParseStatus status;
            uint64_t bits = UINT64_MAX;
            mmay_fail_allocator_init(
                &state,
                mmay_system_allocator(),
                failure_index
            );
            status = mmay_f64_parse_decimal(
                (const unsigned char *)"1.7976931348623157e308",
                strlen("1.7976931348623157e308"),
                mmay_fail_allocator(&state),
                4096,
                &bits
            );
            MMAY_EXPECT(status == MMAY_F64_PARSE_ALLOCATION_FAILED
                || status == MMAY_F64_PARSE_OK);
            MMAY_EXPECT(state.live_allocations == 0 && state.live_bytes == 0);
            if (status == MMAY_F64_PARSE_OK) {
                MMAY_EXPECT(bits == UINT64_C(0x7fefffffffffffff));
                reached_success = true;
                break;
            }
        }
        MMAY_EXPECT(reached_success);
    }
}

static void mmay_test_f64_strict_arithmetic(void) {
    static const int rounding_modes[] = {
        FE_TONEAREST,
        FE_DOWNWARD,
        FE_UPWARD,
        FE_TOWARDZERO
    };
    int original = fegetround();
    size_t index = 0;
    MMAY_EXPECT(mmay_f64_binary64_supported());
    MMAY_EXPECT(original != -1);
    for (index = 0;
        index < sizeof(rounding_modes) / sizeof(rounding_modes[0]);
        index += 1) {
        if (fesetround(rounding_modes[index]) != 0) {
            continue;
        }
        MMAY_EXPECT(mmay_f64_add(
            UINT64_C(0x3ff0000000000000),
            UINT64_C(0x3ca8000000000000)
        ) == UINT64_C(0x3ff0000000000001));
        MMAY_EXPECT(fegetround() == rounding_modes[index]);
        MMAY_EXPECT(mmay_f64_divide(
            UINT64_C(0x0000000000000000),
            UINT64_C(0x0000000000000000)
        ) == UINT64_C(0x7ff8000000000000));
        MMAY_EXPECT(fegetround() == rounding_modes[index]);
    }
    if (original != -1) {
        MMAY_EXPECT(fesetround(original) == 0);
    }
    MMAY_EXPECT(mmay_f64_equal(
        UINT64_C(0x0000000000000000),
        UINT64_C(0x8000000000000000)
    ));
    MMAY_EXPECT(!mmay_f64_less(
        UINT64_C(0x8000000000000000),
        UINT64_C(0x0000000000000000)
    ));
    MMAY_EXPECT(mmay_f64_less(
        UINT64_C(0xbff0000000000000),
        UINT64_C(0x0000000000000000)
    ));
    MMAY_EXPECT(mmay_f64_less(
        UINT64_C(0x0000000000000000),
        UINT64_C(0x3ff0000000000000)
    ));
    MMAY_EXPECT(!mmay_f64_less_equal(
        UINT64_C(0x7ff8000000000001),
        UINT64_C(0x7ff8000000000001)
    ));
}

void mmay_test_base(void) {
    MarimayAllocator system = mmay_system_allocator();
    MarimayVector vector;
    MarimayOwnedBytes bytes;
    MarimayArena arena;
    MarimayFailAllocator fail_state;
    MarimayAllocator failing;
    int value = 0;
    size_t index = 0;
    static const unsigned char payload[] = {'a', 0, 'b'};
    static const unsigned char ordinary_spelling[] =
        "\"line\\n\\u{26cf}\\0end\\'\"";
    static const unsigned char ordinary_value[] =
        "line\n\xe2\x9b\x8f\0end'";
    static const unsigned char raw_spelling[] = "r\"path\\\\raw\"";
    static const unsigned char raw_value[] = "path\\\\raw";
    MarimayOwnedBytes decoded;

    mmay_test_f64_decimal_parser();
    mmay_test_f64_strict_arithmetic();

    MMAY_EXPECT(mmay_vector_init(&vector, system, sizeof(int)));
    for (index = 0; index < 100; index += 1) {
        value = (int)index;
        MMAY_EXPECT(mmay_vector_push(&vector, &value));
    }
    MMAY_EXPECT(vector.length == 100);
    MMAY_EXPECT(vector.capacity >= vector.length);
    for (index = 0; index < vector.length; index += 1) {
        const int *element = mmay_vector_at_const(&vector, index);
        MMAY_EXPECT(element != NULL);
        if (element != NULL) {
            MMAY_EXPECT(*element == (int)index);
        }
    }
    MMAY_EXPECT(mmay_vector_resize_zeroed(&vector, 110));
    for (index = 100; index < 110; index += 1) {
        const int *element = mmay_vector_at_const(&vector, index);
        MMAY_EXPECT(element != NULL && *element == 0);
    }
    MMAY_EXPECT(mmay_vector_resize_zeroed(&vector, 3));
    MMAY_EXPECT(vector.length == 3);
    MMAY_EXPECT(mmay_vector_at(&vector, 3) == NULL);
    mmay_vector_dispose(&vector);

    mmay_fail_allocator_init(&fail_state, system, 0);
    failing = mmay_fail_allocator(&fail_state);
    MMAY_EXPECT(mmay_vector_init(&vector, failing, sizeof(int)));
    value = 7;
    MMAY_EXPECT(!mmay_vector_push(&vector, &value));
    MMAY_EXPECT(vector.data == NULL);
    MMAY_EXPECT(vector.length == 0);
    mmay_vector_dispose(&vector);
    MMAY_EXPECT(fail_state.live_allocations == 0);
    MMAY_EXPECT(fail_state.live_bytes == 0);

    MMAY_EXPECT(mmay_owned_bytes_copy(&bytes, system, payload, sizeof(payload)));
    MMAY_EXPECT(bytes.length == sizeof(payload));
    MMAY_EXPECT(bytes.data != NULL);
    if (bytes.data != NULL) {
        MMAY_EXPECT(bytes.data[0] == 'a');
        MMAY_EXPECT(bytes.data[1] == 0);
        MMAY_EXPECT(bytes.data[2] == 'b');
        MMAY_EXPECT(bytes.data[3] == 0);
    }
    mmay_owned_bytes_dispose(&bytes);

    MMAY_EXPECT(mmay_string_literal_decode(
        ordinary_spelling,
        sizeof(ordinary_spelling) - 1,
        false,
        system,
        &decoded
    ) == MMAY_STRING_LITERAL_OK);
    MMAY_EXPECT(decoded.length == sizeof(ordinary_value) - 1);
    MMAY_EXPECT(decoded.data != NULL && memcmp(
        decoded.data,
        ordinary_value,
        sizeof(ordinary_value) - 1
    ) == 0);
    mmay_owned_bytes_dispose(&decoded);

    MMAY_EXPECT(mmay_string_literal_decode(
        raw_spelling,
        sizeof(raw_spelling) - 1,
        true,
        system,
        &decoded
    ) == MMAY_STRING_LITERAL_OK);
    MMAY_EXPECT(decoded.length == sizeof(raw_value) - 1);
    MMAY_EXPECT(decoded.data != NULL && memcmp(
        decoded.data,
        raw_value,
        sizeof(raw_value) - 1
    ) == 0);
    mmay_owned_bytes_dispose(&decoded);

    MMAY_EXPECT(mmay_string_literal_decode(
        (const unsigned char *)"\"\\u{}\"",
        sizeof("\"\\u{}\"") - 1,
        false,
        system,
        &decoded
    ) == MMAY_STRING_LITERAL_INVALID);
    MMAY_EXPECT(decoded.data == NULL && decoded.length == 0);

    mmay_fail_allocator_init(&fail_state, system, 0);
    failing = mmay_fail_allocator(&fail_state);
    MMAY_EXPECT(mmay_string_literal_decode(
        ordinary_spelling,
        sizeof(ordinary_spelling) - 1,
        false,
        failing,
        &decoded
    ) == MMAY_STRING_LITERAL_ALLOCATION_FAILED);
    MMAY_EXPECT(decoded.data == NULL && decoded.length == 0);
    MMAY_EXPECT(fail_state.live_allocations == 0);
    MMAY_EXPECT(fail_state.live_bytes == 0);

    mmay_arena_init(&arena, system, 32);
    for (index = 0; index < 20; index += 1) {
        MarimayAlignedFixture *fixture = MMAY_ARENA_NEW(&arena, MarimayAlignedFixture);
        MMAY_EXPECT(fixture != NULL);
        if (fixture != NULL) {
            MMAY_EXPECT((uintptr_t)fixture % _Alignof(MarimayAlignedFixture) == 0);
            fixture->value = (int)index;
        }
    }
    MMAY_EXPECT(arena.blocks != NULL);
    mmay_arena_reset(&arena);
    MMAY_EXPECT(arena.blocks == NULL);

    mmay_fail_allocator_init(&fail_state, system, 0);
    failing = mmay_fail_allocator(&fail_state);
    mmay_arena_init(&arena, failing, 32);
    MMAY_EXPECT(MMAY_ARENA_NEW(&arena, MarimayAlignedFixture) == NULL);
    mmay_arena_dispose(&arena);
    MMAY_EXPECT(fail_state.live_allocations == 0);
    MMAY_EXPECT(fail_state.live_bytes == 0);
}
