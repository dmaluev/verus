// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

Lot::Lot()
{
}

Lot::~Lot()
{
	Done();
}

void Lot::Init()
{
	if (_refCount)
		return;

	VERUS_INIT();
	_refCount = 1;
}

bool Lot::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		VERUS_DONE(Lot);
		return true;
	}
	return false;
}

PEntity Lot::InsertEntity(CSZ uid)
{
	return TStoreEntities::Insert(uid);
}

PEntity Lot::FindEntity(CSZ uid)
{
	return TStoreEntities::Find(uid);
}

void Lot::DeleteEntity(CSZ uid)
{
	TStoreEntities::Delete(uid);
}

void Lot::DeleteAllEntities()
{
	TStoreEntities::DeleteAll();
}
