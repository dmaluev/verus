// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

ActiveMechanics::ActiveMechanics()
{
}

ActiveMechanics::~ActiveMechanics()
{
}

void ActiveMechanics::Init()
{
	VERUS_INIT();

	Reset();
}

void ActiveMechanics::Done()
{
	VERUS_DONE(ActiveMechanics);
}

bool ActiveMechanics::InputFocus_Veto()
{
	for (auto p : _vStack)
	{
		if (p->VetoInputFocus())
			return true;
	}
	return false;
}

void ActiveMechanics::InputFocus_HandleInput()
{
	for (auto p : _vStack)
	{
		p->HandleInput();
		if (p->VetoInputFocus())
			break;
	}
}

bool ActiveMechanics::Update()
{
	bool popped = false;
	VERUS_WHILE(Vector<PMechanics>, _vStack, it)
	{
		if ((*it)->CanAutoPop())
		{
			popped = true;
			(*it)->OnEnd();
			it = _vStack.erase(it);
		}
		else
			it++;
	}
	if (popped)
	{
		std::sort(_vStack.begin(), _vStack.end());
		ActiveMechanics_OnChanged();
	}
	for (auto p : _vStack)
	{
		if (Continue::no == p->Update())
			return true;
	}
	return false;
}

bool ActiveMechanics::Draw()
{
	for (auto p : _vStack)
	{
		if (Continue::no == p->Draw())
			return true;
	}
	return false;
}

bool ActiveMechanics::DrawOverlay()
{
	for (auto p : _vStack)
	{
		if (Continue::no == p->DrawOverlay())
			return true;
	}
	return false;
}

void ActiveMechanics::ApplyReport(const void* pReport)
{
	for (auto p : _vStack)
		p->ApplyReport(pReport);
}

bool ActiveMechanics::GetBotDomainCenter(int id, RPoint3 center)
{
	for (auto p : _vStack)
	{
		if (Continue::no == p->GetBotDomainCenter(id, center))
			return true;
	}
	return false;
}

bool ActiveMechanics::GetSpawnPosition(int id, RPoint3 pos)
{
	for (auto p : _vStack)
	{
		if (Continue::no == p->GetSpawnPosition(id, pos))
			return true;
	}
	return false;
}

bool ActiveMechanics::IsDefaultInputEnabled()
{
	for (auto p : _vStack)
	{
		if (!p->IsDefaultInputEnabled())
			return false;
	}
	return true;
}

bool ActiveMechanics::OnDie(int id)
{
	for (auto p : _vStack)
	{
		if (Continue::no == p->OnDie(id))
			return true;
	}
	return false;
}

bool ActiveMechanics::OnMouseMove(float x, float y)
{
	for (auto p : _vStack)
	{
		if (Continue::no == p->OnMouseMove(x, y))
			return true;
	}
	return false;
}

bool ActiveMechanics::OnTakeDamage(int id, float amount)
{
	for (auto p : _vStack)
	{
		if (Continue::no == p->OnTakeDamage(id, amount))
			return true;
	}
	return false;
}

bool ActiveMechanics::UpdateMultiplayer()
{
	for (auto p : _vStack)
	{
		if (Continue::no == p->UpdateMultiplayer())
			return true;
	}
	return false;
}

Scene::PMainCamera ActiveMechanics::GetMainCamera()
{
	for (auto p : _vStack)
	{
		Scene::PMainCamera pCamera = p->GetMainCamera();
		if (pCamera)
			return pCamera;
	}
	return nullptr;
}

void ActiveMechanics::Reset()
{
	for (auto& p : _vStack)
		p->OnEnd();
	_vStack.clear();
	ActiveMechanics_OnChanged();
}

bool ActiveMechanics::Push(RMechanics mech)
{
	if (IsActive(mech))
		return false;
	_vStack.push_back(&mech);
	std::sort(_vStack.begin(), _vStack.end());
	mech.OnBegin();
	ActiveMechanics_OnChanged();
	return true;
}

bool ActiveMechanics::Pop(RMechanics mech)
{
	const auto it = std::remove(_vStack.begin(), _vStack.end(), &mech);
	const bool ret = it != _vStack.end();
	if (ret)
	{
		_vStack.erase(it, _vStack.end());
		std::sort(_vStack.begin(), _vStack.end());
		mech.OnEnd();
		ActiveMechanics_OnChanged();
	}
	return ret;
}

bool ActiveMechanics::IsActive(RcMechanics mech) const
{
	return _vStack.end() != std::find(_vStack.begin(), _vStack.end(), &mech);
}
