// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_MEMORY_ALIGNMENT 16

#include "BaseAllocator.h"
#include "AlignedAllocator.h"
#include "STL.h"
#include "Store.h"
#include "BaseHandle.h"
#include "Blob.h"
#include "Random.h"
#include "Str.h"
#include "Utils.h"
#include "Parallel.h"
#include "Interval.h"
#include "Range.h"
#include "Object.h"
#include "Lockable.h"
#include "Linear.h"
#include "Convert.h"
#include "Timer.h"
#include "Cooldown.h"
#include "EngineInit.h"
#include "GlobalVarsClipboard.h"
#include "DifferenceVector.h"
#include "Pool.h"
#include "LocalPtr.h"
#include "BaseCircularBuffer.h"

namespace verus
{
	void Make_Global();
	void Free_Global();
}
