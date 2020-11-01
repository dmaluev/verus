// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	template<typename T>
	class Range
	{
	public:
		class Iterator
		{
			T _value;

		public:
			Iterator(T x) : _value(x) {}

			T& operator*() { return _value; }
			Iterator operator++() { ++_value; return *this; }
			bool operator!=(const Iterator& that) const { return _value != that._value; }
		};

		T _min;
		T _max;

		Range() : _min(0), _max(0) {}
		Range(T x) : _min(x), _max(x) {}
		Range(T mn, T mx) : _min(mn), _max(mx) { VERUS_RT_ASSERT(_min <= _max); }

		T GetRange() const { return _max - _min; }
		T GetRandomValue() const { return _min + Utils::I().GetRandom().NextFloat() * GetRange(); }

		Iterator begin() const { return _min; }
		Iterator end() const { return _max + 1; }
	};
}
