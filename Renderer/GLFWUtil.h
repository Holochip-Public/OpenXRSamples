//
// Created by Steven Winston on 4/5/20.
//

#ifndef LIGHTFIELDFORWARDRENDERER_GLFWUTIL_H
#define LIGHTFIELDFORWARDRENDERER_GLFWUTIL_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>
#include <vector>

namespace Util {
    namespace Renderer {
        class VulkanUtil;

        class GLFWUtil {
            friend class VulkanUtil;
            uint32_t width;
            uint32_t height;
            GLFWwindow *window;
            std::string title;
            VkSurfaceKHR surface;

        public:
            GLFWwindow * getWindow() const { return window; }
            void window_init(int _width, int _height, VulkanUtil* appPtr);
            VkSurfaceKHR setSurface(VkInstance instance);
            VkSurfaceKHR getSurface() const { return surface; }
            static void GiveTime();
            bool isStillRunning();
            static std::vector<const char*> getRequiredExtensions();
            uint32_t getWidth() {return width;}
            uint32_t getHeight() {return height;}
            ~GLFWUtil();
        };
    }
}


#endif //LIGHTFIELDFORWARDRENDERER_GLFWUTIL_H
