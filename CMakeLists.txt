cmake_minimum_required(VERSION 3.23)
project(dspopusplayer LANGUAGES C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(opusfile REQUIRED IMPORTED_TARGET opusfile)
pkg_check_modules(opusurl REQUIRED IMPORTED_TARGET opusurl)

add_library(${PROJECT_NAME} SHARED "src/main.c")
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::opusfile PkgConfig::opusurl)