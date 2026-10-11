#pragma once

#include "core/context.hpp"
#include "core/window.hpp"
#include "core/swapchain.hpp"
#include "core/frame.hpp"
#include "core/image.hpp"
#include "gfx/pipeline.hpp"
#include "scene/camera.hpp"
#include "scene/loader.hpp"
#include "scene/mesh.hpp"
#include "scene/scene.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <chrono>
#include <optional>
#include <vector>

namespace rh {

class App {
public:
    // FlightHelmet: Khronos glTF-Sample-Assets, CC0. assets/ + this path are
    // yours to swap for other .gltf/.glb files.
    static constexpr const char* MODEL_PATH = ASSET_DIR "FlightHelmet.gltf";

    App() {
        SceneData data;
        loadGltf(data, MODEL_PATH);
        replicate(data, GRID_SIZE, GRID_SPACING);
        buildFloor(data);   // after replicate, or the floor gets copied too
        _scene.emplace(_context, data);
        buildDepthImages();
    }

    void run() {
        auto prevTime = std::chrono::steady_clock::now();
        while (!_window.shouldClose()) {
            _window.pollEvents();
            const auto now = std::chrono::steady_clock::now();
            const float deltaTime = std::chrono::duration<float>(now - prevTime).count();
            prevTime = now;
            drawFrame(deltaTime);
        }
        _context.device().waitIdle();
    }

private:
    static constexpr vk::Format DEPTH_FORMAT = vk::Format::eD32Sfloat;
    static constexpr float FLOOR_HALF_EXTENT = 10.0f;
    static constexpr int   GRID_SIZE = 8;   // 8x8 copies of the loaded model
    static constexpr float GRID_SPACING = 2.5f;

