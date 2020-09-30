#pragma once

namespace verus
{
	template<typename T>
	class Pool
	{
		Vector<T> _v;
		int       _next = 0;

	public:
		void Resize(int size)
		{
			_v.resize(size);
			_next = Math::Min(_next, static_cast<int>(_v.size()));
		}

		int Reserve()
		{
			_next = Math::Min(_next, static_cast<int>(_v.size()));
			VERUS_FOR(i, _v.size())
			{
				if (!_v[_next].IsReserved())
				{
					_v[_next].Reserve();
					return _next++;
				}
				_next = (_next + 1) % _v.size();
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
