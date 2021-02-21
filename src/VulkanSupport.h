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

#include "types.h"

#define VULKAN_HPP_NO_EXCEPTIONS

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

namespace Vulkan
{
enum class VendorID : u32
{
    AMD      = 0x1002,
    ImgTec   = 0x1010,
    Nvidia   = 0x10DE,
    ARM      = 0x13B5,
    Qualcomm = 0x5143,
    Intel    = 0x8086
};

const char* VendorName(VendorID Vendor);

int32_t FindMemoryTypeIndex(
    const vk::PhysicalDevice& PhysicalDevice,
    uint32_t MemoryTypeMask,
    vk::MemoryPropertyFlags Properties,
    vk::MemoryPropertyFlags ExcludeProperties = vk::MemoryPropertyFlagBits::eProtected
);
}
