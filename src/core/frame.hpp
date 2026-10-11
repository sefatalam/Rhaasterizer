#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"
#include "core/swapchain.hpp"

#include <array>
#include <optional>
#include <vector>
#include <stdexcept>
#include <cstdint>

namespace rh {

inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

struct FrameSlot {
    vk::raii::CommandPool pool;
    vk::CommandBuffer cmdBuffer;
    vk::raii::Semaphore acquired;

    FrameSlot(const vk::raii::Device& device, uint32_t queueFamily)
        : pool(device, vk::CommandPoolCreateInfo{
            .flags = vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = queueFamily
        }), 
        cmdBuffer(nullptr),
        acquired(device, vk::SemaphoreCreateInfo{})
    {
        const vk::CommandBufferAllocateInfo allocInfo{
            .commandPool = *pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1
        };

        cmdBuffer = (*device).allocateCommandBuffers(allocInfo).front();
    }
};


class FrameRing {
public:
    FrameRing(const Context& context, const Swapchain& swapchain)
        : _context(context)
    {
        const vk::raii::Device& device = context.device();

        vk::SemaphoreTypeCreateInfo typeInfo{
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue = 0
        };

        _timeline = vk::raii::Semaphore(device, vk::SemaphoreCreateInfo{ .pNext = &typeInfo });

        _slots.reserve(MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            _slots.emplace_back(device, context.graphicsFamily());
        }

        buildPresentSemaphores(swapchain);
    }

    FrameRing(const FrameRing&) = delete;
    FrameRing& operator=(const FrameRing&) = delete;

    void onSwapchainRecreate(const Swapchain& swapchain) {
        _renderFinished.clear();
        buildPresentSemaphores(swapchain);
    }

    void waitForSlot() {
        if (_frameNumber < MAX_FRAMES_IN_FLIGHT) {
            return;
        }

        const uint64_t target = _frameNumber - MAX_FRAMES_IN_FLIGHT + 1;
        const vk::SemaphoreWaitInfo waitInfo{
            .semaphoreCount = 1,
            .pSemaphores = &*_timeline,
            .pValues = &target
        };
        
        if (_context.device().waitSemaphores(waitInfo, UINT64_MAX) != vk::Result::eSuccess) {
            throw std::runtime_error("Timeline wait failed");
        }
    }

    std::optional<uint32_t> acquire(const Swapchain& swapchain) {
        try {
            auto [result, index] = 
                swapchain.swapchainHandle().acquireNextImage(UINT64_MAX, *_currentSlot().acquired);
            (void)result;
            return index;
        } catch (const vk::OutOfDateKHRError&) {
            return std::nullopt;
        }
    }

    uint32_t slotIndex() const { return static_cast<uint32_t>(_frameNumber % MAX_FRAMES_IN_FLIGHT); }

    vk::CommandBuffer beginRecording() {
        FrameSlot& slot = _currentSlot();
        slot.pool.reset();
        slot.cmdBuffer.begin(vk::CommandBufferBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        });

        return slot.cmdBuffer;
    }

    bool submitAndPresent(const Swapchain& swapchain, uint32_t imgIndex) {
        FrameSlot& slot = _currentSlot();
        slot.cmdBuffer.end();

        const vk::SemaphoreSubmitInfo waitAcquire{
            .semaphore = *slot.acquired,
            .value = 0,
            .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
        };

        const std::array signals{
            vk::SemaphoreSubmitInfo{
                .semaphore = *_renderFinished[imgIndex],
                .value     = 0,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
            },
            vk::SemaphoreSubmitInfo{
                .semaphore = *_timeline,
                .value     = _frameNumber + 1,
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
            }
        };

        const vk::CommandBufferSubmitInfo cmdBufferInfo{ .commandBuffer = slot.cmdBuffer };

        const vk::SubmitInfo2 submitInfo{
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitAcquire,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmdBufferInfo,
            .signalSemaphoreInfoCount = static_cast<uint32_t>(signals.size()),
            .pSignalSemaphoreInfos = signals.data()
        };

        _context.graphicsQueue().submit2(submitInfo);
        ++_frameNumber;

        const vk::PresentInfoKHR presentInfo{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &*_renderFinished[imgIndex],
            .swapchainCount = 1,
            .pSwapchains = &*swapchain.swapchainHandle(),
            .pImageIndices = &imgIndex
        };

        try {
            return _context.presentQueue().presentKHR(presentInfo) != vk::Result::eSuboptimalKHR;
        } catch (const vk::OutOfDateKHRError&) {
            return false;
        }
    }

private:
    const Context& _context;

    vk::raii::Semaphore _timeline = nullptr;
    uint64_t _frameNumber = 0;

    std::vector<FrameSlot> _slots;
    std::vector<vk::raii::Semaphore> _renderFinished;

    FrameSlot& _currentSlot() { return _slots[_frameNumber % MAX_FRAMES_IN_FLIGHT]; }

    void buildPresentSemaphores(const Swapchain& swapchain) {
        _renderFinished.reserve(swapchain.imageCount());
        for (uint32_t i = 0; i < swapchain.imageCount(); ++i) {
            _renderFinished.emplace_back(_context.device(), vk::SemaphoreCreateInfo{});
        }
    }
};
}