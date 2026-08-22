#include "mmay/session.h"

void mmay_session_init(MarimaySession *session, MarimayAllocator allocator) {
    if (session != NULL) {
        session->allocator = allocator;
    }
}
