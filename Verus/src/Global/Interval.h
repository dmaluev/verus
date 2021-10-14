// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Interval
	{
	public:
		float _min;
		float _max;

		Interval() : _min(0), _max(0) {}
		Interval(float x) : _min(x), _max(x) {}
		Interval(float mn, float mx) : _min(mn), _max(mx) { VERUS_RT_ASSERT(_min <= _max); }

		float GetLength() const;
		float GetRandomValue() const;
	};
}
