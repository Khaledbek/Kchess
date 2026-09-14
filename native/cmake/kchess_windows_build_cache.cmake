# -----------------------------------------------------------------------------
# Section: Persistent Windows kchess_core build
# -----------------------------------------------------------------------------
#
# Flutter places its normal CMake tree below flutter_app/build. `flutter clean`
# removes that tree. KChess therefore delegates the large native core to a
# per-checkout CMake/MSBuild tree below %LOCALAPPDATA%. The child build remains
# authoritative for dependency tracking: changed sources/headers are rebuilt,
# unchanged object files are reused.

get_filename_component(KCHESS_NATIVE_SOURCE_DIR
  "${CMAKE_CURRENT_LIST_DIR}/.."
  ABSOLUTE
)
include("${CMAKE_CURRENT_LIST_DIR}/windows_build_cache_root.cmake")
kchess_resolve_windows_build_cache_root(
  "${KCHESS_NATIVE_SOURCE_DIR}"
  KCHESS_WINDOWS_BUILD_CACHE_ROOT
)

set(KCHESS_WINDOWS_BUILD_CACHE_VERSION "v2")
set(KCHESS_WINDOWS_BUILD_CACHE_DIR
  "${KCHESS_WINDOWS_BUILD_CACHE_ROOT}/kchess-core/windows-x64/${KCHESS_WINDOWS_BUILD_CACHE_VERSION}"
)

if(CMAKE_CONFIGURATION_TYPES)
  set(_kchess_native_cache_config "$<CONFIG>")
elseif(CMAKE_BUILD_TYPE)
  set(_kchess_native_cache_config "${CMAKE_BUILD_TYPE}")
else()
  set(_kchess_native_cache_config "Debug")
endif()

set(KCHESS_WINDOWS_CORE_DLL
  "${KCHESS_WINDOWS_BUILD_CACHE_DIR}/${_kchess_native_cache_config}/kchess_core.dll"
)

add_custom_target(kchess_native_windows_cache ALL
  COMMAND "${CMAKE_COMMAND}"
    "-DKCHESS_CACHE_CONFIG=${_kchess_native_cache_config}"
    "-DKCHESS_CACHE_BUILD_DIR=${KCHESS_WINDOWS_BUILD_CACHE_DIR}"
    "-DKCHESS_NATIVE_SOURCE_DIR=${KCHESS_NATIVE_SOURCE_DIR}"
    "-DKCHESS_CACHE_GENERATOR=${CMAKE_GENERATOR}"
    "-DKCHESS_CACHE_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    -P "${CMAKE_CURRENT_LIST_DIR}/ensure_kchess_windows_build_cache.cmake"
  VERBATIM
)
