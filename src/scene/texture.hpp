#pragma once

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>
#include <ktx.h>

#include "core/context.hpp"
#include "core/image.hpp"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace rh {

inline GPUImage loadKTX2(const Context& context, const std::filesystem::path& path) {
    ktxTexture2* texture = nullptr;
    if (ktxTexture2_CreateFromNamedFile(path.string().c_str(), 
    KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture) != KTX_SUCCESS)
    {
        throw std::runtime_error("loadKTX2: failed to open " + path.string());
    }

    if (ktxTexture2_NeedsTranscoding(texture)) {
        if (ktxTexture2_TranscodeBasis(texture, KTX_TTF_BC7_RGBA, 0) != KTX_SUCCESS) {
            ktxTexture_Destroy(ktxTexture(texture));
            throw std::runtime_error("loadKTX2: transcode failed for " + path.string());
        }
    }

    const auto format = static_cast<vk::Format>(texture->vkFormat);
    const uint32_t levels = texture->numLevels;

    vma::raii::Image image (
        context.allocator(),
        vk::ImageCreateInfo{
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = { texture->baseWidth, texture->baseHeight, 1 },
            .mipLevels = levels,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eHostTransferEXT
        },
        vma::AllocationCreateInfo{ .usage = vma::MemoryUsage::eAuto }
    );

    const vk::raii::Device& device = context.device();
    const vk::ImageSubresourceRange range{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = levels,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    device.transitionImageLayoutEXT(vk::HostImageLayoutTransitionInfoEXT {
        .image = *image,
        .oldLayout = vk::ImageLayout::eUndefined,
        .newLayout = vk::ImageLayout::eTransferDstOptimal,
        .subresourceRange = range
    });

    std::vector<vk::MemoryToImageCopyEXT> regions(levels);
    for (uint32_t level = 0; level < levels; ++level) {
        size_t offset = 0;
        ktxTexture_GetImageOffset(ktxTexture(texture), level, 0, 0, &offset);
        regions[level] = vk::MemoryToImageCopyEXT {
            .pHostPointer = texture->pData + offset,
            .imageSubresource = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .mipLevel = level,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            .imageExtent = {
                std::max(1u, texture->baseWidth >> level),
                std::max(1u, texture->baseHeight >> level),
                1
            }
        };
    }

    device.copyMemoryToImageEXT(vk::CopyMemoryToImageInfoEXT{
        .dstImage = *image,
        .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
        .regionCount = static_cast<uint32_t>(regions.size()),
        .pRegions = regions.data()
    });

    device.transitionImageLayoutEXT(vk::HostImageLayoutTransitionInfoEXT{
        .image = *image,
        .oldLayout = vk::ImageLayout::eTransferDstOptimal,
        .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        .subresourceRange = range
    });

    ktxTexture_Destroy(ktxTexture(texture));

    vk::raii::ImageView view(device, vk::ImageViewCreateInfo{
        .image = *image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = range
    });

    return { std::move(image), std::move(view) };

}


}