#include "test_support.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

#include "mmay/base/f64.h"
#include "mmay/base/number_format.h"

static uint64_t mmay_number_test_parse_f64(const char *text) {
    uint64_t bits = UINT64_MAX;
    MarimayF64ParseStatus status = mmay_f64_parse_decimal(
        (const unsigned char *)text,
        strlen(text),
        mmay_system_allocator(),
        4096,
        &bits
    );
    MMAY_EXPECT(status == MMAY_F64_PARSE_OK);
    return bits;
}

static void mmay_number_test_f64_text(uint64_t bits, const char *expected) {
    char output[MMAY_NUMBER_FORMAT_F64_CAPACITY];
    size_t length = 0;
    MarimayNumberFormatStatus status = mmay_format_f64(
        bits, output, sizeof(output), &length
    );
    MMAY_EXPECT(status == MMAY_NUMBER_FORMAT_OK);
    MMAY_EXPECT(length == strlen(expected));
    MMAY_EXPECT(strcmp(output, expected) == 0);
}

static void mmay_number_test_f32_text(uint32_t bits, const char *expected) {
    char output[MMAY_NUMBER_FORMAT_F32_CAPACITY];
    size_t length = 0;
    MarimayNumberFormatStatus status = mmay_format_f32(
        bits, output, sizeof(output), &length
    );
    MMAY_EXPECT(status == MMAY_NUMBER_FORMAT_OK);
    MMAY_EXPECT(length == strlen(expected));
    MMAY_EXPECT(strcmp(output, expected) == 0);
}

static void mmay_number_test_f64_round_trip(uint64_t bits) {
    char output[MMAY_NUMBER_FORMAT_F64_CAPACITY];
    const char *magnitude = output;
    uint64_t parsed = 0;
    size_t length = 0;
    size_t index = 0;
    MarimayNumberFormatStatus status = mmay_format_f64(
        bits, output, sizeof(output), &length
    );
    MMAY_EXPECT(status == MMAY_NUMBER_FORMAT_OK);
    if ((bits >> 52) == UINT64_C(0x7ff)) {
        return;
    }
    if (output[0] == '-') {
        magnitude += 1;
    }
    MMAY_EXPECT(length != 0);
    for (index = 0; index < length; index += 1) {
        MMAY_EXPECT((unsigned char)output[index] < 128u);
    }
    parsed = mmay_number_test_parse_f64(magnitude);
    MMAY_EXPECT(parsed == (bits & UINT64_C(0x7fffffffffffffff)));
}

static void mmay_number_test_f32_round_trip(uint32_t bits) {
    char output[MMAY_NUMBER_FORMAT_F32_CAPACITY];
    const char *magnitude = output;
    uint64_t parsed_bits = 0;
    uint32_t parsed_f32 = 0;
    float rounded = 0.0f;
    size_t length = 0;
    size_t index = 0;
    MarimayNumberFormatStatus status = mmay_format_f32(
        bits, output, sizeof(output), &length
    );
    MMAY_EXPECT(status == MMAY_NUMBER_FORMAT_OK);
    if ((bits >> 23) == UINT32_C(0xff)) {
        return;
    }
    if (output[0] == '-') {
        magnitude += 1;
    }
    MMAY_EXPECT(length != 0);
    for (index = 0; index < length; index += 1) {
        MMAY_EXPECT((unsigned char)output[index] < 128u);
    }
    parsed_bits = mmay_number_test_parse_f64(magnitude);
    rounded = (float)mmay_f64_to_double(parsed_bits);
    memcpy(&parsed_f32, &rounded, sizeof(parsed_f32));
    MMAY_EXPECT(parsed_f32 == (bits & UINT32_C(0x7fffffff)));
}

static void mmay_number_test_special_values(void) {
    mmay_number_test_f64_text(
        UINT64_C(0x0000000000000000), "0.0"
    );
    mmay_number_test_f64_text(
        UINT64_C(0x8000000000000000), "-0.0"
    );
    mmay_number_test_f64_text(
        UINT64_C(0x7ff0000000000000), "inf"
    );
    mmay_number_test_f64_text(
        UINT64_C(0xfff0000000000000), "-inf"
    );
    mmay_number_test_f64_text(
        UINT64_C(0x7ff8000000000001), "nan"
    );
    mmay_number_test_f64_text(
        UINT64_C(0xfff0000000000001), "-nan"
    );
    mmay_number_test_f32_text(UINT32_C(0x00000000), "0.0");
    mmay_number_test_f32_text(UINT32_C(0x80000000), "-0.0");
    mmay_number_test_f32_text(UINT32_C(0x7f800000), "inf");
    mmay_number_test_f32_text(UINT32_C(0xff800000), "-inf");
    mmay_number_test_f32_text(UINT32_C(0x7fc00001), "nan");
    mmay_number_test_f32_text(UINT32_C(0xffc00001), "-nan");
}

