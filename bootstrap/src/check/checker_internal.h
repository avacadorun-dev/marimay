#ifndef RUDA_CHECK_CHECKER_INTERNAL_H
#define RUDA_CHECK_CHECKER_INTERNAL_H

#include "ruda/check/checker.h"

/* Result-owner lifecycle used by package publication and rollback. */
void ruda_checked_record_value_dispose(RudaCheckedRecord *record);
void ruda_checked_enum_variant_value_dispose(
    RudaCheckedEnumVariant *variant
);
void ruda_checked_enum_value_dispose(RudaCheckedEnum *enum_value);
void ruda_check_dispose_function_values(RudaCheckResult *result);
void ruda_check_dispose_constant_values(RudaCheckResult *result);
void ruda_check_dispose_type_alias_values(RudaCheckResult *result);
void ruda_check_dispose_record_values(RudaCheckResult *result);
void ruda_check_dispose_enum_values(RudaCheckResult *result);
void ruda_check_dispose_diagnostics(RudaCheckResult *result);
bool ruda_check_function_index_init(RudaCheckResult *result);
void ruda_check_clear_functions(RudaCheckResult *result);

#endif
