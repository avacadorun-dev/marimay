#ifndef RUDA_SESSION_H
#define RUDA_SESSION_H

#include "ruda/base/allocator.h"

typedef struct RudaSession {
    RudaAllocator allocator;
} RudaSession;

/* The allocator state, if any, must outlive the session. */
void ruda_session_init(RudaSession *session, RudaAllocator allocator);

#endif
