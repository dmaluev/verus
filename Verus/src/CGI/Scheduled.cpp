// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::CGI;

void Scheduled::Schedule(INT64 extraFrameCount)
{
	const UINT64 addExtra = (extraFrameCount >= 0) ? extraFrameCount : (Utils::I().GetRandom().Next() & 0xFF);
	const UINT64 newDoneFrame = Renderer::I().GetFrameCount() + BaseRenderer::s_ringBufferSize + 1 + addExtra;
	_doneFrame = (UINT64_MAX == _doneFrame) ? newDoneFrame : Math::Max(_doneFrame, newDoneFrame);
	VERUS_QREF_RENDERER;
	renderer->Schedule(this);
}

void Scheduled::ForceScheduled()
{
	_doneFrame = 0;
	Scheduled_Update();
	VERUS_QREF_RENDERER;
	renderer->Unschedule(this);
}

bool Scheduled::IsScheduledAllowed()
{
	if (Renderer::I().GetFrameCount() < _doneFrame)
		return false;
	_doneFrame = UINT64_MAX;
	return true;
}
