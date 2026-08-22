#include "mmay/runtime/profile.h"

#ifndef MMAY_RUNTIME_BUILD_PROFILE
#error "MMAY_RUNTIME_BUILD_PROFILE must name the native runtime profile"
#endif

#ifndef MMAY_RUNTIME_BUILD_VERSION
#error "MMAY_RUNTIME_BUILD_VERSION must name the native runtime version"
#endif

const char mmay_runtime_profile_marker[] =
    "mmay-native-runtime-profile-v1:" MMAY_RUNTIME_BUILD_PROFILE;
const char mmay_runtime_version_marker[] =
    "mmay-native-runtime-version-v1:" MMAY_RUNTIME_BUILD_VERSION;

const char *mmay_runtime_build_profile(void) {
    return mmay_runtime_profile_marker
        + sizeof("mmay-native-runtime-profile-v1:") - 1;
}

const char *mmay_runtime_build_version(void) {
    return mmay_runtime_version_marker
        + sizeof("mmay-native-runtime-version-v1:") - 1;
}
