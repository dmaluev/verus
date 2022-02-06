// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Cooldown
	{
		float _baseInterval = 0;
		float _actualInterval = 0;
		float _deadline = 0;

	public:
		Cooldown(float interval);

		bool IsFinished(float after = 0) const;
		bool IsInProgress(float partFinished = 1) const;
		void Start(float interval = -1);
		void Reset() { _deadline = 0; }
		void SetBaseInterval(float x) { _baseInterval = x; }
		float GetFinishedRatio() const;
	};
	VERUS_TYPEDEFS(Cooldown);
}
