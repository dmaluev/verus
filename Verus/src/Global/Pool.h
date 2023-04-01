// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	template<typename T>
	class Pool
	{
		Vector<T> _v;
		int       _next = 0;

		void IncrementNext()
		{
			_next = (_next + 1) % _v.size();
		}

	public:
		void Resize(int size)
		{
			_v.resize(size);
			_next = Math::Min(_next, static_cast<int>(_v.size()) - 1);
		}

		int Reserve()
		{
			_next = Math::Min(_next, static_cast<int>(_v.size()) - 1);
			VERUS_FOR(i, _v.size())
			{
				if (!_v[_next].IsReserved())
				{
					_v[_next].Reserve();
					const int ret = _next;
					IncrementNext();
					return ret;
				}
				IncrementNext();
			}
			return -1;
		}

		T& GetBlockAt(int index)
		{
			return _v[index];
		}

		template<typename TFn>
		void ForEachReserved(const TFn& fn)
		{
			for (auto& x : _v)
			{
				if (x.IsReserved())
					fn(x);
			}
		}
	};
}
