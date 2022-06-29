// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

LightMapBaker::LightMapBaker()
{
	VERUS_ZERO_MEM(_queuedCount);
}

LightMapBaker::~LightMapBaker()
{
	Done();
}

void LightMapBaker::Init(RcDesc desc)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;
	VERUS_QREF_TIMER;

	_desc = desc;
	_pathname = desc._pathname;
	_desc._pathname = nullptr;

	_stats = Stats();
	_stats._startTime = timer.GetTime();

	_currentI = 0;
	_currentJ = 0;

	_vMap.resize(_desc._texWidth * _desc._texHeight);
	_invMapSize = 1.f / _vMap.size();

	switch (GetMode())
	{
	case Mode::faces:
	{
		_repsPerUpdate = Math::Max<int>(1, Utils::Cast32(_vMap.size()) / 100);
		std::fill(_vMap.begin(), _vMap.end(), VERUS_COLOR_RGBA(0, 0, 0, 255));
	}
	break;
	case Mode::ambientOcclusion:
	{
		std::fill(_vMap.begin(), _vMap.end(), VERUS_COLOR_RGBA(255, 255, 255, 0));
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

	if (Mode::faces == GetMode())
		return;

	_rph = renderer->CreateRenderPass(
		{
			CGI::RP::Attachment("Color", CGI::Format::floatR32).LoadOpClear().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("Depth", CGI::Format::unormD24uintS8).LoadOpClear().Layout(CGI::ImageLayout::depthStencilAttachment),
		},
		{
			CGI::RP::Subpass("Sp0").Color(
				{
					CGI::RP::Ref("Color", CGI::ImageLayout::colorAttachment)
				}).DepthStencil(CGI::RP::Ref("Depth", CGI::ImageLayout::depthStencilAttachment)),
		},
		{});

	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), renderer.GetShaderQuad(), "#HemicubeMask", _rph);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_MUL;
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_HEMICUBE_MASK].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(renderer.GetGeoQuad(), renderer.GetShaderQuad(), "#", renderer.GetRenderPassHandle_AutoWithDepth());
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc.DisableDepthTest();
		_pipe[PIPE_QUAD].Init(pipeDesc);
	}

	CGI::TextureDesc texDesc;
	texDesc._name = "LightMapBaker.DepthTex";
	texDesc._clearValue = Vector4(1);
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._width = _desc._texLumelSide;
	texDesc._height = _desc._texLumelSide;
	_tex[TEX_DEPTH].Init(texDesc);
	texDesc.Reset();
	texDesc._name = "LightMapBaker.Dummy";
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texDesc._width = 16;
	texDesc._height = 16;
	_tex[TEX_DUMMY].Init(texDesc);

	VERUS_FOR(ringBufferIndex, CGI::BaseRenderer::s_ringBufferSize)
	{
		VERUS_FOR(batchIndex, s_batchSize)
		{
			CGI::TextureDesc texDesc;
			texDesc._name = "LightMapBaker.ColorTex";
			texDesc._clearValue = Vector4::Replicate(1);
			texDesc._format = CGI::Format::floatR32;
			texDesc._width = _desc._texLumelSide;
			texDesc._height = _desc._texLumelSide;
			texDesc._mipLevels = 0;
			texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::generateMips;
			texDesc._readbackMip = -1;
			_texColor[ringBufferIndex][batchIndex].Init(texDesc);
		}
		_cshQuad[ringBufferIndex] = renderer.GetShaderQuad()->BindDescriptorSetTextures(1, { _texColor[ringBufferIndex][0] });
	}
	_cshHemicubeMask = renderer.GetShaderQuad()->BindDescriptorSetTextures(1, { _tex[TEX_DUMMY] });

	VERUS_FOR(ringBufferIndex, CGI::BaseRenderer::s_ringBufferSize)
	{
		VERUS_FOR(batchIndex, s_batchSize)
		{
			CGI::TexturePtr tex = _texColor[ringBufferIndex][batchIndex];
			_fbh[ringBufferIndex][batchIndex] = renderer->CreateFramebuffer(_rph,
				{
					tex,
					_tex[TEX_DEPTH]
				},
				_desc._texLumelSide,
				_desc._texLumelSide);

			// Define texture:
			renderer.GetCommandBuffer()->BeginRenderPass(_rph, _fbh[ringBufferIndex][batchIndex],
				{
					tex->GetClearValue(),
					_tex[TEX_DEPTH]->GetClearValue()
				});
			renderer.GetCommandBuffer()->EndRenderPass();
			tex->GenerateMips();
		}
		_vQueued[ringBufferIndex].resize(s_batchSize);
	}

	_drawEmptyState = CGI::BaseRenderer::s_ringBufferSize + 1;
}

