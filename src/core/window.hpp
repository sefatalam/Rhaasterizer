#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <stdexcept>
#include <utility>

namespace rh {

class Window {

public:
    Window(int width, int height, const char* title) {
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        _window = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!_window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create Window");
        }

        glfwSetWindowUserPointer(_window, this);

        glfwSetFramebufferSizeCallback(_window, [](GLFWwindow* w, int, int) {
            static_cast<Window*>(glfwGetWindowUserPointer(w))->_resized = true;
        });

        glfwSetScrollCallback(_window, [](GLFWwindow* w, double, double yOffset) {
            static_cast<Window*>(glfwGetWindowUserPointer(w))->_scrollDelta += static_cast<float>(yOffset);
        });

    }

    ~Window() {
        if (_window) {
            glfwDestroyWindow(_window);
        }
        glfwTerminate();
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    GLFWwindow* windowHandle() const { return _window; }
    bool shouldClose() const { return glfwWindowShouldClose(_window); }
    void pollEvents() const { 
        glfwPollEvents();
        if (glfwGetKey(_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(_window, GLFW_TRUE);
        }
    }

    std::pair<uint32_t, uint32_t> framebufferSize() const {
        int w = 0;
        int h = 0;

        glfwGetFramebufferSize(_window, &w, &h);
        return { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    }

    void pauseMinimised() const {
        int w = 0;
        int h = 0;
        
        glfwGetFramebufferSize(_window, &w, &h);
        while (w == 0 || h == 0) {
            glfwWaitEvents();
            glfwGetFramebufferSize(_window, &w, &h);
        }
    }

    bool resized() const { return _resized; }
    void clearResized() { _resized = false; }

    float scrollDelta() const { return _scrollDelta; }
    void clearScrollDelta() { _scrollDelta = 0.0f; }

private:
    GLFWwindow* _window = nullptr;
    bool _resized = false;
    float _scrollDelta = 0.0f;
};
}