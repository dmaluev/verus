// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class StateMachine;

		class State : public Input::InputFocus
		{
			StateMachine* _pStateMachine = nullptr;

		public:
			StateMachine* GetStateMachine() const { return _pStateMachine; }
			StateMachine* SetStateMachine(StateMachine* p) { return Utils::Swap(_pStateMachine, p); }

			virtual bool IsValidNextState(State* p);
			virtual void OnEnter(State* pPrev);
			virtual void InputFocus_HandleInput() override;
			virtual void HandleActions();
			virtual void Update();
			virtual void Draw();
			virtual void DrawView(CGI::RcViewDesc viewDesc);
			virtual void OnExit(State* pNext);
		};
		VERUS_TYPEDEFS(State);
	}
}
