// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#include "Types.h"
#include "Formats.h"
#include "RenderPass.h"
#include "Scheduled.h"

#include "BaseGeometry.h"
#include "BaseTexture.h"
#include "BaseShader.h"
#include "BasePipeline.h"
#include "BaseCommandBuffer.h"
#include "BaseExtReality.h"
#include "BaseRenderer.h"

#include "TextureRAM.h"
#include "DebugDraw.h"
#include "DeferredShading.h"
#include "Renderer.h"
#include "RendererParser.h"
#include "DynamicBuffer.h"

namespace verus
{
	void Make_CGI();
	void Free_CGI();
}
