# Compare the actual linked executor in separate processes so the once-read
# switch has its normal initialization semantics. Timing is informative only.
foreach(_mode 0 1)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env
    "AMPED_PB_CONST_FAST=${_mode}" "RECOMP_PB_BACKEND=null"
    "RECOMP_TRACE=" "RECOMP_DEBUG=" "RECOMP_LOG_DIAG=0"
    "${TEST_EXECUTABLE}"
    RESULT_VARIABLE _code OUTPUT_VARIABLE _output ERROR_VARIABLE _error TIMEOUT 25)
  if(NOT _code EQUAL 0)
    message(FATAL_ERROR "Constant mode ${_mode} failed (${_code}): ${_output}\n${_error}")
  endif()
  if(NOT _output MATCHES "STATE ([0-9a-f]+) words=20000 constants=([0-9]+)")
    message(FATAL_ERROR "Missing mixed-stream result: ${_output}")
  endif()
  set(_hash_${_mode} "${CMAKE_MATCH_1}")
  if(CMAKE_MATCH_2 LESS 1)
    message(FATAL_ERROR "No constants executed")
  endif()
  if(_mode AND NOT _error MATCHES "fast route executed")
    message(FATAL_ERROR "Optimization was not exercised")
  endif()
  message(STATUS "mode ${_mode}: ${_output}")
endforeach()
if(NOT _hash_0 STREQUAL _hash_1)
  message(FATAL_ERROR "Mixed-stream state differs between reference and shortcut")
endif()
