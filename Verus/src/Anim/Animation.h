// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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

		// Objects of this type are stored in Collection.
		struct MotionData
		{
			Motion _motion;
			float  _duration = 0;
			bool   _looping = true;
			bool   _fast = false; // For auto easing.
		};
		VERUS_TYPEDEFS(MotionData);

		typedef StoreUniqueWithNoRefCount<String, MotionData> TStoreMotions;
		// The collection of motion objects that can be reused by multiple animation objects.
		class Collection : private TStoreMotions, public IO::AsyncDelegate
		{
			bool _useShortNames = true;

		public:
			Collection(bool useShortNames = true);
			~Collection();

			virtual void Async_WhenLoaded(CSZ url, RcBlob blob) override;

			void AddMotion(CSZ name, bool looping = true, float duration = 0, bool fast = false);
			void DeleteAll();
			PMotionData Find(CSZ name);
			int GetMaxBones();
		};
		VERUS_TYPEDEFS(Collection);

		// Holds the collection of motion objects and provides the ability to interpolate between them.
		// Note: triggers will work properly with 'multiple animations' + 'single collection' only if you add motions before binding collection.
		class Animation : public MotionDelegate
		{
		public:
			static const int s_maxLayers = 8;

			enum class Edge : int
			{
				begin = (1 << 0),
				end = (1 << 1)
			};

			struct Layer
			{
				Animation* _pAnimation = nullptr;
				CSZ _rootBone = nullptr;
			};
			VERUS_TYPEDEFS(Layer);

		private:
			PCollection        _pCollection = nullptr;
			PAnimationDelegate _pDelegate = nullptr;
			PSkeleton          _pSkeleton = nullptr;
			Motion             _transitionMotion;
			String             _currentMotion;
			String             _prevMotion;
			Vector<int>        _vTriggerStates;
			Easing             _easing = Easing::none;
			float              _time = 0;
			float              _transitionDuration = 0;
			float              _transitionTime = 0;
			bool               _transition = false;
			bool               _playing = false;

		public:
			Animation();
			~Animation();

			void Update(int layerCount = 0, PLayer pLayers = nullptr);
			void UpdateSkeleton(int layerCount = 0, PLayer pLayers = nullptr);

			void BindCollection(PCollection p);
			void BindSkeleton(PSkeleton p);
			PAnimationDelegate SetDelegate(PAnimationDelegate p) { return Utils::Swap(_pDelegate, p); }

			bool IsPlaying() const { return _playing; }
			void Play();
			void Stop();
			void Pause();

			Str GetCurrentMotionName() const { return _C(_currentMotion); }

			void TransitionTo(CSZ name,
				Interval duration = 0.5f, int randTime = -1, PMotion pFromMotion = nullptr);
			bool TransitionToNew(std::initializer_list<CSZ> names,
				Interval duration = 0.5f, int randTime = -1, PMotion pFromMotion = nullptr);
			bool IsInTransition() const { return _transition; }

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
