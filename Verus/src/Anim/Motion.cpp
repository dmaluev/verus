// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Anim;

const float Motion::Bone::s_magicValueForCircle = 0.825f * 0.7071f * 4; // Octagon vertices will form a perfect circle.

// Motion::Bone::Rotation:

Motion::Bone::Rotation::Rotation()
{
}

Motion::Bone::Rotation::Rotation(RcQuat q)
{
	_q = q;
}

Motion::Bone::Rotation::Rotation(RcVector3 euler)
{
	euler.EulerToQuaternion(_q);
}

// Motion::Bone:

Motion::Bone::Bone(PMotion pMotion) :
	_pMotion(pMotion)
{
}

Motion::Bone::~Bone()
{
	DeleteAll();
}

void Motion::Bone::DeleteAll()
{
	_mapRot.clear();
	_mapPos.clear();
	_mapScale.clear();
	_mapTrigger.clear();
}

// Insert:
void Motion::Bone::InsertKeyframeRotation(int frame, RcQuat q)
{
	_mapRot[frame] = Rotation(q);
}
void Motion::Bone::InsertKeyframeRotation(int frame, RcVector3 euler)
{
	_mapRot[frame] = Rotation(euler);
}
void Motion::Bone::InsertKeyframePosition(int frame, RcVector3 pos)
{
	_mapPos[frame] = pos;
}
void Motion::Bone::InsertKeyframeScale(int frame, RcVector3 scale)
{
	_mapScale[frame] = scale;
}
void Motion::Bone::InsertKeyframeTrigger(int frame, int state)
{
	_mapTrigger[frame] = state;
}

// Delete:
void Motion::Bone::DeleteKeyframeRotation(int frame)
{
	VERUS_IF_FOUND_IN(TMapRot, _mapRot, frame, it)
		_mapRot.erase(it);
}
void Motion::Bone::DeleteKeyframePosition(int frame)
{
	VERUS_IF_FOUND_IN(TMapPos, _mapPos, frame, it)
		_mapPos.erase(it);
}
void Motion::Bone::DeleteKeyframeScale(int frame)
{
	VERUS_IF_FOUND_IN(TMapScale, _mapScale, frame, it)
		_mapScale.erase(it);
}
void Motion::Bone::DeleteKeyframeTrigger(int frame)
{
	VERUS_IF_FOUND_IN(TMapTrigger, _mapTrigger, frame, it)
		_mapTrigger.erase(it);
}

// Find:
bool Motion::Bone::FindKeyframeRotation(int frame, RVector3 euler, RQuat q) const
{
	euler = Vector3(0);
	q = Quat::identity();
	VERUS_IF_FOUND_IN(TMapRot, _mapRot, frame, it)
	{
		euler.EulerFromQuaternion(it->second._q);
		q = it->second._q;
		return true;
	}
	return false;
}
bool Motion::Bone::FindKeyframePosition(int frame, RVector3 pos) const
{
	pos = Vector3(0);
	VERUS_IF_FOUND_IN(TMapPos, _mapPos, frame, it)
	{
		pos = it->second;
		return true;
	}
	return false;
}
bool Motion::Bone::FindKeyframeScale(int frame, RVector3 scale) const
{
	scale = Vector3(1, 1, 1);
	VERUS_IF_FOUND_IN(TMapScale, _mapScale, frame, it)
	{
		scale = it->second;
		return true;
	}
	return false;
}
bool Motion::Bone::FindKeyframeTrigger(int frame, int& state) const
{
	state = 0;
	VERUS_IF_FOUND_IN(TMapTrigger, _mapTrigger, frame, it)
	{
		state = it->second;
		return true;
	}
	return false;
}

void Motion::Bone::ComputeRotationAt(float time, RVector3 euler, RQuat q) const
{
	int frames[4];
	Rotation keys[4];
	const float alpha = FindControlPoints(_mapRot, frames, keys, time);

	if (_flags & Flags::slerpRot)
	{
		const Rotation null(Quat(0));
		RcRotation prev = (frames[1] == -1) ? null : keys[1];
		RcRotation next = (frames[2] == -1) ? null : keys[2];
		q = VMath::slerp(alpha, prev._q, next._q);
	}
	else
	{
		const Rotation null(Quat(0));
		RcRotation prev = (frames[1] == -1) ? null : keys[1];
		RcRotation next = (frames[2] == -1) ? null : keys[2];
		q = Math::NLerp(alpha, prev._q, next._q);
	}

	PMotion pBlendMotion = _pMotion->GetBlendMotion();
	if (pBlendMotion)
	{
		PBone pBone = pBlendMotion->FindBone(_C(_name));
		if (pBone)
		{
			Vector3 eulerBlend;
			Quat qBlend;
			if (pBone->FindKeyframeRotation(0, eulerBlend, qBlend))
			{
				if (_flags & Flags::slerpRot)
					q = VMath::slerp(_pMotion->GetBlendAlpha(), qBlend, q);
				else
					q = Math::NLerp(_pMotion->GetBlendAlpha(), qBlend, q);
			}
		}
	}

	euler.EulerFromQuaternion(q);
}

