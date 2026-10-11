#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <VkBootstrap.h>

#include "core/context.hpp"
#include "core/window.hpp"

#include <vector>
#include <stdexcept>
#include <cstdint>

namespace rh {
class Swapchain {
public:
    Swapchain(const Context& context, const Window& window)
        : _context(context), _window(window) 
    {
        build(VK_NULL_HANDLE);
    }

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void recreate() {
        _window.pauseMinimised();
        _context.device().waitIdle();

        _imageViews.clear();
        _images.clear();

        build(static_cast<VkSwapchainKHR>(*_swapchain));
    }

    const vk::raii::SwapchainKHR& swapchainHandle() const { return _swapchain; }
    const std::vector<vk::Image>& images() const { return _images; }
    const std::vector<vk::raii::ImageView>& imageViews() const { return _imageViews; }

    vk::Format format() const { return _format; }
    vk::Extent2D extent() const { return _extent; }
    uint32_t imageCount() const { return static_cast<uint32_t>(_images.size()); }

private:
    const Context& _context;
    const Window& _window;

    vk::raii::SwapchainKHR _swapchain = nullptr;
    vk::Format _format = vk::Format::eUndefined;
    vk::Extent2D _extent = {};

    std::vector<vk::Image> _images;
    std::vector<vk::raii::ImageView> _imageViews;


    void build(VkSwapchainKHR oldSwapchain) {
        const auto [width, height] = _window.framebufferSize();

        auto swapchainResult =
            vkb::SwapchainBuilder(static_cast<VkPhysicalDevice>(*_context.physical()),
                                static_cast<VkDevice>(*_context.device()),
                                static_cast<VkSurfaceKHR>(*_context.surface()),
                                _context.graphicsFamily(),
                                _context.presentFamily())
            .set_desired_extent(width, height)
            .set_desired_format({ VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
            .add_fallback_format({ VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
            .set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_min_image_count(vkb::SwapchainBuilder::TRIPLE_BUFFERING)
            .set_image_usage_flags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_old_swapchain(oldSwapchain)
            .build();
        
        if (!swapchainResult) {
            throw std::runtime_error("Swapchain: " + swapchainResult.error().message());
        }

        vkb::Swapchain vkbSwapchain = swapchainResult.value();

        _swapchain = vk::raii::SwapchainKHR(_context.device(), vkbSwapchain.swapchain);
        _format = static_cast<vk::Format>(vkbSwapchain.image_format);
        _extent = vk::Extent2D { .width = vkbSwapchain.extent.width,
                                 .height = vkbSwapchain.extent.height };
        
        auto images = vkbSwapchain.get_images();
        if (!images) {
            throw std::runtime_error("Swapchain Images: " + images.error().message());
        }

        _images.reserve(images.value().size());
        for (VkImage image : images.value()) {
            _images.emplace_back(image);
        }

        _imageViews.reserve(_images.size());
        for (vk::Image image : _images) {
            const vk::ImageViewCreateInfo info {
                .image = image,
                .viewType = vk::ImageViewType::e2D,
                .format = _format,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                }
            };
            _imageViews.emplace_back(_context.device(), info);
        }
    }
};
}