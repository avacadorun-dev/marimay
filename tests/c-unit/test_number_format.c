#include "test_support.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

#include "ruda/base/f64.h"
#include "ruda/base/number_format.h"

static uint64_t ruda_number_test_parse_f64(const char *text) {
    uint64_t bits = UINT64_MAX;
    RudaF64ParseStatus status = ruda_f64_parse_decimal(
        (const unsigned char *)text,
        strlen(text),
        ruda_system_allocator(),
        4096,
        &bits
    );
    RUDA_EXPECT(status == RUDA_F64_PARSE_OK);
    return bits;
}

static void ruda_number_test_f64_text(uint64_t bits, const char *expected) {
    char output[RUDA_NUMBER_FORMAT_F64_CAPACITY];
    size_t length = 0;
    RudaNumberFormatStatus status = ruda_format_f64(
        bits, output, sizeof(output), &length
    );
    RUDA_EXPECT(status == RUDA_NUMBER_FORMAT_OK);
    RUDA_EXPECT(length == strlen(expected));
    RUDA_EXPECT(strcmp(output, expected) == 0);
}

static void ruda_number_test_f32_text(uint32_t bits, const char *expected) {
    char output[RUDA_NUMBER_FORMAT_F32_CAPACITY];
    size_t length = 0;
    RudaNumberFormatStatus status = ruda_format_f32(
        bits, output, sizeof(output), &length
    );
    RUDA_EXPECT(status == RUDA_NUMBER_FORMAT_OK);
    RUDA_EXPECT(length == strlen(expected));
    RUDA_EXPECT(strcmp(output, expected) == 0);
}

static void ruda_number_test_f64_round_trip(uint64_t bits) {
    char output[RUDA_NUMBER_FORMAT_F64_CAPACITY];
    const char *magnitude = output;
    uint64_t parsed = 0;
    size_t length = 0;
    size_t index = 0;
    RudaNumberFormatStatus status = ruda_format_f64(
        bits, output, sizeof(output), &length
    );
    RUDA_EXPECT(status == RUDA_NUMBER_FORMAT_OK);
    if ((bits >> 52) == UINT64_C(0x7ff)) {
        return;
    }
    if (output[0] == '-') {
        magnitude += 1;
    }
    RUDA_EXPECT(length != 0);
    for (index = 0; index < length; index += 1) {
        RUDA_EXPECT((unsigned char)output[index] < 128u);
    }
    parsed = ruda_number_test_parse_f64(magnitude);
    RUDA_EXPECT(parsed == (bits & UINT64_C(0x7fffffffffffffff)));
}

static void ruda_number_test_f32_round_trip(uint32_t bits) {
    char output[RUDA_NUMBER_FORMAT_F32_CAPACITY];
    const char *magnitude = output;
    uint64_t parsed_bits = 0;
    uint32_t parsed_f32 = 0;
    float rounded = 0.0f;
    size_t length = 0;
    size_t index = 0;
    RudaNumberFormatStatus status = ruda_format_f32(
        bits, output, sizeof(output), &length
    );
    RUDA_EXPECT(status == RUDA_NUMBER_FORMAT_OK);
    if ((bits >> 23) == UINT32_C(0xff)) {
        return;
    }
    if (output[0] == '-') {
        magnitude += 1;
    }
    RUDA_EXPECT(length != 0);
    for (index = 0; index < length; index += 1) {
        RUDA_EXPECT((unsigned char)output[index] < 128u);
    }
    parsed_bits = ruda_number_test_parse_f64(magnitude);
    rounded = (float)ruda_f64_to_double(parsed_bits);
    memcpy(&parsed_f32, &rounded, sizeof(parsed_f32));
    RUDA_EXPECT(parsed_f32 == (bits & UINT32_C(0x7fffffff)));
}

static void ruda_number_test_special_values(void) {
    ruda_number_test_f64_text(
        UINT64_C(0x0000000000000000), "0.0"
    );
    ruda_number_test_f64_text(
        UINT64_C(0x8000000000000000), "-0.0"
    );
    ruda_number_test_f64_text(
        UINT64_C(0x7ff0000000000000), "inf"
    );
    ruda_number_test_f64_text(
        UINT64_C(0xfff0000000000000), "-inf"
    );
    ruda_number_test_f64_text(
        UINT64_C(0x7ff8000000000001), "nan"
    );
    ruda_number_test_f64_text(
        UINT64_C(0xfff0000000000001), "-nan"
    );
    ruda_number_test_f32_text(UINT32_C(0x00000000), "0.0");
    ruda_number_test_f32_text(UINT32_C(0x80000000), "-0.0");
    ruda_number_test_f32_text(UINT32_C(0x7f800000), "inf");
    ruda_number_test_f32_text(UINT32_C(0xff800000), "-inf");
    ruda_number_test_f32_text(UINT32_C(0x7fc00001), "nan");
    ruda_number_test_f32_text(UINT32_C(0xffc00001), "-nan");
}

