cmake_minimum_required(VERSION 3.18)

project(kchess_core VERSION 0.1.0 LANGUAGES C CXX)

option(KCHESS_BUILD_TESTS "Build native unit tests" ON)
option(KCHESS_WITH_STOCKFISH "Build the official Stockfish engine" ON)

# -----------------------------------------------------------------------------
# Section: Stockfish source and runtime assets
# -----------------------------------------------------------------------------

set(STOCKFISH_SOURCE_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/stockfish/source/src"
)
set(STOCKFISH_BIG_NET
  "${STOCKFISH_SOURCE_DIR}/nn-c288c895ea92.nnue"
)
set(STOCKFISH_SMALL_NET
  "${STOCKFISH_SOURCE_DIR}/nn-37f18f62d772.nnue"
)

if(KCHESS_WITH_STOCKFISH)
  include(cmake/fetch_stockfish19.cmake)
  set(STOCKFISH19_SOURCE_ROOT "${KCHESS_STOCKFISH19_SOURCE_ROOT}")
  set(STOCKFISH19_SOURCE_DIR "${KCHESS_STOCKFISH19_SOURCE_DIR}")
  set(STOCKFISH19_NET "${KCHESS_STOCKFISH19_NET}")
endif()

# -----------------------------------------------------------------------------
# Section: Stockfish library build strategy
# -----------------------------------------------------------------------------

