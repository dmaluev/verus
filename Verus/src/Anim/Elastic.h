// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Anim
{
	template<typename T, bool angle = false>
	class Elastic
	{
		T     _target = 0;
		T     _actual = 0;
		float _speed = 1;
		float _rate = 1;

	public:
		Elastic() { _rate = log(10.f); }
		Elastic(const T& x) : _target(x), _actual(x) { _rate = log(10.f); }
		Elastic(const T& x, float speed) : _target(x), _actual(x), _speed(speed) { _rate = log(10.f); }

		void operator=(const T& x) { _target = x; }
		operator const T& () const { return _actual; }

		const T& GetTarget() const { return _target; }
		void SetActual(const T& x) { _actual = x; }
		void SetSpeed(float s) { _speed = s; }
		void ForceTarget() { _actual = _target; }
		void ForceTarget(const T& x) { _actual = _target = x; }
		float GetDelta() const { return angle ? Math::WrapAngle(_target - _actual) : _target - _actual; }
		void Update()
		{
			const T d = _target - _actual;
			if (angle)
			{
				VERUS_RT_ASSERT(_target >= T(-4) && _target < T(4));
				VERUS_RT_ASSERT(_actual >= T(-4) && _actual < T(4));
				if (d > T(VERUS_PI))
					_actual += VERUS_2PI;
				else if (d < T(-VERUS_PI))
					_actual -= VERUS_2PI;
			}
			if (abs(d) < VERUS_FLOAT_THRESHOLD)
			{
				_actual = _target;
			}
			else
			{
				VERUS_QREF_TIMER;
				const float ratio = 1 / exp(_speed * dt * _rate); // Exponential decay.
				_actual = Math::Lerp(_target, _actual, ratio);
				if (angle)
					_actual = Math::WrapAngle(_actual);
			}
		}
	};

	template<>
	class Elastic<Vector3, false>
	{
		typedef Vector3 T;

		T     _target = 0;
		T     _actual = 0;
		float _speed = 1;
		float _rate = 1;

	public:
		Elastic() { _rate = log(10.f); }
		Elastic(const T& x) : _target(x), _actual(x) { _rate = log(10.f); }
		Elastic(const T& x, float speed) : _target(x), _actual(x), _speed(speed) { _rate = log(10.f); }

		void operator=(const T& x) { _target = x; }
		operator const T& () const { return _actual; }

		const T& GetTarget() const { return _target; }
		void SetActual(const T& x) { _actual = x; }
		void SetSpeed(float s) { _speed = s; }
		void ForceTarget() { _actual = _target; }
		void ForceTarget(const T& x) { _actual = _target = x; }
		void Update()
		{
			const Vector3 d = _target - _actual;
			if (VMath::dot(d, d) < VERUS_FLOAT_THRESHOLD * VERUS_FLOAT_THRESHOLD)
			{
				_actual = _target;
			}
			else
			{
				VERUS_QREF_TIMER;
				const float ratio = 1 / exp(_speed * dt * _rate); // Exponential decay.
				_actual = VMath::lerp(ratio, _target, _actual);
			}
		}
	};

	template<>
	class Elastic<Point3, false>
	{
		typedef Point3 T;

		T     _target = 0;
		T     _actual = 0;
		float _speed = 1;
		float _rate = 1;

	public:
		Elastic() { _rate = log(10.f); }
		Elastic(const T& x) : _target(x), _actual(x) { _rate = log(10.f); }
		Elastic(const T& x, float speed) : _target(x), _actual(x), _speed(speed) { _rate = log(10.f); }

		void operator=(const T& x) { _target = x; }
		operator const T& () const { return _actual; }

		const T& GetTarget() const { return _target; }
		void SetActual(const T& x) { _actual = x; }
		void SetSpeed(float s) { _speed = s; }
		void ForceTarget() { _actual = _target; }
		void ForceTarget(const T& x) { _actual = _target = x; }
		void Update()
		{
			const Vector3 d = _target - _actual;
			if (VMath::dot(d, d) < VERUS_FLOAT_THRESHOLD * VERUS_FLOAT_THRESHOLD)
			{
				_actual = _target;
			}
			else
			{
				VERUS_QREF_TIMER;
				const float ratio = 1 / exp(_speed * dt * _rate); // Exponential decay.
				_actual = VMath::lerp(ratio, _target, _actual);
			}
		}
	};
}
