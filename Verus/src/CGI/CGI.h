#pragma once

namespace verus
{
	namespace CGI
	{
		enum class ClearFlags : int
		{
			color = (1 << 0),
			depth = (1 << 1),
			stencil = (1 << 2)
		};
	}
}

#include "BaseGeometry.h"
#include "BasePipeline.h"
#include "BaseShader.h"
#include "BaseTexture.h"
#include "BaseCommandBuffer.h"
#include "BaseRenderer.h"

#include "Renderer.h"

namespace verus
{
	void Make_CGI();
	void Free_CGI();
}
