#ifndef RUDA_CHECK_IMPORTED_NOMINAL_API_H
#define RUDA_CHECK_IMPORTED_NOMINAL_API_H

#include "checker_function_context.h"

bool ruda_checker_private_imported_record_layout(
    RudaFunctionChecker *function,
    const RudaCheckedRecord *record,
    RudaTypeId type,
    RudaMirRecordId *layout
);
bool ruda_checker_private_imported_enum_layout(
    RudaFunctionChecker *function,
    const RudaCheckedEnum *enum_value,
    RudaTypeId type,
    RudaMirEnumId *layout
);
bool ruda_checker_private_function_ensure_nominal_layouts(
    RudaFunctionChecker *function
);

#endif
