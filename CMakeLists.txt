cmake_minimum_required(VERSION 3.6)

project(switcher)

set(CMAKE_C_STANDARD 99)

find_package(PkgConfig)
find_library(X11_LIBRARIES X11)
find_library(XEXT_LIBRARIES Xext)
find_library(XRANDR_LIBRARIES Xrandr)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGOCAIRO REQUIRED pangocairo)

add_executable(switcher switcher.c window.c)
target_include_directories(switcher PUBLIC ${CAIRO_INCLUDE_DIRS} ${PANGOCAIRO_INCLUDE_DIRS})
target_link_libraries(switcher ${X11_LIBRARIES} ${XEXT_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGOCAIRO_LIBRARIES} ${XRANDR_LIBRARIES})

