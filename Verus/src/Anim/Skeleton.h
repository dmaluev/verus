// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Anim
	{
		// Standard skeleton, which can be animated using motion object.
		// X3D format can support up to 256 bones, but the hardware has certain limitations.
		// With Shader Model 2.0 the shader can hold about 32 bone matrices.
		// Other bones must be remapped using Primary Bones concept.
		// Layered motions add the ability to mix multiple motions, just like images are mixed with alpha channel.
		// Layered motion affects it's root bone and all descendants of that bone.
		// A ragdoll can be created automatically or configured using XML .rig file.
		// Ragdoll's simulation can start from any motion frame and any simulated pose can be baked back into motion object,
		// allowing smooth transition to ragdoll simulation and back to motion.
		class Skeleton : public Object
		{
		public:
			struct LayeredMotion
			{
				PMotion _pMotion = nullptr;
				CSZ     _rootBone = nullptr;
				float   _alpha = 0;
				float   _time = 0;
			};
			VERUS_TYPEDEFS(LayeredMotion);

			struct Bone : AllocatorAware
			{
				Transform3         _matToBoneSpace = Transform3::identity();
				Transform3         _matFromBoneSpace = Transform3::identity();
				Transform3         _matFinal = Transform3::identity();
				Transform3         _matFinalInv = Transform3::identity();
				Transform3         _matExternal = Transform3::identity();
				Transform3         _matAdapt = Transform3::identity();
				Transform3         _matToActorSpace = Transform3::identity();
				Vector3            _rigRot = Vector3(0); // Rotation angles of ragdoll's rigid component.
				Vector3            _cRot = Vector3(0); // Constraint's rotation angles.
				Vector3            _cLimits = Vector3(0); // Constraint's limits.
				Vector3            _boxSize = Vector3(0); // For a box shape.
				String             _name;
				String             _parentName;
				btCollisionShape* _pShape = nullptr;
				btRigidBody* _pRigidBody = nullptr;
				btTypedConstraint* _pConstraint = nullptr;
				float              _width = 0;
				float              _length = 0;
				float              _mass = 0;
				float              _friction = 0;
				int                _shaderIndex = 0; // Index of a matrix in the vertex shader.
				bool               _ready = false;
				bool               _rigBone = true;
				bool               _hinge = false; // btHingeConstraint vs btConeTwistConstraint.
				bool               _noCollision = false;
			};
			VERUS_TYPEDEFS(Bone);

		private:
			typedef Map<String, Bone> TMapBones;
			typedef Map<int, int> TMapPrimary;

			Transform3   _matParents = Transform3::identity();
			Transform3   _matRagdollToWorld = Transform3::identity();
			Transform3   _matRagdollToWorldInv = Transform3::identity();
			TMapBones    _mapBones;
			TMapPrimary  _mapPrimary;
			PBone        _pCurrentBone = nullptr;
			PMotion      _pCurrentMotion = nullptr;
			PLayeredMotion _pLayeredMotions = nullptr;
			float        _currentTime = 0;
			float        _mass = 0;
			int          _primaryBoneCount = 0;
			int          _layeredMotionCount = 0;
			bool         _ragdollMode = false;

		public:
			Skeleton();
			~Skeleton();

			void operator=(const Skeleton& that);

			void Init();
			void Done();

			void Draw(bool bindPose = true, PcTransform3 pMat = nullptr, int selected = -1);

			static CSZ RootName() { return "$ROOT"; }

			PBone InsertBone(RBone bone);
			PBone FindBone(CSZ name);
			PcBone FindBone(CSZ name) const;
			// Uses shader's array index to find a bone.
			PBone FindBoneByIndex(int index);

			// Sets the current pose using motion object (Motion).
			void ApplyMotion(RMotion motion, float time,
				int layeredMotionCount = 0, PLayeredMotion pLayeredMotions = nullptr);

			// Fills the array of matrices that will be used by a shader.
			void UpdateUniformBufferArray(mataff* p) const;

			void ResetFinalPose();

			VERUS_P(void ResetBones());
			VERUS_P(void RecursiveBoneUpdate());

			int GetBoneCount() const { return _primaryBoneCount ? _primaryBoneCount : Utils::Cast32(_mapBones.size()); }

			template<typename F>
			void ForEachBone(const F& fn)
			{
				for (auto& kv : _mapBones)
					if (Continue::no == fn(kv.second))
						return;
			}
			template<typename F>
			void ForEachBone(const F& fn) const
			{
				for (const auto& kv : _mapBones)
					if (Continue::no == fn(kv.second))
						return;
			}

			// Adds skeleton's bones to motion object (Motion).
			void InsertBonesIntoMotion(RMotion motion) const;
			// Removes motion's bones, which are not skeleton's bones.
			void DeleteOutsiders(RMotion motion) const;

			void AdjustPrimaryBones(const Vector<String>& vPrimaryBones);
			int RemapBoneIndex(int index) const;

			bool IsParentOf(CSZ bone, CSZ parent) const;

			void LoadRigInfo(CSZ url);
			void LoadRigInfoFromPtr(SZ p);
			void BeginRagdoll(RcTransform3 matW, RcVector3 impulse = Vector3(0), CSZ bone = "Spine2");
			void EndRagdoll();
			bool IsRagdollMode() const { return _ragdollMode; }
			RcTransform3 GetRagdollToWorldMatrix() { return _matRagdollToWorld; }

			// Saves the current pose into motion object (Motion) at some frame.
			void BakeMotion(RMotion motion, int frame = 0, bool kinect = false);
			void AdaptBindPoseOf(const Skeleton& that);
			void SimpleIK(CSZ boneDriven, CSZ boneDriver, RcVector3 dirDriverSpace, RcVector3 dirDesiredMeshSpace, float limitDot, float alpha);

			void ProcessKinectData(const BYTE* p, RMotion motion, int frame = 0);
			void ProcessKinectJoint(const BYTE* p, CSZ name, RcVector3 skeletonPos);
			void LoadKinectBindPose(CSZ xml);
			static bool IsKinectBone(CSZ name);
			static bool IsKinectLeafBone(CSZ name);

			void FixateFeet(RMotion motion);

			// Tries to compute the highest speed at which a bone would move with this motion applied.
			// Can be used to sync animation and movement to fix the sliding feet problem.
			Vector3 GetHighestSpeed(RMotion motion, CSZ name, RcVector3 scale = Vector3(1, 0, 1), bool positive = false);

			void ComputeBoneLengths(Map<String, float>& m, bool accumulated = false);
		};
		VERUS_TYPEDEFS(Skeleton);
	}
}
