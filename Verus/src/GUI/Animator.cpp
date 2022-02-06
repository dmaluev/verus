// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::GUI;

// Animated:

template<>
Vector4 Animated<Vector4>::Lerp(RcVector4 from, RcVector4 to, float ratio)
{
	return VMath::lerp(ratio, from, to);
}

// Animator::Video:

void Animator::Video::Update(float dt)
{
	if (_invDuration > 0)
	{
		if (_time >= 0)
			_time += dt;
		const float ratio = Math::Clamp<float>((_time - _delay) * _invDuration, 0, 1);
		const int frame = Math::Clamp(int(ratio * _frameCount), 0, _frameCount - 1);
		const int col = frame % _columnCount;
		const int row = frame / _columnCount;
		_uBias = col * _uScale;
		_vBias = row * _vScale;
		if (dt > 0 && ratio >= 1)
			_invDuration = -_invDuration;
	}
}

void Animator::Video::Reset(float reverseTime)
{
	_uBias = 0;
	_vBias = 0;
	_time = reverseTime;
	_invDuration = abs(_invDuration);
	Update(0);
}

// Animator:

Animator::Animator()
{
}

Animator::~Animator()
{
}

bool Animator::Update()
{
	VERUS_QREF_TIMER_GUI;
	bool ret = false;

	_angle = fmod(_angle + _angleSpeed * dt, VERUS_2PI);

	const float dt2 = _reverse ? -dt : dt;
	_animatedColor.Update(dt2);
	_animatedRect.Update(dt2);
	_video.Update(dt2);

	_pulseScale = 1 + sin(timer.GetTime() * _pulseSpeed * VERUS_2PI) * _pulseScaleAdd;

	if (_maxTimeout >= 0)
	{
		_timeout += dt;
		if (_timeout >= _maxTimeout)
		{
			ret = true;
			_timeout = 0;
			_maxTimeout = -1;
		}
	}

	return ret;
}

void Animator::Parse(pugi::xml_node node, PcWidget pWidget)
{
	_maxTimeout = node.attribute("timeout").as_float(_maxTimeout);

	_angle = Math::ToRadians(node.attribute("angle").as_float());
	_postAngle = Math::ToRadians(node.attribute("postAngle").as_float());

	for (auto fxNode : node.children("fx"))
	{
		if (!strcmp(fxNode.attribute("mode").value(), "color"))
		{
			_animatedColor._from.FromColorString(fxNode.attribute("from").value());
			_animatedColor._to = pWidget->GetColor();
			_animatedColor._current = _animatedColor._from;
			_animatedColor._invDuration = fxNode.attribute("duration").as_float();
			_animatedColor._delay = fxNode.attribute("delay").as_float();
			_animatedColor._easing = Math::EasingFromString(fxNode.attribute("easing").value());
			if (_animatedColor._invDuration)
				_animatedColor._invDuration = 1 / _animatedColor._invDuration;
		}
		if (!strcmp(fxNode.attribute("mode").value(), "pulse"))
		{
			_pulseScaleAdd = fxNode.attribute("scale").as_float();
			_pulseSpeed = fxNode.attribute("speed").as_float();
		}
		if (!strcmp(fxNode.attribute("mode").value(), "angle"))
		{
			_angle = Math::ToRadians(fxNode.attribute("init").as_float());
			_angleSpeed = Math::ToRadians(fxNode.attribute("speed").as_float());
		}
		if (!strcmp(fxNode.attribute("mode").value(), "rect"))
		{
			_animatedRect._to = Vector4::MakeRectangle(
				pWidget->GetX(),
				pWidget->GetY(),
				pWidget->GetX() + pWidget->GetW(),
				pWidget->GetY() + pWidget->GetH());
			_animatedRect._from = _animatedRect._to;
			if (auto attr = fxNode.attribute("x"))
				_animatedRect._from.setX(ViewManager::ParseCoordX(attr.value()));
			if (auto attr = fxNode.attribute("y"))
				_animatedRect._from.setY(ViewManager::ParseCoordY(attr.value()));
			if (auto attr = fxNode.attribute("w"))
				_animatedRect._from.setZ(ViewManager::ParseCoordX(attr.value()));
			if (auto attr = fxNode.attribute("h"))
				_animatedRect._from.setW(ViewManager::ParseCoordY(attr.value()));
			_animatedRect._current = _animatedRect._from;
			_animatedRect._invDuration = fxNode.attribute("duration").as_float();
			_animatedRect._delay = fxNode.attribute("delay").as_float();
			_animatedRect._easing = Math::EasingFromString(fxNode.attribute("easing").value());
			if (_animatedRect._invDuration)
				_animatedRect._invDuration = 1 / _animatedRect._invDuration;
		}
		if (!strcmp(fxNode.attribute("mode").value(), "video"))
		{
			_video._columnCount = fxNode.attribute("cols").as_int();
			_video._frameCount = fxNode.attribute("frames").as_int();
			_video._invDuration = fxNode.attribute("duration").as_float();
			_video._delay = fxNode.attribute("delay").as_float();
			if (_video._invDuration)
				_video._invDuration = 1 / _video._invDuration;
			_video._rowCount = (_video._frameCount + _video._columnCount - 1) / _video._columnCount;
			_video._uScale = 1.f / _video._columnCount;
			_video._vScale = 1.f / _video._rowCount;
		}
	}
}

void Animator::Reset(float reverseTime)
{
	_reverse = (reverseTime > 0);
	_animatedColor.Reset(reverseTime);
	_animatedRect.Reset(reverseTime);
	_video.Reset(reverseTime);
}

RcVector4 Animator::GetColor(RcVector4 original) const
{
	return (_animatedColor._invDuration > 0) ? _animatedColor._current : original;
}

RcVector4 Animator::SetColor(RcVector4 color)
{
	_animatedColor._to = color;
	return color;
}

RcVector4 Animator::SetFromColor(RcVector4 color)
{
	_animatedColor._from = color;
	return color;
}

float Animator::GetX(float original) const
{
	return (_animatedRect._invDuration > 0) ? _animatedRect._current.getX() : original;
}

float Animator::GetY(float original) const
{
	return (_animatedRect._invDuration > 0) ? _animatedRect._current.getY() : original;
}

float Animator::GetW(float original) const
{
	return (_animatedRect._invDuration > 0) ? _animatedRect._current.getZ() : original;
}

float Animator::GetH(float original) const
{
	return (_animatedRect._invDuration > 0) ? _animatedRect._current.getW() : original;
}

float Animator::SetX(float x)
{
	_animatedRect._to.setX(x);
	return x;
}

float Animator::SetY(float y)
{
	_animatedRect._to.setY(y);
	return y;
}

float Animator::SetW(float w)
{
	_animatedRect._to.setZ(w);
	return w;
}

float Animator::SetH(float h)
{
	_animatedRect._to.setW(h);
	return h;
}

bool Animator::GetVideoBias(float& ub, float& vb) const
{
	if (_video._invDuration > 0)
	{
		ub = _video._uBias;
		vb = _video._vBias;
		return true;
	}
	return false;
}

void Animator::SetTimeout(float t)
{
	_maxTimeout = t;
	_timeout = 0;
}
