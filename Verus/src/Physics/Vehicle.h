// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Physics
{
	class Vehicle : public Object, public UserPtr
	{
		LocalPtr<btBoxShape>                _pChassisShape;
		LocalPtr<btCapsuleShape>            _pBumperShape;
		LocalPtr<btCompoundShape>           _pCompoundShape;
		LocalRigidBody                      _pChassis;
		LocalPtr<btDefaultVehicleRaycaster> _pVehicleRaycaster;
		LocalPtr<btRaycastVehicle>          _pRaycastVehicle;
		btRaycastVehicle::btVehicleTuning   _vehicleTuning;
		int                                 _frontRightWheelIndex = -1;
		int                                 _wheelCount = 0;
		float                               _invWheelCount = 0;
		float                               _invHandBrakeWheelCount = 0;

	public:
		struct Steering
		{
			float _angle = 0;
			float _speed = 1;
			float _maxAngle = Math::ToRadians(40);

			void Update(float stiffness)
			{
				VERUS_QREF_TIMER;
				// Steering torque:
				const float deviation = abs(_angle / _maxAngle);
				_angle = Math::Reduce(_angle, (_speed * dt) * deviation * stiffness);
			}

			void SteerLeft()
			{
				VERUS_QREF_TIMER;
				_angle = Math::Clamp(_angle + _speed * dt, -_maxAngle, _maxAngle);
			}

			void SteerRight()
			{
				VERUS_QREF_TIMER;
				_angle = Math::Clamp(_angle - _speed * dt, -_maxAngle, _maxAngle);
			}
		};
		VERUS_TYPEDEFS(Steering);

		struct Desc
		{
			Transform3           _tr = Transform3::identity();
			Math::Bounds         _chassis;
			Vector<Math::Sphere> _vLeftWheels;
			Vector<Math::Sphere> _vRightWheels;
			float                _mass = 1200;
			float                _suspensionRestLength = 0.15f; // 0.25 for buggy, 0.05 for sports car.
		};
		VERUS_TYPEDEFS(Desc);

		Vehicle();
		~Vehicle();

		void Init(RcDesc desc);
		void Done();

		void Update();

		template<typename T>
		void ForEachWheel(const T& fn) const
		{
			VERUS_FOR(i, _wheelCount)
			{
				if (Continue::no == fn(_pRaycastVehicle->getWheelInfo(i)))
					return;
			}
		}

		Transform3 GetTransform() const;

		btRaycastVehicle* GetRaycastVehicle() { return _pRaycastVehicle.Get(); }

		void ApplyAirForce(float scale = 1);
		void SetBrake(float brake, float handBrake = 0, int index = -1);
		void SetEngineForce(float force, int index = -1);
		void SetSteeringAngle(float angle);

		virtual int UserPtr_GetType() override;

		float ComputeEnginePitch() const;
	};
	VERUS_TYPEDEFS(Vehicle);
}
