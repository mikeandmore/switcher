cmake_minimum_required(VERSION 3.6)

project(switcher)

set(CMAKE_C_STANDARD 99)

set(CMAKE_C_FLAGS_RELEASE "-Ofast -g -flto -fno-omit-frame-pointer -DNDEBUG")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto -fwhole-program -g -fno-omit-frame-pointer")

find_package(PkgConfig)
find_library(X11_LIBRARIES X11)
find_library(XEXT_LIBRARIES Xext)
find_library(XRANDR_LIBRARIES Xrandr)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGO REQUIRED pangocairo)

link_directories(${CAIRO_LIBRARY_DIRS} ${PANGO_LIBRARY_DIRS})
add_executable(switcher switcher.c window.c)
target_include_directories(switcher PUBLIC ${CAIRO_INCLUDE_DIRS} ${PANGO_INCLUDE_DIRS})
target_link_libraries(switcher PUBLIC ${X11_LIBRARIES} ${XEXT_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGO_LIBRARIES} ${XRANDR_LIBRARIES})

