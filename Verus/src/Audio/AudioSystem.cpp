#include "verus.h"

using namespace verus;
using namespace verus::Audio;

AudioSystem::AudioSystem()
{
}

AudioSystem::~AudioSystem()
{
	Done();
}

void AudioSystem::Init()
{
	VERUS_INIT();

	const ALCchar* deviceName = nullptr;
#ifdef _WIN32
	deviceName = "DirectSound3D";
#endif

	if (!(_pDevice = alcOpenDevice(deviceName)))
		throw VERUS_RUNTIME_ERROR << "alcOpenDevice()";

	if (!(_pContext = alcCreateContext(_pDevice, nullptr)))
		throw VERUS_RUNTIME_ERROR << "alcCreateContext(), " << alcGetError(_pDevice);

	if (!alcMakeContextCurrent(_pContext))
		throw VERUS_RUNTIME_ERROR << "alcMakeContextCurrent(), " << alcGetError(_pDevice);

	int vMajor = 0, vMinor = 0;
	alcGetIntegerv(_pDevice, ALC_MAJOR_VERSION, sizeof(vMajor), &vMajor);
	alcGetIntegerv(_pDevice, ALC_MINOR_VERSION, sizeof(vMinor), &vMinor);
	StringStream ss;
	ss << vMajor << "." << vMinor;
	_version = ss.str();

	_deviceSpecifier = alcGetString(_pDevice, ALC_DEVICE_SPECIFIER);
}

void AudioSystem::Done()
{
	DeleteAllStreams();
	DeleteAllSounds();

	alcMakeContextCurrent(0);
	if (_pContext)
	{
		alcDestroyContext(_pContext);
		_pContext = nullptr;
	}
	if (_pDevice)
	{
		alcCloseDevice(_pDevice);
		_pDevice = nullptr;
	}

	VERUS_DONE(AudioSystem);
}

void AudioSystem::Update()
{
	if (!IsInitialized())
		return;
	VERUS_UPDATE_ONCE_CHECK;

	VERUS_QREF_TIMER;

	VERUS_FOR(i, VERUS_ARRAY_LENGTH(_streamPlayers))
		_streamPlayers[i].Update();

	// Update every frame:
	for (auto& x : TStoreSounds::_map)
		x.second.Update();

	// Update ~15 times per second:
	if (timer.IsEventEvery(67))
	{
		for (auto& x : TStoreSounds::_map)
			x.second.UpdateHRTF();

		if (VMath::lengthSqr(_listenerDirection) > 0.1f &&
			VMath::lengthSqr(_listenerUp) > 0.1f)
		{
			const float orien[6] =
			{
				_listenerDirection.getX(),
				_listenerDirection.getY(),
				_listenerDirection.getZ(),
				_listenerUp.getX(),
				_listenerUp.getY(),
				_listenerUp.getZ()
			};
			alListenerfv(AL_POSITION, _listenerPosition.ToPointer());
			alListenerfv(AL_VELOCITY, _listenerVelocity.ToPointer());
			alListenerfv(AL_ORIENTATION, orien); // Zero-length vectors can crash OpenAL!
		}
	}
}

void AudioSystem::DeleteAllStreams()
{
	VERUS_FOR(i, VERUS_ARRAY_LENGTH(_streamPlayers))
		_streamPlayers[i].Done();
}

PSound AudioSystem::InsertSound(CSZ url)
{
	return TStoreSounds::Insert(url);
}

PSound AudioSystem::FindSound(CSZ url)
{
	return TStoreSounds::Find(url);
}

void AudioSystem::DeleteSound(CSZ url)
{
	TStoreSounds::Delete(url);
}

void AudioSystem::DeleteAllSounds()
{
	TStoreSounds::DeleteAll();
}

void AudioSystem::UpdateListener(RcPoint3 pos, RcVector3 dir, RcVector3 vel, RcVector3 up)
{
	_listenerPosition = pos;
	_listenerDirection = dir;
	_listenerVelocity = vel;
	_listenerUp = up;
}

float AudioSystem::ComputeTravelDelay(RcPoint3 pos) const
{
	const float speed = alGetFloat(AL_SPEED_OF_SOUND);
	const float dist = VMath::dist(pos, _listenerPosition);
	return Math::Clamp<float>(dist / speed, 0, 3);
}
