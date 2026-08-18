#include "ruda/check/graph.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "checker_api.h"
#include "checker_core_api.h"
#include "package_imports_api.h"
#include "ruda/base/string_literal.h"
#include "ruda/generated/diagnostic_codes.h"

typedef struct RudaCheckGraphEdge {
    RudaOwnedBytes path;
    const RudaAstTree *ast;
    const RudaAstNode *node;
    size_t path_start;
    size_t path_end;
    size_t target;
} RudaCheckGraphEdge;

typedef struct RudaCheckGraphPackage {
    const RudaCheckPackageInput *input;
    RudaVector edges;
    RudaCheckPackageSurface surface;
    unsigned char state;
} RudaCheckGraphPackage;

typedef struct RudaCheckGraph {
    RudaAllocator allocator;
    RudaCheckGraphOptions options;
    RudaCheckResult *result;
    RudaVector packages;
    RudaVector order;
    RudaVector surface_refs;
    RudaVector visit_stack;
    size_t total_imports;
    size_t total_bindings;
    RudaCheckStatus failure;
} RudaCheckGraph;

static bool ruda_graph_equal(
    const void *left,
    size_t left_length,
    const void *right,
    size_t right_length
) {
    return left_length == right_length
        && (left_length == 0 || memcmp(left, right, left_length) == 0);
}

static int ruda_graph_compare_bytes(
    const void *left,
    size_t left_length,
    const void *right,
    size_t right_length
) {
    size_t shared = left_length < right_length ? left_length : right_length;
    int comparison = shared == 0 ? 0 : memcmp(left, right, shared);
    if (comparison != 0) {
        return comparison;
    }
    return left_length < right_length ? -1 : left_length > right_length;
}

static int ruda_graph_compare_packages(const void *left, const void *right) {
    const RudaCheckGraphPackage *left_package = left;
    const RudaCheckGraphPackage *right_package = right;
    return ruda_graph_compare_bytes(
        left_package->input->import_path,
        left_package->input->import_path_length,
        right_package->input->import_path,
        right_package->input->import_path_length
    );
}

static const RudaAstTree *ruda_graph_first_ast(
    const RudaCheckPackageInput *input
) {
    const RudaAstTree *result = NULL;
    size_t index = 0;
    for (index = 0; input != NULL && index < input->ast_count; index += 1) {
        const RudaAstTree *candidate = input->asts[index];
        if (!ruda_checker_private_check_ast_valid(candidate)) {
            return NULL;
        }
        if (result == NULL) {
            result = candidate;
        } else {
            const RudaAstTree *pair[2] = {result, candidate};
            if (ruda_checker_private_check_compare_ast_paths(
                    &pair[1], &pair[0]
                ) < 0) {
                result = candidate;
            }
        }
    }
    return result;
}

