cmake_minimum_required(VERSION 3.10)
project(MC_APP_BOILERPLATE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(platforms/desktop/CMakeLists.txt)
