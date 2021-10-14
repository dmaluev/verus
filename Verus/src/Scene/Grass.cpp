// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CGI::ShaderPwn    Grass::s_shader;
Grass::UB_GrassVS Grass::s_ubGrassVS;
Grass::UB_GrassFS Grass::s_ubGrassFS;

Grass::Grass()
{
	VERUS_CT_ASSERT(16 == sizeof(Vertex));
	VERUS_FOR(i, 4)
	{
		VERUS_FOR(j, 4)
		{
			const char u0 = j * 25 + 1;
			const char v0 = i * 25 + 1;
			const char u1 = u0 + 25 - 2;
			const char v1 = v0 + 25 - 2;
			const int index = (i << 2) + j;
			_bushTexCoords[index][0][0] = u0; _bushTexCoords[index][0][1] = v1;
			_bushTexCoords[index][1][0] = u0; _bushTexCoords[index][1][1] = v0;
			_bushTexCoords[index][2][0] = u1; _bushTexCoords[index][2][1] = v0;
			_bushTexCoords[index][3][0] = u1; _bushTexCoords[index][3][1] = v1;
			_bushTexCoords[index][4][0] = u0; _bushTexCoords[index][4][1] = v1;
			_bushTexCoords[index][5][0] = u0; _bushTexCoords[index][5][1] = v0;
			_bushTexCoords[index][6][0] = u1; _bushTexCoords[index][6][1] = v0;
			_bushTexCoords[index][7][0] = u1; _bushTexCoords[index][7][1] = v1;
		}
	}
}

Grass::~Grass()
{
	Done();
}

void Grass::InitStatic()
{
	VERUS_QREF_CONST_SETTINGS;

	s_shader.Init("[Shaders]:DS_Grass.hlsl");
	s_shader->CreateDescriptorSet(0, &s_ubGrassVS, sizeof(s_ubGrassVS), settings.GetLimits()._grass_ubVSCapacity,
		{
			CGI::Sampler::linearMipL, // Height
			CGI::Sampler::nearestMipN, // Normal
			CGI::Sampler::nearestMipN, // MLayer
		}, CGI::ShaderStageFlags::vs);
	s_shader->CreateDescriptorSet(1, &s_ubGrassFS, sizeof(s_ubGrassFS), settings.GetLimits()._grass_ubFSCapacity,
		{
			CGI::Sampler::linearMipL
		}, CGI::ShaderStageFlags::fs);
	s_shader->CreatePipelineLayout();
}

void Grass::DoneStatic()
{
	s_shader.Done();
}

