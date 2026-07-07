cmake_minimum_required (VERSION 3.10)
project(DesktopBuild)

cmake_policy(SET CMP0077 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DPLATFORM_BUILD_DESKTOP)

set(LV_CONF_INCLUDE_SIMPLE OFF)
set(LV_CONF_PATH ../../lv_conf.h)
add_subdirectory(dependencies/lvgl)

find_package(SDL2 REQUIRED SDL2)
include_directories(PUBLIC ${SDL2_INCLUDE_DIRS})
target_include_directories(lvgl PUBLIC ${SDL2_INCLUDE_DIRS})

file(GLOB_RECURSE APP_LAYER_SRCS
    app/*.c
    app/*.cc
    app/*.cpp
)
set(APP_LAYER_INCS
    app/
    dependencies/json/
)

file(GLOB_RECURSE APP_DESKTOP_BUILD_SRCS
    platforms/desktop/*.cpp
    platforms/desktop/*.cc
    platforms/desktop/*.c
)
add_executable(app_desktop_build ${APP_DESKTOP_BUILD_SRCS} ${APP_LAYER_SRCS})
target_include_directories(app_desktop_build PUBLIC ${APP_LAYER_INCS})
target_link_libraries(app_desktop_build PUBLIC
    lvgl
    lvgl_examples
    lvgl_demos
    ${SDL2_LIBRARIES}
    pthread
)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/desktop)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
