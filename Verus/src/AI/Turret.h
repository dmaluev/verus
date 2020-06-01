#pragma once

namespace verus
{
	namespace AI
	{
		class Turret
		{
			float _targetPitch = 0;
			float _targetYaw = 0;
			float _actualPitch = 0;
			float _actualYaw = 0;
			float _pitchSpeed = 1;
			float _yawSpeed = 1;

		public:
			void Update();

			float GetTargetPitch() const { return _targetPitch; }
			float GetTargetYaw() const { return _targetYaw; }
			float GetActualPitch() const { return _actualPitch; }
			float GetActualYaw() const { return _actualYaw; }

			bool IsTargetPitchReached(float threshold) const;
			bool IsTargetYawReached(float threshold) const;

			// Speed:
			float GetPitchSpeed()	const { return _pitchSpeed; }
			float GetYawSpeed()		const { return _yawSpeed; }
			void SetPitchSpeed(float x) { _pitchSpeed = x; }
			void SetYawSpeed(float x) { _yawSpeed = x; }

			void LookAt(RcVector3 rayFromTurret, bool instantly = false);
		};
		VERUS_TYPEDEFS(Turret);
	}
}
