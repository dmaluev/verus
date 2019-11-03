#pragma once

namespace verus
{
	namespace Audio
	{
		class Source
		{
			friend class SourcePtr; // _pSound @ Attach().
			friend class Sound; // _sid @ NewSource().

			Point3  _position = Point3(0);
			Vector3 _direction = Vector3(0);
			Vector3 _velocity = Vector3(0);
			Sound* _pSound = nullptr;
			ALuint  _sid = 0;
			float   _travelDelay = 0;

		public:
			struct Desc
			{
				float _secOffset = 0;
				float _gain = 1;
				float _pitch = 1;
				bool  _stopSource = true;
			};
			VERUS_TYPEDEFS(Desc);

			Source();
			~Source();

			void Done();

			void Update();
			void UpdateHRTF(bool is3D);

			void Play();
			void PlayAt(RcPoint3 pos, RcVector3 dir = Vector3(0), RcVector3 vel = Vector3(0), float delay = 0);
			void Stop();
			void MoveTo(RcPoint3 pos, RcVector3 dir = Vector3(0), RcVector3 vel = Vector3(0));

			void SetGain(float gain);
			void SetPitch(float pitch);
		};
		VERUS_TYPEDEFS(Source);

		class SourcePtr : public Ptr<Source>
		{
		public:
			void Attach(Source* pSource, Sound* pSound);
			void Done();
		};
		VERUS_TYPEDEFS(SourcePtr);
	}
}
