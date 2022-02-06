// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_ENABLE_COM_RELEASE_CHECK
#ifdef VERUS_ENABLE_COM_RELEASE_CHECK
#	define VERUS_COM_RELEASE_CHECK(p) {if(p) {const ULONG count = p->AddRef(); p->Release(); VERUS_RT_ASSERT(2 == count);}}
#else
#	define VERUS_COM_RELEASE_CHECK(p) {}
#endif

#include "Native.h"
#include "DescriptorHeap.h"
#include "RenderPass.h"
#include "GeometryD3D12.h"
#include "TextureD3D12.h"
#include "ShaderD3D12.h"
#include "PipelineD3D12.h"
#include "CommandBufferD3D12.h"
#include "RendererD3D12.h"
