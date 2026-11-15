# Separate v23 transition-observation build; never edits source translations.
option(AMPED_PAUSE_PROBE_V23 "Build read-only v23 pause transition probes" OFF)
if(AMPED_PAUSE_PROBE_V23)
  if(NOT AMPED_BOARDER_PROBE_V23 OR NOT AMPED_GENERATED_DIR MATCHES "seeded-v23/generated$")
    message(FATAL_ERROR "Pause probe requires AMPED_BOARDER_PROBE_V23 and seeded-v23/generated")
  endif()
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(_pause_out "${CMAKE_CURRENT_BINARY_DIR}/pause-v23-derived")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare.py"
    --source-4 "${CMAKE_CURRENT_SOURCE_DIR}/probes/recomp_0004_boarder_v23.c"
    --source-5 "${AMPED_GENERATED_DIR}/recomp_0005.c" --output-dir "${_pause_out}"
    RESULT_VARIABLE _pause_status)
  if(NOT _pause_status EQUAL 0)
    message(FATAL_ERROR "Pause probe anchor generation failed")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/probes/recomp_0004_boarder_v23.c"
    "${AMPED_GENERATED_DIR}/recomp_0005.c")
  get_target_property(_pause_sources amped_recomp SOURCES)
  list(FILTER _pause_sources EXCLUDE REGEX "/recomp_0004_boarder_v23\\.c$")
  list(FILTER _pause_sources EXCLUDE REGEX "/recomp_0005\\.c$")
  set_property(TARGET amped_recomp PROPERTY SOURCES "${_pause_sources}")
  target_sources(amped_recomp PRIVATE "${_pause_out}/pause_recomp_0004.c"
    "${_pause_out}/pause_recomp_0005.c" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/pause_trace.c")
endif()


option(AMPED_MENU_CONSUMER_PROBE_V23 "Build read-only v23 menu transition consumer probes" OFF)
if(AMPED_MENU_CONSUMER_PROBE_V23)
  if(NOT AMPED_PAUSE_PROBE_V23)
    message(FATAL_ERROR "Menu consumer probe requires AMPED_PAUSE_PROBE_V23")
  endif()
  set(_consumer_out "${CMAKE_CURRENT_BINARY_DIR}/pause-v23-derived/consumer_recomp_0006.c")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_consumer.py"
    --source "${AMPED_GENERATED_DIR}/recomp_0006.c" --output "${_consumer_out}"
    RESULT_VARIABLE _consumer_status)
  if(NOT _consumer_status EQUAL 0)
    message(FATAL_ERROR "Menu consumer probe anchor generation failed")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_consumer.py"
    "${AMPED_GENERATED_DIR}/recomp_0006.c")
  get_target_property(_consumer_sources amped_recomp SOURCES)
  list(FILTER _consumer_sources EXCLUDE REGEX "/recomp_0006\\.c$")
  set_property(TARGET amped_recomp PROPERTY SOURCES "${_consumer_sources}")
  target_sources(amped_recomp PRIVATE "${_consumer_out}")
endif()

option(AMPED_MENU_UPDATE_PROBE_V23 "Build armed UpdateActiveMenu observation" OFF)
if(AMPED_MENU_UPDATE_PROBE_V23)
  if(NOT AMPED_MENU_CONSUMER_PROBE_V23)
    message(FATAL_ERROR "Menu update probe requires AMPED_MENU_CONSUMER_PROBE_V23")
  endif()
  set(_update_out "${CMAKE_CURRENT_BINARY_DIR}/pause-v23-derived/update_recomp_0031.c")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_update.py"
    --source "${AMPED_GENERATED_DIR}/recomp_0031.c" --output "${_update_out}"
    RESULT_VARIABLE _update_status)
  if(NOT _update_status EQUAL 0)
    message(FATAL_ERROR "Menu update probe anchor generation failed")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_update.py"
    "${AMPED_GENERATED_DIR}/recomp_0031.c")
  get_target_property(_update_sources amped_recomp SOURCES)
  list(FILTER _update_sources EXCLUDE REGEX "/recomp_0031\\.c$")
  set_property(TARGET amped_recomp PROPERTY SOURCES "${_update_sources}")
  target_sources(amped_recomp PRIVATE "${_update_out}")