    // Hand-authored, appended into the same SceneData the loader filled.
    // Winding matches the old hardcoded cube's verified top face
    // (CCW as seen from +Y, cullMode eBack).
    void buildFloor(SceneData& data) {
        const std::vector<Vertex> vertices{
            { { -FLOOR_HALF_EXTENT, 0.0f, -FLOOR_HALF_EXTENT }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
            { { -FLOOR_HALF_EXTENT, 0.0f,  FLOOR_HALF_EXTENT }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
            { {  FLOOR_HALF_EXTENT, 0.0f,  FLOOR_HALF_EXTENT }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
            { {  FLOOR_HALF_EXTENT, 0.0f, -FLOOR_HALF_EXTENT }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
        };
        const std::vector<uint32_t> indices{ 0, 1, 2, 0, 2, 3 };
        data.addInstance(data.appendMesh(vertices, indices), glm::mat4{ 1.0f });
    }

    // Culling proves nothing against a single object at the origin, so fan the
    // loaded instances out over a centred grid. Only Instance records are
    // duplicated, never geometry -- the point of the merged buffers.
    static void replicate(SceneData& data, int gridSize, float spacing) {
        const std::vector<Instance> original = data.instances;
        data.instances.clear();
        const float offset = (gridSize - 1) * spacing * 0.5f;
        for (int z = 0; z < gridSize; ++z) {
            for (int x = 0; x < gridSize; ++x) {
                const glm::vec3 t{ x * spacing - offset, 0.0f, z * spacing - offset };
                for (const Instance& inst : original) {
                    data.addInstance(inst.meshIndex,
                        glm::translate(glm::mat4{ 1.0f }, t) * inst.transform);
                }
            }
        }
    }

    void drawFrame(float deltaTime) {
        _frames.waitForSlot();

        const auto imageIndex = _frames.acquire(_swapchain);
        if (!imageIndex) {
            recreate();
            return;
        }

        const vk::Image image = _swapchain.images()[*imageIndex];
        GPUImage& depth = _depthImages[_frames.slotIndex()];

        const vk::ImageSubresourceRange colorSubresource{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        const vk::ImageSubresourceRange depthSubresource{
            .aspectMask = vk::ImageAspectFlagBits::eDepth,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };

        vk::CommandBuffer cmdBuffer = _frames.beginRecording();

        //eUndefined -> eColorAttachmentOptimal
        const vk::ImageMemoryBarrier2 undefToColorAttachment {
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image,
            .subresourceRange = colorSubresource
        };

        //eUndefined -> eDepthStencilAttachmentOptimal
        const vk::ImageMemoryBarrier2 undefToDepthAttachment {
            .srcStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests,
            .srcAccessMask = vk::AccessFlagBits2::eNone,
            .dstStageMask = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                             vk::PipelineStageFlagBits2::eLateFragmentTests,
            .dstAccessMask = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = *depth.image,
            .subresourceRange = depthSubresource
        };

        const std::array preRenderBarriers{ undefToColorAttachment, undefToDepthAttachment };

        const vk::DependencyInfo preRenderDependency{
            .imageMemoryBarrierCount = static_cast<uint32_t>(preRenderBarriers.size()),
            .pImageMemoryBarriers = preRenderBarriers.data()
        };

        cmdBuffer.pipelineBarrier2(preRenderDependency);

        const vk::RenderingAttachmentInfo colorAttachmentRenderInfo{
            .imageView = *_swapchain.imageViews()[*imageIndex],
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{
                vk::ClearColorValue{ std::array<float, 4>{ 0.5f, 0.5f, 0.5f, 1.0f }}
            }
        };

        const vk::RenderingAttachmentInfo depthAttachmentRenderInfo{
            .imageView = *depth.view,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .clearValue = vk::ClearValue{ vk::ClearDepthStencilValue{ 1.0f, 0 } }
        };

        const vk::RenderingInfo renderInfo {
            .renderArea = {
                .offset = { .x = 0, .y = 0},
                .extent = _swapchain.extent()
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRenderInfo,
            .pDepthAttachment = &depthAttachmentRenderInfo
        };

        const vk::Viewport viewport{
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = static_cast<float>(_swapchain.extent().width),
            .height   = static_cast<float>(_swapchain.extent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        const vk::Rect2D scissor{
            .offset = { .x = 0, .y = 0 },
            .extent = _swapchain.extent()
        };

        _camera.update(_frames.slotIndex(), deltaTime);
        const vk::DeviceAddress cameraAddress = _camera.bufferAddress(_frames.slotIndex());

        cmdBuffer.beginRendering(renderInfo);
        cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *_pipeline.pipelineHandle());
        cmdBuffer.setViewport(0, viewport);
        cmdBuffer.setScissor(0, scissor);

        // One index buffer and one push-constant write for the whole scene.
        // The per-instance loop is what vkCmdDrawIndexedIndirectCount replaces
        // next milestone -- every argument is already a field of
        // VkDrawIndexedIndirectCommand.
        const PushConstants pushConstants{
            .vertexBuffer = _scene->vertexAddress(),
            .camera = cameraAddress,
            .instances = _scene->instanceAddress()
        };
        cmdBuffer.pushConstants<PushConstants>(*_pipeline.pipelineLayout(),
            vk::ShaderStageFlagBits::eVertex, 0, pushConstants);
        cmdBuffer.bindIndexBuffer(_scene->indexBuffer(), 0, vk::IndexType::eUint32);

        const auto& instances = _scene->instances();
        for (uint32_t i = 0; i < instances.size(); ++i) {
            const MeshInfo& mesh = _scene->meshes()[instances[i].meshIndex];
            cmdBuffer.drawIndexed(mesh.indexCount, 1, mesh.firstIndex, mesh.vertexOffset, i);
        }

        cmdBuffer.endRendering();

        //eColorAttachmentOptimal -> ePresentSrcKHR
        const vk::ImageMemoryBarrier2 colorAttachmentToPresent{
            .srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
            .dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
            .dstAccessMask = vk::AccessFlagBits2::eNone,
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
            .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
            .image = image,
            .subresourceRange = colorSubresource
        };

        const vk::DependencyInfo presentDependency{
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &colorAttachmentToPresent
        };

        cmdBuffer.pipelineBarrier2(presentDependency);

        if (!_frames.submitAndPresent(_swapchain, *imageIndex) || _window.resized()) {
            _window.clearResized();
            recreate();
        }
    }

    void recreate() {
        _swapchain.recreate();
        _frames.onSwapchainRecreate(_swapchain);
        _camera.onResize(_swapchain.extent());
        buildDepthImages();
    }

    void buildDepthImages() {
        _depthImages.clear();
        _depthImages.reserve(MAX_FRAMES_IN_FLIGHT);
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            _depthImages.push_back(createImage(_context, _swapchain.extent(), DEPTH_FORMAT,
                vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::ImageAspectFlagBits::eDepth));
        }
    }

    Window    _window{1600, 900, "Rhaast"};
    Context   _context{_window};
    Swapchain _swapchain{_context, _window};
    FrameRing _frames{_context, _swapchain};
    Pipeline  _pipeline{_context, _swapchain.format(), DEPTH_FORMAT};
    Camera    _camera{_context, _window, _swapchain.extent()};

    std::optional<Scene> _scene;   // built in App()'s body, after _context

    std::vector<GPUImage> _depthImages;
};
}
