"""Configuration contract: supported C compilers, fallback, profiles and target scope."""
from pathlib import Path
import subprocess
import sys
import tempfile

module = Path(sys.argv[1]).resolve()
cmake = sys.argv[2]
fixture = '''cmake_minimum_required(VERSION 3.25)
project(IpoContract C)
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/mock")
add_library(mmay_bootstrap STATIC unit.c)
add_library(mmay_sljit STATIC unit.c)
add_library(mmay_native_runtime_release STATIC unit.c)
add_executable(unrelated unit.c)
add_executable(mmay unit.c)
target_link_libraries(mmay PRIVATE mmay_bootstrap)
add_subdirectory(child)
include("@MODULE@")
foreach(target mmay_bootstrap mmay_sljit mmay mmay_test mmay_native_runtime_release unrelated)
  get_target_property(all ${target} INTERPROCEDURAL_OPTIMIZATION)
  get_target_property(release ${target} INTERPROCEDURAL_OPTIMIZATION_RELEASE)
  get_target_property(debug ${target} INTERPROCEDURAL_OPTIMIZATION_DEBUG)
  file(APPEND "${CMAKE_BINARY_DIR}/properties.txt" "${target}:${all}:${release}:${debug}\\n")
endforeach()
'''.replace('@MODULE@', module.as_posix())
mock = '''function(check_ipo_supported)
  cmake_parse_arguments(check "" "RESULT;OUTPUT" "LANGUAGES" ${ARGN})
  if(NOT check_LANGUAGES STREQUAL "C")
    message(FATAL_ERROR "The portable bootstrap must probe C IPO")
  endif()
  set(${check_RESULT} ${PROBE_SUPPORTED} PARENT_SCOPE)
  set(${check_OUTPUT} "injected unsupported toolchain" PARENT_SCOPE)
endfunction()
'''
cases = [('AUTO', 'Release', True, True, 'release'),
         ('AUTO', 'Debug', True, True, 'none'),
         ('OFF', 'Release', True, True, 'none'),
         ('AUTO', 'Release', False, True, 'none'),
         ('ON', 'Debug', True, True, 'all'),
         ('ON', 'Release', False, False, 'unavailable'),
         ('invalid', 'Release', True, False, 'must be AUTO')]
with tempfile.TemporaryDirectory(prefix='mmay-ipo-contract-') as temporary:
    root = Path(temporary)
    source = root / 'source'
    (source / 'mock').mkdir(parents=True)
    (source / 'child').mkdir()
    (source / 'unit.c').write_text('int main(void) { return 0; }\n')
    (source / 'CMakeLists.txt').write_text(fixture)
    (source / 'mock/CheckIPOSupported.cmake').write_text(mock)
    (source / 'child/CMakeLists.txt').write_text(
        'add_executable(mmay_test ../unit.c)\ntarget_link_libraries(mmay_test PRIVATE mmay_bootstrap)\n')
    for index, (policy, profile, supported, success, expected) in enumerate(cases):
        build = root / str(index)
        result = subprocess.run([cmake, '-S', str(source), '-B', str(build),
                                 '-DMMAY_COMPILER_IPO=' + policy,
                                 '-DCMAKE_BUILD_TYPE=' + profile,
                                 '-DPROBE_SUPPORTED=' + ('TRUE' if supported else 'FALSE')],
                                capture_output=True, text=True, timeout=30)
        assert (result.returncode == 0) == success, (policy, profile, result.stdout, result.stderr)
        if not success:
            assert expected in result.stdout + result.stderr
            continue
        for line in (build / 'properties.txt').read_text().splitlines():
            target, all_configs, release, debug = line.split(':')
            enabled = target in {'mmay_bootstrap', 'mmay_sljit', 'mmay', 'mmay_test'}
            assert (all_configs == 'TRUE') == (enabled and expected == 'all'), line
            assert (release == 'TRUE') == (enabled and expected == 'release'), line
            assert debug != 'TRUE', line
print('compiler IPO: C-only support probe, AUTO/ON/OFF, fallback, profiles and target scope passed')
