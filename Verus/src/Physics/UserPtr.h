// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Physics
	{
		class UserPtr
		{
		public:
			virtual int UserPtr_GetType() = 0;
		};
		VERUS_TYPEDEFS(UserPtr);
	}
}
