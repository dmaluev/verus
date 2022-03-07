// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// Light:

Light::Light()
{
	_type = NodeType::light;
}

Light::~Light()
{
	Done();
}

void Light::Init(RcDesc desc)
{
	VERUS_QREF_SM;
	_name = sm.EnsureUniqueName(desc._name ? desc._name : "Light");
	_data = desc._data;
	_dynamic = desc._dynamic;
	if (desc._urlIntShaker)
	{
		_shaker.Load(desc._urlIntShaker);
		_shaker.Randomize();
		_shaker.SetScaleBias(0.5f * desc._intShakerScale, 1 - 0.5f * desc._intShakerScale);
		_baseIntensity = GetIntensity();
	}
}

void Light::Done()
{
	SceneManager::I().GetOctree().UnbindElement(this);
}

void Light::Update()
{
	if (!_async_loadedMesh)
	{
		VERUS_QREF_HELPERS;
		RMesh mesh = helpers.GetDeferredLights().Get(_data._lightType);
		if (mesh.IsLoaded())
		{
			_async_loadedMesh = true;
			UpdateBounds();
		}
	}

	if (_shaker.IsLoaded())
	{
		_shaker.Update();
		SetIntensity(_shaker.Get() * _baseIntensity);
	}
}

void Light::SetLightType(CGI::LightType type)
{
	_data._lightType = type;
	ComputeTransform();
}

CGI::LightType Light::GetLightType() const
{
	return _data._lightType;
}

void Light::SetColor(RcVector4 color)
{
	_data._color = color;
}

Vector4 Light::GetColor()
{
	return _data._color;
}

void Light::SetIntensity(float i)
{
	_data._color.setW(i);
}

float Light::GetIntensity() const
{
	return _data._color.getW();
}

void Light::SetDirection(RcVector3 dir)
{
	_data._dir = dir;
	ComputeTransform();
}

RcVector3 Light::GetDirection() const
{
	return _data._dir;
}

void Light::SetRadius(float r)
{
	_data._radius = r;
	ComputeTransform();
}

float Light::GetRadius() const
{
	return _data._radius;
}

void Light::SetCone(float coneIn, float coneOut)
{
	if (coneIn)
		_data._coneIn = coneIn;
	if (coneOut)
		_data._coneOut = coneOut;
	VERUS_RT_ASSERT(_data._coneIn >= _data._coneOut);
	ComputeTransform();
}

float Light::GetConeIn() const
{
	return _data._coneIn;
}

float Light::GetConeOut() const
{
	return _data._coneOut;
}

Vector4 Light::GetInstData() const
{
	return Vector4(_data._color.getXYZ() * _data._color.getW(), _data._coneIn);
}

void Light::DirFromPoint(RcPoint3 point, float radiusScale)
{
	const Vector3 to = point - GetPosition();
	const float len = VMath::length(to);
	if (len > VERUS_FLOAT_THRESHOLD)
	{
		_data._dir = to / len;
		if (radiusScale > 0)
			_data._radius = len * radiusScale;
	}
	ComputeTransform();
}

void Light::ConeFromPoint(RcPoint3 point, bool coneIn)
{
	const Vector3 to = point - GetPosition();
	const Vector3 dir = VMath::normalize(to);
	const float d = VMath::dot(dir, _data._dir);
	if (coneIn)
		_data._coneIn = d;
	else
		_data._coneIn = _data._coneOut = Math::Clamp<float>(d, cos(Math::ToRadians(80)), cos(Math::ToRadians(10)));
	const float angle = acos(_data._coneOut);
	const float minIn = cos(angle - Math::ToRadians(10));
	_data._coneIn = Math::Clamp<float>(_data._coneIn, minIn, 1);
	SetCone(_data._coneIn, _data._coneOut);
}

Transform3 Light::GetTransformNoScale() const
{
	const Matrix3 matR = Matrix3::MakeTrackToZ(_data._dir);
	return Transform3(matR, Vector3(GetPosition()));
}

void Light::SetTransform(RcTransform3 mat)
{
	_data._dir = VMath::normalize(mat.getCol2());
	const Transform3 tr = VMath::appendScale(mat, ComputeScale());
	SceneNode::SetTransform(tr);
}

void Light::RestoreTransform(RcTransform3 tr, RcVector3 rot, RcVector3 scale)
{
	_data._dir = VMath::normalize(tr.getCol2());
	SceneNode::RestoreTransform(tr, rot, scale);
}

Vector3 Light::ComputeScale()
{
	switch (_data._lightType)
	{
	case CGI::LightType::dir:
		return Vector3::Replicate(1);
	case CGI::LightType::omni:
		return Vector3::Replicate(_data._radius);
	case CGI::LightType::spot:
		const float angle = acos(_data._coneOut);
		const float scaleXY = tan(angle);
		return Vector3(_data._radius * scaleXY, _data._radius * scaleXY, _data._radius);
	}
	return Vector3::Replicate(_data._radius);
}

