// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

LightMapBaker::LightMapBaker()
{
}

LightMapBaker::~LightMapBaker()
{
	Done();
}

void LightMapBaker::Init(RcDesc desc)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	_desc = desc;
	_pathname = desc._pathname;
	_desc._pathname = nullptr;

	_currentI = 0;
	_currentJ = 0;

	_vMap.resize(_desc._texWidth * _desc._texHeight);
	_invMapSize = 1.f / _vMap.size();

	switch (GetMode())
	{
	case Mode::faces:
	{
		_repsPerUpdate = Math::Max<int>(1, Utils::Cast32(_vMap.size()) / 100);
	}
	break;
	}

	Vector<Vertex> vVerts;
	vVerts.reserve(_desc._pMesh->GetVertCount());
	_desc._pMesh->ForEachVertex([&vVerts](int index, RcPoint3 pos, RcVector3 nrm, RcPoint3 tc)
		{
			Vertex v;
			v._pos = pos.GLM();
			v._nrm = nrm.GLM();
			v._tc = tc.GLM2();
			vVerts.push_back(v);
			return Continue::yes;
		});
	_vFaces.resize(_desc._pMesh->GetFaceCount());
	VERUS_FOR(i, _vFaces.size())
	{
		const int offset = i * 3;
		const int indices[3] =
		{
			_desc._pMesh->GetIndices()[offset + 0],
			_desc._pMesh->GetIndices()[offset + 1],
			_desc._pMesh->GetIndices()[offset + 2]
		};
		_vFaces[i]._v[0] = vVerts[indices[0]];
		_vFaces[i]._v[1] = vVerts[indices[1]];
		_vFaces[i]._v[2] = vVerts[indices[2]];
	}

	const float limit = Math::Min(0.5f, 2 / sqrt(static_cast<float>(_vFaces.size()))); // 2 shows best result.
	_quadtree.Init(Math::Bounds(Point3(0, 0), Point3(1, 1)), Vector3(limit, limit));
	_quadtree.SetDelegate(this);
	VERUS_FOR(i, _vFaces.size())
	{
		Math::Quadtree::Element element;
		VERUS_FOR(j, 3)
			element._bounds.Include(_vFaces[i]._v[j]._tc);
		element._pToken = reinterpret_cast<void*>(static_cast<INT64>(i));
		_quadtree.BindElement(element);
	}

	_rph = renderer->CreateRenderPass(
		{
			CGI::RP::Attachment("Color", CGI::Format::unormR8).LoadOpClear().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("Depth", CGI::Format::unormD16).LoadOpClear().Layout(CGI::ImageLayout::depthStencilAttachment),
		},
		{
			CGI::RP::Subpass("Sp0").Color(
				{
					CGI::RP::Ref("Color", CGI::ImageLayout::colorAttachment)
				}).DepthStencil(CGI::RP::Ref("Depth", CGI::ImageLayout::depthStencilAttachment)),
		},
		{});

	CGI::TextureDesc texDesc;
	texDesc._name = "LightMapBaker.ColorTex";
	texDesc._clearValue = Vector4::Replicate(1);
	texDesc._format = CGI::Format::unormR8;
	texDesc._width = _desc._texLumelSide;
	texDesc._height = _desc._texLumelSide;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::generateMips;
	texDesc._readbackMip = -1;
	_tex[TEX_COLOR].Init(texDesc);
	texDesc.Reset();
	texDesc._name = "LightMapBaker.DepthTex";
	texDesc._clearValue = Vector4(1);
	texDesc._format = CGI::Format::unormD16;
	texDesc._width = _desc._texLumelSide;
	texDesc._height = _desc._texLumelSide;
	_tex[TEX_DEPTH].Init(texDesc);

	_fbh = renderer->CreateFramebuffer(_rph,
		{
			_tex[TEX_COLOR],
			_tex[TEX_DEPTH]
		},
		_desc._texLumelSide,
		_desc._texLumelSide);
}

void LightMapBaker::Done()
{
	VERUS_QREF_RENDERER;
	renderer->DeleteFramebuffer(_fbh);
	renderer->DeleteRenderPass(_rph);
	VERUS_DONE(LightMapBaker);
}

void LightMapBaker::Update()
{
	if (!IsInitialized() || !IsBaking())
		return;
	VERUS_FOR(rep, _repsPerUpdate)
	{
		if (_currentI == _desc._texHeight)
		{
			Save();
			_desc._mode = Mode::idle;
			break;
		}

		_currentUV = glm::vec2(
			(_currentJ + 0.5f) / _desc._texWidth,
			(_currentI + 0.5f) / _desc._texHeight);

		_quadtree.TraverseVisible(_currentUV);

		_currentJ++;
		if (_currentJ == _desc._texWidth)
		{
			_currentJ = 0;
			_currentI++;
		}
	}
	_progress = Math::Clamp<float>((_currentI * _desc._texWidth + _currentJ) * _invMapSize, 0, 1);
}

void LightMapBaker::DrawLumel(RcPoint3 eyePos, RcVector3 frontDir)
{
	VERUS_QREF_RENDERER;

	renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh,
		{
			_tex[TEX_COLOR]->GetClearValue(),
			_tex[TEX_DEPTH]->GetClearValue()
		});

	renderer.GetCommandBuffer()->EndRenderPass();
}

Continue LightMapBaker::Quadtree_ProcessNode(void* pToken, void* pUser)
{
	RcFace face = _vFaces[reinterpret_cast<INT64>(pToken)];
	const int index = _currentI * _desc._texWidth + _currentJ;
	if (Math::IsPointInsideTriangle(face._v[0]._tc, face._v[1]._tc, face._v[2]._tc, _currentUV))
	{
		switch (GetMode())
		{
		case Mode::faces:
		{
			_vMap[index] = VERUS_COLOR_WHITE;
		}
		break;
		case Mode::ambientOcclusion:
		{
			const Vector3 bc = Math::Barycentric(face._v[0]._tc, face._v[1]._tc, face._v[2]._tc, _currentUV);
			const glm::vec3 pos = Math::BarycentricInterpolation(face._v[0]._pos, face._v[1]._pos, face._v[2]._pos, bc);
			const glm::vec3 nrm = Math::BarycentricInterpolation(face._v[0]._nrm, face._v[1]._nrm, face._v[2]._nrm, bc);

			const glm::vec4 nrm2 = glm::vec4(glm::normalize(nrm) * 0.5f + 0.5f, 1.f);

			_vMap[index] = Convert::ColorFloatToInt32(&nrm2.x);

			DrawLumel(pos, nrm);
		}
		break;
		}
	}
	return Continue::yes;
}

void LightMapBaker::Save()
{
	IO::FileSystem::SaveImage(_C(_pathname), _vMap.data(), _desc._texWidth, _desc._texHeight, IO::ImageFormat::extension);
}
