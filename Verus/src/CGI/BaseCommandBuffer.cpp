// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::CGI;

// BaseCommandBuffer:

void BaseCommandBuffer::SetViewportAndScissor(ViewportScissorFlags vsf, int width, int height)
{
	VERUS_QREF_CONST_SETTINGS;
	if (vsf & ViewportScissorFlags::setAllForFramebuffer)
	{
		const Vector4 rc(0, 0, static_cast<float>(width), static_cast<float>(height));
		if (vsf & ViewportScissorFlags::setViewportForFramebuffer)
			SetViewport({ rc });
		if (vsf & ViewportScissorFlags::setScissorForFramebuffer)
			SetScissor({ rc });
		_viewScaleBias = Vector4(1, 1, 0, 0);
	}
	if (vsf & ViewportScissorFlags::setAllForCurrentView)
	{
		float scale = 1;
		if (vsf & ViewportScissorFlags::applyOffscreenScale)
			scale *= settings.GetScale();
		if (vsf & ViewportScissorFlags::applyHalfScale)
			scale *= 0.5f;
		VERUS_QREF_RENDERER;
		const Vector4 rc(
			static_cast<float>(static_cast<int>(0.5f + scale * renderer.GetCurrentViewX())),
			static_cast<float>(static_cast<int>(0.5f + scale * renderer.GetCurrentViewY())),
			static_cast<float>(static_cast<int>(0.5f + scale * renderer.GetCurrentViewWidth())),
			static_cast<float>(static_cast<int>(0.5f + scale * renderer.GetCurrentViewHeight())));
		if (vsf & ViewportScissorFlags::setViewportForCurrentView)
			SetViewport({ rc });
		if (vsf & ViewportScissorFlags::setScissorForCurrentView)
			SetScissor({ rc });
		const float scaleX = 1.f / width;
		const float scaleY = 1.f / height;
		_viewScaleBias = Vector4(
			rc.getZ() * scaleX,
			rc.getW() * scaleY,
			rc.getX() * scaleX,
			rc.getY() * scaleY);
	}
}

// CommandBufferPtr:

void CommandBufferPtr::Init()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertCommandBuffer();
	_p->Init();
}

void CommandBufferPtr::InitOneTimeSubmit()
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertCommandBuffer();
	_p->InitOneTimeSubmit();
}

void CommandBufferPwn::Done()
{
	if (_p)
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteCommandBuffer(_p);
		_p = nullptr;
	}
}
