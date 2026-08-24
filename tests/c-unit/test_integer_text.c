#include "test_support.h"

#include <stdint.h>
#include <string.h>

#include "mmay/base/integer_text.h"

static void mmay_test_integer_text_case(
    MarimayIntegerValue value,
    unsigned int radix,
    size_t minimum_digits,
    const char *expected
) {
    char buffer[MMAY_INTEGER_TEXT_CAPACITY];
    size_t length = SIZE_MAX;
    MMAY_EXPECT(mmay_format_integer(
        value, radix, minimum_digits, buffer, sizeof(buffer), &length
    ) == MMAY_INTEGER_TEXT_OK);
    MMAY_EXPECT(length == strlen(expected));
    MMAY_EXPECT(strcmp(buffer, expected) == 0);
}

static void mmay_test_integer_text_format(void) {
    char buffer[MMAY_INTEGER_TEXT_CAPACITY];
    size_t length = 0;
    mmay_test_integer_text_case(
        mmay_integer_from_i8(INT8_MIN), 10, 1, "-128"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_i16(INT16_MIN), 10, 1, "-32768"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_i32(INT32_MIN), 10, 1, "-2147483648"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_i64(INT64_MIN), 10, 1,
        "-9223372036854775808"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u64(UINT64_MAX), 10, 1,
        "18446744073709551615"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u64(UINT64_MAX), 16, 1,
        "ffffffffffffffff"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_i8(INT8_MAX), 10, 1, "127"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_i16(INT16_MAX), 10, 1, "32767"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_i32(INT32_MAX), 10, 1, "2147483647"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_i64(INT64_MAX), 10, 1,
        "9223372036854775807"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u8(UINT8_MAX), 10, 1, "255"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u16(UINT16_MAX), 10, 1, "65535"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u32(UINT32_MAX), 10, 1, "4294967295"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u64(0), 2, 1, "0"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u64(511), 8, 1, "777"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u64(35), 36, 1, "z"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_i64(-42), 2, 8, "-00101010"
    );
    mmay_test_integer_text_case(
        mmay_integer_from_u8(0), 36, 3, "000"
    );
    MMAY_EXPECT(mmay_format_bool(
        true, buffer, sizeof(buffer), &length
    ) == MMAY_INTEGER_TEXT_OK);
    MMAY_EXPECT(length == 4 && strcmp(buffer, "true") == 0);
    MMAY_EXPECT(mmay_format_bool(
        false, buffer, 5, &length
    ) == MMAY_INTEGER_TEXT_BUFFER_TOO_SMALL);
    MMAY_EXPECT(length == 5);
    MMAY_EXPECT(mmay_format_integer(
        mmay_integer_from_i64(1), 1, 1,
        buffer, sizeof(buffer), &length
    ) == MMAY_INTEGER_TEXT_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_format_integer(
        mmay_integer_from_i64(1), 37, 1,
        buffer, sizeof(buffer), &length
    ) == MMAY_INTEGER_TEXT_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_format_integer(
        mmay_integer_from_i64(100), 10, 1, buffer, 3, &length
    ) == MMAY_INTEGER_TEXT_BUFFER_TOO_SMALL);
    MMAY_EXPECT(length == 3);
    MMAY_EXPECT(mmay_format_integer(
        (MarimayIntegerValue){(MarimayIntegerKind)999, 0}, 10, 1,
        buffer, sizeof(buffer), &length
    ) == MMAY_INTEGER_TEXT_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_format_integer(
        mmay_integer_from_i64(1), 10, 65,
        buffer, sizeof(buffer), &length
    ) == MMAY_INTEGER_TEXT_INVALID_ARGUMENT);
    MMAY_EXPECT(mmay_format_integer(
        mmay_integer_from_i64(42), 10, 1, NULL, 0, &length
    ) == MMAY_INTEGER_TEXT_BUFFER_TOO_SMALL);
    MMAY_EXPECT(length == 2);
    MMAY_EXPECT(mmay_format_bool(
        true, buffer, sizeof(buffer), NULL
    ) == MMAY_INTEGER_TEXT_INVALID_ARGUMENT);
}

