#pragma once

#define VERUS_CGI_MAX_RT 8

#include "Types.h"
#include "Formats.h"
#include "RenderPass.h"

#include "BaseGeometry.h"
#include "BaseShader.h"
#include "BasePipeline.h"
#include "BaseTexture.h"
#include "BaseCommandBuffer.h"
#include "BaseRenderer.h"

#include "Renderer.h"

namespace verus
{
	void Make_CGI();
	void Free_CGI();
}
