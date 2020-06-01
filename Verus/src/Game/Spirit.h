#pragma once

namespace verus
{
	namespace Game
	{
		class Spirit : public AllocatorAware
		{
		protected:
			static const float s_defaultMaxPitch;

			struct DerivedVars
			{
				Matrix3 _matPitch;
				Matrix3 _matYaw;
				Matrix3 _matLeanYaw;
				Matrix3 _matRot;
				Vector3 _frontDir = Vector3(0);
				Vector3 _frontDir2D = Vector3(0);
				Vector3 _sideDir = Vector3(0);
				Vector3 _sideDir2D = Vector3(0);
			};

			DerivedVars                _dv;
			Point3                     _position = Point3(0);
			Point3                     _prevPosition = Point3(0);
			Point3                     _remotePosition = Point3(0);
			Vector3                    _velocity = Vector3(0);
			Vector3                    _move = Vector3(0);
			Point3                     _smoothPrevPosition = Point3(0);
			Anim::Elastic<Point3>      _smoothPosition;
			Anim::Elastic<float, true> _pitch;
			Anim::Elastic<float, true> _yaw;
			Anim::Elastic<float>       _smoothSpeed;
			float                      _speed = 0;
			float                      _maxSpeed = 10;
			float                      _moveLen = 0;
			float                      _maxPitch = s_defaultMaxPitch;
			float                      _accel = 0;
			float                      _decel = 0;
			float                      _turnLeanStrength = 0;

		public:
			Spirit();
			virtual ~Spirit();

			void ComputeDerivedVars(float smoothSpeed = 0);

			// Move:
			void MoveFront(float x);
			void MoveSide(float x);
			void MoveFront2D(float x);
			void MoveSide2D(float x);
			void TurnPitch(float rad);
			void TurnYaw(float rad);
			void SetPitch(float rad);
			void SetYaw(float rad);

			void SetMaxPitch(float value = s_defaultMaxPitch) { _maxPitch = value; }
			float GetPitch() const { return _pitch; }
			float GetYaw() const { return _yaw; }

			virtual void HandleInput();
			virtual void Update();

			void SetAcceleration(float accel = 5, float decel = 5);
			float GetSpeed() const { return _speed; }
			float GetSmoothSpeed() const { return _smoothSpeed; }
			float GetMaxSpeed() const { return _maxSpeed; }
			void SetMaxSpeed(float v = 10) { _maxSpeed = v; }

			Point3       GetPosition(bool smooth = true);
			virtual void MoveTo(RcPoint3 pos);
			void         SetRemotePosition(RcPoint3 pos);
			bool         FitRemotePosition();

			RcVector3 GetVelocity() const { return _velocity; }
			void      SetVelocity(RcVector3 v) { _velocity = v; }

			RcVector3 GetFrontDirection() const { return _dv._frontDir; }
			RcVector3 GetSideDirection() const { return _dv._sideDir; }

			//! Rotates smoothly across multiple frames.
			void Rotate(RcVector3 front, float speed);
			void LookAt(RcPoint3 point);

			// Matrices:
			RcMatrix3 GetPitchMatrix() const;
			RcMatrix3 GetYawMatrix() const;
			RcMatrix3 GetRotationMatrix() const;
			Transform3 GetMatrix() const;
			Transform3 GetUprightMatrix() const;
			Transform3 GetUprightWithLeanMatrix() const;

			float GetMotionBlur() const;

			void SetTurnLeanStrength(float x) { _turnLeanStrength = x; }
		};
		VERUS_TYPEDEFS(Spirit);
	}
}
