if(NOT DEFINED SOURCE_FILE OR SOURCE_FILE STREQUAL "")
  message(FATAL_ERROR "Kchess NNUE staging: SOURCE_FILE is missing")
endif()
if(NOT DEFINED DESTINATION_DIR OR DESTINATION_DIR STREQUAL "")
  message(FATAL_ERROR "Kchess NNUE staging: DESTINATION_DIR is missing")
endif()
if(NOT EXISTS "${SOURCE_FILE}")
  message(FATAL_ERROR "Kchess NNUE network not found: ${SOURCE_FILE}")
endif()

file(MAKE_DIRECTORY "${DESTINATION_DIR}")
get_filename_component(_network_name "${SOURCE_FILE}" NAME)
set(_destination "${DESTINATION_DIR}/${_network_name}")

# Network filenames contain the upstream content hash. If that exact filename is
# already staged, do not touch it; this also avoids Windows file-lock races.
if(EXISTS "${_destination}")
  message(STATUS "Kchess NNUE already staged: ${_network_name}")
  return()
endif()

set(_copy_ok FALSE)
foreach(_attempt RANGE 1 5)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy "${SOURCE_FILE}" "${_destination}"
    RESULT_VARIABLE _copy_result
    ERROR_VARIABLE _copy_error
  )
  if(_copy_result EQUAL 0 AND EXISTS "${_destination}")
    set(_copy_ok TRUE)
    break()
  endif()
  execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1)
endforeach()

if(NOT _copy_ok)
  message(FATAL_ERROR
    "Kchess could not stage NNUE network '${_network_name}' after 5 attempts. "
    "Source: ${SOURCE_FILE}; destination: ${_destination}; last error: ${_copy_error}")
endif()

message(STATUS "Kchess NNUE staged: ${_network_name}")
