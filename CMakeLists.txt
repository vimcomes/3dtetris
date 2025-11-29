cmake_minimum_required(VERSION 3.24)
project(3dtetris)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Ensure compatibility floor high enough for bundled dependencies.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

include(FetchContent)

# GLFW build tweaks to avoid extra artifacts we do not need.
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
    GIT_SHALLOW TRUE
)

# GLAD configuration: core profile OpenGL 3.3 for modern pipeline.
set(GLAD_PROFILE "core" CACHE STRING "" FORCE)
set(GLAD_API "gl=3.3" CACHE STRING "" FORCE)
set(GLAD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw glad)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp src/*.h)
add_executable(3dtetris ${SRC_FILES})
target_compile_definitions(3dtetris PRIVATE GLFW_INCLUDE_NONE)
target_include_directories(3dtetris PRIVATE src)
target_link_libraries(3dtetris PRIVATE glfw glad ${CMAKE_DL_LIBS})