set(KCHESS_USE_PERSISTENT_WINDOWS_STOCKFISH_CACHE OFF)
if(KCHESS_WITH_STOCKFISH AND WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  # Windows Flutter clean removes flutter_app/build, but it does not touch the
  # source-adjacent persistent cache. The official engines are therefore built
  # once per configuration and then linked as stable prebuilt .lib files.
  set(KCHESS_USE_PERSISTENT_WINDOWS_STOCKFISH_CACHE ON)
  include(cmake/stockfish_windows_cache.cmake)
elseif(KCHESS_WITH_STOCKFISH)
  # Android and non-MSVC native builds keep the existing in-tree source build.
  # This preserves the current Android ABI/toolchain behavior unchanged.
  file(GLOB_RECURSE STOCKFISH_SOURCES CONFIGURE_DEPENDS
    "${STOCKFISH_SOURCE_DIR}/*.cpp"
  )
  list(REMOVE_ITEM STOCKFISH_SOURCES "${STOCKFISH_SOURCE_DIR}/main.cpp")

  # Keep Stockfish in a small subdirectory so its Debug flags can be optimized
  # independently from the rest of KChess.
  add_subdirectory(cmake/stockfish_target)
  target_compile_features(stockfish_official PUBLIC cxx_std_20)
  target_include_directories(stockfish_official PUBLIC ${STOCKFISH_SOURCE_DIR})
  set_target_properties(stockfish_official PROPERTIES POSITION_INDEPENDENT_CODE ON)

  if(ANDROID AND CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    target_compile_definitions(stockfish_official PRIVATE
      USE_NEON=8 USE_POPCNT IS_64BIT
    )
  endif()

  if(MSVC)
    target_compile_options(stockfish_official PRIVATE /W3 /EHsc /bigobj /FS)
  else()
    target_compile_options(stockfish_official PRIVATE
      -O3
      -fno-exceptions
      -fno-rtti
      "-Wa,-I${STOCKFISH_SOURCE_DIR}"
    )
  endif()

  # Keep the Stockfish 19 source list aligned with the official sf_19 Makefile.
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

  add_subdirectory(cmake/stockfish19_target)
  target_compile_features(stockfish19_official PUBLIC cxx_std_20)
  target_include_directories(stockfish19_official PRIVATE ${STOCKFISH19_SOURCE_DIR})
  set_target_properties(stockfish19_official PROPERTIES POSITION_INDEPENDENT_CODE ON)

  if(ANDROID AND CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    target_compile_definitions(stockfish19_official PRIVATE
      USE_NEON=8 USE_POPCNT IS_64BIT
    )
  endif()

  if(MSVC)
    target_compile_options(stockfish19_official PRIVATE /W3 /EHsc /bigobj /FS)
  else()
    target_compile_options(stockfish19_official PRIVATE
      -O3
      -fno-exceptions
      -fno-rtti
      "-Wa,-I${STOCKFISH19_SOURCE_DIR}"
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
  src/analysis/move_classifier_sf19.cpp
  src/chess/fen.cpp
  src/chess/move.cpp
  src/chess/pgn.cpp
  src/chess/position_view.cpp
  src/core/core.cpp
  src/diagnostics/logger.cpp
  src/engine/bot_move_selector.cpp
  src/engine/stockfish_runtime.cpp
  src/engine/stockfish_engine.cpp
  src/engine/stockfish19_engine.cpp
  src/engine/stockfish19_result_coherence.cpp
  src/engine/stockfish_factory.cpp
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
  src/services/bot_service.cpp
  src/services/game_library_service.cpp
  src/services/profile_service.cpp
  src/services/provider_service.cpp
  src/services/settings_service.cpp
  src/services/statistics_service.cpp
  src/services/statistics_details.cpp
  src/services/opening_weakness.cpp
  src/training/training_catalog.cpp
  src/training/training_service.cpp
  src/training/practice_catalog.cpp
  src/training/practice_position.cpp
  src/training/practice_service.cpp
  src/training/practice_session.cpp
  src/theory/opening_name_index.cpp
  src/theory/opening_theory_provider.cpp
  src/theory/polyglot.cpp
  src/theory/polyglot_arrays.cpp
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
if(KCHESS_WITH_STOCKFISH)
  # stockfish19_engine.cpp includes headers as src/<name>.h through this root so
  # it can never accidentally resolve the same-named Stockfish 18 headers.
  target_include_directories(kchess_core PRIVATE ${STOCKFISH19_SOURCE_ROOT})
endif()
target_link_libraries(kchess_core PRIVATE kchess_sqlite)
if(WIN32)
  target_link_libraries(kchess_core PRIVATE winhttp)
endif()
if(KCHESS_WITH_STOCKFISH)
  target_compile_definitions(kchess_core PRIVATE KCHESS_WITH_STOCKFISH=1)
  target_link_libraries(kchess_core PRIVATE stockfish_official stockfish19_official)
  if(KCHESS_USE_PERSISTENT_WINDOWS_STOCKFISH_CACHE)
    # Ensure the one-time cache population completes before the DLL link step.
    add_dependencies(kchess_core kchess_stockfish_windows_cache)
  endif()
endif()
set_target_properties(kchess_core PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  OUTPUT_NAME kchess_core
)

if(KCHESS_WITH_STOCKFISH AND WIN32)
  # MSVC cannot use Stockfish's incbin-based NNUE embedding. Always place the
  # verified SF19 network next to kchess_core.dll as part of a direct native
  # build as well as Flutter's later install/package step. This is additive:
  # the existing Stockfish 18 network handling is left unchanged.
  add_custom_command(TARGET kchess_core POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${STOCKFISH19_NET}"
      "$<TARGET_FILE_DIR:kchess_core>/nn-1a298aa575a0.nnue"
    VERBATIM
  )
endif()

if(MSVC)
  # Embedded opening names are UTF-8 on Windows as well as Android.
  set_source_files_properties(src/training/practice_catalog.cpp
    PROPERTIES COMPILE_OPTIONS "/utf-8")
  target_compile_options(kchess_core PRIVATE /W4 /permissive- /EHsc)
  target_compile_options(kchess_sqlite PRIVATE /W3)

  if(KCHESS_USE_PERSISTENT_WINDOWS_STOCKFISH_CACHE)
    # KChess still includes the official engine headers while linking the
    # persistent prebuilt libraries. Suppress only the known third-party
    # alignment diagnostic and missing-library-PDB linker noise; keep all
    # other KChess compiler/linker diagnostics visible.
    target_compile_options(kchess_core PRIVATE /wd4324)
    target_link_options(kchess_core PRIVATE /IGNORE:4099)
  endif()
else()
  target_compile_options(kchess_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(KCHESS_WITH_STOCKFISH AND WIN32)
  # Development builds can load the external NNUE networks directly from the
  # checked-in Stockfish source tree. Runtime still prefers networks placed
  # next to kchess_core.dll, so packaged builds can remain self-contained.
  target_compile_definitions(kchess_core PRIVATE
    KCHESS_STOCKFISH_ASSET_DIR="${STOCKFISH_SOURCE_DIR}"
    KCHESS_STOCKFISH19_ASSET_DIR="${STOCKFISH19_SOURCE_DIR}"
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
    src/chess/position_view.cpp
    src/engine/stockfish_runtime.cpp
    src/engine/stockfish_engine.cpp
    src/persistence/database.cpp
    src/providers/provider_models.cpp
    src/theory/opening_name_index.cpp
    src/theory/opening_theory_provider.cpp
    src/theory/position_key.cpp
  )
  target_compile_features(kchess_core_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann/include
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

  # Header-only classifier, so this needs no sources beyond the test itself.
  add_executable(kchess_termination_tests tests/termination_tests.cpp)
  target_compile_features(kchess_termination_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_termination_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  add_test(NAME kchess_termination_tests COMMAND kchess_termination_tests)

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

  add_executable(kchess_classifier_sf19_tests
    tests/classifier_sf19_tests.cpp
    src/analysis/accuracy.cpp
    src/analysis/move_classifier_sf19.cpp
  )
  target_compile_features(kchess_classifier_sf19_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_classifier_sf19_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  add_test(NAME kchess_classifier_sf19_tests COMMAND kchess_classifier_sf19_tests)

  add_executable(kchess_bot_workflow_tests tests/bot_workflow_tests.cpp)
  target_compile_features(kchess_bot_workflow_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_bot_workflow_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann/include
  )
  target_link_libraries(kchess_bot_workflow_tests PRIVATE kchess_core)
  add_test(NAME kchess_bot_workflow_tests COMMAND kchess_bot_workflow_tests)

  add_executable(kchess_training_tests tests/training_board_tests.cpp)
  target_compile_features(kchess_training_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_training_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_link_libraries(kchess_training_tests PRIVATE kchess_core)
  add_test(NAME kchess_training_tests COMMAND kchess_training_tests)

  # Drives the opening drill end to end; skips itself when the untracked
  # PolyGlot book is not on disk.
  add_executable(kchess_opening_drill_tests tests/opening_drill_tests.cpp)
  target_compile_features(kchess_opening_drill_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_opening_drill_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann/include
  )
  target_link_libraries(kchess_opening_drill_tests PRIVATE kchess_core)
  add_test(NAME kchess_opening_drill_tests COMMAND kchess_opening_drill_tests)

  add_executable(kchess_polyglot_tests
    tests/polyglot_tests.cpp
    src/theory/polyglot.cpp
    src/theory/polyglot_arrays.cpp
  )
  target_compile_features(kchess_polyglot_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_polyglot_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  add_test(NAME kchess_polyglot_tests COMMAND kchess_polyglot_tests)

  add_executable(kchess_opening_weakness_tests
    tests/opening_weakness_tests.cpp
    src/services/opening_weakness.cpp
  )
  target_compile_features(kchess_opening_weakness_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_opening_weakness_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/nlohmann/include
  )
  add_test(NAME kchess_opening_weakness_tests COMMAND kchess_opening_weakness_tests)

  add_executable(kchess_bot_move_selector_tests
    tests/bot_move_selector_tests.cpp
    src/engine/bot_move_selector.cpp
  )
  target_compile_features(kchess_bot_move_selector_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_bot_move_selector_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  add_test(NAME kchess_bot_move_selector_tests COMMAND kchess_bot_move_selector_tests)

  add_executable(kchess_sf19_result_coherence_tests
    tests/sf19_result_coherence_tests.cpp
    src/engine/stockfish19_result_coherence.cpp
  )
  target_compile_features(kchess_sf19_result_coherence_tests PRIVATE cxx_std_20)
  target_include_directories(kchess_sf19_result_coherence_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  add_test(NAME kchess_sf19_result_coherence_tests COMMAND kchess_sf19_result_coherence_tests)
endif()
