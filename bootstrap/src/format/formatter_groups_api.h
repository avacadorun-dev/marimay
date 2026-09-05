#ifndef MMAY_FORMAT_FORMATTER_GROUPS_API_H
#define MMAY_FORMAT_FORMATTER_GROUPS_API_H

#include "formatter_internal.h"

bool mmay_formatter_boolean_node(const MarimayGreenNode *node);
bool mmay_formatter_boolean_group(MarimayFormatCollector *collector, size_t start);
MarimayDocumentStatus mmay_formatter_boolean_boundary(
    MarimayDocument *document, const MarimayFormatToken *token, bool begin
);
MarimayDocumentStatus mmay_formatter_begin_statement_group(
    MarimayFormatter *formatter, size_t statement_id
);
MarimayDocumentStatus mmay_formatter_end_statement_group(MarimayFormatter *formatter);

#endif