void LightMapBaker::Done()
{
	VERUS_QREF_RENDERER;
	if (renderer.GetShaderQuad())
	{
		VERUS_FOR(ringBufferIndex, CGI::BaseRenderer::s_ringBufferSize)
			renderer.GetShaderQuad()->FreeDescriptorSet(_cshQuad[ringBufferIndex]);
		renderer.GetShaderQuad()->FreeDescriptorSet(_cshHemicubeMask);
	}
	VERUS_FOR(ringBufferIndex, CGI::BaseRenderer::s_ringBufferSize)
	{
		VERUS_FOR(batchIndex, s_batchSize)
			renderer->DeleteFramebuffer(_fbh[ringBufferIndex][batchIndex]);
	}
	renderer->DeleteRenderPass(_rph);
	VERUS_DONE(LightMapBaker);
}

void LightMapBaker::Update()
{
	if (!IsInitialized() || !IsBaking())
		return;

	if (_drawEmptyState)
	{
		if (CGI::BaseRenderer::s_ringBufferSize + 1 == _drawEmptyState)
			DrawEmpty();
		_drawEmptyState--;
		if (!_drawEmptyState)
		{
			float value = 0;
			_texColor[0][0]->ReadbackSubresource(&value, false);
			_normalizationFactor = 1 / value;
		}
		else
		{
			return;
		}
	}

	VERUS_QREF_RENDERER;

	const int ringBufferIndex = renderer->GetRingBufferIndex();

	auto NextLumel = [this]()
	{
		_currentJ++;
		if (_currentJ == _desc._texWidth)
		{
			_currentJ = 0;
			_currentI++;
		}
	};

	int& queuedCount = _queuedCount[ringBufferIndex];
	if (queuedCount > 0) // Was there anything queued?
	{
		VERUS_FOR(i, queuedCount)
		{
			float value = 0;
			_texColor[ringBufferIndex][i]->ReadbackSubresource(&value, false);
			const BYTE value8 = Convert::UnormToUint8(Math::Clamp<float>(value * _normalizationFactor, 0, 1));

			BYTE* pDst = reinterpret_cast<BYTE*>(_vQueued[ringBufferIndex][i]._pDst);
			if (pDst[3]) // Already has some data?
			{
				VERUS_FOR(j, 3)
					pDst[j] = Math::Max(pDst[j], value8);
			}
			else // Empty?
			{
				VERUS_FOR(j, 3)
					pDst[j] = value8;
				pDst[3] = 0xFF;
			}
		}
	}
	// Clear queued:
	if (!_vQueued[ringBufferIndex].empty())
	{
		memset(_vQueued[ringBufferIndex].data(), 0, sizeof(Queued) * s_batchSize);
		queuedCount = 0;
	}

	if (_currentI >= _desc._texHeight)
	{
		bool finish = true;
		VERUS_FOR(ringBufferIndex, CGI::BaseRenderer::s_ringBufferSize)
		{
			if (_queuedCount[ringBufferIndex])
			{
				finish = false;
				break;
			}
		}
		if (finish)
		{
			if (Mode::faces != GetMode())
				ComputeEdgePadding();
			Save();
			_desc._mode = Mode::idle;
		}
		return;
	}

	VERUS_FOR(rep, _repsPerUpdate)
	{
		// Fill queue:
		int& queuedCount = _queuedCount[ringBufferIndex];
		VERUS_RT_ASSERT(!queuedCount);
		while (queuedCount + s_maxLayers <= s_batchSize) // Enough space for this lumel?
		{
			_currentUV = glm::vec2(
				(_currentJ + 0.5f) / _desc._texWidth,
				(_currentI + 0.5f) / _desc._texHeight);

			_currentLayer = 0;
			_quadtree.TraverseVisible(_currentUV);
			_stats._maxLayer = Math::Max<int>(_stats._maxLayer, _currentLayer);

			NextLumel();
			if (_currentI >= _desc._texHeight)
				break; // No more lumels.
		}

		// Draw queue:
		VERUS_FOR(i, queuedCount)
		{
			RQueued queued = _vQueued[ringBufferIndex][i];
			DrawLumel(queued._pos, queued._nrm, i);
		}
	}

	const int progressPrev = static_cast<int>(_stats._progress * 100);
	_stats._progress = Math::Clamp<float>((_currentI * _desc._texWidth + _currentJ) * _invMapSize, 0, 1);
	const int progressNext = static_cast<int>(_stats._progress * 100);
	if (progressPrev != progressNext)
	{
		VERUS_QREF_TIMER;
		const float elapsedTime = timer.GetTime() - _stats._startTime;
		char buffer[80];
		sprintf_s(buffer, "Elapsed time: %.1fs, max layer: %d, progress: %.1f%%", elapsedTime, _stats._maxLayer, _stats._progress * 100);
		_stats._info = buffer;
	}
}

