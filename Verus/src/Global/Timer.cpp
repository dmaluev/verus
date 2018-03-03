#include "verus.h"

using namespace verus;

Timer::Timer()
{
}

Timer::~Timer()
{
}

void Timer::Init()
{
	const TTimePoint timeNow = std::chrono::high_resolution_clock::now();
	VERUS_FOR(i, +Type::count)
	{
		_data[i]._t = _data[i]._dt = 0;
		_data[i]._timePrev = timeNow;
	}
}

void Timer::Update()
{
	const TTimePoint timeNow = std::chrono::high_resolution_clock::now();
	VERUS_FOR(i, +Type::count)
	{
		_data[i]._dtPrev = _data[i]._dt;

		_data[i]._dt = std::chrono::duration_cast<std::chrono::duration<float>>(
			timeNow - _data[i]._timePrev).count()*GetGameSpeed(static_cast<Type>(i));
		if (_data[i]._dt > 0.25f) // Prevent long gaps.
			_data[i]._dt = 0.25f;

		_data[i]._t += _data[i]._dt;
		_data[i]._timePrev2 = _data[i]._timePrev;
		_data[i]._timePrev = timeNow;

		_data[i]._dtInv = _data[i]._dt > 0.0001f ? 1 / _data[i]._dt : 10000;
		_data[i]._dtSq = _data[i]._dt*_data[i]._dt;
		_data[i]._tcvValue = _data[i]._dtPrev > 0.0001f ? _data[i]._dt / _data[i]._dtPrev : 1;

		_data[i]._smoothValue = Math::Clamp((1 / 30.f - _data[i]._dt) * 30, 0.f, 0.5f);
	}

	const int num = Utils::Cast32(_vCountdowns.size());
	VERUS_FOR(i, num)
	{
		if (_vCountdowns[i]._active && _vCountdowns[i]._secondsRemain > 0)
			_vCountdowns[i]._secondsRemain -= GetDeltaTime();
	}
}

bool Timer::IsEventEvery(int ms) const
{
	auto a = std::chrono::duration_cast<std::chrono::milliseconds>(_data[+Type::game]._timePrev.time_since_epoch());
	auto b = std::chrono::duration_cast<std::chrono::milliseconds>(_data[+Type::game]._timePrev2.time_since_epoch());
	return a.count() / ms != b.count() / ms;
}

int Timer::InsertCountdown(float duration, int existingID)
{
	const int num = Utils::Cast32(_vCountdowns.size());

	// Use existing:
	if (existingID >= 0 && existingID < num)
	{
		_vCountdowns[existingID]._active = true;
		_vCountdowns[existingID]._secondsRemain = duration;
		_vCountdowns[existingID]._secondsTotal = duration;
		return existingID;
	}

	// Find free one:
	VERUS_FOR(i, num)
	{
		if (!_vCountdowns[i]._active)
		{
			_vCountdowns[i]._active = true;
			_vCountdowns[i]._secondsRemain = duration;
			_vCountdowns[i]._secondsTotal = duration;
			return i;
		}
	}

	// Add new one:
	_vCountdowns.resize(num + 1);
	_vCountdowns[num]._active = true;
	_vCountdowns[num]._secondsRemain = duration;
	_vCountdowns[num]._secondsTotal = duration;
	return num;
}

void Timer::DeleteCountdown(int id)
{
	const int num = Utils::Cast32(_vCountdowns.size());
	if (id >= 0 && id < num)
		_vCountdowns[id]._active = false;
}

void Timer::DeleteAllCountdowns()
{
	_vCountdowns.clear();
}

bool Timer::GetCountdownData(int id, float& remain)
{
	if (id >= 0 && id < _vCountdowns.size())
	{
		if (_vCountdowns[id]._active)
		{
			if (_vCountdowns[id]._secondsRemain <= 0)
			{
				remain = 0;
				return true;
			}
			else
			{
				remain = _vCountdowns[id]._secondsRemain;
				return false;
			}
		}
		else
			return true;
	}
	return true;
}

// PerfTimer:

void PerfTimer::Begin()
{
	_begin = std::chrono::high_resolution_clock::now();
}

void PerfTimer::End(CSZ func)
{
	const double d = std::chrono::duration_cast<std::chrono::duration<double>>(
		std::chrono::high_resolution_clock::now() - _begin).count() * 1000;

	char info[400];
	sprintf_s(info, "[PerfTimer] End() func=%s d=%f", func, d);
	VERUS_LOG_DEBUG(info);
}
