#include "verus.h"

using namespace verus;
using namespace verus::CGI;

void Scheduled::Schedule(INT64 frameCount)
{
	const INT64 add = (frameCount >= 0) ? frameCount : (Utils::I().GetRandom().Next() & 0xFF);
	const INT64 frame = Renderer::I().GetFrameCount() + BaseRenderer::s_ringBufferSize + add;
	_frame = Math::Max(_frame, frame);
	VERUS_QREF_RENDERER;
	renderer->Schedule(this);
}

void Scheduled::ForceScheduled()
{
	_frame = 0;
	Scheduled_Update();
	VERUS_QREF_RENDERER;
	renderer->Unschedule(this);
}

bool Scheduled::IsScheduledAllowed()
{
	if (-1 == _frame)
		return false;
	if (static_cast<INT64>(Renderer::I().GetFrameCount()) < _frame)
		return false;
	_frame = -1;
	return true;
}
