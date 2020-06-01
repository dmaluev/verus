#pragma once

namespace verus
{
	namespace Game
	{
		class StateMachine;

		class State
		{
			StateMachine* _pStateMachine = nullptr;

		public:
			StateMachine* GetStateMachine() const { return _pStateMachine; }
			StateMachine* SetStateMachine(StateMachine* p) { return Utils::Swap(_pStateMachine, p); }

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
