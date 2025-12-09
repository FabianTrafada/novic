cmake_minimum_required(VERSION 3.10)
project(novic VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(GTK_LAYER_SHELL REQUIRED gtk-layer-shell-0)
pkg_check_modules(GIOMM REQUIRED giomm-2.4)
pkg_check_modules(PULSE REQUIRED libpulse)

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${GTKMM_INCLUDE_DIRS}
    ${GTK_LAYER_SHELL_INCLUDE_DIRS}
    ${GIOMM_INCLUDE_DIRS}
    ${PULSE_INCLUDE_DIRS}
)

link_directories(
    ${GTKMM_LIBRARY_DIRS}
    ${GTK_LAYER_SHELL_LIBRARY_DIRS}
    ${GIOMM_LIBRARY_DIRS}
    ${PULSE_LIBRARY_DIRS}
)

set(SOURCES
    src/main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    ${GTKMM_LIBRARIES}
    ${GTK_LAYER_SHELL_LIBRARIES}
    ${GIOMM_LIBRARIES}
    ${PULSE_LIBRARIES}
)

target_compile_options(${PROJECT_NAME} PRIVATE
    ${GTKMM_CFLAGS_OTHER}
    -Wall -Wextra
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)