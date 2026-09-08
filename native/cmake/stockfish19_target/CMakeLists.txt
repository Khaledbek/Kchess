# Stockfish 19 is compiled in a renamed C++ namespace so it can coexist in the
# same KChess shared library with the untouched Stockfish 18 implementation.
if(MSVC)
  string(REGEX REPLACE "/RTC(su|[1su])" ""
    CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/Od" ""
    CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/Ob0" ""
    CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(APPEND CMAKE_CXX_FLAGS_DEBUG " /O2 /Ob2")
endif()

add_library(stockfish19_official STATIC ${STOCKFISH19_SOURCES})
target_compile_definitions(stockfish19_official PRIVATE Stockfish=Stockfish19)

set_property(SOURCE "${STOCKFISH19_SOURCE_DIR}/nnue/network.cpp" APPEND PROPERTY
  OBJECT_DEPENDS "${STOCKFISH19_NET}"
)
