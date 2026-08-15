cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(riff)

find_package(PkgConfig REQUIRED)

pkg_check_modules(mpv REQUIRED IMPORTED_TARGET mpv)
pkg_check_modules(curl REQUIRED IMPORTED_TARGET libcurl)

add_subdirectory(third_party/FTXUI)
add_executable(riff main.cpp)

target_include_directories(riff PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/ ${CMAKE_CURRENT_SOURCE_DIR}/third_party/FTXUI/include/)

target_link_libraries(riff PRIVATE
    PkgConfig::mpv
    PkgConfig::curl
    ftxui
)
