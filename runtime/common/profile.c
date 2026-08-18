#include "ruda/runtime/profile.h"

#ifndef RUDA_RUNTIME_BUILD_PROFILE
#error "RUDA_RUNTIME_BUILD_PROFILE must name the native runtime profile"
#endif

#ifndef RUDA_RUNTIME_BUILD_VERSION
#error "RUDA_RUNTIME_BUILD_VERSION must name the native runtime version"
#endif

const char ruda_runtime_profile_marker[] =
    "ruda-native-runtime-profile-v1:" RUDA_RUNTIME_BUILD_PROFILE;
const char ruda_runtime_version_marker[] =
    "ruda-native-runtime-version-v1:" RUDA_RUNTIME_BUILD_VERSION;

const char *ruda_runtime_build_profile(void) {
    return ruda_runtime_profile_marker
        + sizeof("ruda-native-runtime-profile-v1:") - 1;
}

const char *ruda_runtime_build_version(void) {
    return ruda_runtime_version_marker
        + sizeof("ruda-native-runtime-version-v1:") - 1;
}
