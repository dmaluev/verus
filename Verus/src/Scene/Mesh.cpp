#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CGI::ShaderPwn       Mesh::s_shader;
CGI::PipelinePwns<1> Mesh::s_pipe;

Mesh::UB_PerFrame    Mesh::s_ubPerFrame;
Mesh::UB_PerMaterial Mesh::s_ubPerMaterial;
Mesh::UB_PerMesh     Mesh::s_ubPerMesh;
Mesh::UB_PerObject   Mesh::s_ubPerObject;

// Mesh:

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
	Done();
}

void Mesh::InitStatic()
{
	CGI::ShaderDesc shaderDesc;
	shaderDesc._url = "[Shaders]:DS_Mesh.hlsl";
	s_shader.Init(shaderDesc);
	s_shader->BindUniformBufferSource(0, &s_ubPerFrame, sizeof(s_ubPerFrame));
	s_shader->BindUniformBufferSource(1, &s_ubPerMaterial, sizeof(s_ubPerMaterial), 1000);
	s_shader->BindUniformBufferSource(2, &s_ubPerMesh, sizeof(s_ubPerMesh), 1000);
	s_shader->BindUniformBufferSource(3, &s_ubPerObject, sizeof(s_ubPerObject), 0);
	s_shader->CreatePipelineLayout();
}

void Mesh::DoneStatic()
{
	s_shader.Done();
}

void Mesh::Init()
{
	CSZ url = "[Models]:Actors/Gulman/Gulman.x3d";
	BaseMesh::Init(url);
}

void Mesh::Done()
{
	BaseMesh::Done();
}

void Mesh::Bind(CGI::CommandBufferPtr cb, UINT32 bindingsFilter)
{
	cb->BindVertexBuffers(_geo, bindingsFilter);
	cb->BindIndexBuffer(_geo);
}

void Mesh::UpdateUniformBufferPerObject(Point3 pos)
{
	VERUS_QREF_TIMER;

	Transform3 matT = Transform3::rotationY(timer.GetTime());
	matT.setTranslation(Vector3(pos));

	s_ubPerObject._matW = matT.UniformBufferFormat();
	s_ubPerObject._userColor = Vector4(1, 0, 0, 1).GLM();
}

void Mesh::UpdateUniformBufferPerMesh()
{
	memcpy(&s_ubPerMesh._posDeqScale, _posDeq + 0, 12);
	memcpy(&s_ubPerMesh._posDeqBias, _posDeq + 3, 12);
	memcpy(&s_ubPerMesh._tc0DeqScaleBias, _tc0Deq, 16);
	memcpy(&s_ubPerMesh._tc1DeqScaleBias, _tc1Deq, 16);
}

void Mesh::UpdateUniformBufferPerMaterial()
{
}

void Mesh::UpdateUniformBufferPerFrame(Scene::RcCamera cam)
{
	s_ubPerFrame._matV = cam.GetMatrixV().UniformBufferFormat();
	s_ubPerFrame._matP = cam.GetMatrixP().UniformBufferFormat();
	s_ubPerFrame._matVP = cam.GetMatrixVP().UniformBufferFormat();
}

void Mesh::CreateDeviceBuffers()
{
	_bindingMask = 0;
	const CGI::InputElementDesc ied[] =
	{
		{0, offsetof(VertexInputBinding0, _pos), CGI::IeType::_short, 4, CGI::IeUsage::position, 0},
		{0, offsetof(VertexInputBinding0, _tc0), CGI::IeType::_short, 2, CGI::IeUsage::texCoord, 0},
		{0, offsetof(VertexInputBinding0, _nrm), CGI::IeType::_ubyte, 4, CGI::IeUsage::normal,   0},
		{1, offsetof(VertexInputBinding1, _bw),  CGI::IeType::_short, 4, CGI::IeUsage::texCoord, 4},
		{1, offsetof(VertexInputBinding1, _bi),  CGI::IeType::_short, 4, CGI::IeUsage::texCoord, 5},
		{2, offsetof(VertexInputBinding2, _tan), CGI::IeType::_short, 4, CGI::IeUsage::tangent,  6},
		{2, offsetof(VertexInputBinding2, _bin), CGI::IeType::_short, 4, CGI::IeUsage::binormal, 7},
		{3, offsetof(VertexInputBinding3, _tc1), CGI::IeType::_short, 2, CGI::IeUsage::texCoord, 1},
		{3, offsetof(VertexInputBinding3, _clr), CGI::IeType::_ubyte, 4, CGI::IeUsage::color,    0},

		{-4, offsetof(PerInstanceData, _matPart0), CGI::IeType::_float, 4, CGI::IeUsage::texCoord, 8},
		{-4, offsetof(PerInstanceData, _matPart1), CGI::IeType::_float, 4, CGI::IeUsage::texCoord, 9},
		{-4, offsetof(PerInstanceData, _matPart2), CGI::IeType::_float, 4, CGI::IeUsage::texCoord, 10},
		{-4, offsetof(PerInstanceData, _instData), CGI::IeType::_float, 4, CGI::IeUsage::texCoord, 11},
		CGI::InputElementDesc::End()
	};

	CGI::GeometryDesc geoDesc;
	geoDesc._pInputElementDesc = ied;
	const int strides[] = { sizeof(VertexInputBinding0), sizeof(VertexInputBinding1), sizeof(VertexInputBinding2), sizeof(VertexInputBinding3), sizeof(PerInstanceData), 0 };
	geoDesc._pStrides = strides;
	geoDesc._32bitIndices = _vIndices.empty();
	_geo.Init(geoDesc);

	// Vertex buffer, binding 0:
	_bindingMask |= (1 << 0);
	_geo->CreateVertexBuffer(Utils::Cast32(_vBinding0.size()), 0);
	_geo->UpdateVertexBuffer(_vBinding0.data(), 0);

	// Vertex buffer, binding 1 (skinning):
	if (!_vBinding1.empty())
	{
		_bindingMask |= (1 << 1);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding1.size()), 1);
		_geo->UpdateVertexBuffer(_vBinding1.data(), 1);
	}

	// Vertex buffer, binding 2 (tangent space):
	if (!_vBinding2.empty())
	{
		_bindingMask |= (1 << 2);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding2.size()), 2);
		_geo->UpdateVertexBuffer(_vBinding2.data(), 2);
	}

	// Vertex buffer, binding 3 (lightmap):
	if (!_vBinding3.empty())
	{
		_bindingMask |= (1 << 3);
		_geo->CreateVertexBuffer(Utils::Cast32(_vBinding3.size()), 3);
		_geo->UpdateVertexBuffer(_vBinding3.data(), 3);
	}

	// Index buffer:
	if (!_vIndices32.empty())
	{
		_geo->CreateIndexBuffer(Utils::Cast32(_vIndices32.size()));
		_geo->UpdateIndexBuffer(_vIndices32.data());
	}
	else if (!_vIndices.empty())
	{
		_geo->CreateIndexBuffer(Utils::Cast32(_vIndices.size()));
		_geo->UpdateIndexBuffer(_vIndices.data());
	}
}

void Mesh::BufferDataVB(const void* p, int binding)
{
	_geo->UpdateVertexBuffer(p, binding);
}