void Motion::Bone::ComputePositionAt(float time, RVector3 pos) const
{
	int frames[4];
	Vector3 keys[4];
	const float alpha = FindControlPoints(_mapPos, frames, keys, time);

	if (_flags & Flags::splinePos)
	{
		const Vector3 null(0);
		if (frames[1] == -1) { frames[1] = 0; keys[1] = null; }
		if (frames[2] == -1) { frames[2] = 0; keys[2] = null; }
		// Extrapolate:
		if (frames[0] == -1) { frames[0] = frames[1] * 2 - frames[2]; keys[0] = keys[1] * 2 - keys[2]; }
		if (frames[3] == -1) { frames[3] = frames[2] * 2 - frames[1]; keys[3] = keys[2] * 2 - keys[1]; }
		// Ratio controls the direction and length of tangents. One keyframe can have different tangent lengths to match speed.
		const int intervals[3] =
		{
			frames[1] - frames[0],
			frames[2] - frames[1],
			frames[3] - frames[2]
		};
		const float ratioA = static_cast<float>(intervals[0]) / (intervals[0] + intervals[1]);
		const float ratioB = static_cast<float>(intervals[1]) / (intervals[1] + intervals[2]);
		const Vector3 tanA = VMath::lerp(ratioA, keys[1] - keys[0], keys[2] - keys[1]) * s_magicValueForCircle * (1 - ratioA);
		const Vector3 tanB = VMath::lerp(ratioB, keys[2] - keys[1], keys[3] - keys[2]) * s_magicValueForCircle * ratioB;
		pos = glm::hermite(keys[1].GLM(), tanA.GLM(), keys[2].GLM(), tanB.GLM(), alpha);
	}
	else
	{
		const Vector3 null(0);
		RcVector3 prev = (frames[1] == -1) ? null : keys[1];
		RcVector3 next = (frames[2] == -1) ? null : keys[2];
		pos = VMath::lerp(alpha, prev, next);
	}

	PMotion pBlendMotion = _pMotion->GetBlendMotion();
	if (pBlendMotion)
	{
		PBone pBone = pBlendMotion->FindBone(_C(_name));
		if (pBone)
		{
			Vector3 posBlend;
			if (pBone->FindKeyframePosition(0, posBlend))
				pos = VMath::lerp(_pMotion->GetBlendAlpha(), posBlend, pos);
		}
	}
}

void Motion::Bone::ComputeScaleAt(float time, RVector3 scale) const
{
	int frames[4];
	Vector3 keys[4];
	const float alpha = FindControlPoints(_mapScale, frames, keys, time);

	if (_flags & Flags::splineScale)
	{
		const Vector3 null(1, 1, 1);
		if (frames[1] == -1) { frames[1] = 0; keys[1] = null; }
		if (frames[2] == -1) { frames[2] = 0; keys[2] = null; }
		// Extrapolate:
		if (frames[0] == -1) { frames[0] = frames[1] * 2 - frames[2]; keys[0] = keys[1] * 2 - keys[2]; }
		if (frames[3] == -1) { frames[3] = frames[2] * 2 - frames[1]; keys[3] = keys[2] * 2 - keys[1]; }
		// Ratio controls the direction and length of tangents. One keyframe can have different tangent lengths to match speed.
		const int intervals[3] =
		{
			frames[1] - frames[0],
			frames[2] - frames[1],
			frames[3] - frames[2]
		};
		const float ratioA = static_cast<float>(intervals[0]) / (intervals[0] + intervals[1]);
		const float ratioB = static_cast<float>(intervals[1]) / (intervals[1] + intervals[2]);
		const Vector3 tanA = VMath::lerp(ratioA, keys[1] - keys[0], keys[2] - keys[1]) * s_magicValueForCircle * (1 - ratioA);
		const Vector3 tanB = VMath::lerp(ratioB, keys[2] - keys[1], keys[3] - keys[2]) * s_magicValueForCircle * ratioB;
		scale = glm::hermite(keys[1].GLM(), tanA.GLM(), keys[2].GLM(), tanB.GLM(), alpha);
	}
	else
	{
		const Vector3 null(1, 1, 1);
		RcVector3 prev = (frames[1] == -1) ? null : keys[1];
		RcVector3 next = (frames[2] == -1) ? null : keys[2];
		scale = VMath::lerp(alpha, prev, next);
	}

	PMotion pBlendMotion = _pMotion->GetBlendMotion();
	if (pBlendMotion)
	{
		PBone pBone = pBlendMotion->FindBone(_C(_name));
		if (pBone)
		{
			Vector3 scaleBlend;
			if (pBone->FindKeyframeScale(0, scaleBlend))
				scale = VMath::lerp(_pMotion->GetBlendAlpha(), scaleBlend, scale);
		}
	}
}