static void mmay_number_test_d046_boundaries(void) {
    mmay_number_test_f64_text(mmay_number_test_parse_f64("1.0"), "1.0");
    mmay_number_test_f64_text(mmay_number_test_parse_f64("0.0001"), "0.0001");
    mmay_number_test_f64_text(mmay_number_test_parse_f64("0.00001"), "1.0e-5");
    mmay_number_test_f64_text(mmay_number_test_parse_f64("1.0e15"),
        "1000000000000000.0");
    mmay_number_test_f64_text(mmay_number_test_parse_f64("1e16"), "1.0e16");
    mmay_number_test_f64_text(
        UINT64_C(0x0000000000000001), "5.0e-324"
    );
    mmay_number_test_f64_text(
        UINT64_C(0x7fefffffffffffff), "1.7976931348623157e308"
    );
    mmay_number_test_f32_text(UINT32_C(0x3f800000), "1.0");
    mmay_number_test_f32_text(UINT32_C(0x3dcccccd), "0.1");
    mmay_number_test_f32_text(UINT32_C(0x00000001), "1.0e-45");
    mmay_number_test_f32_text(UINT32_C(0x7f7fffff), "3.4028235e38");
}

static void mmay_number_test_buffer_contract(void) {
    char output[MMAY_NUMBER_FORMAT_F64_CAPACITY];
    char untouched[4] = {'x', 'x', 'x', 'x'};
    size_t required = 0;
    size_t length = 0;
    MarimayNumberFormatStatus status = mmay_format_f64(
        UINT64_C(0x3ff0000000000000), NULL, 0, &required
    );
    MMAY_EXPECT(status == MMAY_NUMBER_FORMAT_BUFFER_TOO_SMALL);
    MMAY_EXPECT(required == 3);
    status = mmay_format_f64(
        UINT64_C(0x3ff0000000000000), untouched, required, &length
    );
    MMAY_EXPECT(status == MMAY_NUMBER_FORMAT_BUFFER_TOO_SMALL);
    MMAY_EXPECT(length == required);
    MMAY_EXPECT(memcmp(untouched, "xxxx", sizeof(untouched)) == 0);
    status = mmay_format_f64(
        UINT64_C(0x3ff0000000000000), output, sizeof(output), &length
    );
    MMAY_EXPECT(status == MMAY_NUMBER_FORMAT_OK);
    MMAY_EXPECT(strcmp(output, "1.0") == 0);
    MMAY_EXPECT(mmay_format_f64(
        UINT64_C(0x3ff0000000000000), output, sizeof(output), NULL
    ) == MMAY_NUMBER_FORMAT_INVALID_ARGUMENT);
}

static void mmay_number_test_corpus(void) {
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
        mmay_number_test_f64_round_trip(f64_values[index]);
    }
    for (index = 0; index < 256; index += 1) {
        uint64_t bits = state64 & UINT64_C(0x7fefffffffffffff);
        mmay_number_test_f64_round_trip(bits);
        state64 = state64 * UINT64_C(6364136223846793005) + 1;
    }
    if (sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24
        && FLT_MAX_EXP == 128) {
        for (index = 0; index < sizeof(f32_values) / sizeof(f32_values[0]); index += 1) {
            mmay_number_test_f32_round_trip(f32_values[index]);
        }
        for (index = 0; index < 256; index += 1) {
            uint32_t bits = state32 & UINT32_C(0x7f7fffff);
            mmay_number_test_f32_round_trip(bits);
            state32 = state32 * UINT32_C(1664525) + UINT32_C(1013904223);
        }
    }
}

void mmay_test_number_format(void) {
    mmay_number_test_special_values();
    mmay_number_test_d046_boundaries();
    mmay_number_test_buffer_contract();
    mmay_number_test_corpus();
}
