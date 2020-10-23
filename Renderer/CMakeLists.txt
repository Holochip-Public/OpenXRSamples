if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/CMake)
include(funcs)

set(SOURCES
        VulkanUtil.cpp
        Vulkan/SwapChains.cpp
        Vulkan/GPUProgram.cpp
        Vulkan/CommandPool.cpp
        Vulkan/CommandBuffers.cpp
        Vulkan/GPUSemaphores.cpp
        Vulkan/Buffers.cpp
        Vulkan/Texture.cpp
        Vulkan/Device.cpp
        Vulkan/Initializers.cpp
        Vulkan/UIOverlay.cpp
        Vulkan/Debug.cpp
        Vulkan/CommonHelper.cpp
        Camera.cpp
        GLFWUtil.cpp
        OpenXRUtil.cpp
        imguiExtras.cpp
        OpenXR/quad.cpp
        OpenXR/Initializers.cpp
        OpenXR/CommonHelper.cpp
        Vulkan/GLTFModel.cpp
        )

set(IMGUI_DIR ${CMAKE_CURRENT_LIST_DIR}/../External/imgui)
find_package(Vulkan REQUIRED)
find_package(Imgui REQUIRED)
find_package(glfw3 REQUIRED )
find_package(OpenXR REQUIRED COMPONENTS loader)

add_library(VulkanRenderer STATIC ${SOURCES})

if(WIN32)
    find_package( OpenGL REQUIRED )
    target_link_libraries(VulkanRenderer PUBLIC OpenGL::GL OpenGL::GLU)
endif()

target_include_directories(VulkanRenderer INTERFACE ${CMAKE_CURRENT_LIST_DIR} ImGui::ImGui)
target_link_libraries(VulkanRenderer PUBLIC Vulkan::Vulkan OpenXR::Loader ImGui::ImGui ImGui::Sources glfw ktx)
target_include_directories(VulkanRenderer PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/../External/gli
        ${CMAKE_CURRENT_LIST_DIR}/../External/glm
        ${CMAKE_CURRENT_LIST_DIR}/../External/SPIRV-Headers/include
        ${CMAKE_CURRENT_LIST_DIR}/../External/tinygltf)

if(TARGET ImGui::Sources)
    target_link_libraries(VulkanRenderer PUBLIC ImGui::Sources)
endif()
if(TARGET ImGui::SourcesMiscCpp)
    target_link_libraries(VulkanRenderer PUBLIC ImGui::SourcesMiscCpp)
endif()

target_add_spirv_shader(VulkanRenderer ${CMAKE_CURRENT_LIST_DIR}/Vulkan/UIShaders/uioverlay.frag)
target_add_spirv_shader(VulkanRenderer ${CMAKE_CURRENT_LIST_DIR}/Vulkan/UIShaders/uioverlay.vert)