static void ruda_number_test_d046_boundaries(void) {
    ruda_number_test_f64_text(ruda_number_test_parse_f64("1.0"), "1.0");
    ruda_number_test_f64_text(ruda_number_test_parse_f64("0.0001"), "0.0001");
    ruda_number_test_f64_text(ruda_number_test_parse_f64("0.00001"), "1.0e-5");
    ruda_number_test_f64_text(ruda_number_test_parse_f64("1.0e15"),
        "1000000000000000.0");
    ruda_number_test_f64_text(ruda_number_test_parse_f64("1e16"), "1.0e16");
    ruda_number_test_f64_text(
        UINT64_C(0x0000000000000001), "5.0e-324"
    );
    ruda_number_test_f64_text(
        UINT64_C(0x7fefffffffffffff), "1.7976931348623157e308"
    );
    ruda_number_test_f32_text(UINT32_C(0x3f800000), "1.0");
    ruda_number_test_f32_text(UINT32_C(0x3dcccccd), "0.1");
    ruda_number_test_f32_text(UINT32_C(0x00000001), "1.0e-45");
    ruda_number_test_f32_text(UINT32_C(0x7f7fffff), "3.4028235e38");
}

static void ruda_number_test_buffer_contract(void) {
    char output[RUDA_NUMBER_FORMAT_F64_CAPACITY];
    char untouched[4] = {'x', 'x', 'x', 'x'};
    size_t required = 0;
    size_t length = 0;
    RudaNumberFormatStatus status = ruda_format_f64(
        UINT64_C(0x3ff0000000000000), NULL, 0, &required
    );
    RUDA_EXPECT(status == RUDA_NUMBER_FORMAT_BUFFER_TOO_SMALL);
    RUDA_EXPECT(required == 3);
    status = ruda_format_f64(
        UINT64_C(0x3ff0000000000000), untouched, required, &length
    );
    RUDA_EXPECT(status == RUDA_NUMBER_FORMAT_BUFFER_TOO_SMALL);
    RUDA_EXPECT(length == required);
    RUDA_EXPECT(memcmp(untouched, "xxxx", sizeof(untouched)) == 0);
    status = ruda_format_f64(
        UINT64_C(0x3ff0000000000000), output, sizeof(output), &length
    );
    RUDA_EXPECT(status == RUDA_NUMBER_FORMAT_OK);
    RUDA_EXPECT(strcmp(output, "1.0") == 0);
    RUDA_EXPECT(ruda_format_f64(
        UINT64_C(0x3ff0000000000000), output, sizeof(output), NULL
    ) == RUDA_NUMBER_FORMAT_INVALID_ARGUMENT);
}

static void ruda_number_test_corpus(void) {
    static const uint64_t f64_values[] = {
        UINT64_C(0x0000000000000001),
        UINT64_C(0x000fffffffffffff),
        UINT64_C(0x0010000000000000),
        UINT64_C(0x3fd5555555555555),
        UINT64_C(0x3fefffffffffffff),
        UINT64_C(0x4341c37937e08000),
        UINT64_C(0x7fefffffffffffff),
        UINT64_C(0x8000000000000001),
        UINT64_C(0xbfd5555555555555),
        UINT64_C(0xffefffffffffffff)
    };
    static const uint32_t f32_values[] = {
        UINT32_C(0x00000001),
        UINT32_C(0x007fffff),
        UINT32_C(0x00800000),
        UINT32_C(0x3eaaaaab),
        UINT32_C(0x3f7fffff),
        UINT32_C(0x4b189680),
        UINT32_C(0x7f7fffff),
        UINT32_C(0x80000001),
        UINT32_C(0xbeaaaaab),
        UINT32_C(0xff7fffff)
    };
    uint64_t state64 = UINT64_C(0x123456789abcdef0);
    uint32_t state32 = UINT32_C(0x13579bdf);
    size_t index = 0;
    for (index = 0; index < sizeof(f64_values) / sizeof(f64_values[0]); index += 1) {
        ruda_number_test_f64_round_trip(f64_values[index]);
    }
    for (index = 0; index < 256; index += 1) {
        uint64_t bits = state64 & UINT64_C(0x7fefffffffffffff);
        ruda_number_test_f64_round_trip(bits);
        state64 = state64 * UINT64_C(6364136223846793005) + 1;
    }
    if (sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24
        && FLT_MAX_EXP == 128) {
        for (index = 0; index < sizeof(f32_values) / sizeof(f32_values[0]); index += 1) {
            ruda_number_test_f32_round_trip(f32_values[index]);
        }
        for (index = 0; index < 256; index += 1) {
            uint32_t bits = state32 & UINT32_C(0x7f7fffff);
            ruda_number_test_f32_round_trip(bits);
            state32 = state32 * UINT32_C(1664525) + UINT32_C(1013904223);
        }
    }
}

void ruda_test_number_format(void) {
    ruda_number_test_special_values();
    ruda_number_test_d046_boundaries();
    ruda_number_test_buffer_contract();
    ruda_number_test_corpus();
}
