cmake_minimum_required(VERSION 3.23)
project(dspopusplayer LANGUAGES C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(opus REQUIRED IMPORTED_TARGET opus)
pkg_check_modules(opusfile REQUIRED IMPORTED_TARGET opusfile)

add_library(${PROJECT_NAME} SHARED "src/main.c")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::opus PkgConfig::opusfile)