void LightMapBaker::Draw()
{
	if (!IsInitialized() || !IsBaking() || !_debugDraw || Mode::faces == GetMode())
		return;

	VERUS_QREF_RENDERER;

	const int ringBufferIndex = renderer->GetRingBufferIndex();
	auto cb = renderer.GetCommandBuffer();
	auto shader = renderer.GetShaderQuad();

	renderer.GetUbQuadVS()._matW = Math::QuadMatrix().UniformBufferFormat();
	renderer.GetUbQuadVS()._matV = Math::ToUVMatrix().UniformBufferFormat();
	renderer.ResetQuadMultiplexer();

	cb->BindPipeline(_pipe[PIPE_QUAD]);
	shader->BeginBindDescriptors();
	cb->BindDescriptors(shader, 0);
	cb->BindDescriptors(shader, 1, _cshQuad[ringBufferIndex]);
	shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());
}

void LightMapBaker::DrawEmpty()
{
	// Let's compute normalization factor by drawing an empty scene.
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();
	CGI::TexturePtr tex = _texColor[0][0];

	cb->BeginRenderPass(_rph, _fbh[0][0],
		{
			tex->GetClearValue(),
			_tex[TEX_DEPTH]->GetClearValue()
		});

	DrawHemicubeMask();

	cb->EndRenderPass();

	tex->GenerateMips();
	tex->ReadbackSubresource(nullptr);
}

void LightMapBaker::DrawHemicubeMask()
{
	VERUS_QREF_RENDERER;

	auto cb = renderer.GetCommandBuffer();
	auto shader = renderer.GetShaderQuad();

	const float side = static_cast<float>(_desc._texLumelSide);

	renderer.GetUbQuadVS()._matW = Math::QuadMatrix().UniformBufferFormat();
	renderer.GetUbQuadVS()._matV = Math::ToUVMatrix().UniformBufferFormat();
	renderer.ResetQuadMultiplexer();

	cb->BindPipeline(_pipe[PIPE_HEMICUBE_MASK]);
	cb->SetViewport({ Vector4(0, 0, side, side) });
	shader->BeginBindDescriptors();
	cb->BindDescriptors(shader, 0);
	cb->BindDescriptors(shader, 1, _cshHemicubeMask);
	shader->EndBindDescriptors();
	renderer.DrawQuad(cb.Get());
}

