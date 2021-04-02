// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Audio;

// Track:

Track::Track()
{
	_loaded = false;
}

Track::~Track()
{
	Done();
}

void Track::Init(CSZ url)
{
	IO::Async::I().Load(url, this);
}

void Track::Done()
{
	IO::Async::Cancel(this);
}

void Track::Async_Run(CSZ url, RcBlob blob)
{
	_vOggEncodedTrack.assign(blob._p, blob._p + blob._size);

	_ds._p = _vOggEncodedTrack.data();
	_ds._size = _vOggEncodedTrack.size();
	_ds._cursor = 0;

	_loaded = true;
}

// StreamPlayer:

StreamPlayer::StreamPlayer()
{
	VERUS_ZERO_MEM(_oggVorbisFile);
	VERUS_ZERO_MEM(_buffers);
}

StreamPlayer::~StreamPlayer()
{
	Done();
}

void StreamPlayer::Init()
{
	VERUS_INIT();

	_fade.Set(1, 0);
	_fade.SetLimits(0, 1);

	alGenBuffers(2, _buffers);
	alGenSources(1, &_source);

	alSourcei(_source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSource3f(_source, AL_POSITION, 0, 0, 0);
	alSource3f(_source, AL_VELOCITY, 0, 0, 0);
	alSourcef(_source, AL_GAIN, _gain);
	alSourcef(_source, AL_ROLLOFF_FACTOR, 0);

	_vSmallBuffer.resize(441 * 32);
	_vMediumBuffer.resize(441 * 32 * 32);
	_vTracks.reserve(8);

	ResetFlag(StreamPlayerFlags::stopThread);
	ResetFlag(StreamPlayerFlags::noLock);
	_thread = std::thread(&StreamPlayer::ThreadProc, this);
}

void StreamPlayer::Done()
{
	StopThread();
	_nativeTrack.Done();

	if (_source)
	{
		alDeleteSources(1, &_source);
		_source = 0;
	}
	if (_buffers[0])
	{
		alDeleteBuffers(2, _buffers);
		_buffers[0] = _buffers[1] = 0;
	}

	ov_clear(&_oggVorbisFile);

	VERUS_DONE(StreamPlayer);
}

void StreamPlayer::Update()
{
	if (!IsInitialized())
		return;
	VERUS_UPDATE_ONCE_CHECK;

	VERUS_QREF_TIMER;

	_fade.UpdateClamped(dt);

	const float level = Math::SmoothStep(0.f, 1.f, _fade.GetValue());
	alSourcef(_source, AL_GAIN, _gain * level);
}

void StreamPlayer::AddTrack(PTrack pTrack)
{
	VERUS_RT_ASSERT(IsInitialized());
	{
		VERUS_LOCK(*this);
		_vTracks.push_back(pTrack);
		if (!_pTrack)
			_pTrack = pTrack;
	}
	_cv.notify_one();
}

void StreamPlayer::DeleteTrack(PTrack pTrack)
{
	{
		VERUS_LOCK(*this);
		VERUS_WHILE(Vector<PTrack>, _vTracks, it)
		{
			if (*it == pTrack)
				it = _vTracks.erase(it);
			else
				it++;
		}
		if (_pTrack == pTrack)
		{
			SetFlag(StreamPlayerFlags::noLock);
			SwitchToTrack(nullptr);
			ResetFlag(StreamPlayerFlags::noLock);
		}
	}
	_cv.notify_one();
}

void StreamPlayer::SwitchToTrack(PTrack pTrack)
{
	VERUS_LOCK_EX(*this, IsFlagSet(StreamPlayerFlags::noLock));

	VERUS_FOR(i, int(_vTracks.size()))
	{
		if (_vTracks[i] == pTrack)
		{
			_currentTrack = i;
			break;
		}
	}

	_pTrack = pTrack;
	_pVorbisInfo = nullptr;
	ov_clear(&_oggVorbisFile);

	if (!_pTrack || !_pTrack->IsLoaded()) // Possible to start decoding?
		return;

	const int ret = ov_open_callbacks(_pTrack->GetOggDataSource(), &_oggVorbisFile, 0, 0, g_oggCallbacks);
	if (ret < 0)
		throw VERUS_RUNTIME_ERROR << "ov_open_callbacks(), " << ret;

	_pVorbisInfo = ov_info(&_oggVorbisFile, -1);
	_format = (_pVorbisInfo->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
}

void StreamPlayer::Play()
{
	SetFlag(StreamPlayerFlags::play);
	alSourcePlay(_source);
	_cv.notify_one();
}

void StreamPlayer::Stop()
{
	ResetFlag(StreamPlayerFlags::play);
	alSourceStop(_source);
	_cv.notify_one();
}

void StreamPlayer::Seek(int pos)
{
	{
		VERUS_LOCK(*this);

		if (!_pVorbisInfo)
			return;

		int queued;
		alGetSourcei(_source, AL_BUFFERS_QUEUED, &queued);
		while (queued > 0)
		{
			ALuint buffer;
			alSourceUnqueueBuffers(_source, 1, &buffer);
			queued--;
		}
		ov_pcm_seek(&_oggVorbisFile, pos);
	}
	_cv.notify_one();
}

void StreamPlayer::ThreadProc()
{
	VERUS_RT_ASSERT(IsInitialized());
	try
	{
		SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
		while (!IsFlagSet(StreamPlayerFlags::stopThread))
		{
			VERUS_LOCK(*this);

			_cv.wait_for(lock, std::chrono::milliseconds(530));

			if (IsFlagSet(StreamPlayerFlags::play))
			{
				SetFlag(StreamPlayerFlags::noLock);
				if (_pTrack)
				{
					if (_pTrack->IsLoaded() && !_pVorbisInfo) // Loaded but not started?
					{
						SwitchToTrack(_pTrack);
						VERUS_RT_ASSERT(_pVorbisInfo);
					}

					if (_pVorbisInfo) // Can get the data?
					{
						int queued;
						alGetSourcei(_source, AL_BUFFERS_QUEUED, &queued);
						if (!queued)
						{
							FillBuffer(_buffers[0]);
							FillBuffer(_buffers[1]);
							alSourceQueueBuffers(_source, 2, _buffers);
							alSourcePlay(_source);
						}

						int processed;
						alGetSourcei(_source, AL_BUFFERS_PROCESSED, &processed);

#ifdef VERUS_RELEASE_DEBUG
						char debug[80];
						sprintf_s(debug, "ThreadProc() processed=%d", processed);
						VERUS_LOG_DEBUG(debug);
#endif

						while (processed > 0)
						{
							ALuint buffer;
							alSourceUnqueueBuffers(_source, 1, &buffer);
							FillBuffer(buffer);
							alSourceQueueBuffers(_source, 1, &buffer);
							processed--;
						}

						int state;
						alGetSourcei(_source, AL_SOURCE_STATE, &state);
						if (AL_STOPPED == state)
							alSourcePlay(_source);
					}
				}
				else // No track? Stop the music:
				{
					alSourceStop(_source);
				}
				ResetFlag(StreamPlayerFlags::noLock);
			}
		}
		alSourceStop(_source);
	}
	catch (D::RcRuntimeError e)
	{
		D::Log::I().Write(e.what(), e.GetThreadID(), e.GetFile(), e.GetLine(), D::Log::Severity::error);
	}
	catch (const std::exception& e)
	{
		VERUS_LOG_ERROR(e.what());
	}
}

void StreamPlayer::StopThread()
{
	if (_thread.joinable())
	{
		SetFlag(StreamPlayerFlags::stopThread);
		Stop();
		_thread.join();
	}
}

void StreamPlayer::FillBuffer(ALuint buffer)
{
	VERUS_RT_ASSERT(IsInitialized());
	int bitstream, oggCursor = 0, safeSize = Utils::Cast32(_vMediumBuffer.size() - _vSmallBuffer.size());
	while (oggCursor < safeSize)
	{
		long count = -1;
		if (_pTrack && _pTrack->IsLoaded())
			count = ov_read(&_oggVorbisFile, reinterpret_cast<char*>(_vSmallBuffer.data()), Utils::Cast32(_vSmallBuffer.size()), 0, 2, 1, &bitstream);
		if (count > 0)
		{
			memcpy(&_vMediumBuffer[oggCursor], _vSmallBuffer.data(), count);
			oggCursor += count;
		}
		else if (0 == count) // No more data?
		{
			if (!_vTracks.empty()) // Next one in the playlist?
			{
				_currentTrack++;
				_currentTrack %= _vTracks.size();
				SwitchToTrack(_vTracks[_currentTrack]);
			}
			else
				ov_raw_seek(&_oggVorbisFile, 0); // Start from the beginning.
		}
		else
			safeSize = 0; // Still loading? Exit.
	}
	if (_pVorbisInfo)
		alBufferData(buffer, _format, _vMediumBuffer.data(), oggCursor, _pVorbisInfo->rate);
}

void StreamPlayer::FadeIn(float time)
{
	_fade.Plan(1, time);
}

void StreamPlayer::FadeOut(float time)
{
	_fade.Plan(0, time);
}

void StreamPlayer::Mute()
{
	_fade.Set(0, 0);
}

void StreamPlayer::SetGain(float gain)
{
	_gain = gain;
}
