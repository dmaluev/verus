// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Anim
	{
		struct AnimationDelegate
		{
			virtual void Animation_OnEnd(CSZ name) {}
			virtual void Animation_OnTrigger(CSZ name, int state) {}
		};
		VERUS_TYPEDEFS(AnimationDelegate);

		//! Objects of this type are stored in Collection.
		struct MotionData
		{
			Motion _motion;
			float  _duration = 0;
			bool   _loop = true;
		};
		VERUS_TYPEDEFS(MotionData);

		typedef StoreUniqueWithNoRefCount<String, MotionData> TStoreMotions;
		//! The collection of motion objects (Motion) that can be reused by multiple animation objects (Animation).
		class Collection : private TStoreMotions, public IO::AsyncCallback
		{
		public:
			Collection();
			~Collection();

			virtual void Async_Run(CSZ url, RcBlob blob) override;

			void AddMotion(CSZ name, bool loop = true, float duration = 0);
			void DeleteAll();
			PMotionData Find(CSZ name);
			int GetMaxBones();
		};
		VERUS_TYPEDEFS(Collection);

		//! Holds the collection (Collection) of motion objects (Motion) and provides the ability to interpolate between them.

		//! Note: triggers will work properly with 'multiple animations' + 'single
		//! collection' only if you add motions before binding collection.
		//!
		class Animation : public MotionDelegate
		{
		public:
			static const int s_maxAlphaTracks = 4;

			enum class Edge : int
			{
				begin = (1 << 0),
				end = (1 << 1)
			};

			struct AlphaTrack
			{
				Animation* _pAnimation = nullptr;
				CSZ _rootBone = nullptr;
			};
			VERUS_TYPEDEFS(AlphaTrack);

		private:
			PCollection        _pCollection = nullptr;
			PAnimationDelegate _pDelegate = nullptr;
			PSkeleton          _pSkeleton = nullptr;
			Motion             _blendMotion;
			String             _currentMotion;
			String             _prevMotion;
			Vector<int>        _vTriggerStates;
			Easing             _easing = Easing::quadInOut;
			float              _time = 0;
			float              _blendDuration = 0;
			float              _blendTime = 0;
			bool               _blending = false;
			bool               _playing = false;

		public:
			Animation();
			~Animation();

			void Update(int alphaTrackCount = 0, PAlphaTrack pAlphaTracks = nullptr);
			void UpdateSkeleton(int alphaTrackCount = 0, PAlphaTrack pAlphaTracks = nullptr);

			void BindCollection(PCollection p);
			void BindSkeleton(PSkeleton p);
			PAnimationDelegate SetDelegate(PAnimationDelegate p) { return Utils::Swap(_pDelegate, p); }

			bool IsPlaying() const { return _playing; }
			void Play();
			void Stop();
			void Pause();

			Str GetCurrentMotionName() const { return _C(_currentMotion); }

			void BlendTo(CSZ name,
				Range<float> duration = 0.5f, int randTime = -1, PMotion pFromMotion = nullptr);
			bool BlendToNew(std::initializer_list<CSZ> names,
				Range<float> duration = 0.5f, int randTime = -1, PMotion pFromMotion = nullptr);
			bool IsBlending() const { return _blending; }

			void SetEasing(Easing easing) { _easing = easing; }

			virtual void Motion_OnTrigger(CSZ name, int state) override;

			PMotion GetMotion();
			float GetAlpha(CSZ name = nullptr) const;
			float GetTime();
			bool IsNearEdge(float t = 0.1f, Edge edge = Edge::begin | Edge::end);

			int* GetTriggerStatesArray() { return _vTriggerStates.empty() ? nullptr : _vTriggerStates.data(); }
		};
		VERUS_TYPEDEFS(Animation);
	}
}
