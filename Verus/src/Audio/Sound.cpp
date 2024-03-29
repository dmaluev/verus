// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Audio;

// Sound:

Sound::Sound()
{
}

Sound::~Sound()
{
	Done();
}

void Sound::Init(RcDesc desc)
{
	if (_refCount)
		return;

	VERUS_INIT();

	_url = desc._url;
	_refCount = 1;
	if (desc._is3D) SetFlag(SoundFlags::is3D);
	if (desc._looping) SetFlag(SoundFlags::looping);
	if (desc._randomOffset) SetFlag(SoundFlags::randOff);
	if (desc._keepPcmBuffer) SetFlag(SoundFlags::keepPcmBuffer);
	_gain = desc._gain;
	_pitch = desc._pitch;
	_referenceDistance = desc._referenceDistance;

	IO::Async::I().Load(desc._url, this);
}

bool Sound::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		IO::Async::Cancel(this);
		VERUS_FOR(i, VERUS_COUNT_OF(_sources))
			_sources[i].Done();
		if (_buffer)
			alDeleteBuffers(1, &_buffer);
		VERUS_DONE(Sound);
		return true;
	}
	return false;
}

void Sound::Update()
{
	if (!IsLoaded())
		return;
	VERUS_UPDATE_ONCE_CHECK;

	VERUS_FOR(i, VERUS_COUNT_OF(_sources))
		_sources[i].Update();
}

void Sound::UpdateHRTF()
{
	if (!IsLoaded())
		return;
	const bool is3D = IsFlagSet(SoundFlags::is3D);
	VERUS_FOR(i, VERUS_COUNT_OF(_sources))
		_sources[i].UpdateHRTF(is3D);
}

void Sound::Async_WhenLoaded(CSZ url, RcBlob blob)
{
	VERUS_RT_ASSERT(_url == url);
	VERUS_RT_ASSERT(!_buffer);
	VERUS_RT_ASSERT(blob._size);

	OggDataSource oggds;
	oggds._p = blob._p;
	oggds._size = blob._size;

	OggVorbis_File ovf;
	vorbis_info* povi;
	const int ret = ov_open_callbacks(&oggds, &ovf, 0, 0, g_oggCallbacks);
	if (ret < 0)
		throw VERUS_RUNTIME_ERROR << "ov_open_callbacks(); " << ret;

	povi = ov_info(&ovf, -1);
	const ALenum format = (povi->channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

	alGenBuffers(1, &_buffer);

	_length = static_cast<float>(ov_time_total(&ovf, -1));
	const INT64 pcmSize = ov_pcm_total(&ovf, -1) * 2 * povi->channels + 1;
	_vPcmBuffer.resize(pcmSize);
	int bitstream, offset = 0;
	long count = ov_read(&ovf, reinterpret_cast<char*>(_vPcmBuffer.data()), Utils::Cast32(_vPcmBuffer.size()), 0, 2, 1, &bitstream);
	do
	{
		offset += count;
		count = ov_read(&ovf, reinterpret_cast<char*>(&_vPcmBuffer[offset]), Utils::Cast32(_vPcmBuffer.size()) - offset, 0, 2, 1, &bitstream);
	} while (count > 0);
	alBufferData(_buffer, format, _vPcmBuffer.data(), offset, povi->rate);
	ov_clear(&ovf);

	if (!IsFlagSet(SoundFlags::keepPcmBuffer))
	{
		_vPcmBuffer.clear();
		_vPcmBuffer.shrink_to_fit();
	}

	SetFlag(SoundFlags::loaded);
}

SourcePtr Sound::NewSource(PSourcePtr pID, Source::RcDesc desc)
{
	SourcePtr source;
	if (IsLoaded())
	{
		source.Attach(_sources + _next, this);
		VERUS_CIRCULAR_ADD(_next, VERUS_COUNT_OF(_sources));

		if (source->_sid) // Delete existing?
			alDeleteSources(1, &source->_sid);

		alGenSources(1, &source->_sid);
		const ALuint sid = source->_sid;
		if (IsFlagSet(SoundFlags::is3D))
		{
			alSourcef(sid, AL_REFERENCE_DISTANCE, _referenceDistance);
			alSourcef(sid, AL_ROLLOFF_FACTOR, 4);
		}
		else // No 3D effect?
		{
			alSourcei(sid, AL_SOURCE_RELATIVE, AL_TRUE);
			alSource3f(sid, AL_POSITION, 0, 0, 0);
			alSource3f(sid, AL_VELOCITY, 0, 0, 0);
			alSourcef(sid, AL_ROLLOFF_FACTOR, 0);
		}
		alSourcef(sid, AL_PITCH, Math::Clamp<float>(_pitch.GetRandomValue() * desc._pitch, 0.5f, 2));
		alSourcei(sid, AL_LOOPING, IsFlagSet(SoundFlags::looping) ? 1 : 0);
		alSourcei(sid, AL_BUFFER, _buffer);
		alSourcef(sid, AL_GAIN, Math::Clamp<float>(_gain.GetRandomValue() * desc._gain, 0, 1));

		if (desc._secOffset < 0)
		{
			VERUS_QREF_UTILS;
			ALint size = 0;
			alGetBufferi(_buffer, AL_SIZE, &size);
			const ALint offset = utils.GetRandom().Next() % size;
			alSourcei(sid, AL_BYTE_OFFSET, offset);
		}
		else if (desc._secOffset > 0)
		{
			alSourcef(sid, AL_SEC_OFFSET, desc._secOffset);
		}
	}
	if (pID) // Adjust this source later?
	{
		if (desc._stopSource && *pID)
			(*pID)->Stop();
		*pID = source;
	}
	return source;
}

float Sound::GetRandomOffset() const
{
	return _length * Utils::I().GetRandom().NextFloat();
}

Blob Sound::GetPcmBuffer() const
{
	return Blob(_vPcmBuffer.data(), _vPcmBuffer.size());
}

// SoundPtr:

void SoundPtr::Init(Sound::RcDesc desc)
{
	VERUS_QREF_ASYS;
	VERUS_RT_ASSERT(!_p);
	_p = asys.InsertSound(desc._url);
	_p->Init(desc);
}

void SoundPwn::Done()
{
	if (_p)
	{
		AudioSystem::I().DeleteSound(_C(_p->GetURL()));
		_p = nullptr;
	}
}
