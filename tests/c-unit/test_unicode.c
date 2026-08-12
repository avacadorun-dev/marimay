#include "test_support.h"

#include <string.h>

#include "ruda/base/unicode.h"

void ruda_test_unicode(void) {
    RUDA_EXPECT(strcmp(ruda_unicode_version(), "17.0.0") == 0);

    RUDA_EXPECT(ruda_unicode_is_xid_start('A'));
    RUDA_EXPECT(ruda_unicode_is_xid_start('z'));
    RUDA_EXPECT(ruda_unicode_is_xid_continue('A'));
    RUDA_EXPECT(ruda_unicode_is_xid_continue('z'));
    RUDA_EXPECT(!ruda_unicode_is_xid_start('_'));
    RUDA_EXPECT(ruda_unicode_is_xid_continue('_'));
    RUDA_EXPECT(!ruda_unicode_is_xid_start('0'));
    RUDA_EXPECT(ruda_unicode_is_xid_continue('0'));

    RUDA_EXPECT(ruda_unicode_is_xid_start(0x03BBU));
    RUDA_EXPECT(ruda_unicode_is_xid_continue(0x03BBU));
    RUDA_EXPECT(!ruda_unicode_is_xid_start(0x0301U));
    RUDA_EXPECT(ruda_unicode_is_xid_continue(0x0301U));
    RUDA_EXPECT(ruda_unicode_is_xid_continue(0x200CU));

    /* Tai Yo was added in Unicode 17.0 and locks the selected data version. */
    RUDA_EXPECT(ruda_unicode_is_xid_start(0x1E6C0U));
    RUDA_EXPECT(ruda_unicode_is_xid_continue(0x1E6F5U));
    RUDA_EXPECT(ruda_unicode_is_xid_start(0x33479U));
    RUDA_EXPECT(!ruda_unicode_is_xid_start(0x33480U));

    RUDA_EXPECT(!ruda_unicode_is_xid_start(0));
    RUDA_EXPECT(!ruda_unicode_is_xid_continue(0xD800U));
    RUDA_EXPECT(!ruda_unicode_is_xid_start(0x110000U));

    RUDA_EXPECT(ruda_unicode_formatter_width('A') == 1);
    RUDA_EXPECT(ruda_unicode_formatter_width(0x00A1U) == 1); /* EAW ambiguous. */
    RUDA_EXPECT(ruda_unicode_formatter_width(0x0301U) == 0); /* Extend. */
    RUDA_EXPECT(ruda_unicode_formatter_width(0x200DU) == 0); /* Ignorable ZWJ. */
    RUDA_EXPECT(ruda_unicode_formatter_width(0x4E00U) == 2); /* EAW wide. */
    RUDA_EXPECT(ruda_unicode_formatter_width(0xFF21U) == 2); /* EAW fullwidth. */
    RUDA_EXPECT(ruda_unicode_formatter_width(0xD800U) == 0);
    RUDA_EXPECT(ruda_unicode_formatter_width(0x110000U) == 0);
}
