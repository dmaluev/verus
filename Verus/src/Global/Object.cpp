// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

Object::Object()
{
	_flags = 0;
}

Object::Object(const Object& that) :
	_flags(that._flags.load())
{
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

#ifdef VERUS_RELEASE_DEBUG
void Object::UpdateOnceCheck()
{
	const UINT64 frameCount = CGI::Renderer::I().GetFrameCount();
	VERUS_RT_ASSERT(_updateOnceFrame <= frameCount);
	_updateOnceFrame = frameCount + 1;
}

void Object::UpdateOnceCheckDraw()
{
	const UINT64 frameCount = CGI::Renderer::I().GetFrameCount();
	VERUS_RT_ASSERT(_updateOnceFrame > frameCount);
}
#endif
