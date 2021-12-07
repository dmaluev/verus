// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

bool State::IsValidNextState(PState p)
{
	return true;
}

void State::OnEnter(PState pPrev)
{
}

void State::InputFocus_HandleInput()
{
}

void State::HandleActions()
{
}

void State::Update()
{
}

void State::Draw()
{
}

void State::OnExit(PState pNext)
{
}
