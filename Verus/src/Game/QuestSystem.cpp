// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

QuestSystem::QuestSystem()
{
}

QuestSystem::~QuestSystem()
{
	Done();
}

void QuestSystem::Init()
{
	VERUS_INIT();
}

void QuestSystem::Done()
{
	VERUS_DONE(QuestSystem);
}
