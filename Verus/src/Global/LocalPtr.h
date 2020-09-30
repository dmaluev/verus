#pragma once

namespace verus
{
	template<typename T>
	class alignas(alignof(T)) LocalPtr
	{
		BYTE _data[sizeof(T)] = {};
		T* _p = nullptr;

	public:
		void operator=(T* p)
		{
			_p = p;
		}

		T* operator->() const
		{
			return _p;
		}
		T* Get()
		{
			return _p;
		}
		const T* Get() const
		{
			return _p;
		}

		BYTE* GetData()
		{
			return _data;
		}

		void Delete()
		{
			if (_p)
			{
				_p->~T();
				_p = nullptr;
			}
		}
	};
}
