#ifndef RUDA_BASE_UNICODE_H
#define RUDA_BASE_UNICODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Pinned by D023; this value never follows the host Unicode library. */
const char *ruda_unicode_version(void);

bool ruda_unicode_is_xid_start(uint32_t scalar);
bool ruda_unicode_is_xid_continue(uint32_t scalar);

/* D026 canonical formatter width for a valid Unicode scalar. */
size_t ruda_unicode_formatter_width(uint32_t scalar);

#endif
