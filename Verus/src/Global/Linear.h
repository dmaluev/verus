// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	template<typename T>
	class Linear
	{
		T _value;
		T _speed;
		T _min;
		T _max;

	public:
		Linear() : _value(0), _speed(0), _min(0), _max(1) {}

		T& GetValue() { return _value; }
		const T& GetValue() const { return _value; }

		T& GetSpeed() { return _speed; }
		const T& GetSpeed() const { return _speed; }

		void Set(const T& value, const T& speed)
		{
			_value = value;
			_speed = speed;
		}

		void SetValue(const T& value)
		{
			_value = value;
		}

		void SetSpeed(const T& speed)
		{
			_speed = speed;
		}

		void SetLimits(const T& mn, const T& mx)
		{
			_min = mn;
			_max = mx;
		}

		void Plan(const T& value, float time)
		{
			const T delta = value - _value;
			_speed = delta / time;
		}

		void SpinTo(const T& value, float speed)
		{
			if (value >= _value)
			{
				_speed = speed;
				_min = _value;
				_max = value;
			}
			else
			{
				_speed = -speed;
				_min = value;
				_max = _value;
			}
		}

		void Reverse()
		{
			_speed = -_speed;
			if (_speed >= 0)
			{
				_max = -_min;
				_min = _value;
			}
			else
			{
				_min = -_max;
				_max = _value;
			}
		}

		void UpdateUnlimited(float dt)
		{
			_value += _speed * dt;
		}

		void UpdateClamped(float dt)
		{
			_value += _speed * dt;
			_value = Math::Clamp(_value, _min, _max);
		}

		void UpdateClampedV(float dt)
		{
			_value += _speed * dt;
			_value = _value.Clamp(_min, _max);
		}

		void WrapFloatV()
		{
			_value = _value.Mod(1);
		}
	};
}
