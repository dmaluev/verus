#include "verus.h"

using namespace verus;

Cooldown::Cooldown(float interval) :
	_baseInterval(interval)
{
}

bool Cooldown::IsFinished(float after) const
{
	VERUS_QREF_TIMER;
	return _deadline <= timer.GetTime() + after;
}

bool Cooldown::IsInProgress(float partComplete) const
{
	VERUS_QREF_TIMER;
	const float t = timer.GetTime();
	const float startTime = _deadline - _actualInterval;
	if (t >= startTime && t < _deadline)
	{
		const float part = (t - startTime) / _actualInterval;
		return part >= partComplete;
	}
	return false;
}

void Cooldown::Start(float interval)
{
	VERUS_QREF_TIMER;
	_actualInterval = (interval >= 0) ? interval : _baseInterval;
	_deadline = timer.GetTime() + _actualInterval;
}