void LightMapBaker::DrawLumel(RcPoint3 pos, RcVector3 nrm, int batchIndex)
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	const int ringBufferIndex = renderer->GetRingBufferIndex();
	auto cb = renderer.GetCommandBuffer();
	auto shader = renderer.GetShaderQuad();
	CGI::TexturePtr tex = _texColor[ringBufferIndex][batchIndex];

	cb->BeginRenderPass(_rph, _fbh[ringBufferIndex][batchIndex],
		{
			tex->GetClearValue(),
			_tex[TEX_DEPTH]->GetClearValue()
		});
	if (_pDelegate)
	{
		const glm::vec2 randVec = glm::circularRand(1.f);
		const Matrix3 m3 = Matrix3::MakeTrackTo(Vector3(0, 0, 1), nrm);
		const Vector3 perpVecA = m3 * Vector3(randVec.x, randVec.y, 0); // Up.
		const Vector3 perpVecB = VMath::cross(perpVecA, nrm); // Side.

		DrawLumelDesc drawLumelDesc;
		drawLumelDesc._eyePos = pos;
		drawLumelDesc._frontDir = nrm;
		_pDelegate->LightMapBaker_Draw(CGI::CubeMapFace::none, drawLumelDesc);
		VERUS_FOR(i, 5)
		{
			const CGI::CubeMapFace cubeMapFace = static_cast<CGI::CubeMapFace>(i);

			Camera cam;
			cam.MoveEyeTo(drawLumelDesc._eyePos);
			switch (cubeMapFace)
			{
			case CGI::CubeMapFace::posX:
				cam.MoveAtTo(drawLumelDesc._eyePos + perpVecB);
				cam.SetUpDirection(perpVecA);
				break;
			case CGI::CubeMapFace::negX:
				cam.MoveAtTo(drawLumelDesc._eyePos - perpVecB);
				cam.SetUpDirection(perpVecA);
				break;
			case CGI::CubeMapFace::posY:
				cam.MoveAtTo(drawLumelDesc._eyePos + perpVecA);
				cam.SetUpDirection(-nrm);
				break;
			case CGI::CubeMapFace::negY:
				cam.MoveAtTo(drawLumelDesc._eyePos - perpVecA);
				cam.SetUpDirection(nrm);
				break;
			case CGI::CubeMapFace::posZ:
				cam.MoveAtTo(drawLumelDesc._eyePos + nrm);
				cam.SetUpDirection(perpVecA);
				break;
			}
			cam.SetYFov(VERUS_PI * 0.5f);
			cam.SetAspectRatio(1);
			cam.SetZNear(0.00001f);
			cam.SetZFar(_desc._distance);
			cam.Update();
			PCamera pPrevCamera = sm.SetCamera(&cam);

			drawLumelDesc._frontDir = cam.GetFrontDirection();
			_pDelegate->LightMapBaker_Draw(cubeMapFace, drawLumelDesc);

			sm.SetCamera(pPrevCamera);
		}
		_pDelegate->LightMapBaker_Draw(CGI::CubeMapFace::negZ, drawLumelDesc);

		DrawHemicubeMask();
	}
	cb->EndRenderPass();

	tex->GenerateMips();
	tex->ReadbackSubresource(nullptr);
}