void Light::ComputeTransform()
{
	const Matrix3 matR = Matrix3::MakeTrackToZ(_data._dir);
	const Transform3 tr = VMath::appendScale(Transform3(matR, Vector3(GetPosition())), ComputeScale());
	SceneNode::SetTransform(tr);
}

void Light::UpdateBounds()
{
	const bool dir = (CGI::LightType::dir == _data._lightType);
	const bool octreeRoot = (_dynamic || dir);

	VERUS_QREF_HELPERS;
	RMesh mesh = helpers.GetDeferredLights().Get(_data._lightType);
	if (mesh.IsLoaded())
	{
		if (!_octreeBindOnce)
		{
			_bounds = Math::Bounds::MakeFromOrientedBox(mesh.GetBounds(), _tr);
			SceneManager::I().GetOctree().BindElement(Math::Octree::Element(_bounds, this), octreeRoot);
			_octreeBindOnce = octreeRoot;
		}
		if (_dynamic)
		{
			_bounds = Math::Bounds::MakeFromOrientedBox(mesh.GetBounds(), _tr);
			SceneManager::I().GetOctree().UpdateDynamicBounds(Math::Octree::Element(_bounds, this));
		}
	}
}

void Light::Serialize(IO::RSeekableStream stream)
{
	SceneNode::Serialize(stream);

	stream << _data._lightType;
	stream << _data._color;
	stream << _data._dir;
	stream << _data._radius;
	stream << _data._coneIn;
	stream << _data._coneOut;
}

void Light::Deserialize(IO::RStream stream)
{
	SceneNode::Deserialize(stream);
	const String savedName = _C(GetName());
	PreventNameCollision();

	if (stream.GetVersion() >= IO::Xxx::MakeVersion(3, 0))
	{
		Desc desc;
		desc._name = _C(savedName);
		stream >> desc._data._lightType;
		stream >> desc._data._color;
		stream >> desc._data._dir;
		stream >> desc._data._radius;
		stream >> desc._data._coneIn;
		stream >> desc._data._coneOut;
		Init(desc);
	}
}

void Light::SaveXML(pugi::xml_node node)
{
	SceneNode::SaveXML(node);

	switch (_data._lightType)
	{
	case CGI::LightType::dir: node.append_attribute("url") = "DIR"; break;
	case CGI::LightType::omni: node.append_attribute("url") = "OMNI"; break;
	case CGI::LightType::spot: node.append_attribute("url") = "SPOT"; break;
	}
	node.append_attribute("type") = +_data._lightType;
	node.append_attribute("color") = _C(_data._color.ToString(true));
	node.append_attribute("dir") = _C(_data._dir.ToString(true));
	node.append_attribute("radius") = _data._radius;
	node.append_attribute("coneIn") = _data._coneIn;
	node.append_attribute("coneOut") = _data._coneOut;
}

void Light::LoadXML(pugi::xml_node node)
{
	SceneNode::LoadXML(node);
	PreventNameCollision();

	Desc desc;
	if (auto attr = node.attribute("name"))
		desc._name = attr.value();
	const int type = node.attribute("type").as_int(+CGI::LightType::omni);
	desc._data._lightType = static_cast<CGI::LightType>(type);
	if (auto attr = node.attribute("color"))
		desc._data._color.FromString(attr.value());
	if (auto attr = node.attribute("dir"))
		desc._data._dir.FromString(attr.value());
	desc._data._radius = node.attribute("radius").as_float(desc._data._radius);
	desc._data._coneIn = node.attribute("coneIn").as_float(desc._data._coneIn);
	desc._data._coneOut = node.attribute("coneOut").as_float(desc._data._coneOut);
	desc._intShakerScale = node.attribute("intShakerScale").as_float(desc._intShakerScale);
	if (auto attr = node.attribute("urlIntShaker"))
		desc._urlIntShaker = attr.value();

	// Normalize:
	desc._data._dir = VMath::normalize(desc._data._dir);
	// Convert degrees to cosine:
	if (desc._data._coneIn > 1)
		desc._data._coneIn = cos(Math::ToRadians(desc._data._coneIn));
	if (desc._data._coneOut > 1)
		desc._data._coneOut = cos(Math::ToRadians(desc._data._coneOut));

	Init(desc);
}

// LightPtr:

void LightPtr::Init(Light::RcDesc desc)
{
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(!_p);
	_p = sm.InsertLight();
	if (desc._node)
		_p->LoadXML(desc._node);
	else
	{
		_p->Init(desc);
		_p->SetLightType(desc._data._lightType); // Call ComputeTransform().
	}
}

void LightPwn::Done()
{
	if (_p)
	{
		SceneManager::I().DeleteLight(_p);
		_p = nullptr;
	}
}
