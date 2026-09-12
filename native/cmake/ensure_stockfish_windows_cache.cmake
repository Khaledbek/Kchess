# -----------------------------------------------------------------------------
# Section: Persistent Stockfish cache validation
# -----------------------------------------------------------------------------

foreach(_required_var IN ITEMS
    KCHESS_CACHE_CONFIG
    KCHESS_CACHE_BUILD_DIR
    KCHESS_PREBUILT_DIR
    KCHESS_CACHE_SOURCE_DIR
    KCHESS_CACHE_GENERATOR)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "Missing required cache variable: ${_required_var}")
  endif()
endforeach()

set(_sf18_lib
  "${KCHESS_PREBUILT_DIR}/${KCHESS_CACHE_CONFIG}/stockfish_official.lib"
)
set(_sf19_lib
  "${KCHESS_PREBUILT_DIR}/${KCHESS_CACHE_CONFIG}/stockfish19_official.lib"
)

set(_missing_targets)
if(NOT EXISTS "${_sf18_lib}")
  list(APPEND _missing_targets stockfish_official)
endif()
if(NOT EXISTS "${_sf19_lib}")
  list(APPEND _missing_targets stockfish19_official)
endif()

if(NOT _missing_targets)
  message(STATUS
    "KChess: reusing persistent Stockfish 18/19 ${KCHESS_CACHE_CONFIG} libraries"
  )
  return()
endif()

# -----------------------------------------------------------------------------
# Section: One-time cache creation
# -----------------------------------------------------------------------------

file(MAKE_DIRECTORY "${KCHESS_CACHE_BUILD_DIR}")
file(MAKE_DIRECTORY "${KCHESS_PREBUILT_DIR}/${KCHESS_CACHE_CONFIG}")

set(_configure_command
  "${CMAKE_COMMAND}"
  -S "${KCHESS_CACHE_SOURCE_DIR}"
  -B "${KCHESS_CACHE_BUILD_DIR}"
  -G "${KCHESS_CACHE_GENERATOR}"
  "-DKCHESS_STOCKFISH_PREBUILT_DIR=${KCHESS_PREBUILT_DIR}"
)
if(DEFINED KCHESS_CACHE_PLATFORM AND NOT KCHESS_CACHE_PLATFORM STREQUAL "")
  list(APPEND _configure_command -A "${KCHESS_CACHE_PLATFORM}")
endif()

message(STATUS
  "KChess: creating missing persistent Stockfish libraries: ${_missing_targets}"
)
execute_process(
  COMMAND ${_configure_command}
  RESULT_VARIABLE _configure_result
)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
    "KChess persistent Stockfish cache configuration failed (${_configure_result})"
  )
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    --build "${KCHESS_CACHE_BUILD_DIR}"
    --config "${KCHESS_CACHE_CONFIG}"
    --target ${_missing_targets}
  RESULT_VARIABLE _build_result
)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
    "KChess persistent Stockfish cache build failed (${_build_result})"
  )
endif()

if(NOT EXISTS "${_sf18_lib}" OR NOT EXISTS "${_sf19_lib}")
  # Only require the library that was requested to be built. The other one may
  # already have existed before this script ran.
  foreach(_target IN LISTS _missing_targets)
    if(_target STREQUAL "stockfish_official" AND NOT EXISTS "${_sf18_lib}")
      message(FATAL_ERROR "Persistent Stockfish 18 library was not produced")
    endif()
    if(_target STREQUAL "stockfish19_official" AND NOT EXISTS "${_sf19_lib}")
      message(FATAL_ERROR "Persistent Stockfish 19 library was not produced")
    endif()
  endforeach()
endif()
