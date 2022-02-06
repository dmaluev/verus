// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

Entity::Entity()
{
}

Entity::~Entity()
{
	Done();
}

void Entity::Init()
{
	if (_refCount)
		return;

	VERUS_INIT();
	_refCount = 1;
}

bool Entity::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		VERUS_DONE(Entity);
		return true;
	}
	return false;
}
