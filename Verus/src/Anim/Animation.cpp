// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
	RMotionData md = *TStoreMotions::Insert(niceName);
	md._motion.Init();
	md._motion.Deserialize(sp);
	if (md._duration)
		md._motion.ComputePlaybackSpeed(md._duration);
}

void Collection::AddMotion(CSZ name, bool loop, float duration)
{
	CSZ niceName = strrchr(name, '/');
	niceName = niceName ? niceName + 1 : name;
	RMotionData md = *TStoreMotions::Insert(niceName);
	md._duration = duration;
	md._loop = loop;
	IO::Async::I().Load(name, this);
}

void Collection::DeleteAll()
{
	IO::Async::Cancel(this);
	TStoreMotions::DeleteAll();
}

PMotionData Collection::Find(CSZ name)
{
	return TStoreMotions::Find(name);
}

int Collection::GetMaxBones()
{
	int count = 0;
	VERUS_FOREACH(TStoreMotions::TMap, TStoreMotions::_map, it)
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

void Animation::Update(int layerCount, PLayer pLayers)
{
	VERUS_QREF_TIMER;

	// Async BindCollection():
	if (_vTriggerStates.size() != _pCollection->GetMaxBones())
		_vTriggerStates.resize(_pCollection->GetMaxBones());

	// Async BindSkeleton():
	if (!_transitionMotion.GetBoneCount() && _pSkeleton->GetBoneCount())
		_pSkeleton->InsertBonesIntoMotion(_transitionMotion);

	if (_playing)
	{
		float dt2 = dt;
		if (_transition) // Still in transition?
		{
			_transitionTime += dt2;
			if (_transitionTime >= _transitionDuration)
			{
				_transition = false;
				dt2 -= _transitionTime - _transitionDuration;
			}
		}
		if (!_transition && !_currentMotion.empty()) // Done transitioning?
		{
			RMotionData md = *_pCollection->Find(_C(_currentMotion));
			const float duration = md._motion.GetDuration();
			_time += dt2;
			md._motion.ProcessTriggers(_time, this, GetTriggerStatesArray());
			if (_time >= duration)
			{
				_playing = md._loop; // Continue?
				_time = md._loop ? fmod(_time, duration) : duration;
				md._motion.ResetTriggers(GetTriggerStatesArray()); // New loop, reset triggers.
				if (_pDelegate)
					_pDelegate->Animation_OnEnd(_C(_currentMotion)); // This can call TransitionTo and change everything.
			}
		}
	}

	UpdateSkeleton(layerCount, pLayers);
}

void Animation::UpdateSkeleton(int layerCount, PLayer pLayers)
{
	if (!_currentMotion.empty() && layerCount >= 0) // Special layer -1 should not modify the skeleton.
	{
		RMotionData md = *_pCollection->Find(_C(_currentMotion));

		int layeredMotionCount = 0;
		Skeleton::LayeredMotion layeredMotions[s_maxLayers];

		for (int i = 0; i < layerCount && i < s_maxLayers; ++i)
		{
			layeredMotions[i]._pMotion = pLayers[i]._pAnimation->GetMotion(); // Can be in blend state.
			layeredMotions[i]._rootBone = pLayers[i]._rootBone;
			layeredMotions[i]._alpha = pLayers[i]._pAnimation->GetAlpha();
			layeredMotions[i]._time = pLayers[i]._pAnimation->GetTime();
			layeredMotionCount++;
		}

		if (_transition)
		{
			md._motion.BindBlendMotion(&_transitionMotion, Math::ApplyEasing(_easing, _transitionTime / _transitionDuration));
			_pSkeleton->ApplyMotion(md._motion, _time, layeredMotionCount, layeredMotions);
		}
		else
		{
			md._motion.BindBlendMotion(nullptr, 0);
			_pSkeleton->ApplyMotion(md._motion, _time, layeredMotionCount, layeredMotions);
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

void Animation::TransitionTo(CSZ name, Range<float> duration, int randTime, PMotion pFromMotion)
{
	PMotion pMotion = pFromMotion;
	if (!_currentMotion.empty() || pFromMotion) // Deal with previous motion?
	{
		if (!pFromMotion)
			pMotion = &_pCollection->Find(_C(_currentMotion))->_motion;
		if (_transition) // Already in transition?
			pMotion->BindBlendMotion(&_transitionMotion, Math::ApplyEasing(_easing, _transitionTime / _transitionDuration));
		pMotion->BakeMotionAt(_time, _transitionMotion); // Capture current pose.
		pMotion->BindBlendMotion(nullptr, 0);
	}

	if ((0 == duration._max) || (_currentMotion.empty() && name && _prevMotion == name && _transitionTime / _transitionDuration < 0.5f))
	{
		_transition = false; // Special case for layers.
	}
	else
	{
		_transition = true;
		// Transition to same motion should be faster.
		_transitionDuration = (_currentMotion == (name ? name : "")) ? duration._min : duration._max;
		_transitionTime = 0;
	}

	_prevMotion = _currentMotion;
	_currentMotion = name ? name : "";

	// Reset triggers, change motion:
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

	_playing = true;
}

bool Animation::TransitionToNew(std::initializer_list<CSZ> names, Range<float> duration, int randTime, PMotion pFromMotion)
{
	for (auto name : names)
	{
		if (_currentMotion == name)
			return false;
	}
	TransitionTo(*names.begin(), duration, randTime, pFromMotion);
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
	if (p && _transition)
		p->BindBlendMotion(&_transitionMotion, Math::ApplyEasing(_easing, _transitionTime / _transitionDuration));
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
	const bool doTransition = name ? (matchCurrentMotion || matchPrevMotion) : true;
	if (_transition && doTransition)
	{
		bool fadeIn = _prevMotion.empty() && !_currentMotion.empty();
		bool fadeOut = !_prevMotion.empty() && _currentMotion.empty();
		if (name)
		{
			fadeIn = !matchPrevMotion && matchCurrentMotion;
			fadeOut = matchPrevMotion && !matchCurrentMotion;
		}
		if (fadeIn)
			return Math::ApplyEasing(_easing, _transitionTime / _transitionDuration);
		if (fadeOut)
			return Math::ApplyEasing(_easing, 1 - _transitionTime / _transitionDuration);
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
