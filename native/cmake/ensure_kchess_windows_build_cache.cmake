# -----------------------------------------------------------------------------
# Section: Persistent kchess_core cache configuration
# -----------------------------------------------------------------------------

foreach(_required_var IN ITEMS
    KCHESS_CACHE_CONFIG
    KCHESS_CACHE_BUILD_DIR
    KCHESS_NATIVE_SOURCE_DIR
    KCHESS_CACHE_GENERATOR)
  if(NOT DEFINED ${_required_var} OR "${${_required_var}}" STREQUAL "")
    message(FATAL_ERROR "Missing required native build cache variable: ${_required_var}")
  endif()
endforeach()

file(MAKE_DIRECTORY "${KCHESS_CACHE_BUILD_DIR}")

# Configure only when the persistent child tree does not exist yet. Once it
# exists, CMake's own ZERO_CHECK/regeneration logic keeps CMake inputs current.
# The child build therefore preserves its complete dependency graph across a
# Flutter clean without inventing a second source-change detector.
if(NOT EXISTS "${KCHESS_CACHE_BUILD_DIR}/CMakeCache.txt")
  set(_configure_command
    "${CMAKE_COMMAND}"
    -S "${KCHESS_NATIVE_SOURCE_DIR}"
    -B "${KCHESS_CACHE_BUILD_DIR}"
    -G "${KCHESS_CACHE_GENERATOR}"
    -DKCHESS_BUILD_TESTS=OFF
    -DKCHESS_WITH_STOCKFISH=ON
    -DKCHESS_PERSISTENT_WINDOWS_BUILD=ON
  )
  if(DEFINED KCHESS_CACHE_PLATFORM AND NOT KCHESS_CACHE_PLATFORM STREQUAL "")
    list(APPEND _configure_command -A "${KCHESS_CACHE_PLATFORM}")
  endif()

  message(STATUS "KChess: configuring persistent Windows native build cache")
  execute_process(
    COMMAND ${_configure_command}
    RESULT_VARIABLE _configure_result
  )
  if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
      "KChess persistent native cache configuration failed (${_configure_result})"
    )
  endif()
endif()

# Always ask the child build for kchess_core. MSBuild/CMake performs the real
# incremental decision and only recompiles sources whose dependency inputs have
# changed. --parallel enables project/compiler parallelism where supported.
execute_process(
  COMMAND "${CMAKE_COMMAND}"
    --build "${KCHESS_CACHE_BUILD_DIR}"
    --config "${KCHESS_CACHE_CONFIG}"
    --target kchess_core
    --parallel
  RESULT_VARIABLE _build_result
)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
    "KChess persistent native cache build failed (${_build_result})"
  )
endif()

set(_core_dll
  "${KCHESS_CACHE_BUILD_DIR}/${KCHESS_CACHE_CONFIG}/kchess_core.dll"
)
if(NOT EXISTS "${_core_dll}")
  message(FATAL_ERROR
    "KChess persistent native cache did not produce ${_core_dll}"
  )
endif()
