// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// Block:

Block::Block()
{
	_type = NodeType::block;
}

Block::~Block()
{
	Done();
}

void Block::Init(RcDesc desc)
{
	VERUS_QREF_SM;
	_name = sm.EnsureUniqueName(desc._name ? desc._name : desc._model);
	_matIndex = desc._matIndex;
	_collide = desc._collide;

	Model::Desc modelDesc;
	modelDesc._url = desc._model;
	modelDesc._mat = desc._modelMat;
	_model.Init(modelDesc);
	if (1 == _model->GetRefCount())
		_model->AddRef(); // Models must be owned by blocks and scene manager.

	if (desc._blockMat)
	{
		Material::Desc matDesc;
		matDesc._name = desc._blockMat;
		matDesc._load = true;
		_material.Init(matDesc);
	}

	String url = desc._model;
	Str::ReplaceExtension(url, ".Extra.xml");
	Vector<BYTE> vData;
	IO::FileSystem::I().LoadResourceFromCache(_C(url), vData, false);
	if (vData.size() > 1)
		LoadExtra(reinterpret_cast<SZ>(vData.data()));
}

void Block::Done()
{
	RemoveRigidBody();
	_material.Done();
	_model.Done();
	SceneManager::I().GetOctree().UnbindClient(this);
	_vLights.clear();
	_vEmitters.clear();
}

void Block::LoadExtra(SZ xml)
{
	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(xml, strlen(xml));
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
	pugi::xml_node root = doc.first_child();

	int lightCount = 0;
	for (auto node : root.children("light"))
	{
		auto matAttr = node.attribute("mat");
		if (!matAttr || matAttr.as_int() == _matIndex)
			lightCount++;
	}
	int emitterCount = 0;
	for (auto node : root.children("emit"))
		emitterCount++;

	_vLights.resize(lightCount);
	_vEmitters.resize(emitterCount);

	int i = 0;
	for (auto node : root.children("light"))
	{
		auto matAttr = node.attribute("mat");
		if (!matAttr || matAttr.as_int() == _matIndex)
		{
			RLightPwn light = _vLights[i]._light;

			Light::Desc desc;
			desc._node = node;
			light.Init(desc);
			light->SetTransient();
			_vLights[i]._tr = light->GetTransform();
			light->SetTransform(GetTransform() * _vLights[i]._tr);

			i++;
		}
	}
	i = 0;
	for (auto node : root.children("emit"))
	{
		REmitterPwn emitter = _vEmitters[i]._emitter;

		Emitter::Desc desc;
		desc._node = node;
		emitter.Init(desc);
		emitter->SetTransient();
		_vEmitters[i]._tr = emitter->GetTransform();
		emitter->SetTransform(GetTransform() * _vEmitters[i]._tr);

		i++;
	}
}

void Block::Update()
{
	if (!_async_loadedModel && _model->IsLoaded())
	{
		_async_loadedModel = true;
		if (!_dynamic)
		{
			VERUS_QREF_BULLET;
			const btTransform tr = GetTransform().Bullet();
			_pBody = bullet.AddNewRigidBody(0, tr, _model->GetMesh().GetShape(), Physics::Group::immovable);
			_pBody->setFriction(Physics::Bullet::GetFriction(Physics::Material::stone));
			_pBody->setRestitution(Physics::Bullet::GetRestitution(Physics::Material::stone));
			_pBody->setUserPointer(this);
			SetCollisionGroup(_collide ? Physics::Group::immovable : Physics::Group::general);
		}
		UpdateBounds();
	}
}

MaterialPtr Block::GetMaterial(bool orModelMat)
{
	if (!_material && orModelMat)
		return _model->GetMaterial();
	return _material;
}

void Block::UpdateBounds()
{
	if (_model->IsLoaded())
	{
		if (!_octreeBindOnce)
		{
			_bounds = Math::Bounds::MakeFromOrientedBox(_model->GetMesh().GetBounds(), _tr);
			SceneManager::I().GetOctree().BindClient(Math::Octree::Client(_bounds, this), _dynamic);
			_octreeBindOnce = _dynamic;
		}
		if (_dynamic)
		{
			_bounds = Math::Bounds::MakeFromOrientedBox(_model->GetMesh().GetBounds(), _tr);
			SceneManager::I().GetOctree().UpdateDynamicBounds(Math::Octree::Client(_bounds, this));
		}
	}

	for (auto& x : _vLights)
		x._light->SetTransform(GetTransform() * x._tr);
	for (auto& x : _vEmitters)
		x._emitter->SetTransform(GetTransform() * x._tr);
}

void Block::Serialize(IO::RSeekableStream stream)
{
	SceneNode::Serialize(stream);

	stream.WriteString(_C(GetUrl()));
	stream.WriteString(_material ? _C(_material->_name) : "");
	stream << _userColor.GLM();
}

void Block::Deserialize(IO::RStream stream)
{
	SceneNode::Deserialize(stream);
	const String savedName = _C(GetName());
	PreventNameCollision();

	if (stream.GetVersion() >= IO::Xxx::MakeVersion(3, 0))
	{
		char url[IO::Stream::s_bufferSize] = {};
		char material[IO::Stream::s_bufferSize] = {};
		glm::vec4 userColor;
		stream.ReadString(url);
		stream.ReadString(material);
		stream >> userColor;

		_userColor = userColor;

		Desc desc;
		desc._name = _C(savedName);
		desc._model = url;
		desc._blockMat = (strlen(material) > 0) ? material : nullptr;
		if (desc._blockMat)
			desc._matIndex = atoi(strrchr(desc._blockMat, '.') - 1);
		Init(desc);
	}
}

void Block::SaveXML(pugi::xml_node node)
{
	SceneNode::SaveXML(node);

	node.append_attribute("url") = _C(GetUrl());
	if (_material)
		node.append_attribute("mat") = _C(_material->_name);
	if (!_userColor.IsZero())
		node.append_attribute("color") = _C(_userColor.ToColorString());
}

void Block::LoadXML(pugi::xml_node node)
{
	SceneNode::LoadXML(node);
	PreventNameCollision();

	_userColor = Vector4(0);
	if (auto attr = node.attribute("color"))
		_userColor.FromColorString(attr.value());

	Desc desc;
	desc._name = node.attribute("name").value();
	desc._model = node.attribute("url").value();
	if (auto attr = node.attribute("mat"))
		desc._blockMat = attr.value();
	if (desc._blockMat)
		desc._matIndex = atoi(strrchr(desc._blockMat, '.') - 1);
	Init(desc);
}

// BlockPtr:

void BlockPtr::Init(Block::RcDesc desc)
{
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(!_p);
	_p = sm.InsertBlock();
	if (desc._node)
		_p->LoadXML(desc._node);
	else
		_p->Init(desc);
}

void BlockPwn::Done()
{
	if (_p)
	{
		SceneManager::I().DeleteBlock(_p);
		_p = nullptr;
	}
}
