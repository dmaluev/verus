// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Range
	{
	public:
		class Iterator
		{
			int _value;

		public:
			Iterator(int x) : _value(x) {}

			int& operator*() { return _value; }
			Iterator operator++() { ++_value; return *this; }
			bool operator!=(const Iterator& that) const { return _value != that._value; }
		};

		int _begin;
		int _end;

		Range() : _begin(0), _end(0) {}
		Range(int x) : _begin(x), _end(x + 1) {}
		Range(int b, int e) : _begin(b), _end(e) { VERUS_RT_ASSERT(_begin <= _end); }

		int GetCount() const;
		int GetRandomValue() const;
		Range& OffsetBy(int x);

		Iterator begin() const;
		Iterator end() const;
	};
}
