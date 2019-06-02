#pragma once

namespace verus
{
	namespace Game
	{
		class Spirit : public AllocatorAware
		{
			static const float s_defaultMaxPitch;

			struct DerivedVars
			{
				Matrix3 _matPitch;
				Matrix3 _matYaw;
				Matrix3 _matRot;
				Vector3 _dirFront = Vector3(0);
				Vector3 _dirFront2D = Vector3(0);
				Vector3 _dirSide = Vector3(0);
				Vector3 _dirSide2D = Vector3(0);
			};

			DerivedVars                _dv;
			Point3                     _position = Point3(0);
			Point3                     _positionPrev = Point3(0);
			Point3                     _positionRemote = Point3(0);
			Vector3                    _velocity = Vector3(0);
			Vector3                    _move = Vector3(0);
			Point3                     _smoothPositionPrev = Point3(0);
			Anim::Damping<Point3>      _smoothPosition;
			Anim::Damping<float, true> _pitch;
			Anim::Damping<float, true> _yaw;
			float                      _moveLen = 0;
			float                      _maxPitch = s_defaultMaxPitch;
			float                      _accel = 0;
			float                      _decel = 0;
			float                      _speed = 0;
			float                      _maxSpeed = 10;

		public:
			Spirit();
			virtual ~Spirit();

			VERUS_P(void ComputeDerivedVars());

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
			float GetMaxSpeed() const { return _maxSpeed; }
			void SetMaxSpeed(float v = 10) { _maxSpeed = v; }

			Point3 GetPosition(bool smooth = true);
			void   MoveTo(RcPoint3 pos, float onTerrain = -1);
			void   SetRemotePosition(RcPoint3 pos);
			bool   FitRemotePosition();

			RcVector3 GetVelocity() const { return _velocity; }
			void      SetVelocity(RcVector3 v) { _velocity = v; }

			RcVector3 GetDirectionFront() const { return _dv._dirFront; }
			RcVector3 GetDirectionSide() const { return _dv._dirSide; }

			//! Rotates smoothly across multiple frames.
			void Rotate(RcVector3 front, float speed);
			void LookAt(RcPoint3 point);

			// Matrices:
			RcMatrix3 GetPitchMatrix() const;
			RcMatrix3 GetYawMatrix() const;
			RcMatrix3 GetRotationMatrix() const;
			Transform3 GetMatrix() const;
			Transform3 GetUprightMatrix() const;
		};
		VERUS_TYPEDEFS(Spirit);
	}
}
