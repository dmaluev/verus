// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_ENABLE_COM_RELEASE_CHECK
#ifdef VERUS_ENABLE_COM_RELEASE_CHECK
#	define VERUS_COM_RELEASE_CHECK(p) {if(p) {const ULONG count = p->AddRef(); p->Release(); VERUS_RT_ASSERT(2 == count);}}
#else
#	define VERUS_COM_RELEASE_CHECK(p) {}
#endif

namespace verus
{
	inline void SetDebugObjectName(ID3D11DeviceChild* p, CSZ name)
	{
		if (p)
			p->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(strlen(name)), name);
	}
}

#include "Native.h"
#include "RenderPass.h"
#include "GeometryD3D11.h"
#include "TextureD3D11.h"
#include "ShaderD3D11.h"
#include "PipelineD3D11.h"
#include "CommandBufferD3D11.h"
#include "ExtRealityD3D11.h"
#include "RendererD3D11.h"