void Motion::Bone::ComputeTriggerAt(float time, int& state) const
{
	if (_mapTrigger.empty())
	{
		state = 0;
		return;
	}
	const int frame = static_cast<int>(_pMotion->GetFps() * time);
	TMapTrigger::const_iterator it = _mapTrigger.upper_bound(frame); // Find frame after 'time'.
	if (it != _mapTrigger.begin())
	{
		it--;
		state = it->second;
	}
	else // All keyframes are after 'time':
	{
		state = 0;
	}
}

void Motion::Bone::ComputeMatrixAt(float time, RTransform3 mat)
{
	Quat q;
	Vector3 euler, pos, scale;
	ComputeRotationAt(time, euler, q);
	ComputePositionAt(time, pos);
	ComputeScaleAt(time, scale);
	mat = VMath::appendScale(Transform3(q, pos), scale);
}

void Motion::Bone::MoveKeyframe(int direction, Channel channel, int frame)
{
	const int frameDest = (direction >= 0) ? frame + 1 : frame - 1;
	if (frameDest < 0 || frameDest >= _pMotion->GetFrameCount())
		return;

	switch (channel)
	{
	case Channel::rotation:
	{
		VERUS_IF_FOUND_IN(TMapRot, _mapRot, frame, it)
		{
			if (_mapRot.find(frameDest) == _mapRot.end())
			{
				_mapRot[frameDest] = it->second;
				_mapRot.erase(it);
			}
		}
	}
	break;
	case Channel::position:
	{
		VERUS_IF_FOUND_IN(TMapPos, _mapPos, frame, it)
		{
			if (_mapPos.find(frameDest) == _mapPos.end())
			{
				_mapPos[frameDest] = it->second;
				_mapPos.erase(it);
			}
		}
	}
	break;
	case Channel::scale:
	{
		VERUS_IF_FOUND_IN(TMapScale, _mapScale, frame, it)
		{
			if (_mapScale.find(frameDest) == _mapScale.end())
			{
				_mapScale[frameDest] = it->second;
				_mapScale.erase(it);
			}
		}
	}
	break;
	case Channel::trigger:
	{
		VERUS_IF_FOUND_IN(TMapTrigger, _mapTrigger, frame, it)
		{
			if (_mapTrigger.find(frameDest) == _mapTrigger.end())
			{
				_mapTrigger[frameDest] = it->second;
				_mapTrigger.erase(it);
			}
		}
	}
	break;
	}
}

void Motion::Bone::Serialize(IO::RStream stream, UINT16 version)
{
	stream << _flags;

	const int rotKeyframeCount = Utils::Cast32(_mapRot.size());
	const int posKeyframeCount = Utils::Cast32(_mapPos.size());
	const int scaleKeyframeCount = Utils::Cast32(_mapScale.size());
	const int triggerKeyframeCount = Utils::Cast32(_mapTrigger.size());

	stream << rotKeyframeCount;
	VERUS_FOREACH_CONST(TMapRot, _mapRot, it)
	{
		stream << it->first;
		stream.Write(&it->second._q, 16);
	}

	stream << posKeyframeCount;
	VERUS_FOREACH_CONST(TMapPos, _mapPos, it)
	{
		stream << it->first;
		stream.Write(&it->second, 12);
	}

	stream << scaleKeyframeCount;
	VERUS_FOREACH_CONST(TMapScale, _mapScale, it)
	{
		stream << it->first;
		stream.Write(&it->second, 12);
	}

	stream << triggerKeyframeCount;
	VERUS_FOREACH_CONST(TMapTrigger, _mapTrigger, it)
	{
		stream << it->first;
		stream << it->second;
	}
}

void Motion::Bone::Deserialize(IO::RStream stream, UINT16 version)
{
	_flags = Flags::none;
	if (version >= 0x0102)
		stream >> _flags;

	int rotKeyframeCount, posKeyframeCount, scaleKeyframeCount, triggerKeyframeCount, frame, state;
	Vector3 temp;
	Quat q;

	stream >> rotKeyframeCount;
	VERUS_FOR(i, rotKeyframeCount)
	{
		stream >> frame;
		stream.Read(&q, 16);
		InsertKeyframeRotation(frame, q);
	}

	stream >> posKeyframeCount;
	VERUS_FOR(i, posKeyframeCount)
	{
		stream >> frame;
		stream.Read(&temp, 12);
		InsertKeyframePosition(frame, temp);
	}

	stream >> scaleKeyframeCount;
	VERUS_FOR(i, scaleKeyframeCount)
	{
		stream >> frame;
		stream.Read(&temp, 12);
		InsertKeyframeScale(frame, temp);
	}

	stream >> triggerKeyframeCount;
	VERUS_FOR(i, triggerKeyframeCount)
	{
		stream >> frame;
		stream >> state;
		InsertKeyframeTrigger(frame, state);
	}
}

