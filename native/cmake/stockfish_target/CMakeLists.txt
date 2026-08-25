# Create the Stockfish target in its own CMake directory scope.
#
# CMake's classic MSVC Debug defaults include /Od and /RTC1. /RTC cannot be
# combined with optimized /O* code, and /Od makes Stockfish dramatically slower
# while running Kchess through `flutter run`. Strip only those defaults in this
# directory and enable /O2 for Stockfish. The parent Kchess targets remain
# ordinary Debug targets with their normal runtime checks and symbols.
if(MSVC)
  string(REGEX REPLACE "/RTC(su|[1su])" ""
    CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/Od" ""
    CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/Ob0" ""
    CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " /O2 /Ob2")
endif()

add_library(stockfish_official STATIC ${STOCKFISH_SOURCES})

# Preserve the NNUE network dependency tracking in the same directory scope as
# the target's source files.
set_property(SOURCE "${STOCKFISH_SOURCE_DIR}/nnue/network.cpp" APPEND PROPERTY
  OBJECT_DEPENDS "${STOCKFISH_BIG_NET};${STOCKFISH_SMALL_NET}"
)
