#ifndef CHECKER_IMPORTED_ENUMS_API_H
#define CHECKER_IMPORTED_ENUMS_API_H

#include "checker_package_context.h"

bool ruda_checker_private_imported_enums_init(RudaChecker *checker);

void ruda_checker_private_imported_enums_dispose(RudaChecker *checker);

RudaCheckEnumDeclaration *
ruda_checker_private_imported_enum_declaration_by_type(
    RudaChecker *checker,
    RudaTypeId type
);

#endif