void Motion::Bone::DeleteRedundantKeyframes(float boneAccLength)
{
	// Long bones should have smaller error threshold because their influence is bigger.
	const float thresholdScale = 1.f / boneAccLength;

	// Rotation:
	{
		const float threshold = 0.0000025f * thresholdScale;

		TMapRot mapReference; // Ideal case.
		VERUS_FOR(i, _pMotion->GetFrameCount())
		{
			Quat q;
			Vector3 euler;
			ComputeRotationAt(i * _pMotion->GetFpsInv(), euler, q);
			mapReference[i] = q;
		}

		TMapRot mapSaved = _mapRot;
		do
		{
			// Which keyframe can be removed with the least error?
			float leastError = threshold;
			int frameWithLeastError = -1;
			for (const auto& kv : mapSaved)
			{
				const int excludeFrame = kv.first;
				if (!excludeFrame || _pMotion->GetFrameCount() == excludeFrame)
					continue;
				_mapRot.clear();
				for (const auto& kv2 : mapSaved)
				{
					if (kv2.first != excludeFrame)
						_mapRot[kv2.first] = kv2.second;
				}

				float accError = 0;
				VERUS_FOR(i, _pMotion->GetFrameCount())
				{
					Quat q;
					Vector3 euler;
					ComputeRotationAt(i * _pMotion->GetFpsInv(), euler, q);
					const float dot = VMath::dot(mapReference[i]._q, q);
					accError += Math::Clamp(1 - dot, 0.f, 1.f);
					if (accError >= leastError)
						break;
				}
				if (leastError > accError)
				{
					leastError = accError;
					frameWithLeastError = excludeFrame;
				}
			}

			if (leastError < threshold && frameWithLeastError >= 0)
			{
				mapSaved.erase(frameWithLeastError);
				_mapRot = mapSaved;
			}
			else
			{
				_mapRot = mapSaved;
				break;
			}
		} while (true);
	}

	// Position:
	{
		const float threshold = 0.00025f * thresholdScale;

		TMapPos mapReference; // Ideal case.
		VERUS_FOR(i, _pMotion->GetFrameCount())
		{
			Vector3 pos;
			ComputePositionAt(i * _pMotion->GetFpsInv(), pos);
			mapReference[i] = pos;
		}

		TMapPos mapSaved = _mapPos;
		do
		{
			// Which keyframe can be removed with the least error?
			float leastError = threshold;
			int frameWithLeastError = -1;
			for (const auto& kv : mapSaved)
			{
				const int excludeFrame = kv.first;
				if (!excludeFrame || _pMotion->GetFrameCount() == excludeFrame)
					continue;
				_mapPos.clear();
				for (const auto& kv2 : mapSaved)
				{
					if (kv2.first != excludeFrame)
						_mapPos[kv2.first] = kv2.second;
				}

				float accError = 0;
				VERUS_FOR(i, _pMotion->GetFrameCount())
				{
					Vector3 pos;
					ComputePositionAt(i * _pMotion->GetFpsInv(), pos);
					const Vector3 diff = mapReference[i] - pos;
					const float len = VMath::length(diff);
					accError += len + len * len;
					if (accError >= leastError)
						break;
				}
				if (leastError > accError)
				{
					leastError = accError;
					frameWithLeastError = excludeFrame;
				}
			}

			if (leastError < threshold && frameWithLeastError >= 0)
			{
				mapSaved.erase(frameWithLeastError);
				_mapPos = mapSaved;
			}
			else
			{
				_mapPos = mapSaved;
				break;
			}
		} while (true);
	}

	// Scale:
	{
		const float threshold = 0.00025f * thresholdScale;

		TMapScale mapReference; // Ideal case.
		VERUS_FOR(i, _pMotion->GetFrameCount())
		{
			Vector3 scale;
			ComputeScaleAt(i * _pMotion->GetFpsInv(), scale);
			mapReference[i] = scale;
		}

		TMapScale mapSaved = _mapScale;
		do
		{
			// Which keyframe can be removed with the least error?
			float leastError = threshold;
			int frameWithLeastError = -1;
			for (const auto& kv : mapSaved)
			{
				const int excludeFrame = kv.first;
				if (!excludeFrame || _pMotion->GetFrameCount() == excludeFrame)
					continue;
				_mapScale.clear();
				for (const auto& kv2 : mapSaved)
				{
					if (kv2.first != excludeFrame)
						_mapScale[kv2.first] = kv2.second;
				}

				float accError = 0;
				VERUS_FOR(i, _pMotion->GetFrameCount())
				{
					Vector3 scale;
					ComputeScaleAt(i * _pMotion->GetFpsInv(), scale);
					const Vector3 diff = mapReference[i] - scale;
					const float len = VMath::length(diff);
					accError += len + len * len;
					if (accError >= leastError)
						break;
				}
				if (leastError > accError)
				{
					leastError = accError;
					frameWithLeastError = excludeFrame;
				}
			}

			if (leastError < threshold && frameWithLeastError >= 0)
			{
				mapSaved.erase(frameWithLeastError);
				_mapScale = mapSaved;
			}
			else
			{
				_mapScale = mapSaved;
				break;
			}
		} while (true);
	}

	if (2 == _mapRot.size())
	{
		if (!memcmp(
			&_mapRot.begin()->second,
			&_mapRot.rbegin()->second,
			16))
			_mapRot.erase(--_mapRot.end());
	}
	if (1 == _mapRot.size())
	{
		RQuat q = _mapRot.begin()->second._q;
		if (q.IsIdentity())
			_mapRot.clear();
	}

	if (2 == _mapPos.size())
	{
		if (!memcmp(
			&_mapPos.begin()->second,
			&_mapPos.rbegin()->second,
			12))
			_mapPos.erase(--_mapPos.end());
	}

	if (2 == _mapScale.size())
	{
		if (!memcmp(
			&_mapScale.begin()->second,
			&_mapScale.rbegin()->second,
			12))
			_mapScale.erase(--_mapScale.end());
	}
}

