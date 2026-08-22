#include <stdio.h>

#include "mmay/runtime/profile.h"

int main(void) {
    return puts(mmay_runtime_build_profile()) < 0 ? 1 : 0;
}
