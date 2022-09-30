// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// LightNode:

LightNode::LightNode()
{
	_type = NodeType::light;
}

LightNode::~LightNode()
{
	Done();
}

void LightNode::Init(RcDesc desc)
{
	_data = desc._data;
	_trLocal.setCol2(_data._dir);
	SetOctreeElementFlag();
	SetDynamicFlag(desc._dynamic);
	BaseNode::Init(desc._name ? desc._name : "Light");
}

void LightNode::Done()
{
	VERUS_DONE(LightNode);
}

void LightNode::Duplicate(RBaseNode node)
{
	BaseNode::Duplicate(node);

	RLightNode lightNode = static_cast<RLightNode>(node);

	_data = lightNode._data;

	if (NodeType::light == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._data = lightNode._data;
		Init(desc);
	}
}

void LightNode::Update()
{
	if (!_async_loadedMesh)
	{
		VERUS_QREF_WU;
		RMesh mesh = wu.GetDeferredLights().Get(_data._lightType);
		if (mesh.IsLoaded())
		{
			_async_loadedMesh = true;
			UpdateBounds();
		}
	}
}

void LightNode::DrawEditorOverlays(DrawEditorOverlaysFlags flags)
{
	BaseNode::DrawEditorOverlays(flags);

	if (flags & DrawEditorOverlaysFlags::extras)
	{
		VERUS_QREF_EO;
		VERUS_QREF_WM;

		const float fadeDistSq = GetRadius() * GetRadius() * 9;
		const UINT32 orColor = IsSelected() ? VERUS_COLOR_RGBA(255, 255, 255, 0) : 0;
		switch (_data._lightType)
		{
		case CGI::LightType::omni:
		{
			const int alpha = WorldManager::GetEditorOverlaysAlpha(255, GetDistToHeadSq(), fadeDistSq);
			if (alpha)
			{
				const int alpha128 = WorldManager::GetEditorOverlaysAlpha(128, GetDistToHeadSq(), fadeDistSq);
				const Vector3 dir = VMath::normalizeApprox(wm.GetHeadCamera()->GetEyePosition() - GetPosition());
				const Transform3 tr(Matrix3::MakeTrackToZ(dir) * Matrix3::rotationX(VERUS_PI * 0.5f), Vector3(GetPosition()));
				eo.DrawCircle(&tr, _data._radius, EditorOverlays::GetLightColor(alpha128) | orColor);
				eo.DrawLight(GetPosition(), EditorOverlays::GetLightColor(alpha) | orColor);
			}
		}
		break;
		case CGI::LightType::spot:
		{
			const int alpha = WorldManager::GetEditorOverlaysAlpha(255, GetDistToHeadSq(), fadeDistSq);
			if (alpha)
			{
				const int alpha128 = WorldManager::GetEditorOverlaysAlpha(128, GetDistToHeadSq(), fadeDistSq);
				const int alpha64 = WorldManager::GetEditorOverlaysAlpha(64, GetDistToHeadSq(), fadeDistSq);
				const Vector3 dir = VMath::normalizeApprox(GetTransform().getCol2());
				const Point3 targetPos = GetPosition() + dir * _data._radius;
				const Transform3 tr(Matrix3::MakeTrackToZ(dir) * Matrix3::rotationX(VERUS_PI * 0.5f), Vector3(targetPos));
				{
					const float angle = acos(_data._coneOut);
					const float scaleXY = tan(angle);
					eo.DrawCircle(&tr, _data._radius * scaleXY, EditorOverlays::GetLightColor(alpha128) | orColor);
				}
				{
					const float angle = acos(_data._coneIn);
					const float scaleXY = tan(angle);
					eo.DrawCircle(&tr, _data._radius * scaleXY, EditorOverlays::GetLightColor(alpha64) | orColor);
				}
				eo.DrawLight(GetPosition(), EditorOverlays::GetLightColor(alpha) | orColor, &targetPos);
			}
		}
		break;
		}
	}
}

float LightNode::GetPropertyByName(CSZ name) const
{
	if (!strcmp(name, "intensity"))
		return GetIntensity();
	return BaseNode::GetPropertyByName(name);
}

void LightNode::SetPropertyByName(CSZ name, float value)
{
	if (!strcmp(name, "intensity"))
		SetIntensity(value);
	BaseNode::SetPropertyByName(name, value);
}

Vector3 LightNode::ComputeScale()
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

void LightNode::OnLocalTransformUpdated()
{
	_data._dir = VMath::normalizeApprox(GetTransform(true).getCol2());
	const Matrix3 matR = Matrix3::MakeTrackToZ(_data._dir);
	_trLocal = VMath::appendScale(Transform3(matR, Vector3(GetPosition(true))), ComputeScale());
}

void LightNode::UpdateBounds()
{
	VERUS_QREF_WM;

	const bool dir = (CGI::LightType::dir == _data._lightType);
	const bool octreeRoot = (IsDynamic() || dir);

	VERUS_QREF_WU;
	RMesh mesh = wu.GetDeferredLights().Get(_data._lightType);
	if (mesh.IsLoaded())
	{
		if (!IsOctreeBindOnce())
		{
			_bounds = Math::Bounds::MakeFromOrientedBox(mesh.GetBounds(), GetTransform());
			wm.GetOctree().BindElement(Math::Octree::Element(_bounds, this), octreeRoot);
			SetOctreeBindOnceFlag(octreeRoot);
		}
		if (IsDynamic())
		{
			_bounds = Math::Bounds::MakeFromOrientedBox(mesh.GetBounds(), GetTransform());
			wm.GetOctree().UpdateDynamicBounds(Math::Octree::Element(_bounds, this));
		}
	}
}

void LightNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream << _data._color;
	stream << _data._dir;
	stream << _data._lightType;
	stream << _data._radius;
	stream << _data._coneIn;
	stream << _data._coneOut;
}

void LightNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	stream >> _data._color;
	stream >> _data._dir;
	stream >> _data._lightType;
	stream >> _data._radius;
	stream >> _data._coneIn;
	stream >> _data._coneOut;

	if (NodeType::light == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._data = _data;
		Init(desc);
		UpdateRigidBodyTransform();
	}
}

void LightNode::Deserialize_LegacyXXX(IO::RStream stream)
{
	BaseNode::Deserialize_LegacyXXX(stream);

	stream >> _data._lightType;
	stream >> _data._color;
	stream >> _data._dir;
	stream >> _data._radius;
	stream >> _data._coneIn;
	stream >> _data._coneOut;

	if (NodeType::light == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._data = _data;
		Init(desc);
		UpdateRigidBodyTransform();
	}
}

void LightNode::DeserializeXML_LegacyXXX(pugi::xml_node node)
{
	BaseNode::DeserializeXML_LegacyXXX(node);

	CSZ intShakerURL = nullptr;
	float intShakerScale = 0.25f;

	if (auto attr = node.attribute("name"))
		_name = attr.value();
	const int type = node.attribute("type").as_int(+CGI::LightType::omni);
	_data._lightType = static_cast<CGI::LightType>(type);
	if (auto attr = node.attribute("color"))
		_data._color.FromString(attr.value());
	if (auto attr = node.attribute("dir"))
		_data._dir.FromString(attr.value());
	_data._radius = node.attribute("radius").as_float(_data._radius);
	_data._coneIn = node.attribute("coneIn").as_float(_data._coneIn);
	_data._coneOut = node.attribute("coneOut").as_float(_data._coneOut);
	intShakerScale = node.attribute("intShakerScale").as_float(intShakerScale);
	if (auto attr = node.attribute("urlIntShaker"))
		intShakerURL = attr.value();

	// Normalize:
	_data._dir = VMath::normalize(_data._dir);
	// Convert degrees to cosine:
	if (_data._coneIn > 1)
		_data._coneIn = cos(Math::ToRadians(_data._coneIn));
	if (_data._coneOut > 1)
		_data._coneOut = cos(Math::ToRadians(_data._coneOut));

	if (NodeType::light == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._data = _data;
		Init(desc);

		if (intShakerURL)
		{
			ShakerNode::Desc desc;
			desc._name = _C(_name);
			desc._url = intShakerURL;
			ShakerNodePtr shakerNode;
			shakerNode.Init(desc);
			shakerNode->SetPropertyName("intensity");
			shakerNode->GetShaker().SetScale(0.5f * intShakerScale);
			shakerNode->GetShaker().SetBias(1 - 0.5f * intShakerScale);
			shakerNode->SetParent(this, true);
		}
	}
}

CGI::LightType LightNode::GetLightType() const
{
	return _data._lightType;
}

void LightNode::SetLightType(CGI::LightType type)
{
	_data._lightType = type;
	OnLocalTransformUpdated();
	UpdateGlobalTransform();
}

RcVector4 LightNode::GetColor() const
{
	return _data._color;
}

void LightNode::SetColor(RcVector4 color)
{
	_data._color = color;
}

float LightNode::GetIntensity() const
{
	return _data._color.getW();
}

void LightNode::SetIntensity(float i)
{
	_data._color.setW(i);
}

float LightNode::GetRadius() const
{
	return _data._radius;
}

void LightNode::SetRadius(float r)
{
	_data._radius = Math::Max(0.001f, r);
	OnLocalTransformUpdated();
	UpdateGlobalTransform();
}

float LightNode::GetConeIn() const
{
	return _data._coneIn;
}

float LightNode::GetConeOut() const
{
	return _data._coneOut;
}

void LightNode::SetCone(float coneIn, float coneOut)
{
	if (coneIn)
		_data._coneIn = coneIn;
	if (coneOut)
		_data._coneOut = coneOut;
	const float cos1deg = 0.999848f;
	const float cos89deg = 0.017452f;
	_data._coneOut = Math::Clamp<float>(_data._coneOut, cos89deg, cos1deg);
	_data._coneIn = Math::Clamp<float>(_data._coneIn, _data._coneOut + VERUS_FLOAT_THRESHOLD, 1);
	OnLocalTransformUpdated();
	UpdateGlobalTransform();
}

Vector4 LightNode::GetInstData() const
{
	return Vector4(_data._color.getXYZ() * _data._color.getW(), _data._coneIn);
}

// LightNodePtr:

void LightNodePtr::Init(LightNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertLightNode();
	_p->Init(desc);
}

void LightNodePtr::Duplicate(RBaseNode node)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertLightNode();
	_p->Duplicate(node);
}

void LightNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