void Grass::Init(RTerrain terrain, CSZ atlasUrl)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	_pTerrain = &terrain;

	_bushMask = 0;

	if (!Math::IsPowerOfTwo(_pTerrain->GetMapSide()))
		throw VERUS_RECOVERABLE << "Init(); mapSide must be power of two";

	_mapSide = _pTerrain->GetMapSide();
	_mapShift = Math::HighestBit(_mapSide);

	const int patchSide = _mapSide >> 4;
	const int patchCount = patchSide * patchSide;
	const int maxInstances = 128 * 8;

	_vPatches.resize(patchCount);

	CreateBuffers();
	_vInstanceBuffer.resize(maxInstances);

	CGI::GeometryDesc geoDesc;
	geoDesc._name = "Grass.Geo";
	const CGI::VertexInputAttrDesc viaDesc[] =
	{
		{ 0, offsetof(Vertex, _pos),               CGI::ViaType::shorts, 4, CGI::ViaUsage::position, 0},
		{ 0, offsetof(Vertex, _tc),                CGI::ViaType::shorts, 4, CGI::ViaUsage::texCoord, 0},
		{-1, offsetof(PerInstanceData, _patchPos), CGI::ViaType::shorts, 4, CGI::ViaUsage::instData, 0},
		CGI::VertexInputAttrDesc::End()
	};
	geoDesc._pVertexInputAttrDesc = viaDesc;
	const int strides[] = { sizeof(Vertex), sizeof(PerInstanceData), 0 };
	geoDesc._pStrides = strides;
	_geo.Init(geoDesc);
	_geo->CreateVertexBuffer(Utils::Cast32(_vPatchMeshVB.size()), 0);
	_geo->CreateVertexBuffer(maxInstances, 1);
	_geo->CreateIndexBuffer(Utils::Cast32(_vPatchMeshIB.size()));
	_geo->UpdateVertexBuffer(_vPatchMeshVB.data(), 0);
	_geo->UpdateIndexBuffer(_vPatchMeshIB.data());

	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader, "#", renderer.GetDS().GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
		_pipe[PIPE_MAIN].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader, "#Billboards", renderer.GetDS().GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._topology = CGI::PrimitiveTopology::pointList;
		_pipe[PIPE_BILLBOARDS].Init(pipeDesc);
	}

	if (atlasUrl)
	{
		_tex.Init(atlasUrl);
	}
	else
	{
		const int texW = 1024;
		const int texH = 1024;
		_vTextureSubresData.clear();
		_vTextureSubresData.resize(texW * texH);

		CGI::TextureDesc texDesc;
		texDesc._name = "Grass.Tex";
		texDesc._format = CGI::Format::srgbR8G8B8A8;
		texDesc._width = texW;
		texDesc._height = texH;
		texDesc._mipLevels = 0;
		texDesc._flags = CGI::TextureDesc::Flags::generateMips;
		_tex.Init(texDesc);
		_tex->UpdateSubresource(_vTextureSubresData.data());
		_tex->GenerateMips();
	}

	s_shader->FreeDescriptorSet(_cshVS);
	_cshVS = s_shader->BindDescriptorSetTextures(0,
		{
			_pTerrain->GetHeightmapTexture(),
			_pTerrain->GetNormalsTexture(),
			_pTerrain->GetMainLayerTexture(),
		});

	_vMagnets.resize(16);
	std::fill(_vMagnets.begin(), _vMagnets.end(), Magnet());
}

void Grass::Done()
{
	s_shader->FreeDescriptorSet(_cshVS);
	s_shader->FreeDescriptorSet(_cshFS);
	VERUS_DONE(Grass);
}

void Grass::ResetInstanceCount()
{
	_instanceCount = 0;
}

void Grass::Update()
{
	VERUS_UPDATE_ONCE_CHECK;

	VERUS_QREF_ATMO;
	VERUS_QREF_TIMER;

	if (!_cshFS.IsSet() && _tex->IsLoaded())
		_cshFS = s_shader->BindDescriptorSetTextures(1, { _tex });

	_phase = glm::fract(_phase + dt * 2.3f);

	const float windSpeed = atmo.GetWindSpeed();
	const float warpStrength = Math::Clamp<float>(windSpeed * (1 / 13.f), 0, 1.f);
	const float turbulence = Math::Clamp<float>(windSpeed * (1 / 17.f), 0, 0.4f);

	_warpSpring.Update(atmo.GetWindDirection() * (warpStrength * 30.f));
	_turbulence = turbulence * turbulence;

	// Async texture loading:
	if (!_vTextureSubresData.empty())
	{
		VERUS_FOR(i, s_maxBushTypes)
		{
			if (_texLoaded[i].IsInitialized() && _texLoaded[i].IsLoaded())
				OnTextureLoaded(i);
		}
	}
}

void Grass::Layout()
{
	VERUS_QREF_SM;

	const float zFar = sm.GetCamera()->GetZFar();
	sm.GetCamera()->SetZFar(128);
	sm.GetCamera()->Update();

	Math::RQuadtreeIntegral quadtree = _pTerrain->GetQuadtree();
	_visiblePatchCount = 0;
	quadtree.SetDelegate(this);
	quadtree.TraverseVisible();
	quadtree.SetDelegate(_pTerrain);

	sm.GetCamera()->SetZFar(zFar);
	sm.GetCamera()->Update();
}

