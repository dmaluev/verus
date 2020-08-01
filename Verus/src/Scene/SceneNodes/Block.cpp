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
		LoadExtra(reinterpret_cast<CSZ>(vData.data()));
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

void Block::LoadExtra(CSZ xml)
{
	//tinyxml2::XMLDocument doc;
	//doc.Parse(xml);
	//if (doc.Error())
	//	return;
	//tinyxml2::XMLElement* pRoot = doc.FirstChildElement();
	//if (!pRoot)
	//	return;
	//
	//int numLights = 0;
	//for (pugi::xml_node node = pRoot->FirstChildElement("light"); pElem; pElem = pElem->NextSiblingElement("light"))
	//{
	//	CSZ mat = pElem->Attribute("mat");
	//	if (!mat || atoi(mat) == _matIndex)
	//		numLights++;
	//}
	//int numEmitters = 0;
	//for (pugi::xml_node node = pRoot->FirstChildElement("emit"); pElem; pElem = pElem->NextSiblingElement("emit"))
	//	numEmitters++;
	//
	//_vLights.resize(numLights);
	//_vEmitters.resize(numEmitters);
	//
	//int i = 0;
	//for (pugi::xml_node node = pRoot->FirstChildElement("light"); pElem; pElem = pElem->NextSiblingElement("light"))
	//{
	//	CSZ mat = pElem->Attribute("mat");
	//	if (!mat || atoi(mat) == _matIndex)
	//	{
	//		RLightPwn light = _vLights[i]._light;
	//
	//		Light::Desc descLight;
	//		descLight._pLoadXML = pElem;
	//		light.Init(descLight);
	//		_vLights[i]._tr = light->GetTransform();
	//		light->SetTransform(GetTransform() * _vLights[i]._tr);
	//
	//		i++;
	//	}
	//}
	//i = 0;
	//for (pugi::xml_node node = pRoot->FirstChildElement("emit"); pElem; pElem = pElem->NextSiblingElement("emit"))
	//{
	//	REmitterPwn emitter = _vEmitters[i]._emitter;
	//
	//	CEmitter::Desc descEmitter;
	//	descEmitter._pLoadXML = pElem;
	//	emitter.Init(descEmitter);
	//	_vEmitters[i]._tr = emitter->GetTransform();
	//	emitter->SetTransform(GetTransform() * _vEmitters[i]._tr);
	//
	//	i++;
	//}
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
			_pBody = bullet.AddNewRigidBody(0, tr, _model->GetMesh().GetShape(), +Physics::Group::immovable);
			_pBody->setFriction(Physics::Bullet::GetFriction(Physics::Material::stone));
			_pBody->setRestitution(Physics::Bullet::GetRestitution(Physics::Material::stone));
			_pBody->setUserPointer(this);
			SetCollisionGroup(_collide ? Physics::Group::immovable : Physics::Group::general);
		}
		UpdateBounds();
	}
}

void Block::Draw()
{
	//_model->Draw(&_tr, _material);
}

void Block::SetDynamic(bool mode)
{
	SceneNode::SetDynamic(mode);
	_octreeBindOnce = false;
	_async_loadedModel = false;
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
	//for (auto& x : _vEmitters)
	//	x._emitter->SetTransform(GetTransform() * x._tr);
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
	_name.clear();

	_userColor = Vector4(0);
	if (auto attr = node.attribute("color"))
		_userColor.FromColorString(attr.value());

	Desc desc;
	desc._name = node.attribute("name").value();
	desc._model = node.attribute("url").value();
	desc._blockMat = node.attribute("mat").value();
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
