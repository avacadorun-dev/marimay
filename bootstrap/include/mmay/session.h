#ifndef MMAY_SESSION_H
#define MMAY_SESSION_H

#include "mmay/base/allocator.h"

typedef struct MarimaySession {
    MarimayAllocator allocator;
} MarimaySession;

/* The allocator state, if any, must outlive the session. */
void mmay_session_init(MarimaySession *session, MarimayAllocator allocator);

#endif