void Motion::Bone::DeleteOddKeyframes()
{
	{
		TMapRot::iterator it = _mapRot.begin();
		while (it != _mapRot.end())
		{
			if (it->first & 0x1)
				_mapRot.erase(it++);
			else
				++it;
		}
	}
	{
		TMapPos::iterator it = _mapPos.begin();
		while (it != _mapPos.end())
		{
			if (it->first & 0x1)
				_mapPos.erase(it++);
			else
				++it;
		}
	}
	{
		TMapScale::iterator it = _mapScale.begin();
		while (it != _mapScale.end())
		{
			if (it->first & 0x1)
				_mapScale.erase(it++);
			else
				++it;
		}
	}
}

void Motion::Bone::InsertLoopKeyframes()
{
	if (!_mapRot.empty())
		_mapRot[_pMotion->GetFrameCount()] = _mapRot.begin()->second;
	if (!_mapPos.empty())
		_mapPos[_pMotion->GetFrameCount()] = _mapPos.begin()->second;
	if (!_mapScale.empty())
		_mapScale[_pMotion->GetFrameCount()] = _mapScale.begin()->second;
}

void Motion::Bone::Cut(int frame, bool before)
{
	if (before && !frame)
		return;

	{
		TMapRot::iterator it = _mapRot.begin();
		while (it != _mapRot.end())
		{
			if (before)
			{
				if (it->first >= frame)
					_mapRot[it->first - frame] = it->second;
				_mapRot.erase(it++);
			}
			else
			{
				if (it->first > frame)
					_mapRot.erase(it++);
				else
					++it;
			}
		}
	}
	{
		TMapPos::iterator it = _mapPos.begin();
		while (it != _mapPos.end())
		{
			if (before)
			{
				if (it->first >= frame)
					_mapPos[it->first - frame] = it->second;
				_mapPos.erase(it++);
			}
			else
			{
				if (it->first > frame)
					_mapPos.erase(it++);
				else
					++it;
			}
		}
	}
	{
		TMapScale::iterator it = _mapScale.begin();
		while (it != _mapScale.end())
		{
			if (before)
			{
				if (it->first >= frame)
					_mapScale[it->first - frame] = it->second;
				_mapScale.erase(it++);
			}
			else
			{
				if (it->first > frame)
					_mapScale.erase(it++);
				else
					++it;
			}
		}
	}
}

