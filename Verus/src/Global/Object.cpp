#include "verus.h"

using namespace verus;

Object::Object()
{
	_flags = 0;
}

Object::~Object()
{
	VERUS_RT_ASSERT(!IsInitialized());
}

void Object::Init()
{
	VERUS_RT_ASSERT(!IsInitialized());
	SetFlag(ObjectFlags::init);
}

void Object::Done()
{
	_flags = 0;
}

#ifdef VERUS_DEBUG
void Object::UpdateOnceCheck()
{
	const UINT32 numFrames = CGL::CRender::I().GetNumFrames();
	VERUS_RT_ASSERT(_updateOnceFrame <= numFrames);
	_updateOnceFrame = numFrames + 1;
}

void Object::UpdateOnceCheckDraw()
{
	const UINT32 numFrames = CGL::CRender::I().GetNumFrames();
	VERUS_RT_ASSERT(_updateOnceFrame > numFrames);
}
#endif
