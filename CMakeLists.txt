cmake_minimum_required(VERSION 3.23)
project(dspopusplayer LANGUAGES C)

find_package(PkgConfig REQUIRED)

pkg_check_modules(opusfile REQUIRED IMPORTED_TARGET opusfile)
pkg_check_modules(opusurl REQUIRED IMPORTED_TARGET opusurl)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.c")
add_library(${PROJECT_NAME} SHARED ${SOURCES})
unset(SOURCES)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::opusfile PkgConfig::opusurl)