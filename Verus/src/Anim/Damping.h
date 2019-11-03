#pragma once

namespace verus
{
	namespace Anim
	{
		//! Change value smoothly across multiple frames.
		template<typename T, bool angle = false>
		class Damping
		{
			T     _target = 0;
			T     _actual = 0;
			float _speed = 1;
			float _rate = 1;

		public:
			Damping() { _rate = log(10.f); }
			Damping(const T& x) : _target(x), _actual(x) { _rate = log(10.f); }
			Damping(const T& x, float speed) : _target(x), _actual(x), _speed(speed) { _rate = log(10.f); }

			void operator=(const T& x) { _target = x; }
			operator const T& () const { return _actual; }

			const T& GetTarget() const { return _target; }
			void SetActual(const T& x) { _actual = x; }
			void SetSpeed(float s) { _speed = s; }
			void ForceTarget() { _actual = _target; }
			void ForceTarget(const T& x) { _actual = _target = x; }
			void Update()
			{
				if (angle)
				{
					VERUS_RT_ASSERT(_target >= T(-VERUS_PI) && _target < T(VERUS_PI));
					VERUS_RT_ASSERT(_actual >= T(-VERUS_PI) && _actual < T(VERUS_PI));

					const T d = _target - _actual;
					if (d > T(VERUS_PI))
						_actual += VERUS_2PI;
					else if (d < T(-VERUS_PI))
						_actual -= VERUS_2PI;
				}
				if (0 == _speed)
				{
					_actual = _target;
				}
				else
				{
					VERUS_QREF_TIMER;
					const float ratio = 1 / exp(_speed * dt * _rate);
					_actual = Math::Lerp(_target, _actual, ratio);
					if (angle)
						_actual = Math::WrapAngle(_actual);
				}
			}
		};

		//! See Damping.
		template<>
		class Damping<Point3, false>
		{
			typedef Point3 T;

			T     _target = 0;
			T     _actual = 0;
			float _speed = 1;
			float _rate = 1;

		public:
			Damping() { _rate = log(10.f); }
			Damping(const T& x) : _target(x), _actual(x) { _rate = log(10.f); }
			Damping(const T& x, float speed) : _target(x), _actual(x), _speed(speed) { _rate = log(10.f); }

			void operator=(const T& x) { _target = x; }
			operator const T& () const { return _actual; }

			const T& GetTarget() const { return _target; }
			void SetActual(const T& x) { _actual = x; }
			void SetSpeed(float s) { _speed = s; }
			void ForceTarget() { _actual = _target; }
			void ForceTarget(const T& x) { _actual = _target = x; }
			void Update()
			{
				if (0 == _speed)
				{
					_actual = _target;
				}
				else
				{
					VERUS_QREF_TIMER;
					const float ratio = 1 / exp(_speed * dt * _rate);
					_actual = VMath::lerp(ratio, _target, _actual);
				}
			}
		};
	}
}
