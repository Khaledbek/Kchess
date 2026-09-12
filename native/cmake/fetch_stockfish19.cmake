# Fetch the exact immutable Stockfish 19 release and its official NNUE network.
# The existing Stockfish 18 tree is deliberately left untouched.
set(KCHESS_STOCKFISH19_SOURCE_ROOT
  "${CMAKE_CURRENT_LIST_DIR}/../../third_party/stockfish19/source"
)
set(KCHESS_STOCKFISH19_SOURCE_DIR
  "${KCHESS_STOCKFISH19_SOURCE_ROOT}/src"
)
set(KCHESS_STOCKFISH19_NET
  "${KCHESS_STOCKFISH19_SOURCE_DIR}/nn-1a298aa575a0.nnue"
)
set(KCHESS_STOCKFISH19_ARCHIVE_SHA256
  "519b653d0d1ffb96531d982ccbe5c6a19425e8388e0e3c2f70f34b424ab32d76"
)
set(KCHESS_STOCKFISH19_NET_SHA256
  "1a298aa575a085434d29027978dc36867fe9c5bcea9376654b7a8eba1e52dfc2"
)

if(NOT EXISTS "${KCHESS_STOCKFISH19_SOURCE_DIR}/engine.cpp")
  set(_sf19_download_dir "${CMAKE_BINARY_DIR}/kchess_stockfish19_download")
  set(_sf19_extract_dir "${CMAKE_BINARY_DIR}/kchess_stockfish19_extract")
  set(_sf19_archive "${_sf19_download_dir}/stockfish-sf_19.tar.gz")
  file(MAKE_DIRECTORY "${_sf19_download_dir}")
  file(REMOVE_RECURSE "${_sf19_extract_dir}")
  file(MAKE_DIRECTORY "${_sf19_extract_dir}")

  message(STATUS "KChess: downloading official Stockfish 19 source (sf_19)")
  file(DOWNLOAD
    "https://github.com/official-stockfish/Stockfish/archive/refs/tags/sf_19.tar.gz"
    "${_sf19_archive}"
    EXPECTED_HASH "SHA256=${KCHESS_STOCKFISH19_ARCHIVE_SHA256}"
    TLS_VERIFY ON
    SHOW_PROGRESS
    STATUS _sf19_download_status
  )
  list(GET _sf19_download_status 0 _sf19_download_code)
  list(GET _sf19_download_status 1 _sf19_download_message)
  if(NOT _sf19_download_code EQUAL 0)
    message(FATAL_ERROR "Stockfish 19 source download failed: ${_sf19_download_message}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xzf "${_sf19_archive}"
    WORKING_DIRECTORY "${_sf19_extract_dir}"
    RESULT_VARIABLE _sf19_extract_result
  )
  if(NOT _sf19_extract_result EQUAL 0
     OR NOT EXISTS "${_sf19_extract_dir}/Stockfish-sf_19/src/engine.cpp")
    message(FATAL_ERROR "Stockfish 19 source archive could not be extracted")
  endif()

  file(REMOVE_RECURSE "${KCHESS_STOCKFISH19_SOURCE_ROOT}")
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../../third_party/stockfish19")
  file(RENAME
    "${_sf19_extract_dir}/Stockfish-sf_19"
    "${KCHESS_STOCKFISH19_SOURCE_ROOT}"
  )
endif()

if(EXISTS "${KCHESS_STOCKFISH19_NET}")
  # The same file is copied externally on Windows and embedded into the native
  # library on Android. Reject a stale/corrupt file before either package can
  # be produced.
  file(SHA256 "${KCHESS_STOCKFISH19_NET}" _sf19_existing_net_sha256)
  if(NOT _sf19_existing_net_sha256 STREQUAL KCHESS_STOCKFISH19_NET_SHA256)
    message(WARNING
      "KChess: existing Stockfish 19 NNUE has the wrong SHA-256; downloading the official network again")
    file(REMOVE "${KCHESS_STOCKFISH19_NET}")
  endif()
endif()

if(NOT EXISTS "${KCHESS_STOCKFISH19_NET}")
  message(STATUS "KChess: downloading official Stockfish 19 NNUE network")
  file(DOWNLOAD
    "https://tests.stockfishchess.org/api/nn/nn-1a298aa575a0.nnue"
    "${KCHESS_STOCKFISH19_NET}"
    EXPECTED_HASH "SHA256=${KCHESS_STOCKFISH19_NET_SHA256}"
    TLS_VERIFY ON
    SHOW_PROGRESS
    STATUS _sf19_net_status
  )
  list(GET _sf19_net_status 0 _sf19_net_code)
  list(GET _sf19_net_status 1 _sf19_net_message)
  if(NOT _sf19_net_code EQUAL 0)
    file(REMOVE "${KCHESS_STOCKFISH19_NET}")
    message(FATAL_ERROR "Stockfish 19 NNUE download failed: ${_sf19_net_message}")
  endif()
endif()

if(EXISTS "${KCHESS_STOCKFISH19_NET}")
  file(SHA256 "${KCHESS_STOCKFISH19_NET}" _sf19_final_net_sha256)
  if(NOT _sf19_final_net_sha256 STREQUAL KCHESS_STOCKFISH19_NET_SHA256)
    message(FATAL_ERROR
      "Stockfish 19 NNUE integrity check failed after download")
  endif()
endif()
