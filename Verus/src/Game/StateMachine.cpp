// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

void StateMachine::HandleInput()
{
	PcState pCheck = _pCurrentState;
	_pCurrentState->SetStateMachine(this);
	_pCurrentState->HandleInput();
	_changed = (_pCurrentState != pCheck);
}

void StateMachine::Update()
{
	VERUS_UPDATE_ONCE_CHECK;

	PcState pCheck = _pCurrentState;
	_pCurrentState->SetStateMachine(this);
	_pCurrentState->Update();
	_changed = (_pCurrentState != pCheck);
}

void StateMachine::Draw()
{
	PcState pCheck = _pCurrentState;
	_pCurrentState->SetStateMachine(this);
	_pCurrentState->Draw();
	_changed = (_pCurrentState != pCheck);
}

PState StateMachine::GetCurrentState() const
{
	return _pCurrentState;
}

PState StateMachine::GetRequestedState() const
{
	return _pRequestedState;
}

bool StateMachine::CanEnterState(RState state, bool allowSameState)
{
	state.SetStateMachine(this);
	if (&state == _pCurrentState && !allowSameState)
		return false;
	return _pCurrentState ? _pCurrentState->IsValidNextState(&state) : true;
}

bool StateMachine::EnterState(RState state, bool allowSameState)
{
	state.SetStateMachine(this);
	if (!CanEnterState(state, allowSameState))
		return false;

	const UINT64 frame = CGI::Renderer::I().GetFrameCount();
	VERUS_RT_ASSERT(frame != _prevFrame); // Don't change state multiple times per frame!
	_prevFrame = frame;

	PState pPrevState = _pCurrentState;
	if (_pCurrentState)
	{
		PcState pCheck = _pCurrentState;
		_pCurrentState->OnExit(&state);
		VERUS_RT_ASSERT(pCheck == _pCurrentState); // Don't change state while changing state!
	}
	_pCurrentState = &state;
	{
		PcState pCheck = _pCurrentState;
		_pCurrentState->OnEnter(pPrevState);
		VERUS_RT_ASSERT(pCheck == _pCurrentState); // Don't change state while changing state!
	}

	_pRequestedState = nullptr;
	return true;
}

bool StateMachine::EnterRequestedState()
{
	if (_pRequestedState == _pCurrentState)
		_pRequestedState = nullptr;
	if (_pRequestedState)
		return EnterState(*_pRequestedState);
	else
		return false;
}

void StateMachine::ReenterState()
{
	if (_pCurrentState)
	{
		{
			PcState pCheck = _pCurrentState;
			_pCurrentState->OnExit(_pCurrentState);
			VERUS_RT_ASSERT(pCheck == _pCurrentState);
		}
		{
			PcState pCheck = _pCurrentState;
			_pCurrentState->OnEnter(_pCurrentState);
			VERUS_RT_ASSERT(pCheck == _pCurrentState);
		}
	}
}

void StateMachine::RequestState(RState state)
{
	_pRequestedState = &state;
}
