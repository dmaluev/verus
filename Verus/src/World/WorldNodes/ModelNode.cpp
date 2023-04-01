// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

// ModelNode:

ModelNode::ModelNode()
{
	_type = NodeType::model;
}

ModelNode::~ModelNode()
{
	Done();
}

void ModelNode::Init(RcDesc desc)
{
	if (_refCount)
		return;

	BaseNode::Init(desc._name ? desc._name : desc._url);
	_refCount = 1;

	Mesh::Desc meshDesc;
	meshDesc._url = desc._url;
	meshDesc._instanceCapacity = 1000;
	meshDesc._initShape = true;
	_mesh.Init(meshDesc);

	LoadMaterial(desc._materialURL ? desc._materialURL : desc._url);
}

bool ModelNode::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		_material.Done();
		_mesh.Done();

		VERUS_DONE(ModelNode);
		return true;
	}
	return false;
}

void ModelNode::GetEditorCommands(Vector<EditorCommand>& v)
{
	BaseNode::GetEditorCommands(v);

	v.push_back(EditorCommand(nullptr, EditorCommandCode::separator));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::model_insertBlockNode));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::model_insertBlockAndPhysicsNodes));
	v.push_back(EditorCommand(nullptr, EditorCommandCode::model_selectAllBlocks));
}

bool ModelNode::CanSetParent(PBaseNode pNode) const
{
	return !pNode || IsGenerated();
}

void ModelNode::Serialize(IO::RSeekableStream stream)
{
	BaseNode::Serialize(stream);

	stream.WriteString(_C(GetURL()));
	stream.WriteString(_material ? _C(_material->_name) : "");
}

void ModelNode::Deserialize(IO::RStream stream)
{
	BaseNode::Deserialize(stream);

	char url[IO::Stream::s_bufferSize] = {};
	char materialURL[IO::Stream::s_bufferSize] = {};

	stream.ReadString(url);
	stream.ReadString(materialURL);

	if (NodeType::model == _type)
	{
		Desc desc;
		desc._name = _C(_name);
		desc._url = url;
		desc._materialURL = *materialURL ? materialURL : nullptr;
		Init(desc);
	}
}

void ModelNode::Deserialize_LegacyXXX(IO::RStream stream, CSZ url)
{
	char mat[IO::Stream::s_bufferSize] = {};
	stream.ReadString(mat);

	// TODO: handle old names, remove later:
	char* pOldExt = strstr(mat, ".xmt");
	if (pOldExt)
		strncpy(pOldExt, ".vml", 4);

	if (NodeType::model == _type)
	{
		Desc desc;
		desc._url = url;
		desc._materialURL = mat;
		Init(desc);
		UpdateRigidBodyTransform();
	}
}

void ModelNode::LoadMaterial(CSZ url)
{
	if (_material)
	{
		VERUS_QREF_RENDERER;
		renderer->WaitIdle();
		_material.Done();
	}

	Material::Desc matDesc;
	matDesc._name = url;
	matDesc._load = true;
	_material.Init(matDesc);
}

void ModelNode::BindPipeline(CGI::CommandBufferPtr cb)
{
	_mesh.BindPipelineInstanced(cb, false);
	_mesh.UpdateUniformBufferPerFrame();
}

void ModelNode::BindPipelineSimple(DrawSimpleMode mode, CGI::CommandBufferPtr cb)
{
	switch (mode)
	{
	case DrawSimpleMode::envMap:           _mesh.BindPipeline(Mesh::PIPE_SIMPLE_ENV_MAP_INSTANCED, cb); break;
	case DrawSimpleMode::planarReflection: _mesh.BindPipeline(Mesh::PIPE_SIMPLE_PLANAR_REF_INSTANCED, cb); break;
	}
	_mesh.UpdateUniformBufferSimplePerFrame(mode);
}

void ModelNode::BindGeo(CGI::CommandBufferPtr cb)
{
	_mesh.BindGeo(cb);
	_mesh.UpdateUniformBufferPerMeshVS();
}

void ModelNode::MarkFirstInstance()
{
	_mesh.MarkFirstInstance();
}

void ModelNode::PushInstance(RcTransform3 matW, RcVector4 instData)
{
	_mesh.PushInstance(matW, instData);
}

void ModelNode::Draw(CGI::CommandBufferPtr cb)
{
	if (!_mesh.IsInstanceBufferEmpty(true))
	{
		_mesh.UpdateInstanceBuffer();
		cb->DrawIndexed(_mesh.GetIndexCount(), _mesh.GetInstanceCount(true), 0, 0, _mesh.GetFirstInstance());
	}
}

// ModelNodePtr:

void ModelNodePtr::Init(ModelNode::RcDesc desc)
{
	VERUS_QREF_WM;
	VERUS_RT_ASSERT(!_p);
	_p = wm.InsertModelNode(desc._url);
	_p->Init(desc);
}

void ModelNodePwn::Done()
{
	if (_p)
	{
		WorldManager::I().DeleteNode(_p);
		_p = nullptr;
	}
}
