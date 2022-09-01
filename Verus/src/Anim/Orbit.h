// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Anim
	{
		class Orbit
		{
			Matrix3              _matrix;
			Elastic<float, true> _pitch;
			Elastic<float, true> _yaw;
			float                _basePitch = 0;
			float                _baseYaw = 0;
			float                _speed = 1;
			float                _offsetStrength = 0;
			float                _maxPitch = 0.45f;
			bool                 _locked = false;

		public:
			Orbit();
			~Orbit();

			void Update(bool smoothRestore = true);

			void Lock();
			void Unlock();
			bool IsLocked() const { return _locked; }

			void AddPitch(float a);
			void AddYaw(float a);
			void AddPitchFree(float a);
			void AddYawFree(float a);

			float GetPitch() const { return _pitch; }
			float GetYaw() const { return _yaw; }

			RcMatrix3 GetMatrix() const { return _matrix; }

			float GetOffsetStrength() const { return _offsetStrength; }

			void SetSpeed(float x) { _speed = x; }
			void SetMaxPitch(float a) { _maxPitch = a; }
		};
		VERUS_TYPEDEFS(Orbit);
	}
}
