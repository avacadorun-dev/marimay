#ifndef MMAY_CHECK_IMPORTED_NOMINAL_API_H
#define MMAY_CHECK_IMPORTED_NOMINAL_API_H

#include "checker_function_context.h"

bool mmay_checker_private_imported_record_layout(
    MarimayFunctionChecker *function,
    const MarimayCheckedRecord *record,
    MarimayTypeId type,
    MarimayMirRecordId *layout
);
bool mmay_checker_private_imported_enum_layout(
    MarimayFunctionChecker *function,
    const MarimayCheckedEnum *enum_value,
    MarimayTypeId type,
    MarimayMirEnumId *layout
);
bool mmay_checker_private_function_ensure_nominal_layouts(
    MarimayFunctionChecker *function
);

#endif
