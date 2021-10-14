// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class StateMachine : public Object
		{
			PState _pCurrentState = nullptr;
			PState _pRequestedState = nullptr;
			UINT64 _prevFrame = UINT64_MAX;
			bool   _changed = false;

		public:
			void Update();
			void Draw();

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
