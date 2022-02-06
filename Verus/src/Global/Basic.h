// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#include "Macros.h"
#include "Typedef.h"
#include "EnumClass.h"
#include "QuickRefs.h"
#include "AllocatorAware.h"
#include "Singleton.h"

namespace verus
{
	namespace Math
	{
		// Clamp:
		template<typename T>
		T Clamp(T x, T mn, T mx)
		{
			if (x <= mn)
				return mn;
			else if (x >= mx)
				return mx;
			return x;
		}

		// Minimum & maximum:
		template<typename T>
		T Min(T a, T b)
		{
			return a < b ? a : b;
		}
		template<typename T>
		T Max(T a, T b)
		{
			return a > b ? a : b;
		}

		// Memory alignment:
		template<typename T>
		T AlignUp(T val, T align)
		{
			return (val + align - 1) / align * align;
		}
		template<typename T>
		T AlignDown(T val, T align)
		{
			return val / align * align;
		}
		template<typename T>
		T DivideByMultiple(T value, T alignment)
		{
			return (value + alignment - 1) / alignment;
		}
	}
}
