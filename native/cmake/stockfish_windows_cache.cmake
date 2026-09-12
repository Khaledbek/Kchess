# -----------------------------------------------------------------------------
# Section: Persistent Windows Stockfish library locations
# -----------------------------------------------------------------------------

set(KCHESS_STOCKFISH_CACHE_VERSION "sf18-sf19-v1")
set(KCHESS_STOCKFISH_PREBUILT_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/prebuilt/windows/x64/${KCHESS_STOCKFISH_CACHE_VERSION}"
)
set(KCHESS_STOCKFISH_CACHE_BUILD_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/.stockfish_build_cache/windows/x64/${KCHESS_STOCKFISH_CACHE_VERSION}"
)
set(KCHESS_STOCKFISH_CACHE_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stockfish_windows_cache"
)

set(_kchess_stockfish18_debug_lib
  "${KCHESS_STOCKFISH_PREBUILT_DIR}/Debug/stockfish_official.lib"
)
set(_kchess_stockfish18_profile_lib
  "${KCHESS_STOCKFISH_PREBUILT_DIR}/Profile/stockfish_official.lib"
)
set(_kchess_stockfish18_release_lib
  "${KCHESS_STOCKFISH_PREBUILT_DIR}/Release/stockfish_official.lib"
)
set(_kchess_stockfish19_debug_lib
  "${KCHESS_STOCKFISH_PREBUILT_DIR}/Debug/stockfish19_official.lib"
)
set(_kchess_stockfish19_profile_lib
  "${KCHESS_STOCKFISH_PREBUILT_DIR}/Profile/stockfish19_official.lib"
)
set(_kchess_stockfish19_release_lib
  "${KCHESS_STOCKFISH_PREBUILT_DIR}/Release/stockfish19_official.lib"
)

# -----------------------------------------------------------------------------
# Section: Seed from an existing Flutter build when available
# -----------------------------------------------------------------------------

# Update 23 can reuse the libraries from the user's current Debug build before
# the first `flutter clean`. This avoids an otherwise unnecessary one-time
# rebuild while moving the libraries into the persistent source-adjacent cache.
set(_kchess_flutter_windows_build
  "${CMAKE_CURRENT_SOURCE_DIR}/../flutter_app/build/windows/x64"
)
if(EXISTS "${_kchess_flutter_windows_build}")
  file(GLOB_RECURSE _kchess_existing_stockfish_libs
    LIST_DIRECTORIES FALSE
    "${_kchess_flutter_windows_build}/stockfish*_official.lib"
  )
  foreach(_candidate IN LISTS _kchess_existing_stockfish_libs)
    get_filename_component(_candidate_name "${_candidate}" NAME)
    foreach(_config IN ITEMS Debug Profile Release)
      if(_candidate MATCHES "/${_config}/[^/]+$")
        set(_seed_dir "${KCHESS_STOCKFISH_PREBUILT_DIR}/${_config}")
        set(_seed_path "${_seed_dir}/${_candidate_name}")
        if(NOT EXISTS "${_seed_path}")
          file(MAKE_DIRECTORY "${_seed_dir}")
          file(COPY "${_candidate}" DESTINATION "${_seed_dir}")
          message(STATUS
            "KChess: seeded persistent ${_candidate_name} from existing ${_config} build"
          )
        endif()
      endif()
    endforeach()
  endforeach()
endif()

# -----------------------------------------------------------------------------
# Section: Lazy one-time cache population
# -----------------------------------------------------------------------------

if(CMAKE_CONFIGURATION_TYPES)
  set(_kchess_stockfish_cache_config "$<CONFIG>")
elseif(CMAKE_BUILD_TYPE)
  set(_kchess_stockfish_cache_config "${CMAKE_BUILD_TYPE}")
else()
  set(_kchess_stockfish_cache_config "Debug")
endif()

add_custom_target(kchess_stockfish_windows_cache
  COMMAND "${CMAKE_COMMAND}"
    "-DKCHESS_CACHE_CONFIG=${_kchess_stockfish_cache_config}"
    "-DKCHESS_CACHE_BUILD_DIR=${KCHESS_STOCKFISH_CACHE_BUILD_DIR}"
    "-DKCHESS_PREBUILT_DIR=${KCHESS_STOCKFISH_PREBUILT_DIR}"
    "-DKCHESS_CACHE_SOURCE_DIR=${KCHESS_STOCKFISH_CACHE_SOURCE_DIR}"
    "-DKCHESS_CACHE_GENERATOR=${CMAKE_GENERATOR}"
    "-DKCHESS_CACHE_PLATFORM=${CMAKE_GENERATOR_PLATFORM}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ensure_stockfish_windows_cache.cmake"
  VERBATIM
)

# -----------------------------------------------------------------------------
# Section: Imported engine targets used by KChess
# -----------------------------------------------------------------------------

add_library(stockfish_official STATIC IMPORTED GLOBAL)
set_target_properties(stockfish_official PROPERTIES
  IMPORTED_CONFIGURATIONS "Debug;Profile;Release"
  IMPORTED_LOCATION_DEBUG "${_kchess_stockfish18_debug_lib}"
  IMPORTED_LOCATION_PROFILE "${_kchess_stockfish18_profile_lib}"
  IMPORTED_LOCATION_RELEASE "${_kchess_stockfish18_release_lib}"
  INTERFACE_INCLUDE_DIRECTORIES "${STOCKFISH_SOURCE_DIR}"
)
add_dependencies(stockfish_official kchess_stockfish_windows_cache)

add_library(stockfish19_official STATIC IMPORTED GLOBAL)
set_target_properties(stockfish19_official PROPERTIES
  IMPORTED_CONFIGURATIONS "Debug;Profile;Release"
  IMPORTED_LOCATION_DEBUG "${_kchess_stockfish19_debug_lib}"
  IMPORTED_LOCATION_PROFILE "${_kchess_stockfish19_profile_lib}"
  IMPORTED_LOCATION_RELEASE "${_kchess_stockfish19_release_lib}"
)
add_dependencies(stockfish19_official kchess_stockfish_windows_cache)
