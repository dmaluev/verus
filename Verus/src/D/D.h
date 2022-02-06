// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_RELEASE_DEBUG

#ifdef VERUS_RELEASE_DEBUG
#	pragma message("VERUS_RELEASE_DEBUG is defined")
#endif

#include "AssertionRunTime.h"
#include "AssertionCompileTime.h"
#include "RuntimeError.h"
#include "Recoverable.h"
#include "Log.h"

namespace verus
{
	void Make_D();
	void Free_D();
}
