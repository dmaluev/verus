// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Audio
	{
		struct SoundFlags
		{
			enum
			{
				loaded = (ObjectFlags::user << 0),
				is3D = (ObjectFlags::user << 1),
				looping = (ObjectFlags::user << 2),
				randOff = (ObjectFlags::user << 3),
				keepPcmBuffer = (ObjectFlags::user << 4),
				user = (ObjectFlags::user << 5)
			};
		};

		class Sound : public Object, public IO::AsyncDelegate
		{
			Source       _sources[8];
			String       _url;
			Vector<BYTE> _vPcmBuffer;
			ALuint       _buffer = 0;
			int          _refCount = 0;
			int          _next = 0;
			Interval     _gain = 1;
			Interval     _pitch = 1;
			float        _referenceDistance = 4;
			float        _length = 0;

		public:
			// Note that this structure contains some default values for new sources, which can be changed per source.
			struct Desc
			{
				CSZ      _url = nullptr;
				Interval _gain = 0.8f;
				Interval _pitch = 1;
				float    _referenceDistance = 4;
				bool     _is3D = false;
				bool     _looping = false;
				bool     _randomOffset = false;
				bool     _keepPcmBuffer = false;

				Desc(CSZ url) : _url(url) {}
				Desc& Set3D(bool b = true) { _is3D = b; return *this; }
				Desc& SetLooping(bool b = true) { _looping = b; return *this; }
				Desc& SetRandomOffset(bool b = true) { _randomOffset = b; return *this; }
				Desc& SetGain(Interval gain) { _gain = gain; return *this; }
				Desc& SetPitch(Interval pitch) { _pitch = pitch; return *this; }
				Desc& SetReferenceDistance(float rd) { _referenceDistance = rd; return *this; }
			};
			VERUS_TYPEDEFS(Desc);

			Sound();
			~Sound();

			void AddRef() { _refCount++; }
			int GetRefCount() const { return _refCount; }

			void Init(RcDesc desc);
			bool Done();

			void Update();
			void UpdateHRTF();

			// <Resources>
			virtual void Async_WhenLoaded(CSZ url, RcBlob blob) override;
			bool IsLoaded() const { return IsFlagSet(SoundFlags::loaded); }
			Str GetURL() const { return _C(_url); }
			// </Resources>

			SourcePtr NewSource(PSourcePtr pID = nullptr, Source::RcDesc desc = Source::Desc());

			Interval GetGain() const { return _gain; }
			Interval GetPitch() const { return _pitch; }

			float GetLength() const { return _length; }

			float GetRandomOffset() const;
			void SetRandomOffset(bool b) { b ? SetFlag(SoundFlags::randOff) : ResetFlag(SoundFlags::randOff); }
			bool HasRandomOffset() const { return IsFlagSet(SoundFlags::randOff); }

			Blob GetPcmBuffer() const;
		};
		VERUS_TYPEDEFS(Sound);

		class SoundPtr : public Ptr<Sound>
		{
		public:
			void Init(Sound::RcDesc desc);
		};
		VERUS_TYPEDEFS(SoundPtr);

		class SoundPwn : public SoundPtr
		{
		public:
			~SoundPwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(SoundPwn);

		template<int COUNT>
		class SoundPwns
		{
			SoundPwn _sounds[COUNT];
			int      _prev = 0;

		public:
			SoundPwns()
			{
			}

			~SoundPwns()
			{
				Done();
			}

			void Init(Sound::RcDesc desc)
			{
				char buffer[200];
				VERUS_FOR(i, COUNT)
				{
					sprintf_s(buffer, desc._url, i);
					Sound::Desc descs = desc;
					descs._url = buffer;
					_sounds[i].Init(descs);
				}
			}

			void Done()
			{
				VERUS_FOR(i, COUNT)
					_sounds[i].Done();
			}

			RSoundPtr operator[](int i)
			{
				const bool useRand = i < 0;
				if (useRand)
					i = Utils::I().GetRandom().Next() & 0xFF; // Only positive.
				i %= COUNT;
				if (useRand && (i == _prev))
					i = (i + 1) % COUNT;
				_prev = i;
				return _sounds[i];
			}
		};
	}
}
