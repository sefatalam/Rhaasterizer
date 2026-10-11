#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>

#include "core/context.hpp"
#include "core/frame.hpp"
#include "core/window.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstring>

namespace rh {

struct transformMatrices {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewProjection; //Model gets folded into here
};

class Camera {
public:
    Camera(const Context& context, Window& window, vk::Extent2D extent)
        : _window(window)
    {
        vma::AllocationInfo allocInfo{};
        _buffer = vma::raii::Buffer(context.allocator(),
            vk::BufferCreateInfo{
                .size = sizeof(transformMatrices) * MAX_FRAMES_IN_FLIGHT,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer |
                            vk::BufferUsageFlagBits::eShaderDeviceAddress
            },
            vma::AllocationCreateInfo{
                .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                            vma::AllocationCreateFlagBits::eMapped,
                .usage = vma::MemoryUsage::eAuto
            },
            vk::Optional<vma::AllocationInfo>(&allocInfo)
        );
        _mapped = allocInfo.pMappedData;
        _bufferAddress = context.device().getBufferAddress(
            vk::BufferDeviceAddressInfo{ .buffer = *_buffer });
        onResize(extent);

        glfwSetInputMode(_window.windowHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwGetCursorPos(_window.windowHandle(), &_lastMouseX, &_lastMouseY);
    }

    void onResize(vk::Extent2D extent) {
        _aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    }

    void update(uint32_t slot, float deltaTime) {
        processInput(deltaTime);

        const glm::vec3 forward = directionVector();
        const glm::mat4 view = glm::lookAt(_position, _position + forward, glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 proj = glm::perspective(glm::radians(_fov), _aspect, 0.1f, 100.0f);
        proj[1][1] *= -1.0f;

        const transformMatrices data = { .view = view, .projection = proj, .viewProjection = proj * view };

        auto* dst = static_cast<transformMatrices*>(_mapped) + slot;
        memcpy(dst, &data, sizeof(data));
    }

    // Address of the current slot's transformMatrices record; the shader
    // dereferences this directly via buffer_reference, the same way it
    // dereferences the vertex buffer.
    vk::DeviceAddress bufferAddress(uint32_t slot) const {
        return _bufferAddress + static_cast<vk::DeviceAddress>(slot) * sizeof(transformMatrices);
    }

private:
    Window& _window;

    vma::raii::Buffer _buffer = nullptr;
    void* _mapped = nullptr;
    vk::DeviceAddress _bufferAddress = 0;
    float _aspect = 1.0f;
    float _fov = 60.0f;

    glm::vec3 _position{ 3.0f, 2.5f, 3.0f };
    float _yaw = -135.0f;
    float _pitch = -30.5f;

    double _lastMouseX = 0.0;
    double _lastMouseY = 0.0;

    static constexpr float MOUSE_SENSITIVITY = 0.1f;
    static constexpr float MOVE_SPEED = 3.0f;
    static constexpr float ZOOM_SENSITIVITY = 2.0f; // degrees of FOV per scroll step
    static constexpr float MIN_FOV = 10.0f;
    static constexpr float MAX_FOV = 90.0f;

    glm::vec3 directionVector() const {
        const float yawRad = glm::radians(_yaw);
        const float pitchRad = glm::radians(_pitch);
        return glm::normalize(
            glm::vec3(
                glm::cos(pitchRad) * glm::cos(yawRad),
                glm::sin(pitchRad),
                glm::cos(pitchRad) * glm::sin(yawRad)
        ));
    }

    void processInput(float deltaTime) {
        GLFWwindow* handle = _window.windowHandle();

        double mouseX = 0.0;
        double mouseY = 0.0;

        glfwGetCursorPos(handle, &mouseX, &mouseY);

        const float dx = static_cast<float>(mouseX - _lastMouseX);
        const float dy = static_cast<float>(mouseY - _lastMouseY);
        _yaw += dx * MOUSE_SENSITIVITY;
        _pitch = glm::clamp(_pitch - dy * MOUSE_SENSITIVITY, -89.0f, 89.0f);

        _lastMouseX = mouseX;
        _lastMouseY = mouseY;

        _fov = glm::clamp(_fov - _window.scrollDelta() * ZOOM_SENSITIVITY, MIN_FOV, MAX_FOV);
        _window.clearScrollDelta();

        const glm::vec3 fwd = directionVector();
        const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up{ 0.0f, 1.0f, 0.0f };

        const float speed = MOVE_SPEED * deltaTime;

        if (glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS) _position += fwd * speed;
        if (glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS) _position -= fwd * speed;
        if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS) _position += right * speed;
        if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS) _position -= right * speed;
        if (glfwGetKey(handle, GLFW_KEY_SPACE) == GLFW_PRESS) _position += up * speed;
        if (glfwGetKey(handle, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) _position -= up * speed;
    }

};

}