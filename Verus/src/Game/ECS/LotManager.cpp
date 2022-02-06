// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

LotManager::LotManager()
{
}

LotManager::~LotManager()
{
	Done();
}

void LotManager::Init()
{
	VERUS_INIT();
}

void LotManager::Done()
{
	VERUS_DONE(LotManager);
}

PLot LotManager::InsertLot(CSZ uid)
{
	return TStoreLots::Insert(uid);
}

PLot LotManager::FindLot(CSZ uid)
{
	return TStoreLots::Find(uid);
}

void LotManager::DeleteLot(CSZ uid)
{
	TStoreLots::Delete(uid);
}

void LotManager::DeleteAllLots()
{
	TStoreLots::DeleteAll();
}
