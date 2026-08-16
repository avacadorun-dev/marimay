#ifndef RUDA_CHECK_PACKAGE_IMPORTS_API_H
#define RUDA_CHECK_PACKAGE_IMPORTS_API_H

#include "checker_declaration_types.h"

typedef struct RudaChecker RudaChecker;

typedef struct RudaCheckPackageSurface {
    const unsigned char *path;
    size_t path_length;
    size_t function_start;
    size_t function_count;
    size_t constant_start;
    size_t constant_count;
    size_t type_alias_start;
    size_t type_alias_count;
    size_t record_start;
    size_t record_count;
    size_t enum_start;
    size_t enum_count;
    size_t interface_start;
    size_t interface_count;
} RudaCheckPackageSurface;

typedef struct RudaCheckImportBinding {
    const RudaAstTree *ast;
    size_t surface_index;
    const unsigned char *original;
    size_t original_length;
    const unsigned char *local;
    size_t local_length;
    size_t start;
    size_t end;
    bool is_namespace;
} RudaCheckImportBinding;

typedef struct RudaCheckImportEnvironment {
    const RudaCheckPackageSurface *const *surfaces;
    size_t surface_count;
    const RudaCheckImportBinding *bindings;
    size_t binding_count;
} RudaCheckImportEnvironment;

typedef struct RudaCheckCallable {
    const RudaTypeId *parameter_types;
    size_t parameter_count;
    RudaSymbolId symbol;
    RudaSymbolId receiver_symbol;
    RudaTypeId receiver_type;
    RudaTypeId result_type;
    bool dot_eligible;
} RudaCheckCallable;

bool ruda_check_import_binding_exists(
    const RudaChecker *checker,
    const unsigned char *name,
    size_t name_length
);
bool ruda_check_import_namespace_exists(
    const RudaChecker *checker,
    const unsigned char *name,
    size_t name_length
);
bool ruda_check_import_type(
    RudaChecker *checker,
    const RudaAstNode *type_node,
    RudaTypeId *type,
    bool *resolved
);
bool ruda_check_imported_nominal(
    const RudaChecker *checker,
    RudaTypeId type,
    RudaSymbolId *symbol
);
RudaCheckCallResolution ruda_check_resolve_imported_call(
    const RudaChecker *checker,
    RudaCheckName namespace_name,
    bool has_namespace,
    RudaCheckName name,
    bool receiver_known,
    RudaTypeId receiver_type,
    RudaCheckCallable *callable
);
const RudaCheckedRecord *ruda_check_imported_record_by_type(
    const RudaChecker *checker,
    RudaTypeId type
);
const RudaCheckedEnum *ruda_check_imported_enum_by_type(
    const RudaChecker *checker,
    RudaTypeId type
);

#endif