void Motion::Bone::Fix(bool speedLimit)
{
	const float e = 0.02f;

	// Remove rotation keyframes:
	if (true)
	{
		Quat qBase(0);
		TMapRot::iterator it = _mapRot.begin();
		bool hasBroken = false;
		while (it != _mapRot.end())
		{
			RcQuat q = it->second._q;
			const bool broken =
				Math::IsNaN(q.getX()) ||
				Math::IsNaN(q.getY()) ||
				Math::IsNaN(q.getZ()) ||
				Math::IsNaN(q.getW());
			if (broken)
				hasBroken = true;
			const bool same = qBase.IsEqual(q, e);
			if (same || broken)
				_mapRot.erase(it++);
			else
			{
				qBase = q;
				++it;
			}
		}
	}

	// Remove position keyframes:
	if (true)
	{
		Vector3 base(0);
		TMapPos::iterator it = _mapPos.begin();
		while (it != _mapPos.end())
		{
			RcVector3 pos = it->second;
			const bool broken =
				Math::IsNaN(pos.getX()) ||
				Math::IsNaN(pos.getY()) ||
				Math::IsNaN(pos.getZ());
			const bool same = base.IsEqual(pos, e);
			if (same || broken)
				_mapPos.erase(it++);
			else
			{
				base = pos;
				++it;
			}
		}
	}

	// Limit rotation speed:
	if (speedLimit && Skeleton::IsKinectBone(_C(_name)))
	{
		VERUS_FOREACH(TMapRot, _mapRot, it)
		{
			RQuat q = it->second._q;
			if (it->first >= 1)
			{
				Quat qPrev;
				Vector3 euler;
				ComputeRotationAt((it->first - 1) * _pMotion->GetFpsInv(), euler, qPrev);
				const float threshold = 0.5f;
				const Quat qPrevInv = VMath::inverse(Matrix3(qPrev));
				const Quat qD = q * qPrevInv;
				const float a = abs(glm::angle(qD.GLM()));
				if (a > threshold)
				{
					const float back = Math::Clamp((a - threshold) / threshold, 0.f, 0.75f);
					q = VMath::slerp(back, q, qPrev);
				}
			}
		}
	}
}

void Motion::Bone::ApplyScaleBias(RcVector3 scale, RcVector3 bias)
{
	VERUS_FOREACH(TMapPos, _mapPos, it)
		it->second = VMath::mulPerElem(it->second, scale) + bias;
}

void Motion::Bone::Scatter(int srcFrom, int srcTo, int dMin, int dMax)
{
	VERUS_QREF_UTILS;
	int start = srcFrom;
	const int range = dMax - dMin;
	while (true)
	{
		start += dMin + utils.GetRandom().Next() % range;
		if (start >= _pMotion->GetFrameCount())
			break;
		for (int i = srcFrom; i < srcTo; ++i)
		{
			Quat q;
			Vector3 euler, pos;
			if (FindKeyframeRotation(i, euler, q))
				InsertKeyframeRotation(i + start, q);
			if (FindKeyframePosition(i, pos))
				InsertKeyframePosition(i + start, pos);
		}
	}
}

bool Motion::Bone::SpaceTimeSync(Channel channel, int fromFrame, int toFrame)
{
	if (fromFrame == toFrame) // Endpoints must not match.
		return false;
	switch (channel)
	{
	case Channel::position: return SpaceTimeSyncTemplate(_mapPos, fromFrame, toFrame);
	case Channel::scale:    return SpaceTimeSyncTemplate(_mapScale, fromFrame, toFrame);
	}
	return false;
}

int Motion::Bone::GetLastKeyframe() const
{
	int frame = -1;
	if (!_mapRot.empty())
		frame = Math::Max(frame, _mapRot.rbegin()->first);
	if (!_mapPos.empty())
		frame = Math::Max(frame, _mapPos.rbegin()->first);
	if (!_mapScale.empty())
		frame = Math::Max(frame, _mapScale.rbegin()->first);
	if (!_mapTrigger.empty())
		frame = Math::Max(frame, _mapTrigger.rbegin()->first);
	return frame;
}

// Motion:

Motion::Motion()
{
}

Motion::~Motion()
{
	Done();
}

void Motion::Init()
{
	VERUS_INIT();
}

void Motion::Done()
{
	DeleteAllBones();
	VERUS_DONE(Motion);
}

Motion::PBone Motion::GetBoneByIndex(int index)
{
	int i = 0;
	for (auto& kv : _mapBones)
	{
		if (i == index)
			return &kv.second;
		i++;
	}
	return nullptr;
}

int Motion::GetBoneIndex(CSZ name) const
{
	int i = 0;
	VERUS_FOREACH_CONST(TMapBones, _mapBones, it)
	{
		if (it->first == name)
			return i;
		i++;
	}
	return -1;
}

Motion::PBone Motion::InsertBone(CSZ name)
{
	PBone pBone = FindBone(name);
	if (pBone)
		return pBone;
	Bone bone(this);
	bone.Rename(name);
	_mapBones[name] = bone;
	return &_mapBones[name];
}

void Motion::DeleteBone(CSZ name)
{
	VERUS_IF_FOUND_IN(TMapBones, _mapBones, name, it)
		_mapBones.erase(it);
}

void Motion::DeleteAllBones()
{
	_mapBones.clear();
}

Motion::PBone Motion::FindBone(CSZ name)
{
	VERUS_IF_FOUND_IN(TMapBones, _mapBones, name, it)
		return &it->second;
	return nullptr;
}

