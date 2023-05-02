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
	if (!(_flags & Flags::readOnlyFlags))
	{
		SetOctreeElementFlag();
		SetDynamicFlag(desc._dynamic);
		SetShadowFlag();
	}
	BaseNode::Init(desc._name ? desc._name : "Light");
#ifdef VERUS_WORLD_FORCE_FLAGS
	SetShadowFlag();
#endif
	UpdateShadowMapHandle();
}

void LightNode::Done()
{
	UpdateShadowMapHandle(true);
	VERUS_DONE(LightNode);
}

void LightNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

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
		const Vector3 zAxis = VMath::normalizeApprox(wm.GetHeadCamera()->GetEyePosition() - GetPosition());
		Transform3 trCircle(Matrix3::MakeTrackToZ(zAxis) * Matrix3::rotationX(VERUS_PI * 0.5f), Vector3(GetPosition()));
		const UINT32 orColor = IsSelected() ? VERUS_COLOR_RGBA(255, 255, 255, 0) : 0;
		switch (_data._lightType)
		{
		case CGI::LightType::omni:
		{
			const int alpha = WorldManager::GetEditorOverlaysAlpha(255, GetDistToHeadSq(), fadeDistSq);
			if (alpha)
			{
				const int alpha128 = WorldManager::GetEditorOverlaysAlpha(128, GetDistToHeadSq(), fadeDistSq);
				eo.DrawCircle(&trCircle, _data._radius, EditorOverlays::GetLightColor(alpha128) | orColor);
				eo.DrawCircle(&trCircle, ComputeLampRadius(), EditorOverlays::GetLightColor(alpha128) | orColor);
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
				eo.DrawCircle(&trCircle, ComputeLampRadius(), EditorOverlays::GetLightColor(alpha128) | orColor);
				eo.DrawLight(GetPosition(), EditorOverlays::GetLightColor(alpha) | orColor, &targetPos);
			}
		}
		break;
		}
	}
}

void LightNode::SetShadowFlag(bool shadow)
{
	BaseNode::SetShadowFlag(shadow);

	UpdateShadowMapHandle();
}

bool LightNode::CanReserveShadow() const
{
	return !!(_flags & Flags::reserveShadow);
}

