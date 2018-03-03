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

void State::HandleInput()
{
}

void State::Update()
{
}

void State::OnExit(PState pNext)
{
}
