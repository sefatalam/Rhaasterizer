#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <vulkan/vulkan_raii.hpp>
#include <vk_mem_alloc_raii.hpp>

#include <VkBootstrap.h>

#include "core/window.hpp"


#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rh {

class Context {
public:
    explicit Context(const Window& window) {
        // ------------------------------------------------------------------
        // Dispatcher
        // vk::raii objects have their own dispatcher, plain vk handles dont
        // Need a default dispatcher to handle plain vk handles
        // ------------------------------------------------------------------
        PFN_vkGetInstanceProcAddr getInstanceProcAddr =
            _context.getDispatcher()->vkGetInstanceProcAddr;
        VULKAN_HPP_DEFAULT_DISPATCHER.init(getInstanceProcAddr);

        // ------------------------------------------------------------------
        // Instance
        // ------------------------------------------------------------------
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (!glfwExtensions) {
            throw std::runtime_error("GLFW has no Vulkan surface support");
        }

        auto instanceBuilder = 
            vkb::InstanceBuilder(getInstanceProcAddr)
                .set_app_name("Rhaast")
                .set_engine_name("Rhaast")
                .require_api_version(1, 4, 0)
                .enable_extensions(glfwExtensionCount, glfwExtensions);
#ifdef RHAAST_VALIDATION
        instanceBuilder.request_validation_layers()
        .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        .use_default_debug_messenger();
#endif

        auto instanceResult = instanceBuilder.build();
        if (!instanceResult) {
            throw std::runtime_error("Failed to make instance. Error: " + instanceResult.error().message());
        }
        vkb::Instance vkbInstance = instanceResult.value();

        _instance = vk::raii::Instance(_context, vkbInstance.instance);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*_instance);

        if (vkbInstance.debug_messenger) {
            _debugMessenger = vk::raii::DebugUtilsMessengerEXT(_instance, vkbInstance.debug_messenger);
        }

        // ------------------------------------------------------------------
        // Surface
        // ------------------------------------------------------------------
        VkSurfaceKHR plainSurface = VK_NULL_HANDLE;
        if (glfwCreateWindowSurface(*_instance, window.windowHandle(), nullptr, &plainSurface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }
        _surface = vk::raii::SurfaceKHR(_instance, plainSurface);

        // ------------------------------------------------------------------
        // Physical device
        // ------------------------------------------------------------------
        VkPhysicalDeviceFeatures features10{};
        features10.multiDrawIndirect = VK_TRUE;
        features10.samplerAnisotropy = VK_TRUE;
        features10.shaderInt64 = VK_TRUE;
        features10.drawIndirectFirstInstance = VK_TRUE;
        features10.depthClamp = VK_TRUE;

        VkPhysicalDeviceVulkan11Features features11{};
        features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        features11.shaderDrawParameters = VK_TRUE;
        features11.storageBuffer16BitAccess = VK_TRUE;

        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.drawIndirectCount = VK_TRUE;
        features12.bufferDeviceAddress = VK_TRUE;
        features12.storageBuffer8BitAccess = VK_TRUE;
        features12.shaderInt8 = VK_TRUE;
        features12.scalarBlockLayout = VK_TRUE;
        features12.timelineSemaphore = VK_TRUE;
        features12.hostQueryReset = VK_TRUE;
        features12.samplerFilterMinmax = VK_TRUE;
        features12.shaderSubgroupExtendedTypes = VK_TRUE;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;
        features13.maintenance4 = VK_TRUE;
        features13.subgroupSizeControl = VK_TRUE;
        features13.computeFullSubgroups = VK_TRUE;
        features13.shaderZeroInitializeWorkgroupMemory = VK_TRUE;

        VkPhysicalDeviceVulkan14Features features14{};
        features14.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES;
        features14.pushDescriptor = VK_TRUE;
        features14.dynamicRenderingLocalRead = VK_TRUE;
        features14.vertexAttributeInstanceRateDivisor = VK_TRUE;
        features14.vertexAttributeInstanceRateZeroDivisor = VK_TRUE;
        features14.shaderSubgroupRotate = VK_TRUE;
        features14.shaderSubgroupRotateClustered = VK_TRUE;
        features14.shaderExpectAssume = VK_TRUE;
        features14.indexTypeUint8 = VK_TRUE;
        features14.maintenance5 = VK_TRUE;
        features14.maintenance6 = VK_TRUE;
        features14.hostImageCopy = VK_TRUE;
        features14.shaderFloatControls2 = VK_TRUE;

        // No mesh shaders: VK_EXT_mesh_shader needs Turing or newer on NVIDIA,
        // and this is a Pascal card. The GPU-driven path here is therefore
        // compute-shader culling feeding vkCmdDrawIndexedIndirectCount, which
        // is what multiDrawIndirect / drawIndirectCount / shaderDrawParameters
        // above are for.

        VkPhysicalDeviceDescriptorBufferFeaturesEXT descBufferFeatures{};
        descBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
        descBufferFeatures.descriptorBuffer = VK_TRUE;
        descBufferFeatures.descriptorBufferPushDescriptors = VK_TRUE;

        VkPhysicalDeviceExtendedDynamicState3FeaturesEXT dynState3{};
        dynState3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
        dynState3.extendedDynamicState3DepthClampEnable = VK_TRUE;
        dynState3.extendedDynamicState3PolygonMode = VK_TRUE;
        dynState3.extendedDynamicState3RasterizationSamples = VK_TRUE;
        dynState3.extendedDynamicState3SampleMask = VK_TRUE;
        dynState3.extendedDynamicState3AlphaToCoverageEnable = VK_TRUE;
        dynState3.extendedDynamicState3ColorBlendEnable = VK_TRUE;
        dynState3.extendedDynamicState3ColorWriteMask = VK_TRUE;
        dynState3.extendedDynamicState3DepthClipEnable = VK_TRUE;

        const std::vector<const char*> requiredExtensions = {
            VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
            VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
            VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        };

        auto physicalDeviceResult = 
            vkb::PhysicalDeviceSelector { vkbInstance }
                .set_minimum_version(1, 4)
                .set_surface(*_surface)
                .set_required_features(features10)
                .set_required_features_11(features11)
                .set_required_features_12(features12)
                .set_required_features_13(features13)
                .set_required_features_14(features14)
                .add_required_extensions(requiredExtensions)
                .add_required_extension_features(descBufferFeatures)
                .add_required_extension_features(dynState3)
                .select();

        if (!physicalDeviceResult) {
            throw std::runtime_error("Physical device: " + physicalDeviceResult.error().message());
        }

        vkb::PhysicalDevice vkbPhysical = physicalDeviceResult.value();

        _physicalDevice = vk::raii::PhysicalDevice(_instance, vkbPhysical.physical_device);
        _deviceName = vkbPhysical.name;

        // ------------------------------------------------------------------
        // Logical device + queues
        // ------------------------------------------------------------------
        auto deviceResult = vkb::DeviceBuilder(vkbPhysical).build();
        if (!deviceResult) {
            throw std::runtime_error("Logical Device: " + deviceResult.error().message());
        }
        vkb::Device vkbDevice = deviceResult.value();

        _device = vk::raii::Device(_physicalDevice, vkbDevice.device);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*_device);

