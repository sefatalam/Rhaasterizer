#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>

#include "core/buffer.hpp"
#include "core/context.hpp"
#include "scene/mesh.hpp"

#include <stdexcept>
#include <vector>

namespace rh {
class Scene {
public:
    Scene(const Context& context, const SceneData& data) {
        if (data.instances.empty()) {
            throw std::runtime_error("Scene: no instances to draw");
        }

        _vertexBuffer = uploadBuffer(context, data.vertices.size() * sizeof(Vertex), 
            vk::BufferUsageFlagBits::eShaderDeviceAddress, data.vertices.data());
        
        _indexBuffer = uploadBuffer(context, data.indices.size() * sizeof(uint32_t), 
            vk::BufferUsageFlagBits::eIndexBuffer, data.indices.data());
        
        _meshBuffer = uploadBuffer(context, data.meshes.size() * sizeof(MeshInfo), 
            vk::BufferUsageFlagBits::eShaderDeviceAddress, data.meshes.data());

        _instanceBuffer = uploadBuffer(context, data.instances.size() * sizeof(Instance), 
            vk::BufferUsageFlagBits::eShaderDeviceAddress, data.instances.data());

        const auto address = [&](const vma::raii::Buffer& b) {
            return context.device().getBufferAddress(
                vk::BufferDeviceAddressInfo { .buffer = *b }
            );
        };

        _vertexAddress = address(_vertexBuffer);
        _meshAddress = address(_meshBuffer);
        _instanceAddress = address(_instanceBuffer);

        // host-side copies for the CPU draw loop; drop out at the indirect step
        _meshes = data.meshes;
        _instances = data.instances;
    }

    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    vk::Buffer indexBuffer() const { return *_indexBuffer; }
    vk::DeviceAddress vertexAddress() const { return _vertexAddress; }
    vk::DeviceAddress meshAddress() const { return _meshAddress; }
    vk::DeviceAddress instanceAddress() const { return _instanceAddress; }

    const std::vector<MeshInfo>& meshes() const { return _meshes; }
    const std::vector<Instance>& instances() const { return _instances; }

private:
    vma::raii::Buffer _vertexBuffer = nullptr;
    vma::raii::Buffer _indexBuffer = nullptr;
    vma::raii::Buffer _meshBuffer = nullptr;
    vma::raii::Buffer _instanceBuffer = nullptr;

    vk::DeviceAddress _vertexAddress = 0;
    vk::DeviceAddress _meshAddress = 0;
    vk::DeviceAddress _instanceAddress = 0;

    std::vector<MeshInfo> _meshes;
    std::vector<Instance> _instances;

};

}