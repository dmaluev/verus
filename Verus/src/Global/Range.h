#pragma once

namespace verus
{
	template<typename T>
	class Range
	{
	public:
		T _min;
		T _max;

		Range() : _min(0), _max(0) {}
		Range(T x) : _min(x), _max(x) {}
		Range(T mn, T mx) : _min(mn), _max(mx) { VERUS_RT_ASSERT(_min <= _max); }

		T GetRange() const { return _max - _min; }
		T GetRandomValue() const { return _min + Utils::I().GetRandom().NextFloat()*GetRange(); }
	};
}