static void mmay_test_integer_parse_case(
    const char *text,
    int64_t radix,
    MarimayIntegerParseStatus expected_status,
    int64_t expected_value,
    size_t expected_offset
) {
    int64_t value = INT64_C(123);
    size_t offset = SIZE_MAX;
    MarimayIntegerParseStatus status = mmay_parse_i64(
        (const unsigned char *)text, strlen(text), radix, &value, &offset
    );
    MMAY_EXPECT(status == expected_status);
    if (status == MMAY_INTEGER_PARSE_OK) MMAY_EXPECT(value == expected_value);
    if (status == MMAY_INTEGER_PARSE_INVALID_DIGIT) {
        MMAY_EXPECT(offset == expected_offset);
    }
}

static void mmay_test_integer_text_parse(void) {
    mmay_test_integer_parse_case(
        "-9223372036854775808", 10, MMAY_INTEGER_PARSE_OK, INT64_MIN, 0
    );
    mmay_test_integer_parse_case(
        "9223372036854775807", 10, MMAY_INTEGER_PARSE_OK, INT64_MAX, 0
    );
    mmay_test_integer_parse_case(
        "7fffffffffffffff", 16, MMAY_INTEGER_PARSE_OK, INT64_MAX, 0
    );
    mmay_test_integer_parse_case(
        "101010", 2, MMAY_INTEGER_PARSE_OK, 42, 0
    );
    mmay_test_integer_parse_case(
        "777", 8, MMAY_INTEGER_PARSE_OK, 511, 0
    );
    mmay_test_integer_parse_case(
        "Z", 36, MMAY_INTEGER_PARSE_OK, 35, 0
    );
    mmay_test_integer_parse_case(
        "-000", 10, MMAY_INTEGER_PARSE_OK, 0, 0
    );
    mmay_test_integer_parse_case(
        "", 10, MMAY_INTEGER_PARSE_EMPTY, 0, 0
    );
    mmay_test_integer_parse_case(
        "-", 10, MMAY_INTEGER_PARSE_INVALID_DIGIT, 0, 1
    );
    mmay_test_integer_parse_case(
        "+1", 10, MMAY_INTEGER_PARSE_INVALID_DIGIT, 0, 0
    );
    mmay_test_integer_parse_case(
        "12x", 10, MMAY_INTEGER_PARSE_INVALID_DIGIT, 0, 2
    );
    mmay_test_integer_parse_case(
        " 1", 10, MMAY_INTEGER_PARSE_INVALID_DIGIT, 0, 0
    );
    mmay_test_integer_parse_case(
        "0x10", 16, MMAY_INTEGER_PARSE_INVALID_DIGIT, 0, 1
    );
    mmay_test_integer_parse_case(
        "1_0", 10, MMAY_INTEGER_PARSE_INVALID_DIGIT, 0, 1
    );
    mmay_test_integer_parse_case(
        "9223372036854775808", 10, MMAY_INTEGER_PARSE_OUT_OF_RANGE, 0, 0
    );
    mmay_test_integer_parse_case(
        "-9223372036854775809", 10, MMAY_INTEGER_PARSE_OUT_OF_RANGE, 0, 0
    );
    mmay_test_integer_parse_case(
        "1", 1, MMAY_INTEGER_PARSE_INVALID_RADIX, 0, 0
    );
    mmay_test_integer_parse_case(
        "1", 37, MMAY_INTEGER_PARSE_INVALID_RADIX, 0, 0
    );
    {
        static const unsigned char with_nul[] = {'1', 0, '2'};
        int64_t value = 0;
        size_t offset = SIZE_MAX;
        MMAY_EXPECT(mmay_parse_i64(
            with_nul, sizeof(with_nul), 10, &value, &offset
        ) == MMAY_INTEGER_PARSE_INVALID_DIGIT);
        MMAY_EXPECT(offset == 1);
    }
}

void mmay_test_integer_text(void) {
    mmay_test_integer_text_format();
    mmay_test_integer_text_parse();
}
