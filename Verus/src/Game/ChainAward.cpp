// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Game;

ChainAward::ChainAward(float maxInterval, int maxLevel) :
	_maxInterval(maxInterval),
	_maxLevel(maxLevel)
{
	Reset();
}

ChainAward::~ChainAward()
{
}

int ChainAward::Score()
{
	VERUS_QREF_TIMER;
	const float now = timer.GetTime();
	if (now - _lastTime < _maxInterval)
	{
		_lastLevel++;
		if (_lastLevel > _maxLevel)
			_lastLevel = _maxLevel;
	}
	else
	{
		_lastLevel = 0;
	}
	_lastTime = now;
	return _lastLevel;
}

void ChainAward::Reset()
{
	_lastTime = -_maxInterval;
	_lastLevel = 0;
}
