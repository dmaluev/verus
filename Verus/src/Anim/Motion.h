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
			public:
				enum class Channel : int
				{
					rotation,
					position,
					scale,
					trigger
				};

				enum class Flags : UINT32
				{
					none = 0,
					slerpRot = (1 << 0),
					splinePos = (1 << 1),
					splineScale = (1 << 2)
				};

			private:
				friend class Motion;

				static const float s_magicValueForCircle;

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
				Flags       _flags = Flags::none;

				template<typename TMap, typename T>
				float FindControlPoints(const TMap& m, int frames[4], T keys[4], float time) const
				{
					frames[0] = frames[1] = frames[2] = frames[3] = -1;
					if (m.empty()) // No frames at all, so return null.
						return 0;
					time = Math::Max(0.f, time); // Negative time is not allowed.
					float alpha;
					const int frame = static_cast<int>(_pMotion->GetFps() * time); // Frame is before or at 'time'.
					typename TMap::const_iterator it = m.upper_bound(frame); // Find frame after 'time'.
					if (it != m.cend()) // There are frames greater (after 'time'):
					{
						if (it != m.cbegin()) // And there are less than (before 'time'), full interpolation:
						{
							typename TMap::const_iterator itPrev = it;
							itPrev--;
							frames[1] = itPrev->first;
							keys[1] = itPrev->second;
							if (itPrev != m.cbegin())
							{
								itPrev--;
								frames[0] = itPrev->first;
								keys[0] = itPrev->second;
							}
							frames[2] = it->first;
							keys[2] = it->second;
							it++;
							if (it != m.cend())
							{
								frames[3] = it->first;
								keys[3] = it->second;
							}
							alpha = (time - (frames[1] * _pMotion->GetFpsInv())) / ((frames[2] - frames[1]) * _pMotion->GetFpsInv());
						}
						else // But there are no less than:
						{
							frames[2] = it->first;
							keys[2] = it->second;
							it++;
							if (it != m.cend())
							{
								frames[3] = it->first;
								keys[3] = it->second;
							}
							alpha = time / (frames[2] * _pMotion->GetFpsInv());
						}
					}
					else // There are no frames greater, but there are less than:
					{
						it--;
						frames[1] = it->first;
						keys[1] = it->second;
						if (it != m.cbegin())
						{
							it--;
							frames[0] = it->first;
							keys[0] = it->second;
						}
						alpha = 0;
					}
					return alpha;
				}

				template<typename TMap>
				static bool SpaceTimeSyncTemplate(TMap& m, int fromFrame, int toFrame)
				{
					const auto itFrom = m.find(fromFrame);
					const auto itTo = m.find(toFrame);
					if (m.end() == itFrom || m.end() == itTo) // Endpoints must exist.
						return false;

					const int totalFrames = toFrame - fromFrame;

					auto it = itFrom;
					Vector<float> vSegments;
					vSegments.reserve(8);
					float totalDist = 0;
					while (it != itTo)
					{
						const auto& posA = it->second;
						it++;
						const auto& posB = it->second;
						const float d = VMath::dist(Point3(posA), Point3(posB));
						vSegments.push_back(d);
						totalDist += d;
					}
					if (totalDist < 1e-4f) // Distance is too small.
						return false;
					const float invTotalDist = 1.f / totalDist;

					TMap tempMap;
					it = itFrom;
					it++;
					while (it != itTo) // Copy all keys in-between, delete original keys.
					{
						tempMap[it->first] = std::move(it->second);
						it = m.erase(it);
					}

					it = tempMap.begin();
					float distAcc = 0;
					int i = 0;
					while (it != tempMap.end())
					{
						distAcc += vSegments[i++];
						int syncedFrame = fromFrame + static_cast<int>(totalFrames * (distAcc * invTotalDist) + 0.5f);
						while (m.end() != m.find(syncedFrame)) // Frame already occupied?
							syncedFrame++;
						m[syncedFrame] = it->second;
						it++;
					}

					return true;
				}

			public:
				Bone(Motion* pMotion = nullptr);
				~Bone();

				Str GetName() const { return _C(_name); }
				void Rename(CSZ name) { _name = name; }

				int GetLastTriggerState() const { return _lastTriggerState; }
				void SetLastTriggerState(int state) { _lastTriggerState = state; }

				Flags GetFlags() const { return _flags; }
				void SetFlags(Flags flags) { _flags = flags; }

				void DeleteAll();

				// Insert:
				void InsertKeyframeRotation(int frame, RcQuat q);
				void InsertKeyframeRotation(int frame, RcVector3 euler);
				void InsertKeyframePosition(int frame, RcVector3 pos);
				void    InsertKeyframeScale(int frame, RcVector3 scale);
				void  InsertKeyframeTrigger(int frame, int state);

				// Delete:
				void DeleteKeyframeRotation(int frame);
				void DeleteKeyframePosition(int frame);
				void    DeleteKeyframeScale(int frame);
				void  DeleteKeyframeTrigger(int frame);

				// Find:
				bool FindKeyframeRotation(int frame, RVector3 euler, RQuat q) const;
				bool FindKeyframePosition(int frame, RVector3 pos) const;
				bool    FindKeyframeScale(int frame, RVector3 scale) const;
				bool  FindKeyframeTrigger(int frame, int& state) const;

				// Compute:
				void ComputeRotationAt(float time, RVector3 euler, RQuat q) const;
				void ComputePositionAt(float time, RVector3 pos) const;
				void    ComputeScaleAt(float time, RVector3 scale) const;
				void  ComputeTriggerAt(float time, int& state) const;
				void   ComputeMatrixAt(float time, RTransform3 mat);

				void MoveKeyframe(int direction, Channel channel, int frame);

				int GetRotationKeyCount() const { return Utils::Cast32(_mapRot.size()); }
				int GetPositionKeyCount() const { return Utils::Cast32(_mapPos.size()); }
				int GetScaleKeyCount() const { return Utils::Cast32(_mapScale.size()); }
				int GetTriggerKeyCount() const { return Utils::Cast32(_mapTrigger.size()); }

				VERUS_P(void Serialize(IO::RStream stream, UINT16 version));
				VERUS_P(void Deserialize(IO::RStream stream, UINT16 version));

				void DeleteRedundantKeyframes();
				void DeleteOddKeyframes();
				void InsertLoopKeyframes();
				void Cut(int frame, bool before);
				void Fix(bool speedLimit);

				void ApplyScaleBias(RcVector3 scale, RcVector3 bias);

				void Scatter(int srcFrom, int srcTo, int dMin, int dMax);

				bool SpaceTimeSync(Channel channel, int fromFrame, int toFrame);

				int GetLastKeyframe() const;
			};
			VERUS_TYPEDEFS(Bone);

		private:
			static const int s_xanVersion = 0x0102;
			static const int s_maxFps = 10000;
			static const int s_maxBones = 10000;
			static const int s_maxFrames = 32 * 1024 * 1024;

			typedef Map<String, Bone> TMapBones;

			TMapBones _mapBones;
			Motion* _pBlendMotion = nullptr;
			int       _frameCount = 60;
			int       _fps = 12;
			float     _fpsInv = 1 / 12.f;
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

			template<typename T>
			void ForEachBone(const T& fn)
			{
				for (auto& kv : _mapBones)
				{
					if (Continue::no == fn(kv.second))
						break;
				}
			}

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

			void Exec(CSZ code, PBone pBone = nullptr, Bone::Channel channel = Bone::Channel::rotation);

			int GetLastKeyframe() const;
		};
		VERUS_TYPEDEFS(Motion);
	}
}
