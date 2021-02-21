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

#pragma once

#include <array>

#include "GPU3D.h"
#include "Platform.h"

#include "VulkanSupport.h"

namespace GPU3D
{
class VulkanRenderer : public Renderer3D
{
public:
    VulkanRenderer();
    virtual ~VulkanRenderer() override;
    virtual bool Init() override;
    virtual void DeInit() override;
    virtual void Reset() override;

    virtual void SetRenderSettings(GPU::RenderSettings& settings) override;

    virtual void VCount144() override {};
    virtual void RenderFrame() override;
    virtual u32* GetLine(int line) override;
private:
    struct VulkanContext
    {
        vk::UniqueInstance Instance;
        vk::PhysicalDevice PhysicalDevice;
        vk::UniqueDevice Device;
        vk::Queue Queue;
        vk::UniqueCommandPool CommandPool;
    } VkContext;

    struct VulkanState
    {
        vk::UniqueFence Fence;
        vk::UniqueCommandBuffer CommandBuffer;

        static constexpr size_t StagingBufferSize = 256 * 192 * sizeof(uint32_t);
        vk::UniqueDeviceMemory StagingBufferMemory;
        vk::UniqueBuffer StagingBuffer;

        std::byte* MappedStagingBuffer;
        size_t StagingBufferWritePoint;

        vk::UniqueDeviceMemory FrameColorMemory;
        vk::UniqueImage FrameColorImage;

        vk::UniqueDeviceMemory FrameDepthStencilMemory;
        vk::UniqueImage FrameDepthStencilImage;
    } VkState;

};
}
