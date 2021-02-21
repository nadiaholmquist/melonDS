/*
    Copyright 2016-2020 Arisotura

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include "GPU3D_Vulkan.h"

#include <array>
#include <algorithm>

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

namespace GPU3D
{
    VulkanRenderer::VulkanRenderer()
        : Renderer3D(false)
    {

        //// Create Instance
        vk::InstanceCreateInfo InstanceCreateInfo = {};

        vk::ApplicationInfo ApplicationInfo = {};
        ApplicationInfo.pApplicationName = "melonDS " MELONDS_VERSION;
        ApplicationInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
        ApplicationInfo.pEngineName = "melonDS " MELONDS_VERSION;
        ApplicationInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
        ApplicationInfo.apiVersion = VK_API_VERSION_1_2;

        InstanceCreateInfo.pApplicationInfo = &ApplicationInfo;

        // Instance validation layers
        static const char* ValidationLayers[] = {
        #ifndef NDEBUG
            "VK_LAYER_KHRONOS_validation"
        #endif
        };
        InstanceCreateInfo.enabledLayerCount = std::extent_v<decltype(ValidationLayers)>;
        InstanceCreateInfo.ppEnabledLayerNames = ValidationLayers;

        // Instance extensions
        InstanceCreateInfo.enabledExtensionCount = 0;
        InstanceCreateInfo.ppEnabledExtensionNames = nullptr;

        if (
            auto InstanceResult = vk::createInstanceUnique(InstanceCreateInfo);
            InstanceResult.result == vk::Result::eSuccess
        )
        {
            VkContext.Instance = std::move(InstanceResult.value);
        }
        else
        {
            // Failed to create instance
        }

        VULKAN_HPP_DEFAULT_DISPATCHER.init(VkContext.Instance.get());
        
        //// Pick physical device
        // Todo: Make this something the user selects
        std::vector<vk::PhysicalDevice> PhysicalDevices = {};

        if (
            auto EnumerateResult = VkContext.Instance->enumeratePhysicalDevices();
            EnumerateResult.result == vk::Result::eSuccess
        )
        {
            PhysicalDevices = std::move(EnumerateResult.value);
        }
        else
        {
            // Failed to enumerate physical devices
        }

        // Partition the Physical device array as much as possible to pick the best devices
        auto CurPivot = PhysicalDevices.end();

        // Discrete GPUs
        CurPivot = std::stable_partition(
            PhysicalDevices.begin(), CurPivot,
            [](const vk::PhysicalDevice& CurrentDevice) -> bool
            {
                const auto DeviceProperties = CurrentDevice.getProperties();
                return DeviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
            }
        );

        // Supports extended image formats for storage-images
        // Mostly for R8_UINT and R8_UNORM and such
        CurPivot = std::stable_partition(
            PhysicalDevices.begin(), CurPivot,
            [](const vk::PhysicalDevice& CurrentDevice) -> bool
            {
                const auto DeviceFeatures = CurrentDevice.getFeatures();
                return DeviceFeatures.shaderStorageImageExtendedFormats == true;
            }
        );

        // At this point, the devices are now sorted from best to worst based on our criteria above
        VkContext.PhysicalDevice = PhysicalDevices[0];

        //// Create device
        vk::DeviceCreateInfo DeviceInfo = {};

        // Device Layers
        DeviceInfo.enabledLayerCount = 0;
        DeviceInfo.ppEnabledLayerNames = nullptr;

        // Device Extensions
        DeviceInfo.enabledExtensionCount = 0;
        DeviceInfo.ppEnabledExtensionNames = nullptr;

        /// Queue allocation
        // We just want 1 queue for everything for now

        vk::DeviceQueueCreateInfo QueueInfo = {};

        QueueInfo.queueCount = 1;
        static constexpr float QueuePriorityMax = 1.0f; // Highest priority
        QueueInfo.pQueuePriorities = &QueuePriorityMax;

        // On all platforms, this is the "generic" queue that supports all
        // graphics/compute/transfer operations
        QueueInfo.queueFamilyIndex = 0;

        DeviceInfo.queueCreateInfoCount = 1;
        DeviceInfo.pQueueCreateInfos = &QueueInfo;

        if (
            auto DeviceResult = VkContext.PhysicalDevice.createDeviceUnique(DeviceInfo);
            DeviceResult.result == vk::Result::eSuccess
        )
        {
            VkContext.Device = std::move(DeviceResult.value);
        }
        else
        {
            // Error creating Logical device
        }

        VkContext.Queue = VkContext.Device->getQueue(0, 0);

        //// Allocate command pool
        vk::CommandPoolCreateInfo CommandPoolInfo = {};
        CommandPoolInfo.queueFamilyIndex = 0;
        // We want to be able to recycle the command buffer, so we want to be able to reset them
        CommandPoolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

        if(
            auto CommandPoolResult = VkContext.Device->createCommandPoolUnique(CommandPoolInfo);
            CommandPoolResult.result == vk::Result::eSuccess
        )
        {
            VkContext.CommandPool = std::move(CommandPoolResult.value);
        }
        else
        {
            // Error allocating command pool
        }

        const vk::PhysicalDeviceProperties DeviceProperties = VkContext.PhysicalDevice.getProperties();
        printf(
            "Vulkan Context created:\n"
            "\tDevice: %.256s\n"
            "\tType: %.64s\n"
            "\tID: %08X\n"
            "\tVendor: %s\n"
            "\tDriver: %u.%u.%u\n"
            "\tAPI: %u.%u.%u\n",
            DeviceProperties.deviceName.data(),
            vk::to_string(DeviceProperties.deviceType).c_str(),
            DeviceProperties.deviceID,
            Vulkan::VendorName(static_cast<Vulkan::VendorID>(DeviceProperties.vendorID)),
            VK_VERSION_MAJOR(DeviceProperties.driverVersion),
            VK_VERSION_MINOR(DeviceProperties.driverVersion),
            VK_VERSION_PATCH(DeviceProperties.driverVersion),
            VK_VERSION_MAJOR(DeviceProperties.apiVersion),
            VK_VERSION_MINOR(DeviceProperties.apiVersion),
            VK_VERSION_PATCH(DeviceProperties.apiVersion)
        );
    }

    VulkanRenderer::~VulkanRenderer()
    {
        puts("Vulkan DTOR");
    }

    bool VulkanRenderer::Init()
    {
        //// Allocate fence
        vk::FenceCreateInfo FenceInfo = {};

        if(
            auto FenceResult = VkContext.Device->createFenceUnique(FenceInfo);
            FenceResult.result == vk::Result::eSuccess
        )
        {
            VkState.Fence = std::move(FenceResult.value);
        }

        //// Allocate command buffer
        vk::CommandBufferAllocateInfo CommandBufferInfo = {};
        CommandBufferInfo.commandPool = VkContext.CommandPool.get();
        CommandBufferInfo.commandBufferCount = 1;
        CommandBufferInfo.level = vk::CommandBufferLevel::ePrimary;

        if(
            auto CommandBufferResult = VkContext.Device->allocateCommandBuffersUnique(CommandBufferInfo);
            CommandBufferResult.result == vk::Result::eSuccess
        )
        {
            VkState.CommandBuffer = std::move(CommandBufferResult.value[0]);
        }
        else
        {
            // Error allocating command buffer
        }

        //// Create staging buffer
        {
            vk::BufferCreateInfo StagingBufferInfo = {};
            StagingBufferInfo.size = VulkanState::StagingBufferSize;
            StagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc;
            StagingBufferInfo.sharingMode = vk::SharingMode::eExclusive;

            if(
                auto BufferResult = VkContext.Device->createBufferUnique(StagingBufferInfo);
                BufferResult.result == vk::Result::eSuccess
            )
            {
                VkState.StagingBuffer = std::move(BufferResult.value);
            }
            else
            {
                // Error creating staging buffer
                return false;
            }
        }

        //// Allocate Staging buffer
        {
            const vk::MemoryRequirements MemoryRequirements = VkContext.Device->getBufferMemoryRequirements(
                VkState.StagingBuffer.get()
            );
            const int32_t BufferMemoryIndex = Vulkan::FindMemoryTypeIndex(
                VkContext.PhysicalDevice,
                MemoryRequirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eHostVisible |
                vk::MemoryPropertyFlagBits::eHostCoherent |
                vk::MemoryPropertyFlagBits::eHostCached
            );

            if (BufferMemoryIndex < 0)
            {
                // Unable to find a suitable memory index for this buffer
                return false;
            }

            vk::MemoryAllocateInfo AllocationInfo = {};
            AllocationInfo.allocationSize = MemoryRequirements.size;
            AllocationInfo.memoryTypeIndex = BufferMemoryIndex;

            if(
                auto AllocationResult = VkContext.Device->allocateMemoryUnique(AllocationInfo);
                AllocationResult.result == vk::Result::eSuccess
            )
            {
                VkState.StagingBufferMemory = std::move(AllocationResult.value);
            }
            else
            {
                // Failed to allocate staging memory
                return false;
            }

            // Bind Buffer to memory

            if(
                auto BindResult = VkContext.Device->bindBufferMemory(VkState.StagingBuffer.get(), VkState.StagingBufferMemory.get(), 0);
                BindResult == vk::Result::eSuccess
            )
            {
                // Successfully binded buffer to memory
            }
            else
            {
                // Error binding staging buffer to staging memory
                return false;
            }
        }

        // Map staging buffer to host-visible pointer
        if(
            auto MapResult = VkContext.Device->mapMemory(VkState.StagingBufferMemory.get(), 0, VK_WHOLE_SIZE);
            MapResult.result == vk::Result::eSuccess
        )
        {
            VkState.MappedStagingBuffer = reinterpret_cast<decltype(VkState.MappedStagingBuffer)>(MapResult.value);
        }
        else
        {
            // Error mapping staging buffer
            return false;
        }

        //// Create main framebuffer
        {
            vk::ImageCreateInfo FrameColorInfo = {};
            FrameColorInfo.imageType = vk::ImageType::e2D;
            FrameColorInfo.format = vk::Format::eR8G8B8A8Unorm;
            FrameColorInfo.extent.width = 256;
            FrameColorInfo.extent.height = 192;
            FrameColorInfo.extent.depth = 1;
            FrameColorInfo.mipLevels = 1;
            FrameColorInfo.arrayLayers = 1;
            FrameColorInfo.samples = vk::SampleCountFlagBits::e1;
            FrameColorInfo.tiling = vk::ImageTiling::eOptimal;
            FrameColorInfo.initialLayout = vk::ImageLayout::eUndefined;
            FrameColorInfo.sharingMode = vk::SharingMode::eExclusive;
            FrameColorInfo.usage =
                vk::ImageUsageFlagBits::eTransferDst |
                vk::ImageUsageFlagBits::eTransferSrc |
                vk::ImageUsageFlagBits::eColorAttachment;

            if(
                auto ImageResult = VkContext.Device->createImageUnique(FrameColorInfo);
                ImageResult.result == vk::Result::eSuccess
            )
            {
                VkState.FrameColorImage = std::move(ImageResult.value);
            }
            else
            {
                // Error creating color framebuffer
                return false;
            }
        }

        // Allocate main framebuffer image memory
        {
            const vk::MemoryRequirements MemoryRequirements = VkContext.Device->getImageMemoryRequirements(
                VkState.FrameColorImage.get()
            );
            const int32_t ImageMemoryIndex = Vulkan::FindMemoryTypeIndex(
                VkContext.PhysicalDevice,
                MemoryRequirements.memoryTypeBits,
                vk::MemoryPropertyFlagBits::eDeviceLocal
            );

            if (ImageMemoryIndex < 0)
            {
                // Unable to find a suitable memory index for this buffer
                return false;
            }

            vk::MemoryAllocateInfo AllocationInfo = {};
            AllocationInfo.allocationSize = MemoryRequirements.size;
            AllocationInfo.memoryTypeIndex = ImageMemoryIndex;

            if(
                auto AllocationResult = VkContext.Device->allocateMemoryUnique(AllocationInfo);
                AllocationResult.result == vk::Result::eSuccess
            )
            {
                VkState.FrameColorMemory = std::move(AllocationResult.value);
            }
            else
            {
                // Failed to allocate staging memory
                return false;
            }

            // Bind Image to memory

            if(
                auto BindResult = VkContext.Device->bindImageMemory(VkState.FrameColorImage.get(), VkState.FrameColorMemory.get(), 0);
                BindResult == vk::Result::eSuccess
            )
            {
                // Successfully binded frame color image to memory
            }
            else
            {
                // Error binding frame color image to staging memory
                return false;
            }
        }

        puts("Vulkan Renderer Init");

        return true;
    }

    void VulkanRenderer::DeInit()
    {
        VkState = std::move(VulkanState{});
        puts("Vulkan Renderer DeInit");
    }

    void VulkanRenderer::Reset()
    {

    }

    void VulkanRenderer::SetRenderSettings(GPU::RenderSettings& settings)
    {

    }

    void VulkanRenderer::RenderFrame()
    {
        VkState.CommandBuffer->reset(vk::CommandBufferResetFlagBits::eReleaseResources);

        vk::CommandBufferBeginInfo BeginInfo = {};
        BeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        if(
            auto BeginResult = VkState.CommandBuffer->begin(BeginInfo);
            BeginResult == vk::Result::eSuccess
        )
        {
        }
        else
        {
            // Failed to begin command buffer recording
            return;
        }

        // Transition to write
        VkState.CommandBuffer->pipelineBarrier(
            vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer,
            vk::DependencyFlagBits::eByRegion,
            {},
            {},
            {
                vk::ImageMemoryBarrier(
                    vk::AccessFlags(),
                    vk::AccessFlagBits::eTransferWrite,
                    vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    VkState.FrameColorImage.get(),
                    vk::ImageSubresourceRange(
                        vk::ImageAspectFlagBits::eColor,
                        0, VK_REMAINING_MIP_LEVELS,
                        0, VK_REMAINING_ARRAY_LAYERS
                    )
                )
            }
        );

        // Clear framebuffer
        // Test DISP3DCNT register bit 14 for Rear-Plane mode
        if (RenderDispCnt & (1<<14))
        {
            // Bitmap clear
            // Texture slot 2 and 3 are used as 256x2566 bitmap textures
            // that are blit into the rear-plane of the depth and depth/fog buffer
            const u8 xoff = (RenderClearAttr2 >> 16) & 0xFF;
            const u8 yoff = (RenderClearAttr2 >> 24) & 0xFF;
            // Todo
        }
        else
        {
            // Simple clear
            vk::ClearColorValue ClearColor = {};
            {
                u32 ClearR = (RenderClearAttr1 >>  0) & 0x1F;
                u32 ClearG = (RenderClearAttr1 >>  5) & 0x1F;
                u32 ClearB = (RenderClearAttr1 >> 10) & 0x1F;
                u32 ClearA = (RenderClearAttr1 >> 16) & 0x1F;
                ClearColor.float32[0] = ClearR / float(0x1F);
                ClearColor.float32[1] = ClearG / float(0x1F);
                ClearColor.float32[2] = ClearB / float(0x1F);
                ClearColor.float32[3] = ClearA / float(0x1F);
            }

            VkState.CommandBuffer->clearColorImage(
                VkState.FrameColorImage.get(),
                vk::ImageLayout::eGeneral,
                ClearColor,
                {
                    vk::ImageSubresourceRange(
                        vk::ImageAspectFlagBits::eColor,
                        0, VK_REMAINING_MIP_LEVELS,
                        0, VK_REMAINING_ARRAY_LAYERS
                    )
                }
            );
        }

        // Transition to read
        VkState.CommandBuffer->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
            vk::DependencyFlagBits::eByRegion,
            {},
            {},
            {
                vk::ImageMemoryBarrier(
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eTransferRead,
                    vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    VkState.FrameColorImage.get(),
                    vk::ImageSubresourceRange(
                        vk::ImageAspectFlagBits::eColor,
                        0, VK_REMAINING_MIP_LEVELS,
                        0, VK_REMAINING_ARRAY_LAYERS
                    )
                )
            }
        );

        // Upload to staging buffer
        VkState.CommandBuffer->copyImageToBuffer(
            VkState.FrameColorImage.get(), vk::ImageLayout::eTransferSrcOptimal,
            VkState.StagingBuffer.get(),
            {
                vk::BufferImageCopy(
                    0, 256, 192,
                    vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                    vk::Offset3D(),vk::Extent3D(256, 192, 1)
                )
            }
        );


        // Ensure all writes finished before host reads
        VkState.CommandBuffer->pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
            vk::DependencyFlagBits::eByRegion,
            {},
            {},
            {
                vk::ImageMemoryBarrier(
                    vk::AccessFlagBits::eTransferWrite,
                    vk::AccessFlagBits::eHostRead,
                    vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eTransferSrcOptimal,
                    VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
                    VkState.FrameColorImage.get(),
                    vk::ImageSubresourceRange(
                        vk::ImageAspectFlagBits::eColor,
                        0, VK_REMAINING_MIP_LEVELS,
                        0, VK_REMAINING_ARRAY_LAYERS
                    )
                )
            }
        );

        if (VkState.CommandBuffer->end() != vk::Result::eSuccess)
        {
            // Error ending command buffer recording
            return;
        }

        // Submit command buffer
        vk::SubmitInfo SubmitInfo = {};
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &VkState.CommandBuffer.get();

        // Reset fence
        if(VkContext.Device->resetFences({VkState.Fence.get()}) != vk::Result::eSuccess)
        {
            // Error resetting fence
            return;
        }

        // Submit command buffer
        if(
            auto SubmitResult = VkContext.Queue.submit({SubmitInfo}, VkState.Fence.get());
            SubmitResult == vk::Result::eSuccess
        )
        {
        }
        else
        {
            // Error submitting fence
            return;
        }

        // Wait for fence to be signaled+
        if (VkContext.Device->waitForFences({VkState.Fence.get()}, true, ~0ull) != vk::Result::eSuccess)
        {
            // Error waiting for fences
            return;
        }
    }

    u32* VulkanRenderer::GetLine(int line)
    {
        u32* row = reinterpret_cast<u32*>(VkState.MappedStagingBuffer) + 256 * line;
        // R8G8B8A8 to R5G5B5A5
        for (size_t i = 0; i < 256; ++i)
        {
            row[i] &= 0b11111000'11111000'11111000'11111000;
            row[i] >>= 2;
        }
        return row;
    }
}
