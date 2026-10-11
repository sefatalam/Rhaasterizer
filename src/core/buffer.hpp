#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>

#include "core/context.hpp"

#include <cstring>
#include <stdexcept>

namespace rh {

inline vma::raii::Buffer uploadBuffer(const Context& context, vk::DeviceSize size,
                                        vk::BufferUsageFlags usage, const void* data)
{
    vma::AllocationInfo stagingInfo{};
    vma::raii::Buffer stagingBuffer(
        context.allocator(),
        vk::BufferCreateInfo{ .size = size, .usage = vk::BufferUsageFlagBits::eTransferSrc },
        vma::AllocationCreateInfo{
            .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite |
                    vma::AllocationCreateFlagBits::eMapped,
            .usage = vma::MemoryUsage::eAuto
        },
        vk::Optional<vma::AllocationInfo>(&stagingInfo)
    );
    memcpy(stagingInfo.pMappedData, data, static_cast<size_t>(size));

    vma::raii::Buffer resultBuffer(
        context.allocator(),
        vk::BufferCreateInfo{ .size = size, .usage = usage | vk::BufferUsageFlagBits::eTransferDst },
        vma::AllocationCreateInfo { .usage = vma::MemoryUsage::eAuto 
    });

    const vk::raii::Device& device = context.device();
    vk::raii::CommandPool pool(
        device,
        vk::CommandPoolCreateInfo {
            .flags = vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = context.graphicsFamily()
    });

    vk::raii::CommandBuffers cmdBuffers(device, vk::CommandBufferAllocateInfo{
            .commandPool = *pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
    });
    vk::CommandBuffer cmdBuffer = *cmdBuffers.front();

    cmdBuffer.begin(vk::CommandBufferBeginInfo{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });
    cmdBuffer.copyBuffer(*stagingBuffer, *resultBuffer, vk::BufferCopy{ .size = size });
    cmdBuffer.end();

    vk::raii::Fence fence(device, vk::FenceCreateInfo{});
    const vk::CommandBufferSubmitInfo submitInfo{ .commandBuffer = cmdBuffer };
    context.graphicsQueue().submit2(vk::SubmitInfo2{
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &submitInfo
        },
        *fence
    );

    if (device.waitForFences(*fence, VK_TRUE, UINT64_MAX) != vk::Result::eSuccess) {
        throw std::runtime_error("uploadBuffer : upload fence wait failed");
    }

    return resultBuffer;
}
}