void LightNode::SetReserveShadowFlag(bool reserveShadow)
{
	if (reserveShadow)
		VERUS_BITMASK_SET(_flags, Flags::reserveShadow);
	else
		VERUS_BITMASK_UNSET(_flags, Flags::reserveShadow);

	UpdateShadowMapHandle();
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
	{
		const bool b = _shadowMapUpdateRequired;
		SetIntensity(value);
		_shadowMapUpdateRequired = b;
	}
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

void LightNode::OnNodeTransformed(PBaseNode pNode, bool afterEvent)
{
	BaseNode::OnNodeTransformed(pNode, afterEvent);

	if (afterEvent)
	{
		if (pNode == this)
			RequestShadowMapUpdate();
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
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	_data._lightType = type;
	OnLocalTransformUpdated();
	UpdateGlobalTransform();
	wm.BroadcastOnNodeTransformed(this, true);
}

RcVector4 LightNode::GetColor() const
{
	return _data._color;
}

void LightNode::SetColor(RcVector4 color)
{
	_data._color = color;
	RequestShadowMapUpdate();
}

float LightNode::GetIntensity() const
{
	return _data._color.getW();
}

void LightNode::SetIntensity(float i)
{
	_data._color.setW(i);
	RequestShadowMapUpdate();
}

float LightNode::GetRadius() const
{
	return _data._radius;
}

void LightNode::SetRadius(float r)
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	_data._radius = Math::Max(0.001f, r);
	OnLocalTransformUpdated();
	UpdateGlobalTransform();
	wm.BroadcastOnNodeTransformed(this, true);
}

float LightNode::GetConeIn() const
{
	return _data._coneIn;
}

void LightNode::SetConeIn(float coneIn)
{
	_data._coneIn = Math::Clamp(coneIn, _data._coneOut + VERUS_FLOAT_THRESHOLD, 1.f);
}

float LightNode::GetConeOut() const
{
	return _data._coneOut;
}

void LightNode::SetConeOut(float coneOut)
{
	VERUS_QREF_WM;
	wm.BroadcastOnNodeTransformed(this, false);
	const float cos1deg = 0.999848f;
	const float cos89deg = 0.017452f;
	_data._coneOut = Math::Clamp(coneOut, cos89deg, cos1deg);
	SetConeIn(_data._coneIn);
	OnLocalTransformUpdated();
	UpdateGlobalTransform();
	wm.BroadcastOnNodeTransformed(this, true);
}

Vector4 LightNode::GetInstData() const
{
	return Vector4(_data._color.getXYZ() * _data._color.getW(), _data._coneIn);
}

float LightNode::ComputeLampRadius() const
{
	return _data._radius * 10 / VMath::dot(_data._color.getXYZ() * abs(_data._color.getW()), Vector3::Replicate(1 / 3.f));
}

void LightNode::UpdateShadowMapHandle(bool freeCell)
{
	VERUS_QREF_SMBP;
	const bool hasShadow = HasShadow() && (CGI::LightType::dir != _data._lightType);
	if (hasShadow && CanReserveShadow() && !freeCell)
	{
		if (!_shadowMapHandle.IsSet())
		{
			_shadowMapHandle = smbp.ReserveCell();
			_shadowMapUpdateRequired = true;
		}
	}
	else
	{
		if (_shadowMapHandle.IsSet())
		{
			smbp.FreeCell(_shadowMapHandle);
			_shadowMapHandle = ShadowMapHandle();
			_shadowMapUpdateRequired = false;
		}
	}
}

bool LightNode::CanRequestShadowMapUpdate() const
{
	const bool hasShadow = HasShadow() && (CGI::LightType::dir != _data._lightType);
	return hasShadow && CanReserveShadow() && _shadowMapHandle.IsSet(); // Ready to bake?
}

void LightNode::RequestShadowMapUpdate(bool mustBeStatic)
{
	if (_shadowMapUpdateRequired)
		return;
	VERUS_QREF_WM;
	const bool updateRequired = CanRequestShadowMapUpdate();
	if (updateRequired && (mustBeStatic && wm.IsSmbpDynamic(this)))
		return;
	_shadowMapUpdateRequired = updateRequired;
}

bool LightNode::IsShadowMapUpdateRequired() const
{
	return _shadowMapUpdateRequired;
}

void LightNode::OnShadowMapUpdated()
{
	_shadowMapUpdateRequired = false;
}

bool LightNode::CanBeSmbpStatic() const
{
	return IsShadowMapUpdateRequired(); // Shadow map is not yet updated or this light is downgraded to static from dynamic.
}

bool LightNode::CanBeSmbpDynamic() const
{
	return
		!IsShadowMapUpdateRequired() && // Shadow map is updated or no shadow map at all.
		CanRequestShadowMapUpdate(); // Must have valid handle.
}

void LightNode::SetupShadowMapCamera(RCamera camera)
{
	VERUS_QREF_SMBP;
	VERUS_QREF_WM;

	WorldManager::Query query;
	query._pParent = this;
	query._type = NodeType::shaker;
	wm.ForEachNode(query, [](RBaseNode node)
		{
			static_cast<RShakerNode>(node).ApplyInitialValue();
			return Continue::yes;
		});

	const float yFov = (CGI::LightType::spot == _data._lightType) ? acos(_data._coneOut) * 2 : Math::ToRadians(120);
	camera.MoveEyeTo(_trGlobal.getTranslation());
	camera.MoveAtTo(_trGlobal.getTranslation() + _trGlobal.getCol2());
	camera.SetUpDirection(_trGlobal.getCol1());
	camera.SetYFov(yFov);
	camera.SetAspectRatio(1);
	camera.SetZNear(Math::Min(_data._radius - 0.001f, ComputeLampRadius()));
	camera.SetZFar(_data._radius);
	camera.Update();

	const Matrix4 toUV = Math::ToUVMatrix();
	_matShadow = smbp.GetOffsetMatrix(_shadowMapHandle) * toUV * camera.GetMatrixVP();
}

RcMatrix4 LightNode::GetShadowMatrix() const
{
	return _matShadow;
}

Matrix4 LightNode::GetShadowMatrixForDS() const
{
	VERUS_QREF_WM;
	return _matShadow * wm.GetViewCamera()->GetMatrixInvV();
}

float LightNode::GetInfluenceAt(Math::RcSphere sphere)
{
	if (CGI::LightType::dir == _data._lightType)
	{
		_cachedInfluence = abs(GetIntensity());
	}
	else
	{
		const Vector3 toSphere = sphere.GetCenter() - GetPosition();
		const float lenSq = VMath::lengthSqr(toSphere);
		if (lenSq >= sphere.GetRadiusSq() * 100) // Far enough to ignore radius?
		{
			const float invSquareLaw = 1 / Math::Max(VERUS_FLOAT_THRESHOLD, lenSq);
			_cachedInfluence = abs(GetIntensity()) * invSquareLaw;
		}
		else
		{
			const float len = Math::Max(VERUS_FLOAT_THRESHOLD, sqrt(lenSq) - sphere.GetRadius());
			const float invSquareLaw = 1 / (len * len);
			_cachedInfluence = abs(GetIntensity()) * invSquareLaw;
		}
		if (CGI::LightType::spot == _data._lightType)
		{
			const Point3 newLightPos = GetPosition() - _data._dir * sphere.GetRadius(); // Move backwards.
			const Vector3 toSphereExpanded = sphere.GetCenter() - newLightPos;
			const float coneApprox = (VMath::dot(_data._dir, toSphereExpanded) >= 0) ? 1.f : 0.1f;
			_cachedInfluence *= coneApprox;
		}
	}
	return _cachedInfluence;
}

// LightNodePtr:

void LightNodePtr::Init(LightNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertLightNode();
	_p->Init(desc);
}

void LightNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertLightNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void LightNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
