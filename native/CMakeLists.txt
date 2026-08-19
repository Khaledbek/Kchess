cmake_minimum_required(VERSION 3.18)

project(kchess_core VERSION 0.1.0 LANGUAGES C CXX)

option(KCHESS_BUILD_TESTS "Build native unit tests" ON)
option(KCHESS_WITH_STOCKFISH "Build the official Stockfish engine" ON)

set(STOCKFISH_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/stockfish/source/src"
)

if(KCHESS_WITH_STOCKFISH)
  file(GLOB_RECURSE STOCKFISH_SOURCES CONFIGURE_DEPENDS
    "${STOCKFISH_SOURCE_DIR}/*.cpp"
  )
  list(REMOVE_ITEM STOCKFISH_SOURCES "${STOCKFISH_SOURCE_DIR}/main.cpp")

  add_library(stockfish_official STATIC ${STOCKFISH_SOURCES})
  target_compile_features(stockfish_official PUBLIC cxx_std_20)
  target_include_directories(stockfish_official PUBLIC ${STOCKFISH_SOURCE_DIR})
  set_target_properties(stockfish_official PROPERTIES POSITION_INDEPENDENT_CODE ON)

  set(STOCKFISH_BIG_NET
    "${STOCKFISH_SOURCE_DIR}/nn-c288c895ea92.nnue"
  )
  set(STOCKFISH_SMALL_NET
    "${STOCKFISH_SOURCE_DIR}/nn-37f18f62d772.nnue"
  )
  set_property(SOURCE "${STOCKFISH_SOURCE_DIR}/nnue/network.cpp" APPEND PROPERTY
    OBJECT_DEPENDS "${STOCKFISH_BIG_NET};${STOCKFISH_SMALL_NET}"
  )

  if(MSVC)
    target_compile_options(stockfish_official PRIVATE /W3 /EHsc /bigobj)
  else()
    target_compile_options(stockfish_official PRIVATE
      -O3
      -fno-exceptions
      -fno-rtti
      "-Wa,-I${STOCKFISH_SOURCE_DIR}"
    )
  endif()
endif()

