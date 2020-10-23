cmake_minimum_required(VERSION 3.9)
project(openxr)

set(CMAKE_SYSTEM_PROCESSOR "i686")
set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/Renderer/CMake)
include(funcs)

add_subdirectory(External/KTX-Software)
add_subdirectory(Renderer)

add_executable(openxr_example
        main.cpp
        computer.cpp
        particlefire.cpp
        )
target_link_libraries(openxr_example PUBLIC VulkanRenderer)

target_add_spirv_shader(openxr_example shaders/normalmap.vert)
target_add_spirv_shader(openxr_example shaders/normalmap.frag)
target_add_spirv_shader(openxr_example shaders/particle.vert)
target_add_spirv_shader(openxr_example shaders/particle.frag)
target_add_spirv_shader(openxr_example shaders/skinnedmodel.vert)
target_add_spirv_shader(openxr_example shaders/skinnedmodel.frag)


