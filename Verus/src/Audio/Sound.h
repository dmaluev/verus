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
				loop = (ObjectFlags::user << 2),
				randOff = (ObjectFlags::user << 3),
				user = (ObjectFlags::user << 4)
			};
		};

		class Sound : public Object, public IO::AsyncCallback
		{
			Source       _sources[8];
			String       _url;
			ALuint       _buffer = 0;
			int          _next = 0;
			int          _refCount = 0;
			Range<float> _gain = 1;
			Range<float> _pitch = 1;
			float        _length = 0;
			float        _referenceDistance = 4;

		public:
			struct Desc
			{
				CSZ          _url = nullptr;
				Range<float> _gain = 0.8f;
				Range<float> _pitch = 1;
				float        _referenceDistance = 4;
				bool         _is3D = false;
				bool         _loop = false;
				bool         _randomOffset = false;

				Desc(CSZ url) : _url(url) {}
				Desc& Set3D(bool b = true) { _is3D = b; return *this; }
				Desc& SetLoop(bool b = true) { _loop = b; return *this; }
				Desc& SetGain(const Range<float>& gain) { _gain = gain; return *this; }
				Desc& SetPitch(const Range<float>& pitch) { _pitch = pitch; return *this; }
				Desc& SetRandomOffset(bool b = true) { _randomOffset = b; return *this; }
				Desc& SetReferenceDistance(float rd) { _referenceDistance = rd; return *this; }
			};
			VERUS_TYPEDEFS(Desc);

			Sound();
			~Sound();

			void Init(RcDesc desc);
			bool Done();

			virtual void Async_Run(CSZ url, RcBlob blob) override;
			bool IsLoaded() const { return IsFlagSet(SoundFlags::loaded); }
			void AddRef() { _refCount++; }

			Str GetUrl() const { return _C(_url); }

			void Update();
			void UpdateHRTF();

			SourcePtr NewSource(PSourcePtr pID = nullptr, Source::RcDesc desc = Source::Desc());

			const Range<float>& GetGain() const { return _gain; }
			const Range<float>& GetPitch() const { return _pitch; }

			float GetLength() const { return _length; }

			float GetRandomOffset() const;
			void SetRandomOffset(bool b) { b ? SetFlag(SoundFlags::randOff) : ResetFlag(SoundFlags::randOff); }
			bool HasRandomOffset() const { return IsFlagSet(SoundFlags::randOff); }
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

		template<int NUM>
		class SoundPwns
		{
			SoundPwn _sounds[NUM];
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
				VERUS_FOR(i, NUM)
				{
					sprintf_s(buffer, desc._url, i);
					Sound::Desc descs = desc;
					descs._url = buffer;
					_sounds[i].Init(descs);
				}
			}

			void Done()
			{
				VERUS_FOR(i, NUM)
					_sounds[i].Done();
			}

			RSoundPtr operator[](int i)
			{
				if (i < 0)
					i = Utils::I().GetRandom().Next() & 0xFF; // Only positive.
				i %= NUM;
				if (i == _prev)
					i = (i + 1) % NUM;
				_prev = i;
				return _sounds[i];
			}
		};
	}
}
