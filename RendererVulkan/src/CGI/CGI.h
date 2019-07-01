#pragma once

#define VERUS_VULKAN_DESTROY(vk, fn) {if (VK_NULL_HANDLE != vk) {fn; vk = VK_NULL_HANDLE;}}

#include "Native.h"

#include "GeometryVulkan.h"
#include "ShaderVulkan.h"
#include "PipelineVulkan.h"
#include "TextureVulkan.h"
#include "CommandBufferVulkan.h"
#include "RendererVulkan.h"
