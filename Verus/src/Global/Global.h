#pragma once

#define VERUS_MEMORY_ALIGNMENT 16

#include "BaseAllocator.h"
#include "AlignedAllocator.h"
#include "STL.h"
#include "Store.h"
#include "Blob.h"
#include "Random.h"
#include "Utils.h"
#include "Parallel.h"
#include "Range.h"
#include "Object.h"
#include "Lockable.h"
#include "Linear.h"
#include "Str.h"
#include "Convert.h"
#include "Timer.h"
#include "EngineInit.h"
#include "GlobalVarsClipboard.h"

namespace verus
{
	void Make_Global();
	void Free_Global();
}
