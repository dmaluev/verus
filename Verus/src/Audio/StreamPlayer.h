// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Audio
	{
		class Track : public IO::AsyncDelegate
		{
			Vector<BYTE>     _vOggEncodedTrack;
			OggDataSource    _ds;
			std::atomic_bool _loaded;

		public:
			Track();
			~Track();

			POggDataSource GetOggDataSource() { return &_ds; }
			void Init(CSZ url);
			void Done();

			virtual void Async_WhenLoaded(CSZ url, RcBlob blob) override;
			bool IsLoaded() const { return _loaded; }
		};
		VERUS_TYPEDEFS(Track);

		struct StreamPlayerFlags
		{
			enum
			{
				stopThread = (ObjectFlags::user << 0),
				play = (ObjectFlags::user << 1),
				noLock = (ObjectFlags::user << 2)
			};
		};

		class StreamPlayer : public Object, public Lockable
		{
			Track                   _nativeTrack;
			Vector<PTrack>          _vTracks;
			Vector<BYTE>            _vSmallBuffer;
			Vector<BYTE>            _vMediumBuffer;
			std::thread             _thread;
			std::condition_variable _cv;
			vorbis_info* _pVorbisInfo = nullptr;
			PTrack                  _pTrack = nullptr;
			OggVorbis_File          _oggVorbisFile;
			ALuint                  _buffers[2];
			ALuint                  _source = 0;
			ALenum                  _format = 0;
			Linear<float>           _fade;
			float                   _gain = 0.25f;
			int                     _currentTrack = 0;

		public:
			StreamPlayer();
			~StreamPlayer();

			void Init();
			void Done();

			void Update();

			void AddTrack(PTrack pTrack);
			void DeleteTrack(PTrack pTrack);
			void SwitchToTrack(PTrack pTrack);

			void Play();
			void Stop();
			void Seek(int pos);

			VERUS_P(void ThreadProc());
			VERUS_P(void StopThread());
			VERUS_P(void FillBuffer(ALuint buffer));

			void FadeIn(float time);
			void FadeOut(float time);
			void Mute();
			void SetGain(float gain);
		};
		VERUS_TYPEDEFS(StreamPlayer);
	}
}
