#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/utf8.h"

static void mmay_expect_scalar(
    const unsigned char *bytes,
    size_t length,
    uint32_t expected,
    size_t width
) {
    MarimayUtf8Scalar scalar;
    MMAY_EXPECT(mmay_utf8_decode(bytes, length, &scalar));
    MMAY_EXPECT(scalar.value == expected);
    MMAY_EXPECT(scalar.width == width);
}

static void mmay_expect_invalid(const unsigned char *bytes, size_t length) {
    MarimayUtf8Scalar scalar;
    MMAY_EXPECT(!mmay_utf8_decode(bytes, length, &scalar));
    MMAY_EXPECT(scalar.value == 0xfffdU);
    MMAY_EXPECT(scalar.width == 1);
}

void mmay_test_utf8(void) {
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

    mmay_expect_scalar(ascii, sizeof(ascii), 0x41U, 1);
    mmay_expect_scalar(two, sizeof(two), 0xa2U, 2);
    mmay_expect_scalar(three, sizeof(three), 0x20acU, 3);
    mmay_expect_scalar(four, sizeof(four), 0x1f600U, 4);
    mmay_expect_invalid(overlong, sizeof(overlong));
    mmay_expect_invalid(surrogate, sizeof(surrogate));
    mmay_expect_invalid(too_large, sizeof(too_large));
    mmay_expect_invalid(truncated, sizeof(truncated));
    mmay_expect_invalid(NULL, 0);

    MMAY_EXPECT(mmay_utf8_validate(valid, strlen(valid), &invalid_offset));
    MMAY_EXPECT(invalid_offset == strlen(valid));
    MMAY_EXPECT(!mmay_utf8_validate(invalid, sizeof(invalid) - 1, &invalid_offset));
    MMAY_EXPECT(invalid_offset == 2);
    MMAY_EXPECT(mmay_utf8_validate(NULL, 0, &invalid_offset));

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
            MarimayUtf8Scalar decoded;
            MMAY_EXPECT(mmay_utf8_encode(values[index], encoded, &width));
            MMAY_EXPECT(mmay_utf8_decode(encoded, width, &decoded));
            MMAY_EXPECT(decoded.value == values[index]);
            MMAY_EXPECT(decoded.width == width);
        }
        MMAY_EXPECT(!mmay_utf8_encode(0xd800U, encoded, &width));
        MMAY_EXPECT(!mmay_utf8_encode(0x110000U, encoded, &width));
        MMAY_EXPECT(!mmay_utf8_encode(0, NULL, &width));
        MMAY_EXPECT(!mmay_utf8_encode(0, encoded, NULL));
    }
}