Continue LightMapBaker::Quadtree_ProcessNode(void* pToken, void* pUser)
{
	RcFace face = _vFaces[reinterpret_cast<INT64>(pToken)];
	const int index = _currentI * _desc._texWidth + _currentJ;
	if (Math::IsPointInsideTriangle(face._v[0]._tc, face._v[1]._tc, face._v[2]._tc, _currentUV))
	{
		VERUS_QREF_RENDERER;
		switch (GetMode())
		{
		case Mode::faces:
		{
			_vMap[index] = VERUS_COLOR_WHITE;
		}
		break;
		case Mode::ambientOcclusion:
		{
			VERUS_RT_ASSERT(_currentLayer < s_maxLayers);

			const Vector3 bc = Math::Barycentric(face._v[0]._tc, face._v[1]._tc, face._v[2]._tc, _currentUV);
			const glm::vec3 pos = Math::BarycentricInterpolation(face._v[0]._pos, face._v[1]._pos, face._v[2]._pos, bc);
			const glm::vec3 nrm = Math::BarycentricInterpolation(face._v[0]._nrm, face._v[1]._nrm, face._v[2]._nrm, bc);

			const int ringBufferIndex = renderer->GetRingBufferIndex();
			int& queuedCount = _queuedCount[ringBufferIndex];
			RQueued queued = _vQueued[ringBufferIndex][queuedCount];
			queued._pos = pos;
			queued._nrm = glm::normalize(nrm);
			queued._pos += Vector3(nrm) * _desc._bias;
			queued._pDst = &_vMap[index];

			_currentLayer++;
			queuedCount++;

			return (_currentLayer >= s_maxLayers) ? Continue::no : Continue::yes;
		}
		break;
		}
	}
	return Continue::yes;
}

void LightMapBaker::ComputeEdgePadding()
{
	Utils::ComputeEdgePadding(
		reinterpret_cast<BYTE*>(_vMap.data()) + 0, sizeof(UINT32),
		reinterpret_cast<BYTE*>(_vMap.data()) + 3, sizeof(UINT32),
		_desc._texWidth, _desc._texHeight);
}

void LightMapBaker::Save()
{
	IO::FileSystem::SaveImage(_C(_pathname), _vMap.data(), _desc._texWidth, _desc._texHeight, IO::ImageFormat::extension);
}

void LightMapBaker::BindPipeline(RcMesh mesh, CGI::CommandBufferPtr cb)
{
	static const PIPE pipes[] =
	{
		PIPE_MESH_SIMPLE_BAKE_AO,
		PIPE_MESH_SIMPLE_BAKE_AO_INSTANCED,
		PIPE_MESH_SIMPLE_BAKE_AO_ROBOTIC,
		PIPE_MESH_SIMPLE_BAKE_AO_SKINNED
	};
	const PIPE pipe = pipes[mesh.GetPipelineIndex()];
	if (!_pipe[pipe])
	{
		static CSZ branches[] =
		{
			"#BakeAO",
			"#BakeAOInstanced",
			"#BakeAORobotic",
			"#BakeAOSkinned"
		};
		CGI::PipelineDesc pipeDesc(mesh.GetGeometry(), Mesh::GetSimpleShader(), branches[pipe], _rph);
		pipeDesc._vertexInputBindingsFilter = mesh.GetBindingsMask();
		_pipe[pipe].Init(pipeDesc);
	}
	cb->BindPipeline(_pipe[pipe]);
}

void LightMapBaker::SetViewportFor(CGI::CubeMapFace cubeMapFace, CGI::CommandBufferPtr cb)
{
	const float sideDiv2 = static_cast<float>(_desc._texLumelSide >> 1);
	const float sideDiv4 = static_cast<float>(_desc._texLumelSide >> 2);
	switch (cubeMapFace)
	{
	case CGI::CubeMapFace::posX:
		cb->SetViewport({ Vector4(-sideDiv4, sideDiv4, sideDiv2, sideDiv2) });
		break;
	case CGI::CubeMapFace::negX:
		cb->SetViewport({ Vector4(sideDiv2 + sideDiv4, sideDiv4, sideDiv2, sideDiv2) });
		break;
	case CGI::CubeMapFace::posY:
		cb->SetViewport({ Vector4(sideDiv4, -sideDiv4, sideDiv2, sideDiv2) });
		break;
	case CGI::CubeMapFace::negY:
		cb->SetViewport({ Vector4(sideDiv4, sideDiv2 + sideDiv4, sideDiv2, sideDiv2) });
		break;
	case CGI::CubeMapFace::posZ:
		cb->SetViewport({ Vector4(sideDiv4, sideDiv4, sideDiv2, sideDiv2) });
		break;
	}
}
