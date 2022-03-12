cmake_minimum_required(VERSION 3.0)
project(finale)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs." FORCE)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options("-Wno-nullability-completeness")
elseif(MSVC)
endif()

add_subdirectory(deps/assimp-5.2.2)
add_subdirectory(deps/glfw-3.3.6)
add_subdirectory(deps/glm)
add_subdirectory(src)
