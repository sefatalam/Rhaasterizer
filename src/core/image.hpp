#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>

#include "core/context.hpp"

#include <utility>

namespace rh {

struct GPUImage {
    vma::raii::Image image = nullptr;
    vk::raii::ImageView view = nullptr;
};

inline GPUImage createImage(const Context& context, vk::Extent2D extent, vk::Format format,
                            vk::ImageUsageFlags usage, vk::ImageAspectFlags aspect)
{
    vma::raii::Image image(context.allocator(),
        vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = { extent.width, extent.height, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage
        },
        vma::AllocationCreateInfo { .usage = vma::MemoryUsage::eAuto }
    );

    vk::raii::ImageView view(context.device(),
        vk::ImageViewCreateInfo{
            .image = *image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {
                .aspectMask = aspect,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        }
    );

    return { std::move(image), std::move(view) };
}

}