endif()

option(AMPED_VM_ENTRY_PROBE_V23 "Build armed read-only Lua VM entry observation" OFF)
if(AMPED_VM_ENTRY_PROBE_V23)
  if(NOT AMPED_MENU_UPDATE_PROBE_V23)
    message(FATAL_ERROR "VM entry probe requires AMPED_MENU_UPDATE_PROBE_V23")
  endif()
  set(_vm_out "${CMAKE_CURRENT_BINARY_DIR}/pause-v23-derived/vm_recomp_0042.c")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_vm_entry.py"
    --source "${AMPED_GENERATED_DIR}/recomp_0042.c" --output "${_vm_out}"
    RESULT_VARIABLE _vm_status)
  if(NOT _vm_status EQUAL 0)
    message(FATAL_ERROR "VM entry probe anchor generation failed")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_vm_entry.py"
    "${AMPED_GENERATED_DIR}/recomp_0042.c")
  get_target_property(_vm_sources amped_recomp SOURCES)
  list(FILTER _vm_sources EXCLUDE REGEX "/recomp_0042\\.c$")
  set_property(TARGET amped_recomp PROPERTY SOURCES "${_vm_sources}")
  target_sources(amped_recomp PRIVATE "${_vm_out}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/vm_entry_trace.c")
endif()

option(AMPED_LUA_INPUT_PROBE_V23 "Observe existing Lua input closure results" OFF)
if(AMPED_LUA_INPUT_PROBE_V23)
  if(NOT AMPED_MENU_UPDATE_PROBE_V23)
    message(FATAL_ERROR "Lua input probe requires AMPED_MENU_UPDATE_PROBE_V23")
  endif()
  set(_lua_input_out "${CMAKE_CURRENT_BINARY_DIR}/pause-v23-derived/lua_input_recomp_0032.c")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_lua_input.py"
    --source "${AMPED_GENERATED_DIR}/recomp_0032.c" --output "${_lua_input_out}"
    RESULT_VARIABLE _lua_input_status)
  if(NOT _lua_input_status EQUAL 0)
    message(FATAL_ERROR "Lua input probe anchor generation failed")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_lua_input.py"
    "${AMPED_GENERATED_DIR}/recomp_0032.c")
  get_target_property(_lua_input_sources amped_recomp SOURCES)
  list(FILTER _lua_input_sources EXCLUDE REGEX "/recomp_0032\\.c$")
  set_property(TARGET amped_recomp PROPERTY SOURCES "${_lua_input_sources}")
  target_sources(amped_recomp PRIVATE "${_lua_input_out}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/lua_input_trace.c")
endif()

option(AMPED_CONTINUE_PROBE_V23 "Observe registered Continue Run closure" OFF)
if(AMPED_CONTINUE_PROBE_V23)
  if(NOT AMPED_MENU_CONSUMER_PROBE_V23)
    message(FATAL_ERROR "Continue probe requires AMPED_MENU_CONSUMER_PROBE_V23")
  endif()
  set(_continue_out "${CMAKE_CURRENT_BINARY_DIR}/pause-v23-derived/continue_recomp_0006.c")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_continue.py"
    --source "${_consumer_out}" --output "${_continue_out}" RESULT_VARIABLE _continue_status)
  if(NOT _continue_status EQUAL 0)
    message(FATAL_ERROR "Continue probe anchor generation failed")
  endif()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/prepare_continue.py")
  get_target_property(_continue_sources amped_recomp SOURCES)
  list(REMOVE_ITEM _continue_sources "${_consumer_out}")
  set_property(TARGET amped_recomp PROPERTY SOURCES "${_continue_sources}")
  target_sources(amped_recomp PRIVATE "${_continue_out}" "${CMAKE_CURRENT_SOURCE_DIR}/probes/pause_v23/continue_trace.c")
endif()