void Motion::Serialize(IO::RStream stream)
{
	const UINT32 magic = '2NAX';
	stream << magic;

	const UINT16 version = s_xanVersion;
	stream << version;

	stream << _frameCount;
	stream << _fps;
	stream << GetBoneCount();

	for (auto& kv : _mapBones)
	{
		RBone bone = kv.second;
		stream.WriteString(_C(bone.GetName()));
		bone.Serialize(stream, version);
	}
}

void Motion::Deserialize(IO::RStream stream)
{
	UINT32 magic = 0;
	stream >> magic;
	if ('2NAX' != magic)
		throw VERUS_RECOVERABLE << "Deserialize(); Invalid XAN format";

	UINT16 version = 0;
	stream >> version;
	if (s_xanVersion < version)
		throw VERUS_RECOVERABLE << "Deserialize(); Invalid XAN version";

	stream >> _frameCount;
	stream >> _fps;
	if (_frameCount < 0 || _frameCount > s_maxFrames)
		throw VERUS_RECOVERABLE << "Deserialize(); Invalid number of frames in XAN";
	if (_fps <= 0 || _fps > s_maxFps)
		throw VERUS_RECOVERABLE << "Deserialize(); Invalid FPS in XAN";

	SetFps(_fps);

	int boneCount = 0;
	stream >> boneCount;
	if (boneCount < 0 || boneCount > s_maxBones)
		throw VERUS_RECOVERABLE << "Deserialize(); Invalid number of bones in XAN";

	char buffer[IO::Stream::s_bufferSize] = {};
	VERUS_FOR(i, boneCount)
	{
		stream.ReadString(buffer);
		PBone pBone = InsertBone(buffer);
		pBone->Deserialize(stream, version);
	}
}

void Motion::BakeMotionAt(float time, Motion& dest) const
{
	float nativeTime = time * _playbackSpeed;
	if (_reversed)
		nativeTime = GetNativeDuration() - nativeTime;

	VERUS_FOREACH_CONST(TMapBones, _mapBones, it)
	{
		PcBone pBone = &it->second;
		PBone pBoneDest = dest.FindBone(_C(it->first));
		if (pBoneDest)
		{
			Vector3 rot, pos, scale;
			Quat q;

			pBone->ComputeRotationAt(nativeTime, rot, q);
			pBone->ComputePositionAt(nativeTime, pos);
			pBone->ComputeScaleAt(nativeTime, scale);

			pBoneDest->InsertKeyframeRotation(0, q);
			pBoneDest->InsertKeyframePosition(0, pos);
			pBoneDest->InsertKeyframeScale(0, scale);
		}
	}
}

void Motion::BindBlendMotion(PMotion p, float alpha)
{
	_pBlendMotion = p;
	_blendAlpha = alpha;
}

void Motion::DeleteRedundantKeyframes(Map<String, float>& mapBoneAccLengths)
{
	VERUS_P_FOR(i, static_cast<int>(_mapBones.size()))
	{
		auto it = _mapBones.begin();
		std::advance(it, i);
		auto itBoneAccLength = mapBoneAccLengths.find(_C(it->second.GetName()));
		const float leafBoneLength = 0.02f;
		it->second.DeleteRedundantKeyframes(mapBoneAccLengths.end() != itBoneAccLength ? itBoneAccLength->second : leafBoneLength);
	});
}

void Motion::DeleteOddKeyframes()
{
	for (auto& kv : _mapBones)
		kv.second.DeleteOddKeyframes();
}

void Motion::InsertLoopKeyframes()
{
	for (auto& kv : _mapBones)
		kv.second.InsertLoopKeyframes();
}

void Motion::Cut(int frame, bool before)
{
	for (auto& kv : _mapBones)
		kv.second.Cut(frame, before);
	_frameCount = before ? _frameCount - frame : frame + 1;
}

void Motion::Fix(bool speedLimit)
{
	for (auto& kv : _mapBones)
		kv.second.Fix(speedLimit);
}

void Motion::ProcessTriggers(float time, PMotionDelegate p, int* pUserTriggerStates)
{
	VERUS_RT_ASSERT(p);

	float nativeTime = time * _playbackSpeed;
	if (_reversed)
		nativeTime = GetNativeDuration() - nativeTime;

	int i = 0;
	for (auto& kv : _mapBones)
	{
		PBone pBone = &kv.second;
		int state = 0;
		if (!(_reversed && nativeTime < _fpsInv * 0.5f)) // Avoid edge case for the first frame in reverse.
			pBone->ComputeTriggerAt(nativeTime, state);
		const int last = pUserTriggerStates ? pUserTriggerStates[i] : pBone->GetLastTriggerState();
		if (state != last)
		{
			p->Motion_OnTrigger(_C(pBone->GetName()), state);
			if (pUserTriggerStates)
				pUserTriggerStates[i] = state;
			else
				pBone->SetLastTriggerState(state);
		}
		i++;
	}
}

