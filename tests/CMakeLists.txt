cmake_minimum_required(VERSION 3.24)
project(spectra5_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(APP_DIR ${CMAKE_SOURCE_DIR}/../app)

enable_testing()

# Fuentes de dominio/UI puras (sin LVGL) bajo prueba.
set(PURE_SRCS
    ${APP_DIR}/ui/navigation/navigation_model.cpp
    ${APP_DIR}/services/capabilities/capability.cpp
    ${APP_DIR}/ui/design_system/theme.cpp
)

set(JSON_DIR ${CMAKE_SOURCE_DIR}/../dependencies/json)

function(spectra5_add_test name)
    add_executable(${name} unit/${name}.cpp ${ARGN})
    target_include_directories(${name} PRIVATE ${APP_DIR} ${JSON_DIR})
    find_package(Threads REQUIRED)
    target_link_libraries(${name} PRIVATE Threads::Threads)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

spectra5_add_test(smoke_test)
spectra5_add_test(navigation_test ${APP_DIR}/ui/navigation/navigation_model.cpp ${APP_DIR}/services/capabilities/capability.cpp)
spectra5_add_test(theme_test ${APP_DIR}/ui/design_system/theme.cpp)
spectra5_add_test(capability_test ${APP_DIR}/services/capabilities/capability.cpp)
spectra5_add_test(event_bus_test ${APP_DIR}/core/event_bus/event_bus.cpp)
spectra5_add_test(command_bus_test)
spectra5_add_test(rpc_codec_test ${APP_DIR}/domain/radio/rpc_codec.cpp)
spectra5_add_test(offensive_protocol_test
    ${APP_DIR}/domain/radio/rpc_codec.cpp
    ${APP_DIR}/domain/radio/offensive_codec.cpp)
spectra5_add_test(radio_engine_test
    ${APP_DIR}/domain/radio/rpc_codec.cpp
    ${APP_DIR}/domain/radio/offensive_codec.cpp
    ${APP_DIR}/services/radio/radio_engine.cpp
    ${APP_DIR}/services/radio/mock_radio_engine.cpp)
spectra5_add_test(frames_test ${APP_DIR}/domain/radio/frames.cpp)
spectra5_add_test(task_manager_test ${APP_DIR}/core/scheduler/task_manager.cpp)
spectra5_add_test(session_test
    ${APP_DIR}/services/storage/filesystem_session_store.cpp
    ${APP_DIR}/application/sessions/session_service.cpp)
spectra5_add_test(workflow_test
    ${APP_DIR}/application/workflows/workflow_engine.cpp
    ${APP_DIR}/application/sessions/session_service.cpp
    ${APP_DIR}/services/storage/filesystem_session_store.cpp
    ${APP_DIR}/core/scheduler/task_manager.cpp)
