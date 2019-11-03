#include "verus.h"

using namespace verus;
using namespace verus::Audio;

// Source:

Source::Source()
{
}

Source::~Source()
{
	Done();
}

void Source::Done()
{
	if (_sid)
	{
		alDeleteSources(1, &_sid);
		_sid = 0;
		_pSound = nullptr;
	}
}

void Source::Update()
{
	if (!_sid)
		return;

	VERUS_QREF_TIMER;

	int state;
	alGetSourcei(_sid, AL_SOURCE_STATE, &state);
	if (AL_INITIAL == state)
	{
		_travelDelay -= dt;
		if (_travelDelay <= 0)
		{
			if (_pSound->HasRandomOffset())
				alSourcef(_sid, AL_SEC_OFFSET, _pSound->GetRandomOffset());
			alSourcePlay(_sid);
		}
	}
}

void Source::UpdateHRTF(bool is3D)
{
	if (!_sid)
		return;

	int state;
	alGetSourcei(_sid, AL_SOURCE_STATE, &state);
	if (AL_STOPPED == state)
	{
		alDeleteSources(1, &_sid);
		_sid = 0;
	}
	else if (is3D)
	{
		alSourcefv(_sid, AL_POSITION, _position.ToPointer());
		alSourcefv(_sid, AL_DIRECTION, _direction.ToPointer());
		alSourcefv(_sid, AL_VELOCITY, _velocity.ToPointer());
	}
}

void Source::Play()
{
	if (!this)
		return; // For NewSource()-> pattern.
	if (_pSound && _pSound->IsLoaded())
	{
		if (_pSound->HasRandomOffset())
			alSourcef(_sid, AL_SEC_OFFSET, _pSound->GetRandomOffset());
		alSourcePlay(_sid);
	}
}

void Source::PlayAt(RcPoint3 pos, RcVector3 dir, RcVector3 vel, float delay)
{
	if (!this)
		return; // For NewSource()-> pattern.
	VERUS_QREF_ASYS;
	if (_pSound && _pSound->IsLoaded())
	{
		_travelDelay = asys.ComputeTravelDelay(pos) + delay;
		MoveTo(pos, dir, vel);
		UpdateHRTF(_pSound->IsFlagSet(SoundFlags::is3D));
	}
}

void Source::Stop()
{
	if (_pSound && _pSound->IsLoaded())
		alSourceStop(_sid);
}

void Source::MoveTo(RcPoint3 pos, RcVector3 dir, RcVector3 vel)
{
	if (_pSound && _pSound->IsLoaded())
	{
		_position = pos;
		_direction = dir;
		_velocity = vel;
	}
}

void Source::SetGain(float gain)
{
	if (_pSound && _pSound->IsLoaded())
		alSourcef(_sid, AL_GAIN, gain * _pSound->GetGain().GetRandomValue());
}

void Source::SetPitch(float pitch)
{
	if (_pSound && _pSound->IsLoaded())
		alSourcef(_sid, AL_PITCH, pitch * _pSound->GetPitch().GetRandomValue());
}

// SourcePtr:

void SourcePtr::Attach(Source* pSource, Sound* pSound)
{
	_p = pSource;
	_p->_pSound = pSound;
}

void SourcePtr::Done()
{
	if (_p)
	{
		_p->Done();
		_p = nullptr;
	}
}
