#pragma once

namespace verus
{
	namespace Game
	{
		class StateMachine;

		struct State
		{
			StateMachine* _pStateMachine = nullptr;

			virtual bool IsValidNextState(State* p);
			virtual void OnEnter(State* pPrev);
			virtual void HandleInput();
			virtual void Update();
			virtual void Draw();
			virtual void OnExit(State* pNext);
		};
		VERUS_TYPEDEFS(State);
	}
}
