// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Game
	{
		class BaseCharacter : public Spirit
		{
			Vector3                      _extraForce = Vector3(0);
			Physics::CharacterController _cc;
			float                        _idleRadius = 1;
			float                        _idleHeight = 1;
			float                        _stepHeight = 1;
			float                        _airborneControl = 0.1f;
			float                        _jumpSpeed = 6;
			float                        _minCameraRadius = 0.1f;
			float                        _maxCameraRadius = 7.5f;
			Anim::Elastic<float>         _smoothSpeedOnGround;
			Anim::Elastic<float>         _fallSpeed;
			Linear<float>                _cameraRadius;
			Cooldown                     _jumpCooldown = 0.25f; // To filter BaseCharacter_OnJump().
			Cooldown                     _wallHitCooldown = 2;
			Cooldown                     _unstuckCooldown = 0.25f;
			bool                         _airborne = false;
			bool                         _ragdoll = false;

		public:
			BaseCharacter();
			virtual ~BaseCharacter();

			virtual void HandleActions() override;
			virtual void Update() override;

			// Creates the underlying controller.
			// EndRagdoll() calls this method automatically.
			void InitController();
			void DoneController();
			Physics::RCharacterController GetController() { return _cc; }

			virtual void MoveTo(RcPoint3 pos) override;
			virtual bool FitRemotePosition() override;

			// Dimensions:
			float GetIdleRadius() const { return _idleRadius; }
			void SetIdleRadius(float r) { _idleRadius = r; }
			float GetIdleHeight() const { return _idleHeight; }
			void SetIdleHeight(float h) { _idleHeight = h; }

			void SetStepHeight(float sh) { _stepHeight = sh; }

			// Jumps:
			bool IsOnGround() const;
			bool TryJump(bool whileAirborne = false);
			void Jump();
			void SetJumpSpeed(float v = 6);
			float GetFallSpeed() const { return _fallSpeed; }
			virtual void BaseCharacter_OnFallImpact(float speed) {}
			virtual void BaseCharacter_OnWallImpact(float speed) {}
			virtual void BaseCharacter_OnJump() {}

			void AddExtraForce(RcVector3 v) { _extraForce += v; }

			// For AI:
			// Check if the speed is less than expected.
			bool IsStuck();
			// Is typically called when AI gets a new task.
			void StartUnstuckCooldown() { _unstuckCooldown.Start(); }

			// Ragdoll:
			bool IsRagdoll() const { return _ragdoll; }
			void ToggleRagdoll();
			void BeginRagdoll();
			void EndRagdoll();

			virtual void BaseCharacter_OnInitRagdoll(RcTransform3 matW) {}
			virtual void BaseCharacter_OnDoneRagdoll() {}

			virtual void ComputeThirdPersonCameraArgs(RcVector3 offset, RPoint3 eye, RPoint3 at);
			float ComputeThirdPersonCamera(Scene::RCamera camera, Anim::RcOrbit orbit, RcVector3 offset = Vector3(0));
			void ComputeThirdPersonAim(RPoint3 aimPos, RVector3 aimDir, RcVector3 offset = Vector3(0));
			void SetMaxCameraRadius(float r);
			float GetCameraRadius() const { return _cameraRadius.GetValue(); }
		};
		VERUS_TYPEDEFS(BaseCharacter);
	}
}
