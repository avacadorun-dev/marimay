#include "test_support.h"

#include <string.h>

#include "mmay/base/unicode.h"

void mmay_test_unicode(void) {
    MMAY_EXPECT(strcmp(mmay_unicode_version(), "17.0.0") == 0);

    MMAY_EXPECT(mmay_unicode_is_xid_start('A'));
    MMAY_EXPECT(mmay_unicode_is_xid_start('z'));
    MMAY_EXPECT(mmay_unicode_is_xid_continue('A'));
    MMAY_EXPECT(mmay_unicode_is_xid_continue('z'));
    MMAY_EXPECT(!mmay_unicode_is_xid_start('_'));
    MMAY_EXPECT(mmay_unicode_is_xid_continue('_'));
    MMAY_EXPECT(!mmay_unicode_is_xid_start('0'));
    MMAY_EXPECT(mmay_unicode_is_xid_continue('0'));

    MMAY_EXPECT(mmay_unicode_is_xid_start(0x03BBU));
    MMAY_EXPECT(mmay_unicode_is_xid_continue(0x03BBU));
    MMAY_EXPECT(!mmay_unicode_is_xid_start(0x0301U));
    MMAY_EXPECT(mmay_unicode_is_xid_continue(0x0301U));
    MMAY_EXPECT(mmay_unicode_is_xid_continue(0x200CU));

    /* Tai Yo was added in Unicode 17.0 and locks the selected data version. */
    MMAY_EXPECT(mmay_unicode_is_xid_start(0x1E6C0U));
    MMAY_EXPECT(mmay_unicode_is_xid_continue(0x1E6F5U));
    MMAY_EXPECT(mmay_unicode_is_xid_start(0x33479U));
    MMAY_EXPECT(!mmay_unicode_is_xid_start(0x33480U));

    MMAY_EXPECT(!mmay_unicode_is_xid_start(0));
    MMAY_EXPECT(!mmay_unicode_is_xid_continue(0xD800U));
    MMAY_EXPECT(!mmay_unicode_is_xid_start(0x110000U));

    MMAY_EXPECT(mmay_unicode_formatter_width('A') == 1);
    MMAY_EXPECT(mmay_unicode_formatter_width(0x00A1U) == 1); /* EAW ambiguous. */
    MMAY_EXPECT(mmay_unicode_formatter_width(0x0301U) == 0); /* Extend. */
    MMAY_EXPECT(mmay_unicode_formatter_width(0x200DU) == 0); /* Ignorable ZWJ. */
    MMAY_EXPECT(mmay_unicode_formatter_width(0x4E00U) == 2); /* EAW wide. */
    MMAY_EXPECT(mmay_unicode_formatter_width(0xFF21U) == 2); /* EAW fullwidth. */
    MMAY_EXPECT(mmay_unicode_formatter_width(0xD800U) == 0);
    MMAY_EXPECT(mmay_unicode_formatter_width(0x110000U) == 0);
}
