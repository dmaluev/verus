// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// BlockNode:

BlockNode::BlockNode()
{
	_type = NodeType::block;
}

BlockNode::~BlockNode()
{
	Done();
}

void BlockNode::Init(RcDesc desc)
{
	if (!(_flags & Flags::readOnlyFlags))
	{
		SetOctreeElementFlag();
		SetShadowFlag();
	}
	BaseNode::Init(desc._name ? desc._name : desc._modelURL);
#ifdef VERUS_WORLD_FORCE_FLAGS
	SetShadowFlag();
#endif

	ModelNode::Desc modelDesc;
	modelDesc._url = desc._modelURL;
	modelDesc._materialURL = desc._materialURL;
	_modelNode.Init(modelDesc);

	if (desc._materialURL)
		LoadMaterial(desc._materialURL);
}

void BlockNode::Done()
{
	RemoveMaterial();
	_modelNode.Done();

	VERUS_DONE(BlockNode);
}

void BlockNode::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	BaseNode::Duplicate(node, hierarchyDuplication);

	RBlockNode blockNode = static_cast<RBlockNode>(node);

	_userColor = blockNode._userColor;

	if (NodeType::block == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._modelURL = _C(blockNode._modelNode->GetURL());
		desc._materialURL = blockNode._material ? _C(blockNode._material->_name) : nullptr;
		Init(desc);
	}
}

void BlockNode::Update()
{
	if (!_async_loadedModel && _modelNode->IsLoaded())
	{
		_async_loadedModel = true;
		UpdateBounds();
	}
}

void BlockNode::UpdateBounds()
{
	if (_modelNode && _modelNode->IsLoaded())
	{
		if (!IsOctreeBindOnce())
		{
			_bounds = Math::Bounds::MakeFromOrientedBox(_modelNode->GetMesh().GetBounds(), GetTransform());
			WorldManager::I().GetOctree().BindElement(Math::Octree::Element(_bounds, this), IsDynamic());
			SetOctreeBindOnceFlag(IsDynamic());
		}
		if (IsDynamic())
		{
			_bounds = Math::Bounds::MakeFromOrientedBox(_modelNode->GetMesh().GetBounds(), GetTransform());
			WorldManager::I().GetOctree().UpdateDynamicBounds(Math::Octree::Element(_bounds, this));
		}
	}
}

void BlockNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream << _userColor.GLM();
	stream.WriteString(_C(_modelNode->GetURL()));
	stream.WriteString(_material ? _C(_material->_name) : "");
}

void BlockNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	glm::vec4 userColor;
	char modelURL[IO::Stream::s_bufferSize] = {};
	char materialURL[IO::Stream::s_bufferSize] = {};

	stream >> userColor;
	stream.ReadString(modelURL);
	stream.ReadString(materialURL);

	_userColor = userColor;

	if (NodeType::block == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._modelURL = modelURL;
		desc._materialURL = *materialURL ? materialURL : nullptr;
		Init(desc);
	}
}

void BlockNode::Deserialize_LegacyXXX(IO::RStream stream)
{
	BaseNode::Deserialize_LegacyXXX(stream);

	char url[IO::Stream::s_bufferSize] = {};
	char material[IO::Stream::s_bufferSize] = {};
	glm::vec4 userColor;
	stream.ReadString(url);
	stream.ReadString(material);
	stream >> userColor;

	_userColor = userColor;

	if (NodeType::block == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._modelURL = url;
		desc._materialURL = *material ? material : nullptr;
		Init(desc);
		UpdateRigidBodyTransform();

		PhysicsNodePtr physicsNode;
		physicsNode.Init(_C(_name));
		physicsNode->SetParent(this, true);
	}

	String extraUrl(url);
	Str::ReplaceExtension(extraUrl, ".Extra.xml");
	Vector<BYTE> vData;
	IO::FileSystem::I().LoadResourceFromCache(_C(extraUrl), vData, false);
	if (vData.size() > 1)
		LoadExtra_LegacyXXX(reinterpret_cast<SZ>(vData.data()));
}

void BlockNode::LoadExtra_LegacyXXX(SZ xml)
{
	VERUS_QREF_WM;

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(xml, strlen(xml));
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
	pugi::xml_node root = doc.first_child();

	for (auto node : root.children("light"))
	{
		auto matAttr = node.attribute("mat");
		if (!matAttr || matAttr.as_int() == _materialIndex)
		{
			auto pLightNode = wm.InsertLightNode();
			pLightNode->DeserializeXML_LegacyXXX(node);
			pLightNode->SetParent(this, true);
		}
	}
	for (auto node : root.children("emit"))
	{
		auto pEmitterNode = wm.InsertEmitterNode();
		pEmitterNode->DeserializeXML_LegacyXXX(node);
		pEmitterNode->SetParent(this, true);
	}
}

void BlockNode::LoadMaterial(CSZ url)
{
	RemoveMaterial();

	Material::Desc matDesc;
	matDesc._name = url;
	matDesc._load = true;
	_material.Init(matDesc);

	_materialIndex = atoi(strrchr(url, '.') - 1);
}

void BlockNode::RemoveMaterial()
{
	if (_material)
	{
		VERUS_QREF_RENDERER;
		renderer->WaitIdle();
		_material.Done();
	}
	_materialIndex = 0;
}

MaterialPtr BlockNode::GetMaterial(bool orModelMat)
{
	if (!_material && orModelMat)
		return _modelNode->GetMaterial();
	return _material;
}

// BlockNodePtr:

void BlockNodePtr::Init(BlockNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertBlockNode();
	_p->Init(desc);
}

void BlockNodePtr::Duplicate(RBaseNode node, HierarchyDuplication hierarchyDuplication)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertBlockNode();
	_p->Duplicate(node, hierarchyDuplication);
}

void BlockNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
