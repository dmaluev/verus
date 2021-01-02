// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

// Model:

Model::Model()
{
}

Model::~Model()
{
	Done();
}

void Model::Init(RcDesc desc)
{
	if (_refCount)
		return;

	VERUS_INIT();
	_refCount = 1;

	Mesh::Desc meshDesc;
	meshDesc._url = desc._url;
	meshDesc._instanceCapacity = 1000;
	meshDesc._initShape = true;
	_mesh.Init(meshDesc);

	Material::Desc matDesc;
	matDesc._name = desc._mat ? desc._mat : desc._url;
	matDesc._load = true;
	_material.Init(matDesc);
}

bool Model::Done()
{
	_refCount--;
	if (_refCount <= 0)
	{
		VERUS_DONE(Model);
		return true;
	}
	return false;
}

void Model::MarkFirstInstance()
{
	_mesh.MarkFirstInstance();
}

void Model::Draw(CGI::CommandBufferPtr cb)
{
	if (!_mesh.IsInstanceBufferEmpty(true))
	{
		_mesh.UpdateInstanceBuffer();
		cb->DrawIndexed(_mesh.GetIndexCount(), _mesh.GetInstanceCount(true), 0, 0, _mesh.GetFirstInstance());
	}
}

void Model::BindPipeline(CGI::CommandBufferPtr cb)
{
	_mesh.BindPipelineInstanced(cb, false);
	_mesh.UpdateUniformBufferPerFrame();
}

void Model::BindPipelineSimple(DrawSimpleMode mode, CGI::CommandBufferPtr cb)
{
	switch (mode)
	{
	case DrawSimpleMode::envMap:           _mesh.BindPipeline(Mesh::PIPE_SIMPLE_ENV_MAP_INSTANCED, cb); break;
	case DrawSimpleMode::planarReflection: _mesh.BindPipeline(Mesh::PIPE_SIMPLE_PLANAR_REF_INSTANCED, cb); break;
	}
	_mesh.UpdateUniformBufferSimplePerFrame(mode);
}

void Model::BindGeo(CGI::CommandBufferPtr cb)
{
	_mesh.BindGeo(cb);
	_mesh.UpdateUniformBufferPerMeshVS();
}

void Model::PushInstance(RcTransform3 matW, RcVector4 instData)
{
	_mesh.PushInstance(matW, instData);
}

void Model::Serialize(IO::RSeekableStream stream)
{
	stream.WriteString(_C(_material->_name));
}

void Model::Deserialize(IO::RStream stream, CSZ url)
{
	char mat[IO::Stream::s_bufferSize] = {};
	stream.ReadString(mat);

	Desc desc;
	desc._url = url;
	desc._mat = mat;
	Init(desc);
}

// ModelPtr:

void ModelPtr::Init(Model::RcDesc desc)
{
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(!_p);
	_p = sm.InsertModel(desc._url);
	_p->Init(desc);
}

void ModelPwn::Done()
{
	if (_p)
	{
		SceneManager::I().DeleteModel(_C(_p->GetMesh().GetUrl()));
		_p = nullptr;
	}
}
