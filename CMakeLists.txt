cmake_minimum_required(VERSION 3.16)
project(InfiniteVoxelEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(EPOXY REQUIRED epoxy)
find_package(Threads REQUIRED)

include_directories(src ${GLFW3_INCLUDE_DIRS} ${EPOXY_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})

set(SOURCES
    src/Main.cpp
    src/SimplexNoise.cpp
    src/WorldGenerator.cpp
    src/LightingSystem.cpp
    src/Mesher.cpp
    src/ChunkStore.cpp
    src/StreamPlanner.cpp
    src/Renderer.cpp
)

add_executable(InfiniteVoxelEngine ${SOURCES})

target_link_libraries(InfiniteVoxelEngine PRIVATE
    ${GLFW3_LIBRARIES}
    ${EPOXY_LIBRARIES}
    OpenGL::GL
    Threads::Threads
)

target_compile_options(InfiniteVoxelEngine PRIVATE
    -Wall -Wextra
    $<$<CONFIG:Release>:-O3 -march=native -ffast-math>
    $<$<CONFIG:Debug>:-g -O0 -fsanitize=address,undefined>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g -march=native>
)
enable_testing()
add_executable(unit_tests tests/unit_tests.cpp src/SimplexNoise.cpp src/WorldGenerator.cpp src/LightingSystem.cpp src/Mesher.cpp src/ChunkStore.cpp src/StreamPlanner.cpp src/Renderer.cpp)
target_include_directories(unit_tests PRIVATE src ${GLFW3_INCLUDE_DIRS} ${EPOXY_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})
target_link_libraries(unit_tests PRIVATE ${GLFW3_LIBRARIES} ${EPOXY_LIBRARIES} OpenGL::GL Threads::Threads)
target_compile_options(unit_tests PRIVATE -Wall -Wextra)
add_test(NAME unit_tests COMMAND unit_tests)
