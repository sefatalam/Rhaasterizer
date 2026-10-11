#pragma once

#include <vulkan/vulkan_raii.hpp>

#include "core/context.hpp"
#include "gfx/shader.hpp"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

namespace rh {

// BDA vertex + camera pulling: the vertex shader indexes the vertex buffer's
// device address directly via gl_VertexIndex and dereferences the camera
// buffer's device address for view/projection, so the pipeline declares no
// vertex input state and the push constant carries two addresses, not a mat4.
struct PushConstants {
    vk::DeviceAddress vertexBuffer;
    vk::DeviceAddress camera;
    vk::DeviceAddress instances;
};

class Pipeline {
public:
    Pipeline(const Context& context, vk::Format colorFormat, vk::Format depthFormat) {
        const vk::raii::Device& device = context.device();

        const vk::PushConstantRange pushConstantRange{
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .offset = 0,
            .size = sizeof(PushConstants)
        };

        // No descriptor sets: shading is normal-based, so nothing is sampled
        // and the only thing the shaders need is the two push-constant
        // addresses.
        _layout = vk::raii::PipelineLayout(device, vk::PipelineLayoutCreateInfo{
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange
        });

        const Shader vert("cube.vert");
        const Shader frag("cube.frag");

        const vk::ShaderModuleCreateInfo vertCode = vert.moduleInfo();
        const vk::ShaderModuleCreateInfo fragCode = frag.moduleInfo();

        const std::array stages{
            vk::PipelineShaderStageCreateInfo{
                .pNext = &vertCode,
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = nullptr,
                .pName = "main"
            },
            vk::PipelineShaderStageCreateInfo{
                .pNext = &fragCode,
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = nullptr,
                .pName = "main"
            }
        };

        const vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

        const vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE
        };

        const vk::PipelineViewportStateCreateInfo viewportInfo{
            .viewportCount = 1,
            .scissorCount = 1
        };

        const vk::PipelineRasterizationStateCreateInfo rasterizerInfo{
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f
        };

        const vk::PipelineMultisampleStateCreateInfo multisampleInfo{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE
        };

        const vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLess,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE
        };

        const vk::PipelineColorBlendAttachmentState blendAttachment{
            .blendEnable = VK_FALSE,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
        };

        const vk::PipelineColorBlendStateCreateInfo blendInfo {
            .logicOpEnable = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments = &blendAttachment
        };

        const std::array dynamicStates{
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        const vk::PipelineDynamicStateCreateInfo dynamicStateInfo{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        const vk::PipelineRenderingCreateInfo renderingInfo {
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &colorFormat,
            .depthAttachmentFormat = depthFormat
        };

        const vk::GraphicsPipelineCreateInfo pipelineInfo{
            .pNext = &renderingInfo,
            .stageCount = static_cast<uint32_t>(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssemblyInfo,
            .pViewportState = &viewportInfo,
            .pRasterizationState = &rasterizerInfo,
            .pMultisampleState = &multisampleInfo,
            .pDepthStencilState = &depthStencilInfo,
            .pColorBlendState = &blendInfo,
            .pDynamicState = &dynamicStateInfo,
            .layout = *_layout
        };

        _pipeline = vk::raii::Pipeline(device, nullptr, pipelineInfo);
    }

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    const vk::raii::Pipeline& pipelineHandle() const { return _pipeline; }
    const vk::raii::PipelineLayout& pipelineLayout() const { return _layout; }

private:
    vk::raii::PipelineLayout _layout = nullptr;
    vk::raii::Pipeline _pipeline = nullptr;

};
}