        _graphicsQueue = getQueue(vkbDevice, vkb::QueueType::graphics, _graphicsFamily);
        _presentQueue = getQueue(vkbDevice, vkb::QueueType::present, _presentFamily);
    
        // ------------------------------------------------------------------
        // Allocator
        // ------------------------------------------------------------------
        _allocator = vma::raii::Allocator(_instance, _device, vma::AllocatorCreateInfo{
            .flags = vma::AllocatorCreateFlagBits::eBufferDeviceAddress,
            .physicalDevice = *_physicalDevice,
            .vulkanApiVersion = VK_API_VERSION_1_4
        });

        std::cout << "Rhaast: " << _deviceName
                << " (graphics queue family " << _graphicsFamily
                << ", present queue family " << _presentFamily << ")\n";
    }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    const vk::raii::Instance&       instance() const { return _instance; }
    const vk::raii::SurfaceKHR&     surface()  const { return _surface; }
    const vk::raii::PhysicalDevice& physical() const { return _physicalDevice; }
    const vk::raii::Device&         device()   const { return _device; }

    vk::Queue graphicsQueue() const { return _graphicsQueue; }
    vk::Queue presentQueue() const { return _presentQueue; }
    uint32_t graphicsFamily() const { return _graphicsFamily; }
    uint32_t presentFamily() const { return _presentFamily; }
    const vma::raii::Allocator& allocator() const { return _allocator; }
    const std::string& deviceName() const { return _deviceName; }

private:
    vk::raii::Context _context;
    vk::raii::Instance _instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT _debugMessenger = nullptr;
    vk::raii::SurfaceKHR _surface = nullptr;
    vk::raii::PhysicalDevice _physicalDevice = nullptr;
    vk::raii::Device _device = nullptr;
    
    vk::Queue _graphicsQueue = nullptr;
    vk::Queue _presentQueue = nullptr;
    uint32_t _graphicsFamily = 0;
    uint32_t _presentFamily = 0;
    vma::raii::Allocator _allocator = nullptr;
    std::string _deviceName;

    static vk::Queue getQueue(const vkb::Device& device, vkb::QueueType type, uint32_t& indexOut) {
        auto queue = device.get_queue(type);
        if (!queue) {
            throw std::runtime_error("Queue: " + queue.error().message());
        }

        auto queueIndex = device.get_queue_index(type);
        if (!queueIndex) {
            throw std::runtime_error("Queue index: " + queueIndex.error().message());
        }

        indexOut = queueIndex.value();
        return vk::Queue(queue.value());
    }
};
}