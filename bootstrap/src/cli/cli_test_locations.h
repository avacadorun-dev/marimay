#ifndef MMAY_CLI_TEST_LOCATIONS_H
#define MMAY_CLI_TEST_LOCATIONS_H

#include "mmay/check/result.h"
#include "mmay/base/writer.h"
#include "mmay/native/locations.h"

/* Owns a canonical vector of location records; source paths are borrowed from
 * checked snapshots through synchronous test execution. Output is reset on
 * failure. Source positions are resolved once, before native emission. */
bool mmay_cli_test_locations_prepare(const MarimayCheckResult *checked,
    MarimayAllocator allocator, MarimayVector *output);
/* Allocation-free lookup/render. Missing metadata is an integrity failure. */
bool mmay_cli_test_location_write(const MarimayVector *locations,
    MarimaySymbolId symbol, size_t start, size_t end, MarimayWriter writer);

#endif