add_library(kchess_sqlite STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sqlite/sqlite3.c
)
target_include_directories(kchess_sqlite PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/sqlite
)
target_compile_definitions(kchess_sqlite PRIVATE
  SQLITE_DQS=0
  SQLITE_DEFAULT_FOREIGN_KEYS=1
  SQLITE_ENABLE_FTS5
  SQLITE_OMIT_LOAD_EXTENSION
  SQLITE_THREADSAFE=1
)
set_target_properties(kchess_sqlite PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(kchess_core SHARED
  src/api/core_api.cpp
  src/analysis/accuracy.cpp
  src/analysis/move_classifier.cpp
  src/chess/fen.cpp
  src/chess/move.cpp
  src/chess/pgn.cpp
  src/core/core.cpp
  src/diagnostics/logger.cpp
  src/engine/stockfish_runtime.cpp
  src/engine/stockfish_engine.cpp
  src/http/http_client.cpp
  src/http/android_http_client.cpp
  src/http/platform_http_client.cpp
  src/persistence/database.cpp
  src/providers/chess_com_provider.cpp
  src/providers/lichess_provider.cpp
  src/providers/local_provider.cpp
  src/providers/provider_common.cpp
  src/providers/provider_models.cpp
  src/providers/provider_request_scheduler.cpp
  src/services/analysis_service.cpp
  src/services/game_library_service.cpp
  src/services/profile_service.cpp
  src/services/provider_service.cpp
  src/services/settings_service.cpp
  src/theory/opening_theory_provider.cpp
  src/theory/position_key.cpp
)
target_compile_features(kchess_core PUBLIC cxx_std_20)
target_compile_definitions(kchess_core PRIVATE KCHESS_CORE_BUILD)
target_include_directories(kchess_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann/include
)
target_link_libraries(kchess_core PRIVATE kchess_sqlite)
if(WIN32)
  target_link_libraries(kchess_core PRIVATE winhttp)
endif()
if(KCHESS_WITH_STOCKFISH)
  target_compile_definitions(kchess_core PRIVATE KCHESS_WITH_STOCKFISH=1)
  target_link_libraries(kchess_core PRIVATE stockfish_official)
endif()
set_target_properties(kchess_core PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  OUTPUT_NAME kchess_core
)

if(MSVC)
  target_compile_options(kchess_core PRIVATE /W4 /permissive- /EHsc)
  target_compile_options(kchess_sqlite PRIVATE /W3)
else()
  target_compile_options(kchess_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(KCHESS_WITH_STOCKFISH AND WIN32)
  add_custom_command(TARGET kchess_core POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${STOCKFISH_BIG_NET}" "$<TARGET_FILE_DIR:kchess_core>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${STOCKFISH_SMALL_NET}" "$<TARGET_FILE_DIR:kchess_core>"
    VERBATIM
  )
endif()

if(KCHESS_BUILD_TESTS AND NOT ANDROID)
  enable_testing()
  add_executable(kchess_core_tests
    tests/core_tests.cpp
    src/analysis/accuracy.cpp
    src/analysis/move_classifier.cpp
    src/chess/fen.cpp
    src/chess/move.cpp
    src/chess/pgn.cpp
    src/engine/stockfish_runtime.cpp
    src/engine/stockfish_engine.cpp
    src/persistence/database.cpp
    src/providers/provider_models.cpp
    src/theory/opening_theory_provider.cpp
    src/theory/position_key.cpp
  )
  target_compile_features(kchess_core_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(kchess_core_tests PRIVATE
    kchess_core
    kchess_sqlite
    stockfish_official
  )
  add_test(NAME kchess_core_tests COMMAND kchess_core_tests)

  add_executable(kchess_provider_tests
    tests/provider_tests.cpp
    src/http/http_client.cpp
    src/providers/chess_com_provider.cpp
    src/providers/lichess_provider.cpp
    src/providers/local_provider.cpp
    src/providers/provider_common.cpp
    src/providers/provider_models.cpp
    src/providers/provider_request_scheduler.cpp
  )
  target_compile_features(kchess_provider_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_provider_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann/include
  )
  add_test(NAME kchess_provider_tests COMMAND kchess_provider_tests)

  add_executable(kchess_provider_persistence_tests
    tests/provider_persistence_tests.cpp
    src/persistence/database.cpp
    src/providers/provider_models.cpp
  )
  target_compile_features(kchess_provider_persistence_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_provider_persistence_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(kchess_provider_persistence_tests PRIVATE kchess_sqlite)
  add_test(NAME kchess_provider_persistence_tests COMMAND kchess_provider_persistence_tests)

  add_executable(kchess_phase6_tests
    tests/phase6_tests.cpp
    src/engine/stockfish_runtime.cpp
    src/engine/stockfish_engine.cpp
    src/persistence/database.cpp
    src/providers/provider_models.cpp
  )
  target_compile_features(kchess_phase6_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_phase6_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_compile_definitions(kchess_phase6_tests PRIVATE
    KCHESS_STOCKFISH_ASSET_DIR="${STOCKFISH_SOURCE_DIR}"
  )
  target_link_libraries(kchess_phase6_tests PRIVATE
    kchess_sqlite
    stockfish_official
  )
  add_test(NAME kchess_phase6_tests COMMAND kchess_phase6_tests)

  add_executable(kchess_phase6_ffi_tests tests/phase6_ffi_tests.cpp)
  target_compile_features(kchess_phase6_ffi_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_phase6_ffi_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann/include
  )
  target_link_libraries(kchess_phase6_ffi_tests PRIVATE kchess_core)
  add_test(NAME kchess_phase6_ffi_tests COMMAND kchess_phase6_ffi_tests)

  add_executable(kchess_analysis_workflow_tests tests/analysis_workflow_tests.cpp)
  target_compile_features(kchess_analysis_workflow_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_analysis_workflow_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann/include
  )
  target_link_libraries(kchess_analysis_workflow_tests PRIVATE kchess_core)
  add_test(NAME kchess_analysis_workflow_tests COMMAND kchess_analysis_workflow_tests)

  add_executable(kchess_classifier_quality_tests
    tests/classifier_quality_tests.cpp
    src/analysis/move_classifier.cpp
    src/chess/fen.cpp
    src/chess/move.cpp
    src/engine/stockfish_runtime.cpp
  )
  target_compile_features(kchess_classifier_quality_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_classifier_quality_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(kchess_classifier_quality_tests PRIVATE stockfish_official)
  add_test(NAME kchess_classifier_quality_tests COMMAND kchess_classifier_quality_tests)
endif()
