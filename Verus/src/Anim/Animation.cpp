#include "verus.h"

using namespace verus;
using namespace verus::Anim;

// Collection:

Collection::Collection()
{
}

Collection::~Collection()
{
	DeleteAll();
}

void Collection::Async_Run(CSZ url, RcBlob blob)
{
	IO::StreamPtr sp(blob);
	CSZ niceName = strrchr(url, '/');
	niceName = niceName ? niceName + 1 : url;
	RMotionData md = _map[niceName];
	md._motion.Init();
	md._motion.Deserialize(sp);
	if (md._duration)
		md._motion.ComputePlaybackSpeed(md._duration);
}

void Collection::AddMotion(CSZ name, bool loop, float duration)
{
	CSZ niceName = strrchr(name, '/');
	niceName = niceName ? niceName + 1 : name;
	RMotionData md = _map[niceName];
	md._duration = duration;
	md._loop = loop;
	IO::Async::I().Load(name, this);
}

void Collection::DeleteAll()
{
	IO::Async::Cancel(this);
	_map.clear();
}

PMotionData Collection::Find(CSZ name)
{
	return TStoreMotions::Find(name);
}

int Collection::GetMaxBones()
{
	int count = 0;
	VERUS_FOREACH(TStoreMotions::TMap, _map, it)
		count = Math::Max(count, it->second._motion.GetBoneCount());
	return count;
}

// Animation:

Animation::Animation()
{
}

Animation::~Animation()
{
}

void Animation::Update(int alphaTrackCount, PAlphaTrack pAlphaTracks)
{
	VERUS_QREF_TIMER;

	// Async BindCollection():
	if (_vTriggerStates.size() != _pCollection->GetMaxBones())
		_vTriggerStates.resize(_pCollection->GetMaxBones());

	// Async BindSkeleton():
	if (!_blendMotion.GetBoneCount() && _pSkeleton->GetBoneCount())
		_pSkeleton->InsertBonesIntoMotion(_blendMotion);

	if (_playing)
	{
		if (_blending)
		{
			_blendTimer += dt;
			if (_blendTimer >= _blendDuration)
				_blending = false;
		}
		else if (!_currentMotion.empty())
		{
			RMotionData md = *_pCollection->Find(_C(_currentMotion));
			const float len = md._motion.GetDuration();
			if (_time < len + 0.5f)
			{
				_time += dt;
				md._motion.ProcessTriggers(_time, this, GetTriggerStatesArray());
				if (_time >= len)
				{
					if (_pDelegate)
						_pDelegate->Animation_OnEnd(_C(_currentMotion));
					_time = md._loop ? fmod(_time, len) : len + 1;
					md._motion.ResetTriggers(GetTriggerStatesArray()); // New loop, reset triggers.
				}
			}
		}
	}

	if (!_currentMotion.empty() && alphaTrackCount >= 0) // Alpha track (-1) should not modify the skeleton.
	{
		RMotionData md = *_pCollection->Find(_C(_currentMotion));

		int alphaMotionCount = 0;
		Skeleton::AlphaMotion alphaMotions[4];

		for (int i = 0; i < alphaTrackCount && i < 4; ++i)
		{
			alphaMotions[i]._pMotion = pAlphaTracks[i]._pAnimation->GetMotion(); // Can be in blend state.
			alphaMotions[i]._rootBone = pAlphaTracks[i]._rootBone;
			alphaMotions[i]._alpha = pAlphaTracks[i]._pAnimation->GetAlpha();
			alphaMotions[i]._time = pAlphaTracks[i]._pAnimation->GetTime();
			alphaMotionCount++;
		}

		if (_blending)
		{
			md._motion.BindBlendMotion(&_blendMotion, _blendTimer / _blendDuration);
			_pSkeleton->ApplyMotion(md._motion, _time, alphaMotionCount, alphaMotions);
		}
		else
		{
#ifdef VERUS_COMPARE_MODE
			_pSkeleton->ApplyMotion(md._motion, 0, alphaMotionCount, alphaMotions);
#else
			_pSkeleton->ApplyMotion(md._motion, _time, alphaMotionCount, alphaMotions);
#endif
		}
	}
}

void Animation::BindCollection(PCollection p)
{
	_pCollection = p;
}

void Animation::BindSkeleton(PSkeleton p)
{
	_pSkeleton = p;
}

void Animation::SetCurrentMotion(CSZ name)
{
	// Reset triggers:
	if (!_currentMotion.empty())
		_pCollection->Find(_C(_currentMotion))->_motion.ResetTriggers(GetTriggerStatesArray());
	if (name)
	{
		_pCollection->Find(name)->_motion.ResetTriggers(GetTriggerStatesArray());
		_currentMotion = name;
	}
	else
		_currentMotion.clear();
}

