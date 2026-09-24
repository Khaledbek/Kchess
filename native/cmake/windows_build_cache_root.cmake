# -----------------------------------------------------------------------------
# Section: Per-checkout Windows build-cache root
# -----------------------------------------------------------------------------
#
# Large CMake/MSBuild intermediates live outside the repository so a normal
# `flutter clean` can return the project to a compact transportable state
# without discarding reusable native compilation state. The source-path hash
# prevents two KChess checkouts from sharing one incompatible CMake cache.

function(kchess_resolve_windows_build_cache_root NATIVE_SOURCE_DIR OUTPUT_VAR)
  if(NOT DEFINED ENV{LOCALAPPDATA} OR "$ENV{LOCALAPPDATA}" STREQUAL "")
    message(FATAL_ERROR
      "KChess Windows build cache requires the LOCALAPPDATA environment variable"
    )
  endif()

  get_filename_component(_kchess_native_source_dir
    "${NATIVE_SOURCE_DIR}"
    ABSOLUTE
  )
  file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}" _kchess_local_app_data)
  file(TO_CMAKE_PATH "${_kchess_native_source_dir}" _kchess_native_source_key)
  string(TOLOWER "${_kchess_native_source_key}" _kchess_native_source_key)
  string(SHA256 _kchess_native_source_hash "${_kchess_native_source_key}")
  string(SUBSTRING "${_kchess_native_source_hash}" 0 16 _kchess_checkout_id)

  set(${OUTPUT_VAR}
    "${_kchess_local_app_data}/KChess/build-cache/${_kchess_checkout_id}"
    PARENT_SCOPE
  )
endfunction()
