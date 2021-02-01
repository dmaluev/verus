// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// Trigger:

Trigger::Trigger()
{
	_type = NodeType::trigger;
}

Trigger::~Trigger()
{
	Done();
}

void Trigger::Init(RcDesc desc)
{
}

void Trigger::Done()
{
}

// TriggerPtr:

void TriggerPtr::Init(Trigger::RcDesc desc)
{
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(!_p);
	_p = sm.InsertTrigger();
	if (desc._node)
		_p->LoadXML(desc._node);
	else
		_p->Init(desc);
}

void TriggerPwn::Done()
{
	if (_p)
	{
		SceneManager::I().DeleteTrigger(_p);
		_p = nullptr;
	}
}
