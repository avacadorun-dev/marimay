#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "ruda/base/utf8.h"

static void ruda_expect_scalar(
    const unsigned char *bytes,
    size_t length,
    uint32_t expected,
    size_t width
) {
    RudaUtf8Scalar scalar;
    RUDA_EXPECT(ruda_utf8_decode(bytes, length, &scalar));
    RUDA_EXPECT(scalar.value == expected);
    RUDA_EXPECT(scalar.width == width);
}

static void ruda_expect_invalid(const unsigned char *bytes, size_t length) {
    RudaUtf8Scalar scalar;
    RUDA_EXPECT(!ruda_utf8_decode(bytes, length, &scalar));
    RUDA_EXPECT(scalar.value == 0xfffdU);
    RUDA_EXPECT(scalar.width == 1);
}

void ruda_test_utf8(void) {
    static const unsigned char ascii[] = {'A'};
    static const unsigned char two[] = {0xc2U, 0xa2U};
    static const unsigned char three[] = {0xe2U, 0x82U, 0xacU};
    static const unsigned char four[] = {0xf0U, 0x9fU, 0x98U, 0x80U};
    static const unsigned char overlong[] = {0xe0U, 0x80U, 0x80U};
    static const unsigned char surrogate[] = {0xedU, 0xa0U, 0x80U};
    static const unsigned char too_large[] = {0xf4U, 0x90U, 0x80U, 0x80U};
    static const unsigned char truncated[] = {0xf0U, 0x9fU, 0x98U};
    static const char valid[] = "A\xc2\xa2\xe2\x82\xac\xf0\x9f\x98\x80";
    static const char invalid[] = "ok\xed\xa0\x80";
    size_t invalid_offset = 0;

    ruda_expect_scalar(ascii, sizeof(ascii), 0x41U, 1);
    ruda_expect_scalar(two, sizeof(two), 0xa2U, 2);
    ruda_expect_scalar(three, sizeof(three), 0x20acU, 3);
    ruda_expect_scalar(four, sizeof(four), 0x1f600U, 4);
    ruda_expect_invalid(overlong, sizeof(overlong));
    ruda_expect_invalid(surrogate, sizeof(surrogate));
    ruda_expect_invalid(too_large, sizeof(too_large));
    ruda_expect_invalid(truncated, sizeof(truncated));
    ruda_expect_invalid(NULL, 0);

    RUDA_EXPECT(ruda_utf8_validate(valid, strlen(valid), &invalid_offset));
    RUDA_EXPECT(invalid_offset == strlen(valid));
    RUDA_EXPECT(!ruda_utf8_validate(invalid, sizeof(invalid) - 1, &invalid_offset));
    RUDA_EXPECT(invalid_offset == 2);
    RUDA_EXPECT(ruda_utf8_validate(NULL, 0, &invalid_offset));

    {
        static const uint32_t values[] = {
            0,
            0x7fU,
            0x80U,
            0x7ffU,
            0x800U,
            0xffffU,
            0x10000U,
            0x10ffffU
        };
        unsigned char encoded[4];
        size_t width = 0;
        size_t index = 0;
        for (index = 0; index < sizeof(values) / sizeof(values[0]); index += 1) {
            RudaUtf8Scalar decoded;
            RUDA_EXPECT(ruda_utf8_encode(values[index], encoded, &width));
            RUDA_EXPECT(ruda_utf8_decode(encoded, width, &decoded));
            RUDA_EXPECT(decoded.value == values[index]);
            RUDA_EXPECT(decoded.width == width);
        }
        RUDA_EXPECT(!ruda_utf8_encode(0xd800U, encoded, &width));
        RUDA_EXPECT(!ruda_utf8_encode(0x110000U, encoded, &width));
        RUDA_EXPECT(!ruda_utf8_encode(0, NULL, &width));
        RUDA_EXPECT(!ruda_utf8_encode(0, encoded, NULL));
    }
}
