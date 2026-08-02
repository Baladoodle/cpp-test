cmake_minimum_required(VERSION 3.16)
project(InfiniteVoxelEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(EPOXY REQUIRED epoxy)
find_package(Threads REQUIRED)

include_directories(src ${GLFW3_INCLUDE_DIRS} ${EPOXY_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})

set(SOURCES
    src/Main.cpp
    src/SimplexNoise.cpp
)

add_executable(InfiniteVoxelEngine ${SOURCES})

target_link_libraries(InfiniteVoxelEngine PRIVATE
    ${GLFW3_LIBRARIES}
    ${EPOXY_LIBRARIES}
    OpenGL::GL
    Threads::Threads
)

target_compile_options(InfiniteVoxelEngine PRIVATE -O3 -march=native -ffast-math)