void Grass::Draw()
{
	if (!_visiblePatchCount)
		return;
	if (!_cshFS.IsSet())
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_RT_ASSERT(_pTerrain->GetMapSide() == _mapSide);

	const bool drawingDepth = Scene::SceneManager::IsDrawingDepth(Scene::DrawDepth::automatic);

	const UINT32 bushMask = _bushMask & 0xFF; // Use only first 8 bushes, next 8 are reserved.
	const int half = _mapSide >> 1;
	const int offset = _instanceCount;

	auto cb = renderer.GetCommandBuffer();

	s_ubGrassVS._matW = Transform3::UniformBufferFormatIdentity();
	s_ubGrassVS._matWV = sm.GetCamera()->GetMatrixV().UniformBufferFormat();
	s_ubGrassVS._matVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubGrassVS._matP = sm.GetCamera()->GetMatrixP().UniformBufferFormat();
	s_ubGrassVS._phase_mapSideInv_bushMask.x = _phase;
	s_ubGrassVS._phase_mapSideInv_bushMask.y = 1.f / _mapSide;
	s_ubGrassVS._phase_mapSideInv_bushMask.z = *(float*)&bushMask;
	s_ubGrassVS._posEye = float4(sm.GetMainCamera()->GetEyePosition().GLM(), 0);
	s_ubGrassVS._viewportSize = cb->GetViewportSize().GLM();
	s_ubGrassVS._warp_turb = Vector4(_warpSpring.GetOffset(), _turbulence).GLM();

	cb->BindVertexBuffers(_geo);
	cb->BindIndexBuffer(_geo);

	s_shader->BeginBindDescriptors();

	cb->BindPipeline(_pipe[PIPE_BILLBOARDS]);
	cb->BindDescriptors(s_shader, 0, _cshVS);
	cb->BindDescriptors(s_shader, 1, _cshFS);
	int pointSpriteInstCount = 0;
	VERUS_FOR(i, _visiblePatchCount)
	{
		RcPatch patch = _vPatches[i];
		if (patch._type & 0x2)
		{
			const int at = _instanceCount + pointSpriteInstCount;
			_vInstanceBuffer[at]._patchPos[0] = patch._j - half;
			_vInstanceBuffer[at]._patchPos[1] = patch._h;
			_vInstanceBuffer[at]._patchPos[2] = patch._i - half;
			_vInstanceBuffer[at]._patchPos[3] = 0;
			pointSpriteInstCount++;
		}
	}
	cb->Draw(_bbVertCount, pointSpriteInstCount, _vertCount, _instanceCount);
	_instanceCount += pointSpriteInstCount;

	cb->BindPipeline(_pipe[PIPE_MAIN]);
	cb->BindDescriptors(s_shader, 0, _cshVS);
	cb->BindDescriptors(s_shader, 1, _cshFS);
	int meshInstCount = 0;
	VERUS_FOR(i, _visiblePatchCount)
	{
		RcPatch patch = _vPatches[i];
		if (patch._type & 0x1)
		{
			const int at = _instanceCount + meshInstCount;
			_vInstanceBuffer[at]._patchPos[0] = patch._j - half;
			_vInstanceBuffer[at]._patchPos[1] = patch._h;
			_vInstanceBuffer[at]._patchPos[2] = patch._i - half;
			_vInstanceBuffer[at]._patchPos[3] = 0;
			meshInstCount++;
		}
	}
	cb->DrawIndexed(Utils::Cast32(_vPatchMeshIB.size()), meshInstCount, 0, 0, _instanceCount);
	_instanceCount += meshInstCount;

	s_shader->EndBindDescriptors();

	_geo->UpdateVertexBuffer(_vInstanceBuffer.data(), 1, cb.Get(), _instanceCount - offset, offset);
}

