#include "ruda/session.h"

void ruda_session_init(RudaSession *session, RudaAllocator allocator) {
    if (session != NULL) {
        session->allocator = allocator;
    }
}
