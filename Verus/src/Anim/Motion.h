// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Anim
	{
		struct MotionDelegate
		{
			virtual void Motion_OnTrigger(CSZ name, int state) = 0;
		};
		VERUS_TYPEDEFS(MotionDelegate);

		//! Motion holds a series of keyframes and provides the ability to interpolate between them.

		//! Motion can be stored in XAN format. Motion's rate can be
		//! scaled and even reversed. Animation object (Animation) can be used to
		//! handle multiple motion objects.
		//!
		class Motion : public Object
		{
		public:
			class Bone : public AllocatorAware
			{
				friend class Motion;

				class Rotation
				{
				public:
					Quat _q;

					Rotation();
					Rotation(RcQuat q);
					Rotation(RcVector3 euler);
				};
				VERUS_TYPEDEFS(Rotation);

				typedef Map<int, Rotation> TMapRot;
				typedef Map<int, Vector3> TMapPos;
				typedef Map<int, Vector3> TMapScale;
				typedef Map<int, int> TMapTrigger;

				String      _name;
				Motion* _pMotion = nullptr;
				TMapRot     _mapRot; //!< Rotation keyframes.
				TMapPos     _mapPos; //!< Position keyframes.
				TMapScale   _mapScale; //!< Scaling keyframes.
				TMapTrigger _mapTrigger; //!< Trigger keyframes.
				int         _lastTriggerState = 0;

				template<typename TMap, typename T>
				float GetAlpha(const TMap& m, T& prev, T& next, const T& null, float time) const
				{
					if (m.empty()) // No frames at all, so return null.
					{
						prev = null;
						next = null;
						return 0;
					}
					float alpha;
					const int frame = static_cast<int>(_pMotion->GetFps() * time); // Frame is before or at 'time'.
					typename TMap::const_iterator it = m.upper_bound(frame); // Find frame after 'time'.
					if (it != m.end()) // There are frames greater (after 'time'):
					{
						if (it != m.begin()) // And there are less than (before 'time'), full interpolation:
						{
							typename TMap::const_iterator itPrev = it;
							itPrev--;
							const float prevTime = itPrev->first * _pMotion->GetFpsInv();
							const float nextTime = it->first * _pMotion->GetFpsInv();
							const float delta = nextTime - prevTime;
							const float offset = nextTime - time;
							alpha = 1 - offset / delta;
							prev = itPrev->second;
							next = it->second;
						}
						else // But there are no less than:
						{
							const float nextTime = it->first * _pMotion->GetFpsInv();
							const float delta = nextTime;
							const float offset = time;
							alpha = offset / delta;
							prev = null;
							next = it->second;
						}
					}
					else // There are no frames greater, but there are less than:
					{
						it--;
						alpha = 0;
						prev = it->second;
						next = null;
					}
					return alpha;
				}

			public:
				enum class Type : int
				{
					rotation,
					position,
					scale,
					trigger
				};

				Bone(Motion* pMotion = nullptr);
				~Bone();

				Str GetName() const { return _C(_name); }
				void Rename(CSZ name) { _name = name; }

				int GetLastTriggerState() const { return _lastTriggerState; }
				void SetLastTriggerState(int state) { _lastTriggerState = state; }

				void DeleteAll();

				void InsertKeyframeRotation(int frame, RcQuat q);
				void InsertKeyframeRotation(int frame, RcVector3 euler);
				void InsertKeyframePosition(int frame, RcVector3 pos);
				void    InsertKeyframeScale(int frame, RcVector3 scale);
				void  InsertKeyframeTrigger(int frame, int state);

				void DeleteKeyframeRotation(int frame);
				void DeleteKeyframePosition(int frame);
				void    DeleteKeyframeScale(int frame);
				void  DeleteKeyframeTrigger(int frame);

				bool FindKeyframeRotation(int frame, RVector3 euler, RQuat q) const;
				bool FindKeyframePosition(int frame, RVector3 pos) const;
				bool    FindKeyframeScale(int frame, RVector3 scale) const;
				bool  FindKeyframeTrigger(int frame, int& state) const;

				void ComputeRotationAt(float time, RVector3 euler, RQuat q) const;
				void ComputePositionAt(float time, RVector3 pos) const;
				void    ComputeScaleAt(float time, RVector3 scale) const;
				void  ComputeTriggerAt(float time, int& state) const;
				void   ComputeMatrixAt(float time, RTransform3 mat);

				void MoveKeyframe(int direction, Type type, int frame);

				int GetRotationKeyCount() const { return Utils::Cast32(_mapRot.size()); }
				int GetPositionKeyCount() const { return Utils::Cast32(_mapPos.size()); }
				int GetScaleKeyCount() const { return Utils::Cast32(_mapScale.size()); }
				int GetTriggerKeyCount() const { return Utils::Cast32(_mapTrigger.size()); }

				VERUS_P(void Serialize(IO::RStream stream));
				VERUS_P(void Deserialize(IO::RStream stream));

				void DeleteRedundantKeyframes();
				void DeleteOddKeyframes();
				void InsertLoopKeyframes();
				void Cut(int frame, bool before);
				void Fix(bool speedLimit);

				void ApplyScaleBias(RcVector3 scale, RcVector3 bias);

				void Scatter(int srcFrom, int srcTo, int dMin, int dMax);

				int GetLastKeyframe() const;
			};
			VERUS_TYPEDEFS(Bone);

		private:
			static const int s_xanVersion = 0x0101;
			static const int s_maxFps = 10000;
			static const int s_maxBones = 10000;
			static const int s_maxFrames = 32 * 1024 * 1024;

			typedef Map<String, Bone> TMapBones;

			TMapBones _mapBones;
			Motion* _pBlendMotion = nullptr;
			int       _frameCount = 50;
			int       _fps = 10;
			float     _fpsInv = 0.1f;
			float     _blendAlpha = 0;
			float     _playbackSpeed = 1;
			float     _playbackSpeedInv = 1;
			bool      _reversed = false;

		public:
			Motion();
			~Motion();

			void Init();
			void Done();

			int GetFps() const { return _fps; }
			float GetFpsInv() const { return _fpsInv; }
			void SetFps(int fps) { _fps = fps; _fpsInv = 1.f / _fps; }

			int GetFrameCount() const { return _frameCount; }
			void SetFrameCount(int count) { _frameCount = count; }

			int GetBoneCount() const { return Utils::Cast32(_mapBones.size()); }

			float GetDuration() const { return GetNativeDuration() * _playbackSpeedInv; }
			float GetNativeDuration() const { return _frameCount * _fpsInv; }

			PBone GetBoneByIndex(int index);
			int GetBoneIndex(CSZ name) const;

			PBone InsertBone(CSZ name);
			void DeleteBone(CSZ name);
			void DeleteAllBones();
			PBone FindBone(CSZ name);

			void Serialize(IO::RStream stream);
			void Deserialize(IO::RStream stream);

			void BakeMotionAt(float time, Motion& dest) const;
			void BindBlendMotion(Motion* p, float alpha);
			Motion* GetBlendMotion() const { return _pBlendMotion; }
			float GetBlendAlpha() const { return _blendAlpha; }

			void DeleteRedundantKeyframes();
			void DeleteOddKeyframes();
			void InsertLoopKeyframes();
			void Cut(int frame, bool before = true);
			void Fix(bool speedLimit);

			// Triggers:
			void ProcessTriggers(float time, PMotionDelegate p, int* pUserTriggerStates = nullptr);
			void ResetTriggers(int* pUserTriggerStates = nullptr);
			void SkipTriggers(float time, int* pUserTriggerStates = nullptr);

			void ApplyScaleBias(CSZ name, RcVector3 scale, RcVector3 bias);

			float GetPlaybackSpeed() const { return _playbackSpeed; }
			float GetPlaybackSpeedInv() const { return _playbackSpeedInv; }
			void SetPlaybackSpeed(float x);
			void ComputePlaybackSpeed(float duration);
			bool IsReversed() const { return _reversed; }

			void Exec(CSZ code);

			int GetLastKeyframe() const;
		};
		VERUS_TYPEDEFS(Motion);
	}
}
