#ifndef MMAY_LSP_SERVER_H
#define MMAY_LSP_SERVER_H

#include "mmay/base/stream.h"
#include "mmay/base/writer.h"
#include "mmay/session.h"

/* Synchronous, bounded LSP 3.17 server. Streams remain caller-owned. */
int mmay_lsp_run(MarimaySession *session, MarimayReadStream input,
    MarimayWriter output, MarimayWriter error);

#endif
