#ifndef CHECKER_IMPORTED_ENUMS_API_H
#define CHECKER_IMPORTED_ENUMS_API_H

#include "checker_package_context.h"

bool mmay_checker_private_imported_enums_init(MarimayChecker *checker);

void mmay_checker_private_imported_enums_dispose(MarimayChecker *checker);

MarimayCheckEnumDeclaration *
mmay_checker_private_imported_enum_declaration_by_type(
    MarimayChecker *checker,
    MarimayTypeId type
);

#endif