void Grass::QuadtreeIntegral_ProcessVisibleNode(const short ij[2], RcPoint3 center)
{
	VERUS_QREF_SM;

	const RcPoint3 eyePos = sm.GetMainCamera()->GetEyePosition();
	const float distSq = VMath::distSqr(eyePos, center);
	if (distSq >= 128 * 128.f)
		return;

	Patch patch;
	patch._i = ij[0];
	patch._j = ij[1];
	patch._h = Terrain::ConvertHeight(center.getY());
	patch._type = 0;
	if (distSq <= 64 * 64.f)
		patch._type |= 0x1;
	if (distSq >= 16 * 16.f)
		patch._type |= 0x2;
	_vPatches[_visiblePatchCount++] = patch;
}

void Grass::QuadtreeIntegral_GetHeights(const short ij[2], float height[2])
{
	VERUS_RT_FAIL(__FUNCTION__);
}

void Grass::CreateBuffers()
{
	VERUS_QREF_CONST_SETTINGS;

	const int density = settings._sceneGrassDensity;
	const int side = static_cast<int>(sqrt(float(density)));
	const int count = side * side;
	const float step = 16.f / side;

	Random random(9052);
	auto RandomStep = [&random, step]()
	{
		return (0.1f + 0.8f * random.NextFloat()) * step;
	};
	auto RandomScale = [&random]()
	{
		return random.NextFloat() * 0.4f;
	};

	static const Point3 bushVerts[8] =
	{
		Point3(-0.5f, 0.f, 0.f), // A (0)
		Point3(-0.5f, 1.f, 0.f), // B (1)
		Point3(+0.5f, 1.f, 0.f), // C (2)
		Point3(+0.5f, 0.f, 0.f), // D (3)
		Point3(0.f, 0.f, -0.5f), // A2 (4)
		Point3(0.f, 1.f, -0.5f), // B2 (5)
		Point3(0.f, 1.f, +0.5f), // C2 (6)
		Point3(0.f, 0.f, +0.5f)  // D2 (7)
	};
	static const UINT16 bushIndices[12] =
	{
		1, 0, 2,
		2, 0, 3,
		5, 6, 4,
		4, 6, 7
	};

	struct Bush
	{
		float _x, _z;
		Point3 _vertPos[8];
		float _phaseShift;
	};
	Vector<Bush> vBushes;
	vBushes.reserve(count);

	VERUS_FOR(i, side)
	{
		VERUS_FOR(j, side)
		{
			const Point3 bushPos(
				j * step + RandomStep(),
				0,
				i * step + RandomStep());
			const Vector3 scale(
				0.8f + RandomScale(),
				0.8f + RandomScale(),
				0.8f + RandomScale());
			const float phaseSin =
				sin(bushPos.getX() * (VERUS_2PI / 16)) *
				sin(bushPos.getZ() * (VERUS_2PI / 16)) * 0.5f + 0.5f;
			const float phase = phaseSin * 0.75f + random.NextFloat() * 0.25f;
			const float angle = random.NextFloat() * VERUS_2PI;

			const Matrix3 matR = Matrix3::rotationY(angle);
			const Transform3 tr = VMath::appendScale(Transform3(matR, Vector3(bushPos)), scale);

			Bush bush;
			bush._x = bushPos.getX();
			bush._z = bushPos.getZ();
			VERUS_FOR(k, 8)
				bush._vertPos[k] = tr * bushVerts[k];
			bush._phaseShift = phase;
			vBushes.push_back(bush);
		}
	}

	std::shuffle(vBushes.begin(), vBushes.end(), random.GetGenerator());

	const int indexedMeshVertCount = count * 8;
	_vertCount = 0;
	_bbVertCount = 0;
	_vPatchMeshVB.clear();
	_vPatchMeshVB.resize(indexedMeshVertCount + count);
	_vPatchMeshIB.clear();
	_vPatchMeshIB.reserve(count * 12);
	for (const auto& bush : vBushes)
	{
		const int indexOffset = _vertCount;
		VERUS_FOR(i, 8)
		{
			Vertex vertex;
			vertex._pos[0] = static_cast<short>(bush._vertPos[i].getX() * 1000);
			vertex._pos[1] = static_cast<short>(bush._vertPos[i].getY() * 1000);
			vertex._pos[2] = static_cast<short>(bush._vertPos[i].getZ() * 1000);
			vertex._pos[3] = static_cast<short>(bush._phaseShift * 99);
			vertex._tc[0] = _bushTexCoords[0][i][0];
			vertex._tc[1] = _bushTexCoords[0][i][1];
			vertex._tc[2] = static_cast<short>(bush._x * 1000);
			vertex._tc[3] = static_cast<short>(bush._z * 1000);
			_vPatchMeshVB[_vertCount++] = vertex;

			if (1 == i) // Index with top-left tex coords:
			{
				const int offset = indexedMeshVertCount + _bbVertCount;
				_vPatchMeshVB[offset] = vertex;
				_bbVertCount++;
			}
		}
		VERUS_FOR(i, 12)
			_vPatchMeshIB.push_back(bushIndices[i] + indexOffset);
	}
}

