// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::AI;

TaskDriver::TaskDriver()
{
}

TaskDriver::~TaskDriver()
{
}

void TaskDriver::Update()
{
	VERUS_QREF_TIMER;
	VERUS_QREF_UTILS;

	if (_cooldownTimer > 0)
	{
		_cooldownTimer -= dt;
		if (_cooldownTimer < 0)
			_cooldownTimer = 0;
		else
			return;
	}

	RMode mode = _mapModes.find(_currentMode)->second;

	VERUS_FOREACH(TMapTasks, mode._mapTasks, it)
	{
		RTask task = it->second;
		task._time += dt;
		if (task._time >= task._deadline)
		{
			if (utils.GetRandom().NextFloat() * 100 < task._chance)
			{
				if (_pDelegate)
					_pDelegate->TaskDriver_OnTask(_C(task._name), _C(_currentMode));
				_cooldownTimer = _cooldown;
			}
			task._deadline = task._intervalMin + task._intervalSize * utils.GetRandom().NextFloat();
			task._time = 0;
		}
	}
}

void TaskDriver::AddMode(CSZ name)
{
	if (_mapModes.find(name) != _mapModes.end())
		return;
	Mode mode;
	mode._name = name;
	_mapModes[name] = mode;
	_currentMode = name;
}

void TaskDriver::AddTask(CSZ name, float chance, float intervalMin, float intervalMax, CSZ mode)
{
	VERUS_QREF_UTILS;

	RMode thisMode = _mapModes.find(mode ? mode : _currentMode)->second;

	Task task;
	task._name = name;
	task._chance = chance;
	task._intervalMin = intervalMin;
	task._intervalSize = intervalMax - intervalMin;
	task._deadline = task._intervalMin + task._intervalSize * utils.GetRandom().NextFloat();

	thisMode._mapTasks[name] = task;
}
