// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#include "Macros.h"
#include "Typedef.h"
#include "EnumClass.h"
#include "AllocatorAware.h"
#include "Singleton.h"
#include "QuickRefs.h"

namespace verus::Math
{
	// Minimum & maximum:
	template<typename T>
	T Min(T a, T b)
	{
		return std::min(a, b);
	}
	template<typename T>
	T Max(T a, T b)
	{
		return std::max(a, b);
	}

	// Clamp:
	template<typename T>
	T Clamp(T x, T min, T max)
	{
		return std::clamp(x, min, max);
	}

	// Memory alignment:
	template<typename T>
	T AlignUp(T value, T alignment)
	{
		return (value + alignment - 1) / alignment * alignment;
	}
	template<typename T>
	T AlignDown(T value, T alignment)
	{
		return value / alignment * alignment;
	}
	template<typename T>
	T DivideByMultiple(T value, T alignment)
	{
		return (value + alignment - 1) / alignment;
	}
}