void Grass::ResetAllTextures()
{
	_bushMask = 0;
}

void Grass::LoadLayersFromFile(CSZ url)
{
	if (!url)
		url = "[Misc]:TerrainLayers.xml";
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData);
	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size());
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
	pugi::xml_node root = doc.first_child();
	for (auto node : root.children("grass"))
	{
		const int id = node.attribute("id").as_int();
		SetTexture(id, node.attribute("url").value(), node.attribute("urlEx").value());
	}
}

void Grass::SetTexture(int layer, CSZ url, CSZ url2)
{
	if (_vTextureSubresData.empty()) // Using DDS?
	{
		_bushMask |= (1 << layer);
		if (layer < 8)
			_bushMask |= (1 << (layer + 8));
		return;
	}

	VERUS_RT_ASSERT(url);
	_texLoaded[layer].LoadDDS(url);
	if (layer < 8)
		SetTexture(layer + 8, url2 ? url2 : url);
}

void Grass::SaveTexture(CSZ url)
{
	IO::FileSystem::SaveImage(url, _vTextureSubresData.data(), 1024, 1024);
}

void Grass::OnTextureLoaded(int layer)
{
	VERUS_QREF_RENDERER;

	const int srcW = 256;
	const int srcH = 256;
	const int dstW = 1024;
	const int xFrom = (layer & 0x3) << 8;
	const int yFrom = (layer >> 2) << 8;

	const UINT32* pSrc = reinterpret_cast<const UINT32*>(_texLoaded[layer].GetData());

	VERUS_FOR(y, srcH)
	{
		const int ySrcOffset = y * srcW;
		const int yDstOffset = (yFrom + y) * dstW;
		memcpy(_vTextureSubresData.data() + yDstOffset + xFrom, pSrc + ySrcOffset, srcW * sizeof(UINT32));
	}
	_texLoaded[layer].Done();

	_tex->UpdateSubresource(_vTextureSubresData.data());
	_tex->GenerateMips();

	_bushMask |= (1 << layer);
}

int Grass::BeginMagnet(RcPoint3 pos, float radius)
{
	VERUS_FOR(i, _vMagnets.size())
	{
		if (!_vMagnets[i]._active)
		{
			_vMagnets[i]._active = true;
			_vMagnets[i]._pos = pos;
			_vMagnets[i]._radius = radius;
			_vMagnets[i]._radiusSq = radius * radius;
			_vMagnets[i]._radiusSqInv = 1 / _vMagnets[i]._radiusSq;
			_vMagnets[i]._strength = 0.5f;
			return i;
		}
	}
	return -1;
}

void Grass::EndMagnet(int index)
{
	_vMagnets[index]._active = false;
}

void Grass::UpdateMagnet(int index, RcPoint3 pos, float radius)
{
	_vMagnets[index]._active = true;
	_vMagnets[index]._pos = pos;
	if (radius)
	{
		_vMagnets[index]._radius = radius;
		_vMagnets[index]._radiusSq = radius * radius;
		_vMagnets[index]._radiusSqInv = 1 / _vMagnets[index]._radiusSq;
	}
}
