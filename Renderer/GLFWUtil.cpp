//
// Created by Steven Winston on 4/5/20.
//

#include <stdexcept>
#include "GLFWUtil.h"
#include "VulkanUtil.h"


namespace Util {
    namespace Renderer {
        void GLFWUtil::window_init(const int _width, const int _height, VulkanUtil* appPtr) {
            if (!glfwInit())
                throw std::runtime_error("glfwInit failed.");
            printf("GLFW successfully init'ed\n");
            width = _width;
            height = _height;

            int count;
            std::vector<GLFWmonitor *> useMonitor;
//            GLFWmonitor **monitors = glfwGetMonitors(&count);
//            for (int i = 0; i < count; i++) {
//                GLFWmonitor *monitor = monitors[i];
//                useMonitor.push_back(monitor);
//            }
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            window = glfwCreateWindow(width, height, title.c_str(), (useMonitor.size() > 1) ? useMonitor[1] : nullptr,
                                      nullptr);
            if (!window)
                throw std::runtime_error("GLFW window or GL context creation failed.");
            glfwSetWindowUserPointer(window, appPtr);
            glfwSetFramebufferSizeCallback(window, [](GLFWwindow *window_, int, int) {
                auto app = reinterpret_cast<VulkanUtil *>(glfwGetWindowUserPointer(window_));
                app->windowResize();
            });
            glfwSetCursorPosCallback(window, [](GLFWwindow *window_, double x, double y) {
                auto app = reinterpret_cast<VulkanUtil *>(glfwGetWindowUserPointer(window_));
                app->handleMouseMove(x, y);
            });
            glfwSetMouseButtonCallback(window, [](GLFWwindow *window_, int button, int action, int mods) {
                auto app = reinterpret_cast<VulkanUtil *>(glfwGetWindowUserPointer(window_));
                app->mouseButtons.left = button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS;
                app->mouseButtons.right = button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS;
                app->mouseButtons.middle = button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS;
            });
            glfwSetKeyCallback(window, [](GLFWwindow *window_, int keycode, int scanCode, int action, int mods) {
                auto app = reinterpret_cast<VulkanUtil *>(glfwGetWindowUserPointer(window_));
                app->keyPressed(keycode);
            });
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            width = w;
            height = h;
        }

        VkSurfaceKHR GLFWUtil::setSurface(VkInstance instance) {
            if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
                throw std::runtime_error("failed to create window surface!");
            }
            return surface;
        }

        void GLFWUtil::GiveTime() {
            glfwPollEvents();
        }

        bool GLFWUtil::isStillRunning() {
            if(!window)
                return false;
            return glfwWindowShouldClose(window);
        }

        std::vector<const char *> GLFWUtil::getRequiredExtensions() {
            uint32_t glfwExtensionsCount = 0;
            const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionsCount);
            std::vector<const char *> returnMe;
            for(int i=0; i < glfwExtensionsCount; i++)
                returnMe.push_back(glfwExtensions[i]);
            return returnMe;
        }

        GLFWUtil::~GLFWUtil() {
            glfwDestroyWindow(window);
            glfwTerminate();
        }
    }
}