static bool ruda_graph_add_diagnostic(
    RudaCheckGraph *graph,
    const char *code,
    const char *message,
    const RudaAstTree *ast,
    size_t start,
    size_t end
) {
    RudaCheckDiagnostic diagnostic;
    if (graph->result->diagnostics.length
        >= graph->options.package_options.max_diagnostics) {
        graph->failure = RUDA_CHECK_LIMIT_REACHED;
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.code = code;
    diagnostic.message = message;
    diagnostic.snapshot = ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!ruda_vector_push(&graph->result->diagnostics, &diagnostic)) {
        graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static bool ruda_graph_add_owned_diagnostic(
    RudaCheckGraph *graph,
    const char *code,
    const unsigned char *message,
    size_t message_length,
    const RudaAstTree *ast,
    size_t start,
    size_t end
) {
    RudaCheckDiagnostic diagnostic;
    if (graph->result->diagnostics.length
        >= graph->options.package_options.max_diagnostics) {
        graph->failure = RUDA_CHECK_LIMIT_REACHED;
        return false;
    }
    memset(&diagnostic, 0, sizeof(diagnostic));
    if (!ruda_owned_bytes_copy(
            &diagnostic.owned_message,
            graph->allocator,
            message,
            message_length
        )) {
        graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
        return false;
    }
    diagnostic.code = code;
    diagnostic.message = (const char *)diagnostic.owned_message.data;
    diagnostic.snapshot = ast->snapshot;
    diagnostic.start = start;
    diagnostic.end = end;
    if (!ruda_vector_push(&graph->result->diagnostics, &diagnostic)) {
        ruda_owned_bytes_dispose(&diagnostic.owned_message);
        graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static bool ruda_graph_add_cycle_diagnostic(
    RudaCheckGraph *graph,
    size_t target,
    const RudaCheckGraphEdge *edge
) {
    static const unsigned char prefix[] =
        "local package dependency cycle: ";
    static const unsigned char arrow[] = " -> ";
    RudaVector message;
    size_t start_index = 0;
    size_t index = 0;
    bool accepted = false;
    memset(&message, 0, sizeof(message));
    while (start_index < graph->visit_stack.length) {
        const size_t *item = ruda_vector_at_const(
            &graph->visit_stack, start_index
        );
        if (item != NULL && *item == target) {
            break;
        }
        start_index += 1;
    }
    if (start_index == graph->visit_stack.length
        || !ruda_vector_init(&message, graph->allocator,
            sizeof(unsigned char))
        || !ruda_vector_append(&message, prefix, sizeof(prefix) - 1)) {
        graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
        goto cleanup;
    }
    for (index = start_index; index < graph->visit_stack.length; index += 1) {
        const size_t *package_index = ruda_vector_at_const(
            &graph->visit_stack, index
        );
        const RudaCheckGraphPackage *package = package_index == NULL
            ? NULL : ruda_vector_at_const(&graph->packages, *package_index);
        if (package == NULL || (index != start_index
                && !ruda_vector_append(&message, arrow, sizeof(arrow) - 1))
            || !ruda_vector_append(
                &message,
                package->input->import_path,
                package->input->import_path_length
            )) {
            graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    {
        const RudaCheckGraphPackage *package = ruda_vector_at_const(
            &graph->packages, target
        );
        if (package == NULL
            || !ruda_vector_append(&message, arrow, sizeof(arrow) - 1)
            || !ruda_vector_append(&message, package->input->import_path,
                package->input->import_path_length)) {
            graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
            goto cleanup;
        }
    }
    accepted = ruda_graph_add_owned_diagnostic(
        graph, RUDA_DIAGNOSTIC_PACKAGE_CYCLE,
        message.data, message.length,
        edge->ast, edge->path_start, edge->path_end
    );
cleanup:
    ruda_vector_dispose(&message);
    return accepted;
}

static size_t ruda_graph_find_package(
    const RudaCheckGraph *graph,
    const void *path,
    size_t path_length
) {
    size_t low = 0;
    size_t high = graph->packages.length;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const RudaCheckGraphPackage *package = ruda_vector_at_const(
            &graph->packages, middle
        );
        int comparison = ruda_graph_compare_bytes(
            package->input->import_path,
            package->input->import_path_length,
            path,
            path_length
        );
        if (comparison < 0) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low < graph->packages.length) {
        const RudaCheckGraphPackage *package = ruda_vector_at_const(
            &graph->packages, low
        );
        if (ruda_graph_equal(package->input->import_path,
                package->input->import_path_length, path, path_length)) {
            return low;
        }
    }
    return SIZE_MAX;
}

static const RudaAstNode *ruda_graph_child_node(
    const RudaAstNode *node,
    RudaSyntaxKind kind,
    size_t ordinal
) {
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
            && element->as.node != NULL && element->as.node->kind == kind) {
            if (ordinal == 0) {
                return element->as.node;
            }
            ordinal -= 1;
        }
    }
    return NULL;
}

static const RudaSyntaxToken *ruda_graph_child_token(
    const RudaAstNode *node,
    RudaTokenKind kind,
    size_t ordinal
) {
    size_t index = 0;
    for (index = 0; node != NULL && index < node->child_count; index += 1) {
        const RudaAstElement *element = ruda_ast_child(node, index);
        if (element != NULL && element->kind == RUDA_AST_ELEMENT_TOKEN
            && element->as.token.kind == kind) {
            if (ordinal == 0) {
                return &element->as.token;
            }
            ordinal -= 1;
        }
    }
    return NULL;
}

static bool ruda_graph_collect_edge(
    RudaCheckGraph *graph,
    RudaCheckGraphPackage *package,
    const RudaAstTree *ast,
    const RudaAstNode *node
) {
    const RudaSyntaxToken *path_token = ruda_graph_child_token(
        node, RUDA_TOKEN_STRING, 0
    );
    RudaCheckGraphEdge edge;
    RudaStringLiteralStatus decode_status;
    const unsigned char *source = ruda_source_bytes(ast->snapshot);
    memset(&edge, 0, sizeof(edge));
    edge.target = SIZE_MAX;
    if (path_token == NULL || path_token->synthetic || source == NULL
        || path_token->end > ruda_source_length(ast->snapshot)) {
        graph->failure = RUDA_CHECK_INVALID_ARGUMENT;
        return false;
    }
    decode_status = ruda_string_literal_decode(
        source + path_token->start,
        path_token->end - path_token->start,
        false,
        graph->allocator,
        &edge.path
    );
    if (decode_status != RUDA_STRING_LITERAL_OK) {
        graph->failure = decode_status == RUDA_STRING_LITERAL_ALLOCATION_FAILED
            ? RUDA_CHECK_ALLOCATION_FAILED : RUDA_CHECK_INVALID_ARGUMENT;
        return false;
    }
    edge.ast = ast;
    edge.node = node;
    edge.path_start = path_token->start;
    edge.path_end = path_token->end;
    if (edge.path.length != 0
        && memchr(edge.path.data, 0, edge.path.length) == NULL) {
        edge.target = ruda_graph_find_package(
            graph, edge.path.data, edge.path.length
        );
    }
    if (!ruda_vector_push(&package->edges, &edge)) {
        ruda_owned_bytes_dispose(&edge.path);
        graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
        return false;
    }
    graph->total_imports += 1;
    return true;
}

static int ruda_graph_compare_edges(const void *left, const void *right) {
    const RudaCheckGraphEdge *left_edge = left;
    const RudaCheckGraphEdge *right_edge = right;
    const RudaSourceSnapshot *left_snapshot = left_edge->ast->snapshot;
    const RudaSourceSnapshot *right_snapshot = right_edge->ast->snapshot;
    int comparison = ruda_graph_compare_bytes(
        ruda_source_path(left_snapshot),
        ruda_source_path_length(left_snapshot),
        ruda_source_path(right_snapshot),
        ruda_source_path_length(right_snapshot)
    );
    if (comparison != 0) {
        return comparison;
    }
    return left_edge->node->start < right_edge->node->start ? -1
        : left_edge->node->start > right_edge->node->start;
}

static bool ruda_graph_collect_edges(
    RudaCheckGraph *graph,
    RudaCheckGraphPackage *package
) {
    size_t ast_index = 0;
    for (ast_index = 0; ast_index < package->input->ast_count; ast_index += 1) {
        const RudaAstTree *ast = package->input->asts[ast_index];
        size_t import_count = 0;
        size_t index = 0;
        for (index = 0; index < ast->root->child_count; index += 1) {
            const RudaAstElement *element = ruda_ast_child(ast->root, index);
            if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
                && element->as.node != NULL
                && element->as.node->kind == RUDA_SYNTAX_IMPORT_DECL) {
                import_count += 1;
                if (import_count > graph->options.max_imports_per_file
                    || graph->total_imports
                        >= graph->options.max_total_imports) {
                    graph->failure = RUDA_CHECK_LIMIT_REACHED;
                    return false;
                }
                if (!ruda_graph_collect_edge(
                        graph, package, ast, element->as.node
                    )) {
                    return false;
                }
            }
        }
    }
    if (package->edges.length > 1) {
        qsort(package->edges.data, package->edges.length,
            package->edges.element_size, ruda_graph_compare_edges);
    }
    return true;
}

static bool ruda_graph_visit(
    RudaCheckGraph *graph,
    size_t package_index,
    size_t depth
) {
    RudaCheckGraphPackage *package = ruda_vector_at(
        &graph->packages, package_index
    );
    size_t target = 0;
    if (depth > graph->options.max_dependency_depth) {
        graph->failure = RUDA_CHECK_LIMIT_REACHED;
        return false;
    }
    if (package->state == 2) {
        return true;
    }
    if (package->state == 1) {
        return false;
    }
    package->state = 1;
    if (!ruda_vector_push(&graph->visit_stack, &package_index)) {
        graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
        return false;
    }
    for (target = 0; target < graph->packages.length; target += 1) {
        size_t edge_index = 0;
        for (edge_index = 0; edge_index < package->edges.length;
            edge_index += 1) {
            const RudaCheckGraphEdge *edge = ruda_vector_at_const(
                &package->edges, edge_index
            );
            if (edge->target == SIZE_MAX) {
                return ruda_graph_add_diagnostic(
                    graph, RUDA_DIAGNOSTIC_UNKNOWN_PACKAGE,
                    "imported local package is not in the supplied graph",
                    edge->ast, edge->path_start, edge->path_end
                );
            }
            if (edge->target == target) {
                RudaCheckGraphPackage *dependency = ruda_vector_at(
                    &graph->packages, target
                );
                if (dependency->state == 1) {
                    return ruda_graph_add_cycle_diagnostic(
                        graph, target, edge
                    );
                }
                if (dependency->state == 0
                    && !ruda_graph_visit(graph, target, depth + 1)) {
                    return false;
                }
                break;
            }
        }
    }
    package->state = 2;
    graph->visit_stack.length -= 1;
    if (!ruda_vector_push(&graph->order, &package_index)) {
        graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
        return false;
    }
    return true;
}

static bool ruda_graph_surface_item(
    const RudaCheckGraph *graph,
    const RudaCheckPackageSurface *surface,
    const unsigned char *name,
    size_t name_length,
    bool *is_public
) {
    size_t index = 0;
    bool found = false;
    *is_public = false;
    for (index = 0; index < surface->function_count; index += 1) {
        const RudaCheckedFunction *item = ruda_checked_function(
            graph->result, surface->function_start + index
        );
        if (item != NULL && ruda_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            found = true;
            *is_public = *is_public || item->is_public;
        }
    }
    for (index = 0; index < surface->constant_count; index += 1) {
        const RudaCheckedConstant *item = ruda_checked_constant(
            graph->result, surface->constant_start + index
        );
        if (item != NULL && ruda_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    for (index = 0; index < surface->type_alias_count; index += 1) {
        const RudaCheckedTypeAlias *item = ruda_checked_type_alias(
            graph->result, surface->type_alias_start + index
        );
        if (item != NULL && ruda_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    for (index = 0; index < surface->record_count; index += 1) {
        const RudaCheckedRecord *item = ruda_checked_record(
            graph->result, surface->record_start + index
        );
        if (item != NULL && ruda_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    for (index = 0; index < surface->enum_count; index += 1) {
        const RudaCheckedEnum *item = ruda_checked_enum(
            graph->result, surface->enum_start + index
        );
        if (item != NULL && ruda_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    for (index = 0; index < surface->interface_count; index += 1) {
        const RudaCheckedInterface *item = ruda_checked_interface(
            graph->result, surface->interface_start + index
        );
        if (item != NULL && ruda_graph_equal(item->name.data,
                item->name.length, name, name_length)) {
            *is_public = item->is_public;
            return true;
        }
    }
    if (found) {
        return true;
    }
    return false;
}

static bool ruda_graph_push_binding(
    RudaCheckGraph *graph,
    RudaVector *bindings,
    const RudaCheckGraphEdge *edge,
    const RudaSyntaxToken *original,
    const RudaSyntaxToken *local,
    bool is_namespace
) {
    RudaCheckImportBinding binding;
    const unsigned char *source = ruda_source_bytes(edge->ast->snapshot);
    const RudaCheckPackageSurface *surface = NULL;
    bool is_public = true;
    size_t index = 0;
    memset(&binding, 0, sizeof(binding));
    if (source == NULL || local == NULL || local->synthetic
        || edge->target >= graph->packages.length) {
        graph->failure = RUDA_CHECK_INVALID_ARGUMENT;
        return false;
    }
    binding.ast = edge->ast;
    binding.surface_index = edge->target;
    binding.local = source + local->start;
    binding.local_length = local->end - local->start;
    binding.start = local->start;
    binding.end = local->end;
    binding.is_namespace = is_namespace;
    if (!is_namespace) {
        binding.original = source + original->start;
        binding.original_length = original->end - original->start;
        surface = ((const RudaCheckPackageSurface *const *)
            graph->surface_refs.data)[edge->target];
        if (!ruda_graph_surface_item(graph, surface, binding.original,
                binding.original_length, &is_public)) {
            return ruda_graph_add_diagnostic(
                graph, RUDA_DIAGNOSTIC_UNKNOWN_IMPORT,
                "imported package has no item with this name",
                edge->ast, original->start, original->end
            );
        }
        if (!is_public) {
            return ruda_graph_add_diagnostic(
                graph, RUDA_DIAGNOSTIC_PRIVATE_IMPORT,
                "imported item is package-private",
                edge->ast, original->start, original->end
            );
        }
    }
    for (index = 0; index < bindings->length; index += 1) {
        const RudaCheckImportBinding *existing = ruda_vector_at_const(
            bindings, index
        );
        if (existing->ast == binding.ast && ruda_graph_equal(
                existing->local, existing->local_length,
                binding.local, binding.local_length
            )) {
            return ruda_graph_add_diagnostic(
                graph, RUDA_DIAGNOSTIC_IMPORT_CONFLICT,
                "import binding conflicts with an earlier binding",
                edge->ast, binding.start, binding.end
            );
        }
    }
    if (graph->total_bindings >= graph->options.max_total_bindings) {
        graph->failure = RUDA_CHECK_LIMIT_REACHED;
        return false;
    }
    if (!ruda_vector_push(bindings, &binding)) {
        graph->failure = RUDA_CHECK_ALLOCATION_FAILED;
        return false;
    }
    graph->total_bindings += 1;
    return true;
}

static bool ruda_graph_build_bindings(
    RudaCheckGraph *graph,
    const RudaCheckGraphPackage *package,
    RudaVector *bindings
) {
    size_t edge_index = 0;
    for (edge_index = 0; edge_index < package->edges.length; edge_index += 1) {
        const RudaCheckGraphEdge *edge = ruda_vector_at_const(
            &package->edges, edge_index
        );
        const RudaAstNode *clause = ruda_graph_child_node(
            edge->node, RUDA_SYNTAX_IMPORT_CLAUSE, 0
        );
        const RudaSyntaxToken *namespace_name = ruda_graph_child_token(
            clause, RUDA_TOKEN_IDENTIFIER, 0
        );
        size_t item_index = 0;
        size_t item_count = 0;
        if (ruda_graph_child_token(clause, RUDA_TOKEN_STAR, 0) != NULL) {
            if (!ruda_graph_push_binding(graph, bindings, edge, NULL,
                    namespace_name, true)) {
                return false;
            }
            continue;
        }
        for (item_index = 0; item_index < clause->child_count;
            item_index += 1) {
            const RudaAstElement *element = ruda_ast_child(clause, item_index);
            if (element != NULL && element->kind == RUDA_AST_ELEMENT_NODE
                && element->as.node != NULL
                && element->as.node->kind == RUDA_SYNTAX_IMPORT_ITEM) {
                const RudaSyntaxToken *original = ruda_graph_child_token(
                    element->as.node, RUDA_TOKEN_IDENTIFIER, 0
                );
                const RudaSyntaxToken *alias = ruda_graph_child_token(
                    element->as.node, RUDA_TOKEN_IDENTIFIER, 1
                );
                item_count += 1;
                if (item_count > graph->options.max_items_per_import) {
                    graph->failure = RUDA_CHECK_LIMIT_REACHED;
                    return false;
                }
                if (!ruda_graph_push_binding(
                        graph, bindings, edge, original,
                        alias == NULL ? original : alias, false
                    )) {
                    return false;
                }
            }
        }
    }
    for (edge_index = 0; edge_index < bindings->length; edge_index += 1) {
        const RudaCheckImportBinding *binding = ruda_vector_at_const(
            bindings, edge_index
        );
        size_t pass = 0;
        size_t ast_index = 0;
        for (pass = 0; pass < 2; pass += 1) {
            for (ast_index = 0; ast_index < package->input->ast_count;
                ast_index += 1) {
                const RudaAstTree *ast = package->input->asts[ast_index];
                const unsigned char *source = ruda_source_bytes(ast->snapshot);
                size_t declaration_index = 0;
                if ((pass == 0) != (ast == binding->ast)) {
                    continue;
                }
                for (declaration_index = 0;
                    declaration_index < ast->root->child_count;
                    declaration_index += 1) {
                    const RudaAstElement *element = ruda_ast_child(
                        ast->root, declaration_index
                    );
                    const RudaSyntaxToken *name = NULL;
                    if (element == NULL
                        || element->kind != RUDA_AST_ELEMENT_NODE
                        || element->as.node == NULL
                        || element->as.node->kind == RUDA_SYNTAX_IMPORT_DECL) {
                        continue;
                    }
                    name = ruda_graph_child_token(
                        element->as.node, RUDA_TOKEN_IDENTIFIER, 0
                    );
                    if (name != NULL && !name->synthetic
                        && ruda_graph_equal(
                            binding->local, binding->local_length,
                            source + name->start, name->end - name->start
                        )) {
                        bool declaration_is_later = ast == binding->ast
                            && name->start > binding->start;
                        return ruda_graph_add_diagnostic(
                            graph, RUDA_DIAGNOSTIC_IMPORT_CONFLICT,
                            "import binding conflicts with a package declaration",
                            declaration_is_later ? ast : binding->ast,
                            declaration_is_later
                                ? name->start : binding->start,
                            declaration_is_later ? name->end : binding->end
                        );
                    }
                }
            }
        }
    }
    return true;
}

static void ruda_graph_capture_surface_start(
    const RudaCheckResult *result,
    RudaCheckPackageSurface *surface
) {
    surface->function_start = result->functions.length;
    surface->constant_start = result->constants.length;
    surface->type_alias_start = result->type_aliases.length;
    surface->record_start = result->records.length;
    surface->enum_start = result->enums.length;
    surface->interface_start = result->interfaces.length;
}

static void ruda_graph_capture_surface_count(
    const RudaCheckResult *result,
    RudaCheckPackageSurface *surface
) {
    surface->function_count = result->functions.length - surface->function_start;
    surface->constant_count = result->constants.length - surface->constant_start;
    surface->type_alias_count = result->type_aliases.length
        - surface->type_alias_start;
    surface->record_count = result->records.length - surface->record_start;
    surface->enum_count = result->enums.length - surface->enum_start;
    surface->interface_count = result->interfaces.length
        - surface->interface_start;
}

static void ruda_graph_dispose(RudaCheckGraph *graph) {
    size_t package_index = 0;
    for (package_index = 0; package_index < graph->packages.length;
        package_index += 1) {
        RudaCheckGraphPackage *package = ruda_vector_at(
            &graph->packages, package_index
        );
        size_t edge_index = 0;
        for (edge_index = 0; edge_index < package->edges.length;
            edge_index += 1) {
            RudaCheckGraphEdge *edge = ruda_vector_at(
                &package->edges, edge_index
            );
            ruda_owned_bytes_dispose(&edge->path);
        }
        ruda_vector_dispose(&package->edges);
    }
    ruda_vector_dispose(&graph->order);
    ruda_vector_dispose(&graph->surface_refs);
    ruda_vector_dispose(&graph->visit_stack);
    ruda_vector_dispose(&graph->packages);
}

static RudaCheckStatus ruda_graph_finish_diagnostics(
    RudaCheckGraph *graph
) {
    if (!ruda_check_clear_semantics(graph->result)) {
        ruda_graph_dispose(graph);
        ruda_check_result_dispose(graph->result);
        return RUDA_CHECK_ALLOCATION_FAILED;
    }
    ruda_graph_dispose(graph);
    return RUDA_CHECK_OK;
}

RudaCheckGraphOptions ruda_check_graph_default_options(void) {
    RudaCheckGraphOptions options;
    options.max_packages = 4096;
    options.max_total_imports = 100000;
    options.max_total_bindings = 1000000;
    options.max_imports_per_file = 10000;
    options.max_items_per_import = 10000;
    options.max_dependency_depth = 1024;
    options.package_options = ruda_check_default_options();
    return options;
}

bool ruda_check_graph_options_valid(
    RudaAllocator allocator,
    RudaCheckGraphOptions options
) {
    return options.max_packages != 0 && options.max_total_imports != 0
        && options.max_total_bindings != 0
        && options.max_imports_per_file != 0
        && options.max_items_per_import != 0
        && options.max_dependency_depth != 0
        && ruda_check_options_valid(allocator, options.package_options);
}

static RudaCheckStatus ruda_check_graph(
    const RudaCheckPackageInput *inputs,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    bool all_packages,
    RudaAllocator allocator,
    RudaCheckGraphOptions options,
    RudaCheckResult *result
) {
    RudaCheckGraph graph;
    size_t package_index = 0;
    size_t root_index = SIZE_MAX;
    const RudaAstTree *root_ast = NULL;
    memset(&graph, 0, sizeof(graph));
    if (result == NULL) {
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (inputs == NULL || package_count == 0
        || (!all_packages && (root_import_path == NULL
            || root_import_path_length == 0))
        || package_count > options.max_packages
        || !ruda_check_graph_options_valid(allocator, options)) {
        return package_count > options.max_packages
            ? RUDA_CHECK_LIMIT_REACHED : RUDA_CHECK_INVALID_ARGUMENT;
    }
    graph.allocator = allocator;
    graph.options = options;
    graph.result = result;
    graph.failure = RUDA_CHECK_OK;
    if (!ruda_vector_init(&graph.packages, allocator,
            sizeof(RudaCheckGraphPackage))
        || !ruda_vector_init(&graph.order, allocator, sizeof(size_t))
        || !ruda_vector_init(&graph.surface_refs, allocator,
            sizeof(RudaCheckPackageSurface *))
        || !ruda_vector_init(&graph.visit_stack, allocator, sizeof(size_t))) {
        ruda_graph_dispose(&graph);
        return RUDA_CHECK_ALLOCATION_FAILED;
    }
    for (package_index = 0; package_index < package_count; package_index += 1) {
        RudaCheckGraphPackage package;
        const RudaCheckPackageInput *input = &inputs[package_index];
        memset(&package, 0, sizeof(package));
        if (input->import_path_length
            > options.package_options.max_package_name_bytes) {
            ruda_graph_dispose(&graph);
            return RUDA_CHECK_LIMIT_REACHED;
        }
        if (input->import_path == NULL || input->import_path_length == 0
            || memchr(input->import_path, 0, input->import_path_length) != NULL
            || input->asts == NULL || input->ast_count == 0
            || input->ast_count > options.package_options.max_source_files
            || ruda_graph_first_ast(input) == NULL) {
            ruda_graph_dispose(&graph);
            return RUDA_CHECK_INVALID_ARGUMENT;
        }
        package.input = input;
        if (!ruda_vector_init(&package.edges, allocator,
                sizeof(RudaCheckGraphEdge))
            || !ruda_vector_push(&graph.packages, &package)) {
            ruda_vector_dispose(&package.edges);
            ruda_graph_dispose(&graph);
            return RUDA_CHECK_ALLOCATION_FAILED;
        }
    }
    qsort(graph.packages.data, graph.packages.length,
        graph.packages.element_size, ruda_graph_compare_packages);
    for (package_index = 1; package_index < graph.packages.length;
        package_index += 1) {
        const RudaCheckGraphPackage *previous = ruda_vector_at_const(
            &graph.packages, package_index - 1
        );
        const RudaCheckGraphPackage *current = ruda_vector_at_const(
            &graph.packages, package_index
        );
        if (ruda_graph_compare_packages(previous, current) == 0) {
            ruda_graph_dispose(&graph);
            return RUDA_CHECK_INVALID_ARGUMENT;
        }
    }
    root_index = all_packages ? 0 : ruda_graph_find_package(
        &graph, root_import_path, root_import_path_length
    );
    if (root_index == SIZE_MAX) {
        ruda_graph_dispose(&graph);
        return RUDA_CHECK_INVALID_ARGUMENT;
    }
    root_ast = ruda_graph_first_ast(((const RudaCheckGraphPackage *)
        graph.packages.data)[root_index].input);
    if (all_packages) {
        const RudaCheckGraphPackage *root = ruda_vector_at_const(
            &graph.packages, root_index
        );
        root_import_path = root->input->import_path;
        root_import_path_length = root->input->import_path_length;
    }
    if (!ruda_check_result_init(result, allocator, options.package_options,
            root_ast->snapshot, root_import_path, root_import_path_length)) {
        ruda_graph_dispose(&graph);
        return RUDA_CHECK_ALLOCATION_FAILED;
    }
    for (package_index = 0; package_index < graph.packages.length;
        package_index += 1) {
        RudaCheckGraphPackage *package = ruda_vector_at(
            &graph.packages, package_index
        );
        package->surface.path = (const unsigned char *)package->input->import_path;
        package->surface.path_length = package->input->import_path_length;
        {
            RudaCheckPackageSurface *surface = &package->surface;
            if (!ruda_vector_push(&graph.surface_refs, &surface)) {
                graph.failure = RUDA_CHECK_ALLOCATION_FAILED;
                goto fail;
            }
        }
        if (!ruda_graph_collect_edges(&graph, package)) {
            goto fail;
        }
    }
    if (all_packages) {
        for (package_index = 0; package_index < graph.packages.length;
            package_index += 1) {
            RudaCheckGraphPackage *package = ruda_vector_at(
                &graph.packages, package_index
            );
            if (package->state == 0
                && !ruda_graph_visit(&graph, package_index, 1)) {
                goto fail;
            }
        }
    } else if (!ruda_graph_visit(&graph, root_index, 1)) {
        goto fail;
    }
    if (result->diagnostics.length != 0) {
        return ruda_graph_finish_diagnostics(&graph);
    }
    for (package_index = 0; package_index < graph.order.length;
        package_index += 1) {
        const size_t *ordered_index = ruda_vector_at_const(
            &graph.order, package_index
        );
        RudaCheckGraphPackage *package = ruda_vector_at(
            &graph.packages, *ordered_index
        );
        RudaVector bindings;
        RudaCheckImportEnvironment imports;
        RudaCheckStatus status;
        memset(&bindings, 0, sizeof(bindings));
        memset(&imports, 0, sizeof(imports));
        if (!ruda_vector_init(&bindings, allocator,
                sizeof(RudaCheckImportBinding))) {
            graph.failure = RUDA_CHECK_ALLOCATION_FAILED;
            goto fail;
        }
        if (!ruda_graph_build_bindings(&graph, package, &bindings)) {
            ruda_vector_dispose(&bindings);
            goto fail;
        }
        if (result->diagnostics.length != 0) {
            ruda_vector_dispose(&bindings);
            return ruda_graph_finish_diagnostics(&graph);
        }
        imports.surfaces = graph.surface_refs.data;
        imports.surface_count = graph.packages.length;
        imports.bindings = bindings.data;
        imports.binding_count = bindings.length;
        ruda_graph_capture_surface_start(result, &package->surface);
        status = ruda_checker_check_package_unit(
            package->input->asts,
            package->input->ast_count,
            package->input->import_path,
            package->input->import_path_length,
            &imports,
            false,
            allocator,
            options.package_options,
            result
        );
        ruda_vector_dispose(&bindings);
        if (status != RUDA_CHECK_OK) {
            graph.failure = status;
            goto fail;
        }
        if (result->functions.length
                > options.package_options.max_functions
            || result->constants.length
                > options.package_options.max_constants
            || result->type_aliases.length
                > options.package_options.max_type_aliases
            || result->records.length > options.package_options.max_records
            || result->enums.length > options.package_options.max_enums
            || result->interfaces.length
                > options.package_options.max_interfaces) {
            graph.failure = RUDA_CHECK_LIMIT_REACHED;
            goto fail;
        }
        ruda_graph_capture_surface_count(result, &package->surface);
        if (result->diagnostics.length != 0) {
            return ruda_graph_finish_diagnostics(&graph);
        }
    }
    ruda_graph_dispose(&graph);
    return RUDA_CHECK_OK;
fail:
    ruda_graph_dispose(&graph);
    if (graph.failure != RUDA_CHECK_OK) {
        ruda_check_result_dispose(result);
        return graph.failure;
    }
    return RUDA_CHECK_OK;
}

RudaCheckStatus ruda_check_package_graph(
    const RudaCheckPackageInput *inputs,
    size_t package_count,
    const char *root_import_path,
    size_t root_import_path_length,
    RudaAllocator allocator,
    RudaCheckGraphOptions options,
    RudaCheckResult *result
) {
    return ruda_check_graph(
        inputs, package_count, root_import_path, root_import_path_length,
        false, allocator, options, result
    );
}

RudaCheckStatus ruda_check_module_graph(
    const RudaCheckPackageInput *inputs,
    size_t package_count,
    RudaAllocator allocator,
    RudaCheckGraphOptions options,
    RudaCheckResult *result
) {
    return ruda_check_graph(
        inputs, package_count, NULL, 0, true, allocator, options, result
    );
}
