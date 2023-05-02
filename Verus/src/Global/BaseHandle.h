// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	template<typename T>
	class BaseHandle
	{
		int _h = -1;

	protected:
		static T Make(int value)
		{
			T ret;
			ret._h = value;
			return ret;
		}

	public:
		int Get() const { return _h; }
		bool IsSet() const { return _h >= 0; }
	};
}
