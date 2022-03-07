// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Timer : public Singleton<Timer>
	{
	public:
		enum class Type
		{
			game,
			gui,
			count
		};

	private:
		typedef std::chrono::high_resolution_clock::time_point TTimePoint;

		struct Countdown
		{
			float _secondsTotal = 0;
			float _secondsRemain = 0;
			bool  _active = false;
		};

		struct Data
		{
			TTimePoint _tpPrev;
			TTimePoint _tpPrev2;
			double     _t = 0;
			float      _dt = 0;
			float      _dtInv = 0;
			float      _dtSq = 0;
			float      _dtPrev = 0;
			float      _tcvValue = 0; // Time-Corrected Verlet.
			float      _smoothValue = 0;
			float      _gameSpeed = 1;
			bool       _pause = false;
		};

		TTimePoint        _tpInit;
		Vector<Countdown> _vCountdowns;
		Data              _data[static_cast<int>(Type::count)];

	public:
		Timer();
		~Timer();

		void Init();
		void Update();

		bool IsEventEvery(int ms) const;

		float GetTime() const;
		float GetAccumulatedTime(Type type = Type::game) const { return static_cast<float>(_data[+type]._t); }
		float GetDeltaTime(Type type = Type::game) const { return _data[+type]._dt; }
		float GetGameSpeed(Type type = Type::game) const { return _data[+type]._pause ? 0 : _data[+type]._gameSpeed; }
		void SetGameSpeed(float speed, Type type = Type::game) { _data[+type]._gameSpeed = speed; }

		// Extra:
		float GetDeltaTimeInv(Type type = Type::game) const { return _data[+type]._dtInv; }
		float GetDeltaTimeSq(Type type = Type::game) const { return _data[+type]._dtSq; }
		float GetPreviousDeltaTime(Type type = Type::game) const { return _data[+type]._dtPrev; }
		float GetVerletValue(Type type = Type::game) const { return _data[+type]._tcvValue; }
		float GetSmoothValue(Type type = Type::game) const { return _data[+type]._smoothValue; }

		void SetPause(bool pause, Type type = Type::game) { _data[+type]._pause = pause; }

		UINT32 GetTicks() const { return SDL_GetTicks(); }

		int InsertCountdown(float duration, int existingID = -1);
		void DeleteCountdown(int id);
		void DeleteAllCountdowns();
		bool GetCountdownData(int id, float& remain);

		static CSZ GetSingletonFailMessage() { return "Make_Utils(); // FAIL.\r\n"; }
	};
	VERUS_TYPEDEFS(Timer);

	class PerfTimer
	{
		typedef std::chrono::high_resolution_clock::time_point TTimePoint;

		TTimePoint _begin;

	public:
		void Begin();
		void End(CSZ func);
	};
	VERUS_TYPEDEFS(PerfTimer);
}

#define VERUS_PERF_BEGIN Utils::PerfTimer perfTimer; perfTimer.Begin();
#define VERUS_PERF_END   perfTimer.End(__FUNCTION__);
