// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

WorldUtils::WorldUtils()
{
}

WorldUtils::~WorldUtils()
{
	Done();
}

void WorldUtils::Init()
{
	VERUS_INIT();

	VERUS_QREF_RENDERER;

	renderer.GetDS().Load();
}

void WorldUtils::Done()
{
	VERUS_DONE(WorldUtils);
}