void Motion::ResetTriggers(int* pUserTriggerStates)
{
	int i = 0;
	for (auto& kv : _mapBones)
	{
		PBone pBone = &kv.second;
		int state = 0;
		if (_reversed)
			pBone->ComputeTriggerAt(GetNativeDuration(), state);
		if (pUserTriggerStates)
			pUserTriggerStates[i] = state;
		else
			pBone->SetLastTriggerState(state);
		i++;
	}
}

void Motion::SkipTriggers(float time, int* pUserTriggerStates)
{
	float nativeTime = time * _playbackSpeed;
	if (_reversed)
		nativeTime = GetNativeDuration() - nativeTime;

	int i = 0;
	for (auto& kv : _mapBones)
	{
		PBone pBone = &kv.second;
		int state;
		pBone->ComputeTriggerAt(nativeTime, state);
		if (pUserTriggerStates)
			pUserTriggerStates[i] = state;
		else
			pBone->SetLastTriggerState(state);
		i++;
	}
}

void Motion::ApplyScaleBias(CSZ name, RcVector3 scale, RcVector3 bias)
{
	PBone pBone = FindBone(name);
	if (pBone)
		pBone->ApplyScaleBias(scale, bias);
}

void Motion::SetPlaybackSpeed(float x)
{
	VERUS_RT_ASSERT(x != 0);
	_playbackSpeed = x;
	_reversed = x < 0;
	if (_reversed)
		_playbackSpeed = -_playbackSpeed;
	_playbackSpeedInv = 1 / _playbackSpeed;
}

void Motion::ComputePlaybackSpeed(float duration)
{
	VERUS_RT_ASSERT(duration != 0);
	_playbackSpeed = GetNativeDuration() / duration;
	_reversed = duration < 0;
	if (_reversed)
		_playbackSpeed = -_playbackSpeed;
	_playbackSpeedInv = 1 / _playbackSpeed;
}

void Motion::Exec(CSZ code, PBone pBone, Bone::Channel channel)
{
	if (Str::StartsWith(code, "copy "))
	{
		char boneSrc[80] = {};
		char boneDst[80] = {};
		sscanf(code, "%*s %s %s", boneSrc, boneDst);
		PBone pSrc = FindBone(boneSrc);
		PBone pDst = FindBone(boneDst);
		pDst->_mapRot = pSrc->_mapRot;
		pDst->_mapPos = pSrc->_mapPos;
	}
	if (Str::StartsWith(code, "scatter "))
	{
		int srcFrom = 0, srcTo = 0;
		int dMin = 0, dMax = 0;
		char bone[80] = {};
		sscanf(code, "%*s %d %d %d %d %s", &srcFrom, &srcTo, &dMin, &dMax, bone);
		const int srcSize = srcTo - srcFrom;
		if (dMin < srcSize)
		{
			const int d = srcSize - dMin;
			dMin += d;
			dMax += d;
		}
		PBone p = FindBone(bone + 5);
		if (p)
			p->Scatter(srcFrom, srcTo, dMin, dMax);
	}
	if (Str::StartsWith(code, "delete "))
	{
		char what[80] = {};
		char name[80] = {};
		sscanf(code, "%*s %s %s", what, name);
		if (strlen(name) > 5)
		{
			PBone p = FindBone(name + 5);
			if (p)
			{
				if (!strcmp(what, "rot"))
					p->_mapRot.clear();
				if (!strcmp(what, "pos"))
					p->_mapPos.clear();
				if (!strcmp(what, "scale"))
					p->_mapScale.clear();
			}
		}
		else
		{
			for (auto& bone : _mapBones)
			{
				if (!strcmp(what, "rot"))
					bone.second._mapRot.clear();
				if (!strcmp(what, "pos"))
					bone.second._mapPos.clear();
				if (!strcmp(what, "scale"))
					bone.second._mapScale.clear();
			}
		}
	}
	if (pBone && Str::StartsWith(code, "sts "))
	{
		int from = 0, to = 0;
		sscanf(code, "%*s %d %d", &from, &to);
		pBone->SpaceTimeSync(channel, from, to);
	}
}

int Motion::GetLastKeyframe() const
{
	int frame = -1;
	VERUS_FOREACH_CONST(TMapBones, _mapBones, it)
		frame = Math::Max(frame, it->second.GetLastKeyframe());
	return frame;
}

bool Motion::ExtractNestBone(CSZ name, SZ nestBone)
{
	CSZ pBegin = strstr(name, "[");
	CSZ pEnd = strstr(name, "]:");
	if (pBegin && pEnd && pBegin < pEnd)
	{
		const size_t count = pEnd - pBegin - 1;
		strncpy(nestBone, pBegin + 1, count);
		nestBone[count] = 0;
		return true;
	}
	return false;
}
