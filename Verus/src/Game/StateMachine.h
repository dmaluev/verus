#pragma once

namespace verus
{
	namespace Game
	{
		class StateMachine : public Object
		{
			PState _pCurrentState = nullptr;
			PState _pRequestedState = nullptr;
			UINT32 _prevFrame = -1;
			bool   _changed = false;

		public:
			void HandleInput();
			void Update();
			PState GetCurrentState() const;
			PState GetRequestedState() const;
			bool CanEnterState(RState state, bool allowSameState = false);
			bool EnterState(RState state, bool allowSameState = false);
			bool EnterRequestedState();
			void ReenterState();
			void RequestState(RState state);
			bool IsStateChanged() const { return _changed; }
		};
		VERUS_TYPEDEFS(StateMachine);
	}
}