void Animation::Play()
{
	_playing = true;
}

void Animation::Stop()
{
	_playing = false;
	_time = 0;
}

void Animation::Pause()
{
	_playing = false;
}

void Animation::BlendTo(CSZ name, Range<float> duration, int randTime, PMotion pMotionFrom)
{
	VERUS_QREF_UTILS;

	PMotion pMotion = pMotionFrom;
	if (!_currentMotion.empty() || pMotionFrom)
	{
		if (!pMotionFrom)
			pMotion = &_pCollection->Find(_C(_currentMotion))->_motion;
		if (_blending) // Already blending?
			pMotion->BindBlendMotion(&_blendMotion, _blendTimer / _blendDuration);
		pMotion->BakeMotionAt(_time, _blendMotion); // Capture current pose.
		pMotion->BindBlendMotion(nullptr, 0);
	}

	if ((0 == duration._max) || (_currentMotion.empty() && name && _prevMotion == name && _blendTimer / _blendDuration < 0.5f))
	{
		_blending = false; // Special case for alpha tracks.
	}
	else
	{
		_blending = true;
		_blendDuration = (_currentMotion == (name ? name : "")) ? duration._min : duration._max;
		_blendTimer = 0;
	}

	_prevMotion = _currentMotion;
	_currentMotion = name ? name : "";

	// Reset triggers:
	if (pMotion)
		pMotion->ResetTriggers(GetTriggerStatesArray());
	if (name)
	{
		pMotion = &_pCollection->Find(name)->_motion;
		pMotion->ResetTriggers(GetTriggerStatesArray());
	}

	// Reset time:
	_time = (randTime >= 0 && pMotion) ? (randTime / 255.f) * pMotion->GetDuration() : 0;
	if (_time)
		pMotion->SkipTriggers(_time, GetTriggerStatesArray());
}

bool Animation::BlendToNew(std::initializer_list<CSZ> names, Range<float> duration, int randTime, PMotion pMotionFrom)
{
	for (auto name : names)
	{
		if (_currentMotion == name)
			return false;
	}
	BlendTo(*names.begin(), duration, randTime, pMotionFrom);
	return true;
}

void Animation::Motion_OnTrigger(CSZ name, int state)
{
	if (_pDelegate)
		_pDelegate->Animation_OnTrigger(name, state);
}

PMotion Animation::GetMotion()
{
	PMotion p = nullptr;
	if (_currentMotion.empty())
	{
		if (!_prevMotion.empty())
			p = &_pCollection->Find(_C(_prevMotion))->_motion;
	}
	else
	{
		p = &_pCollection->Find(_C(_currentMotion))->_motion;
	}
	if (p && _blending)
		p->BindBlendMotion(&_blendMotion, _blendTimer / _blendDuration);
	return p;
}

float Animation::GetAlpha(CSZ name) const
{
	bool matchCurrentMotion = false;
	bool matchPrevMotion = false;
	if (name)
	{
		matchCurrentMotion = Str::StartsWith(_C(_currentMotion), name);
		matchPrevMotion = Str::StartsWith(_C(_prevMotion), name);
	}
	const bool doBlend = name ? (matchCurrentMotion || matchPrevMotion) : true;
	if (_blending && doBlend)
	{
		bool fadeIn = _prevMotion.empty() && !_currentMotion.empty();
		bool fadeOut = !_prevMotion.empty() && _currentMotion.empty();
		if (name)
		{
			fadeIn = !matchPrevMotion && matchCurrentMotion;
			fadeOut = matchPrevMotion && !matchCurrentMotion;
		}
		if (fadeIn)
			return Math::SmoothStep(0, 1, _blendTimer / _blendDuration);
		if (fadeOut)
			return Math::SmoothStep(0, 1, 1 - _blendTimer / _blendDuration);
	}
	if (name)
		return matchCurrentMotion ? 1.f : 0.f;
	else
		return _currentMotion.empty() ? 0.f : 1.f;
}

float Animation::GetTime()
{
	if (_currentMotion.empty())
	{
		if (_prevMotion.empty())
			return _time;
		else // When alpha goes to zero, use last motion frame:
			return _pCollection->Find(_C(_prevMotion))->_motion.GetDuration();
	}
	else
	{
		return _time;
	}
}

bool Animation::IsNearEdge(float t, Edge edge)
{
	PMotion pMotion = GetMotion();
	if (!pMotion)
		return false;
	const float at = _time / pMotion->GetDuration();
	return ((edge & Edge::begin) && at < t) || ((edge & Edge::end) && at >= 1 - t);
}
