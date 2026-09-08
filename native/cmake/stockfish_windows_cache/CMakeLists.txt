cmake_minimum_required(VERSION 3.18)

project(kchess_stockfish_windows_cache LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Section: Persistent Windows cache inputs
# -----------------------------------------------------------------------------

if(NOT MSVC)
  message(FATAL_ERROR "KChess persistent Stockfish cache currently supports MSVC only")
endif()

if(NOT DEFINED KCHESS_STOCKFISH_PREBUILT_DIR
   OR KCHESS_STOCKFISH_PREBUILT_DIR STREQUAL "")
  message(FATAL_ERROR "KCHESS_STOCKFISH_PREBUILT_DIR must be provided")
endif()

get_filename_component(KCHESS_NATIVE_DIR
  "${CMAKE_CURRENT_LIST_DIR}/../.."
  ABSOLUTE
)
get_filename_component(KCHESS_REPOSITORY_ROOT
  "${KCHESS_NATIVE_DIR}/.."
  ABSOLUTE
)

set(STOCKFISH_SOURCE_DIR
  "${KCHESS_REPOSITORY_ROOT}/third_party/stockfish/source/src"
)
include("${KCHESS_NATIVE_DIR}/cmake/fetch_stockfish19.cmake")
set(STOCKFISH19_SOURCE_ROOT "${KCHESS_STOCKFISH19_SOURCE_ROOT}")
set(STOCKFISH19_SOURCE_DIR "${KCHESS_STOCKFISH19_SOURCE_DIR}")
set(STOCKFISH19_NET "${KCHESS_STOCKFISH19_NET}")

if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "Debug;Profile;Release"
    CACHE STRING "" FORCE)
endif()
set(CMAKE_CXX_FLAGS_PROFILE "${CMAKE_CXX_FLAGS_RELEASE}")

# Match the Windows Flutter parent project's global character-set definitions
# so a library seeded from an existing build and a freshly cached library use
# the same compilation environment.
add_definitions(-DUNICODE -D_UNICODE)

# -----------------------------------------------------------------------------
# Section: Stockfish 18 static library
# -----------------------------------------------------------------------------

file(GLOB_RECURSE STOCKFISH_SOURCES CONFIGURE_DEPENDS
  "${STOCKFISH_SOURCE_DIR}/*.cpp"
)
list(REMOVE_ITEM STOCKFISH_SOURCES "${STOCKFISH_SOURCE_DIR}/main.cpp")

set(STOCKFISH_BIG_NET
  "${STOCKFISH_SOURCE_DIR}/nn-c288c895ea92.nnue"
)
set(STOCKFISH_SMALL_NET
  "${STOCKFISH_SOURCE_DIR}/nn-37f18f62d772.nnue"
)

add_subdirectory(
  "${KCHESS_NATIVE_DIR}/cmake/stockfish_target"
  "${CMAKE_CURRENT_BINARY_DIR}/stockfish18_target"
)
target_compile_features(stockfish_official PUBLIC cxx_std_20)
target_include_directories(stockfish_official PUBLIC ${STOCKFISH_SOURCE_DIR})
set_target_properties(stockfish_official PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(stockfish_official PRIVATE USE_SSE2)
target_compile_options(stockfish_official PRIVATE /W3 /EHsc /bigobj /FS)

# -----------------------------------------------------------------------------
# Section: Stockfish 19 static library
# -----------------------------------------------------------------------------

set(STOCKFISH19_SOURCES
  "${STOCKFISH19_SOURCE_DIR}/attacks.cpp"
  "${STOCKFISH19_SOURCE_DIR}/benchmark.cpp"
  "${STOCKFISH19_SOURCE_DIR}/bitboard.cpp"
  "${STOCKFISH19_SOURCE_DIR}/engine.cpp"
  "${STOCKFISH19_SOURCE_DIR}/evaluate.cpp"
  "${STOCKFISH19_SOURCE_DIR}/memory.cpp"
  "${STOCKFISH19_SOURCE_DIR}/misc.cpp"
  "${STOCKFISH19_SOURCE_DIR}/movegen.cpp"
  "${STOCKFISH19_SOURCE_DIR}/movepick.cpp"
  "${STOCKFISH19_SOURCE_DIR}/position.cpp"
  "${STOCKFISH19_SOURCE_DIR}/score.cpp"
  "${STOCKFISH19_SOURCE_DIR}/search.cpp"
  "${STOCKFISH19_SOURCE_DIR}/thread.cpp"
  "${STOCKFISH19_SOURCE_DIR}/timeman.cpp"
  "${STOCKFISH19_SOURCE_DIR}/tt.cpp"
  "${STOCKFISH19_SOURCE_DIR}/uci.cpp"
  "${STOCKFISH19_SOURCE_DIR}/ucioption.cpp"
  "${STOCKFISH19_SOURCE_DIR}/tune.cpp"
  "${STOCKFISH19_SOURCE_DIR}/syzygy/tbprobe.cpp"
  "${STOCKFISH19_SOURCE_DIR}/nnue/nnue_accumulator.cpp"
  "${STOCKFISH19_SOURCE_DIR}/nnue/nnue_misc.cpp"
  "${STOCKFISH19_SOURCE_DIR}/nnue/network.cpp"
  "${STOCKFISH19_SOURCE_DIR}/nnue/features/half_ka_v2_hm.cpp"
  "${STOCKFISH19_SOURCE_DIR}/nnue/features/full_threats.cpp"
  "${STOCKFISH19_SOURCE_DIR}/nnue/features/pp_3wide.cpp"
)
foreach(_sf19_source IN LISTS STOCKFISH19_SOURCES)
  if(NOT EXISTS "${_sf19_source}")
    message(FATAL_ERROR "Stockfish 19 source file is missing: ${_sf19_source}")
  endif()
endforeach()

add_subdirectory(
  "${KCHESS_NATIVE_DIR}/cmake/stockfish19_target"
  "${CMAKE_CURRENT_BINARY_DIR}/stockfish19_target"
)
target_compile_features(stockfish19_official PUBLIC cxx_std_20)
target_include_directories(stockfish19_official PRIVATE ${STOCKFISH19_SOURCE_DIR})
set_target_properties(stockfish19_official PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(stockfish19_official PRIVATE USE_SSE2)
target_compile_options(stockfish19_official PRIVATE /W3 /EHsc /bigobj /FS)

# -----------------------------------------------------------------------------
# Section: Stable configuration-specific library outputs
# -----------------------------------------------------------------------------

foreach(_target IN ITEMS stockfish_official stockfish19_official)
  set_target_properties(${_target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG
      "${KCHESS_STOCKFISH_PREBUILT_DIR}/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_PROFILE
      "${KCHESS_STOCKFISH_PREBUILT_DIR}/Profile"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE
      "${KCHESS_STOCKFISH_PREBUILT_DIR}/Release"
  )
endforeach()
