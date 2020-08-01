#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CGI::ShaderPwn      Forest::s_shader;
Forest::UB_ForestVS Forest::s_ubForestVS;
Forest::UB_ForestFS Forest::s_ubForestFS;

// Forest::Plant:

float Forest::Plant::GetSize() const
{
	return _mesh.GetBounds().GetMaxExtentFromOrigin(Vector3(2, 1, 2));
}

// Forest:

Forest::Forest()
{
	VERUS_CT_ASSERT(16 == sizeof(Vertex));
}

Forest::~Forest()
{
	Done();
}

void Forest::InitStatic()
{
	s_shader.Init("[Shaders]:DS_Forest.hlsl");
	s_shader->CreateDescriptorSet(0, &s_ubForestVS, sizeof(s_ubForestVS), 100, {}, CGI::ShaderStageFlags::vs_hs_ds_fs);
	s_shader->CreateDescriptorSet(1, &s_ubForestFS, sizeof(s_ubForestFS), 100,
		{
			CGI::Sampler::aniso,
			CGI::Sampler::aniso,
			CGI::Sampler::aniso
		}, CGI::ShaderStageFlags::fs);
	s_shader->CreatePipelineLayout();
}

void Forest::DoneStatic()
{
	s_shader.Done();
}

void Forest::Init(PTerrain pTerrain)
{
	VERUS_INIT();

	_pTerrain = pTerrain;

	_octree.SetDelegate(this);
	const float hf = _pTerrain->GetMapSide() * 0.5f;
	Math::Bounds bounds;
	bounds.Set(
		Vector3(-hf, -hf, -hf),
		Vector3(+hf, +hf, +hf));
	const Vector3 limit(hf / 6, hf / 6, hf / 6);
	_octree.Init(bounds, limit);

	const Scatter::TypeDesc id[] =
	{
		Scatter::TypeDesc(SCATTER_TYPE_100, 100),
		Scatter::TypeDesc(SCATTER_TYPE_50, 50),
		Scatter::TypeDesc(SCATTER_TYPE_25, 25),
		Scatter::TypeDesc(SCATTER_TYPE_20, 20),
		Scatter::TypeDesc(SCATTER_TYPE_15, 15),
		Scatter::TypeDesc(SCATTER_TYPE_10, 10),
		Scatter::TypeDesc(SCATTER_TYPE_5, 5),
		Scatter::TypeDesc(SCATTER_TYPE_3, 3)
	};
	_scatter.Init(32, SCATTER_TYPE_COUNT, id, 19201);
	_scatter.SetDelegate(this);

	_vPlants.reserve(16);
	_vLayerPlants.reserve(16);
	_vDrawPlants.resize(_capacity);
}

void Forest::Done()
{
	VERUS_DONE(Forest);
}

void Forest::ResetInstanceCount()
{
	for (auto& plant : _vPlants)
		plant._mesh.ResetInstanceCount();
}

void Forest::Update()
{
	if (!IsInitialized())
		return;
	VERUS_UPDATE_ONCE_CHECK;

	if (!_async_initPlants)
	{
		_async_initPlants = true;
		for (auto& plant : _vPlants)
		{
			Mesh::Desc meshDesc;
			meshDesc._url = _C(plant._url);
			meshDesc._instanceCapacity = _capacity;
			meshDesc._initShape = true;
			plant._mesh.Init(meshDesc);

			Material::Desc matDesc;
			matDesc._name = _C(plant._url);
			matDesc._load = true;
			plant._material.Init(matDesc);
		}
	}

	for (auto& plant : _vPlants)
	{
		plant._material->IncludePart(0);

		if (!plant._maxSize && plant._mesh.IsLoaded())
		{
			// Time to get the size of this mesh.
			plant._maxSize = plant.GetSize() * plant._maxScale;
			BakeChunks(plant);
			if (plant._maxSize > _maxSizeAll)
			{
				_maxSizeAll = plant._maxSize;
				_pTerrain->FattenQuadtreeNodesBy(_maxSizeAll);
			}
			plant._aoSize = plant._mesh.GetBounds().GetAverageSize() * 1.7f;
		}

		if (!plant._tex[0] && plant._mesh.IsLoaded() && plant._material->IsLoaded())
			LoadSprite(plant);

		if (!plant._csh.IsSet())
		{
			if (plant._tex[Plant::TEX_GBUFFER_0] && plant._tex[Plant::TEX_GBUFFER_0]->IsLoaded() &&
				plant._tex[Plant::TEX_GBUFFER_1] && plant._tex[Plant::TEX_GBUFFER_1]->IsLoaded() &&
				plant._tex[Plant::TEX_GBUFFER_2] && plant._tex[Plant::TEX_GBUFFER_2]->IsLoaded())
			{
				plant._csh = s_shader->BindDescriptorSetTextures(1,
					{
						plant._tex[Plant::TEX_GBUFFER_0],
						plant._tex[Plant::TEX_GBUFFER_1],
						plant._tex[Plant::TEX_GBUFFER_2]
					});
			}
		}
	}

	if (!_geo)
	{
		bool allLoaded = !_vPlants.empty();
		for (auto& plant : _vPlants)
		{
			if (!plant._mesh.IsLoaded())
			{
				allLoaded = false;
				break;
			}
		}
		if (allLoaded)
		{
			int vertCount = 0;
			for (auto& plant : _vPlants)
			{
				for (auto& bc : plant._vBakedChunks)
				{
					bc._vbOffset = vertCount;
					vertCount += Utils::Cast32(bc._vSprites.size());
				}
			}
			Vector<Vertex> vVB;
			vVB.resize(vertCount);
			vertCount = 0;
			for (auto& plant : _vPlants)
			{
				for (auto& bc : plant._vBakedChunks)
				{
					Math::Bounds bounds;
					for (auto& s : bc._vSprites)
						bounds.Include(s._pos);
					bounds.FattenBy(plant.GetSize());
					_octree.BindClient(Math::Octree::Client(bounds, &bc));

					if (!bc._vSprites.empty())
						memcpy(&vVB[vertCount], bc._vSprites.data(), bc._vSprites.size() * sizeof(Vertex));
					vertCount += Utils::Cast32(bc._vSprites.size());
				}
			}

			CGI::GeometryDesc geoDesc;
			const CGI::VertexInputAttrDesc viaDesc[] =
			{
				{0, offsetof(Vertex, _pos), CGI::ViaType::floats, 3, CGI::ViaUsage::position, 0},
				{0, offsetof(Vertex, _tc),  CGI::ViaType::shorts, 2, CGI::ViaUsage::texCoord, 0},
				CGI::VertexInputAttrDesc::End()
			};
			geoDesc._pVertexInputAttrDesc = viaDesc;
			const int strides[] = { sizeof(Vertex), 0 };
			geoDesc._pStrides = strides;
			_geo.Init(geoDesc);
			_geo->CreateVertexBuffer(vertCount, 0);
			_geo->UpdateVertexBuffer(vVB.data(), 0);

			VERUS_QREF_RENDERER;
			VERUS_QREF_ATMO;

			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader, "#", renderer.GetDS().GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_MAIN].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader, "#Depth", atmo.GetShadowMap().GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = "";
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_DEPTH].Init(pipeDesc);
			}
		}
	}
}

void Forest::Layout()
{
	if (!IsInitialized())
		return;

	_visibleCount = 0;

	VERUS_QREF_SM;

	for (auto& plant : _vPlants)
		for (auto& bc : plant._vBakedChunks)
			bc._visible = false;

	{
		const float zFarWas = sm.GetCamera()->GetZFar();

		const bool drawingDepth = Scene::SceneManager::IsDrawingDepth(DrawDepth::automatic);
		if (drawingDepth)
		{
			const int csmSplit = Atmosphere::I().GetShadowMap().GetCurrentSplit();
			if (!csmSplit)
				return;
		}
		else
			sm.GetCamera()->SetFrustumFar(_maxDist);

		Math::RQuadtreeIntegral qt = _pTerrain->GetQuadtree();
		qt.SetDelegate(&_scatter);
		qt.TraverseVisible();
		qt.SetDelegate(_pTerrain);

		if (drawingDepth)
			_octree.TraverseVisible(sm.GetCamera()->GetFrustum());
		sm.GetCamera()->SetFrustumFar(_maxDist * 8);
		if (!drawingDepth)
			_octree.TraverseVisible(sm.GetCamera()->GetFrustum());

		sm.GetCamera()->SetFrustumFar(zFarWas);
	}

	const float tessDistSq = _tessDist * _tessDist;
	std::sort(_vDrawPlants.begin(), _vDrawPlants.begin() + _visibleCount, [tessDistSq](RDrawPlant plantA, RDrawPlant plantB)
		{
			const bool tessA = plantA._distToEyeSq < tessDistSq;
			const bool tessB = plantB._distToEyeSq < tessDistSq;
			if (tessA != tessB)
				return tessA;
			if (plantA._plantIndex != plantB._plantIndex)
				return plantA._plantIndex < plantB._plantIndex;
			return plantA._distToEyeSq < plantB._distToEyeSq;
		});
}

void Forest::Draw(bool allowTess)
{
	if (!IsInitialized())
		return;
	VERUS_UPDATE_ONCE_CHECK_DRAW;

	DrawModels(allowTess);
	DrawSprites();
}

void Forest::DrawModels(bool allowTess)
{
	if (!_visibleCount)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_CONST_SETTINGS;

	PMesh pMesh = nullptr;
	MaterialPtr material;
	int bindPipelineStage = -1;
	bool tess = true;
	const float tessDistSq = _tessDist * _tessDist;

	auto cb = renderer.GetCommandBuffer();
	auto shader = Scene::Mesh::GetShader();

	auto DrawMesh = [cb](PMesh pMesh)
	{
		if (pMesh && !pMesh->IsInstanceBufferEmpty(true))
		{
			pMesh->UpdateInstanceBuffer();
			cb->DrawIndexed(pMesh->GetIndexCount(), pMesh->GetInstanceCount(true), 0, 0, pMesh->GetFirstInstance());
		}
	};

	shader->BeginBindDescriptors();
	for (int i = 0; i <= _visibleCount; ++i)
	{
		if (i == _visibleCount) // The end?
		{
			DrawMesh(pMesh);
			break;
		}

		RcDrawPlant drawPlant = _vDrawPlants[i];
		RPlant plant = _vPlants[drawPlant._plantIndex];
		PMesh pNextMesh = &plant._mesh;
		MaterialPtr nextMaterial = plant._material;
		const bool nextTess = drawPlant._distToEyeSq < tessDistSq;

		if (!pNextMesh->IsLoaded() || !nextMaterial->IsLoaded())
			continue;

		if (pNextMesh != pMesh || nextTess != tess)
		{
			DrawMesh(pMesh);

			pMesh = pNextMesh;
			pMesh->MarkFirstInstance();
			if (bindPipelineStage)
			{
				if (-1 == bindPipelineStage)
				{
					bindPipelineStage = (nextTess && allowTess && settings._gpuTessellation) ? 1 : 0;
					pMesh->BindPipelineInstanced(cb, 1 == bindPipelineStage, true);
					pMesh->UpdateUniformBufferPerFrame(1 / (_tessDist - 10));
					cb->BindDescriptors(shader, 0);
				}
				else if (1 == bindPipelineStage && !nextTess)
				{
					bindPipelineStage = 0;
					pMesh->BindPipelineInstanced(cb, false, true);
					pMesh->UpdateUniformBufferPerFrame();
					cb->BindDescriptors(shader, 0);
				}
			}
			tess = nextTess;
			pMesh->BindGeo(cb);
			pMesh->UpdateUniformBufferPerMeshVS();
			cb->BindDescriptors(shader, 2);
		}
		if (nextMaterial != material)
		{
			material = nextMaterial;
			material->UpdateMeshUniformBuffer();
			cb->BindDescriptors(shader, 1, material->GetComplexSetHandle());
		}

		if (pMesh)
		{
			const Transform3 matW = VMath::appendScale(Transform3(drawPlant._basis * Matrix3::rotationY(drawPlant._angle),
				Vector3(drawPlant._pos + drawPlant._pushBack)), Vector3::Replicate(drawPlant._scale));
			pMesh->PushInstance(matW, Vector4(Vector3(drawPlant._pos), 1));
		}
	}
	shader->EndBindDescriptors();
}

void Forest::DrawSprites()
{
	if (!_geo)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_ATMO;

	const bool drawingDepth = Scene::SceneManager::IsDrawingDepth(DrawDepth::automatic);

	auto cb = renderer.GetCommandBuffer();

	s_ubForestVS._matP = sm.GetCamera()->GetMatrixP().UniformBufferFormat();
	s_ubForestVS._matWVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubForestVS._viewportSize = renderer.GetCommandBuffer()->GetViewportSize().GLM();
	s_ubForestVS._eyePos = float4(sm.GetCamera()->GetEyePosition().GLM(), 0);
	s_ubForestVS._eyePosScreen = float4(atmo.GetEyePosition().GLM(), 0);

	cb->BindPipeline(_pipe[drawingDepth ? PIPE_DEPTH : PIPE_MAIN]);
	cb->BindVertexBuffers(_geo);
	s_shader->BeginBindDescriptors();
	cb->BindDescriptors(s_shader, 0);
	for (auto& plant : _vPlants)
	{
		if (!plant._csh.IsSet())
			continue;
		cb->BindDescriptors(s_shader, 1, plant._csh);
		for (auto& bc : plant._vBakedChunks)
		{
			if (bc._visible)
				cb->Draw(bc._vSprites.size(), 1, bc._vbOffset);
		}
	}
	s_shader->EndBindDescriptors();
}

void Forest::DrawAO()
{
	if (!_visibleCount)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_HELPERS;

	CGI::LightType type = CGI::LightType::none;
	PMesh pMesh = nullptr;

	auto& ds = renderer.GetDS();
	auto cb = renderer.GetCommandBuffer();

	auto DrawMesh = [cb](PMesh pMesh)
	{
		if (pMesh && !pMesh->IsInstanceBufferEmpty(true))
		{
			pMesh->UpdateInstanceBuffer();
			cb->DrawIndexed(pMesh->GetIndexCount(), pMesh->GetInstanceCount(true), 0, 0, pMesh->GetFirstInstance());
		}
	};

	for (int i = 0; i <= _visibleCount; ++i)
	{
		if (i == _visibleCount) // The end?
		{
			DrawMesh(pMesh);
			break;
		}

		RcDrawPlant drawPlant = _vDrawPlants[i];
		RPlant plant = _vPlants[drawPlant._plantIndex];

		const CGI::LightType nextType = CGI::LightType::omni;

		if (nextType != type)
		{
			DrawMesh(pMesh);

			type = nextType;
			ds.OnNewAOType(cb, type);

			pMesh = (type != CGI::LightType::none) ? &helpers.GetDeferredLights().Get(type) : nullptr;

			if (pMesh)
			{
				pMesh->MarkFirstInstance();
				pMesh->BindGeo(cb, (1 << 0) | (1 << 4));
				pMesh->CopyPosDeqScale(&ds.GetUbAOPerMeshVS()._posDeqScale.x);
				pMesh->CopyPosDeqBias(&ds.GetUbAOPerMeshVS()._posDeqBias.x);
				ds.BindDescriptorsAOPerMeshVS(cb);
			}
		}

		if (pMesh)
		{
			const float size = drawPlant._scale * plant._aoSize;
			const Transform3 matW = VMath::appendScale(Transform3(Matrix3::identity(),
				Vector3(drawPlant._pos + drawPlant._pushBack + Vector3(0, size * 0.25f, 0))), Vector3::Replicate(size));
			pMesh->PushInstance(matW, Vector4::Replicate(1));
		}
	}
}

void Forest::SetLayer(int layer, RcLayerDesc desc)
{
	if (_vLayerPlants.size() < layer + 1)
		_vLayerPlants.resize(layer + 1);

	Random random(2247 + layer);

	VERUS_FOR(type, SCATTER_TYPE_COUNT)
	{
		RcPlantDesc plantDesc = desc._forType[type];
		if (plantDesc._url)
		{
			const int plantIndex = FindPlant(plantDesc._url);
			if (plantIndex >= 0)
			{
				_vLayerPlants[layer]._plants[type] = plantIndex;
			}
			else // Add new plant?
			{
				_vLayerPlants[layer]._plants[type] = Utils::Cast32(_vPlants.size());
				_vPlants.resize(_vPlants.size() + 1);
				RPlant plant = _vPlants[_vPlants.size() - 1];
				plant._url = plantDesc._url;
				plant._alignToNormal = plantDesc._alignToNormal;
				plant._maxScale = plantDesc._maxScale;
				plant._allowedNormal = plantDesc._allowedNormal;
				const float ds = plantDesc._maxScale - plantDesc._minScale;
				const int count = 64;
				plant._vScales.resize(count);
				VERUS_FOR(i, count)
					plant._vScales[i] = plantDesc._minScale + ds * (i * i * i) / (count * count * count);
				std::shuffle(plant._vScales.begin(), plant._vScales.end(), random.GetGenerator());
			}
		}
	}
}

int Forest::FindPlant(CSZ url) const
{
	VERUS_FOR(i, _vPlants.size())
	{
		if (_vPlants[i]._url == url)
			return i;
	}
	return -1;
}

void Forest::BakeChunks(RPlant plant)
{
	const int chunkSide = 128;
	const int side = _pTerrain->GetMapSide() / chunkSide;
	const int half = _pTerrain->GetMapSide() / 2;

	auto BakeChunk = [this, side, half, chunkSide](int ic, int jc, RPlant plant)
	{
		RBakedChunk bc = plant._vBakedChunks[ic * side + jc];
		const int iOffset = ic * chunkSide;
		const int jOffset = jc * chunkSide;
		VERUS_FOR(i, chunkSide)
		{
			VERUS_FOR(j, chunkSide)
			{
				const int ij[] = { iOffset + i, jOffset + j };

				const int layer = _pTerrain->GetMainLayerAt(ij);
				if (layer >= _vLayerPlants.size())
					continue;

				if (_pTerrain->GetNormalAt(ij)[1] < plant._allowedNormal)
					continue;

				VERUS_FOR(type, SCATTER_TYPE_COUNT)
				{
					const int index = _vLayerPlants[layer]._plants[type];
					if (index >= 0 && &plant == &_vPlants[index])
					{
						Scatter::RcInstance instance = _scatter.GetInstanceAt(ij);
						if (type == instance._type)
						{
							const float h = _pTerrain->GetHeightAt(ij);

							int ij4[2] = { ij[0], ij[1] };
							float h4[4];
							h4[0] = h;
							ij4[0]++; h4[1] = _pTerrain->GetHeightAt(ij4);
							ij4[1]++; h4[2] = _pTerrain->GetHeightAt(ij4);
							ij4[0]--; h4[3] = _pTerrain->GetHeightAt(ij4);
							const float hMin = *std::min_element(h4 + 0, h4 + 4);

							const int xOffset = ij[1] & ~0xF;
							const int zOffset = ij[0] & ~0xF;

							const float psize = plant.GetSize() * plant._vScales[instance._rand % plant._vScales.size()] * _margin;
							Vertex v;
							v._pos = glm::vec3(
								xOffset - half + instance._x,
								hMin - psize * 0.05f,
								zOffset - half + instance._z);
							v._tc[0] = Math::Clamp<int>(static_cast<int>(psize * 500), 0, SHRT_MAX);
							v._tc[1] = Math::Clamp<int>(static_cast<int>(instance._angle * SHRT_MAX / VERUS_2PI), 0, SHRT_MAX);
							v._pos.y += psize * 0.5f / _margin;
							if (!bc._vSprites.capacity())
								bc._vSprites.reserve(256);
							bc._vSprites.push_back(v);
						}
					}
				}
			}
		}
	};

	plant._vBakedChunks.resize(side * side);
	VERUS_FOR(i, side)
	{
		VERUS_FOR(j, side)
		{
			BakeChunk(i, j, plant);
		}
	}
}

bool Forest::LoadSprite(RPlant plant)
{
	int count = 0;
	const CSZ ext[] = { "", ".FX", ".FX" };
	VERUS_FOR(i, 3)
	{
		String pathname = _C(plant._mesh.GetUrl());
		char filename[20];
		sprintf_s(filename, "GB%d%s.dds", i, ext[i]);
		Str::ReplaceFilename(pathname, filename);
		pathname = Str::ToPakFriendlyUrl(_C(pathname));
		pathname = "[Textures]:Forest/" + pathname;

		if (IO::FileSystem::FileExist(_C(pathname)))
		{
			count++;
			plant._tex[i].Init(_C(pathname));
		}
	}
	return 3 == count;
}

void Forest::BakeSprite(RPlant plant, CSZ url)
{
	const bool drawingDepth = Scene::SceneManager::IsDrawingDepth(DrawDepth::automatic);
	if (drawingDepth)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;

	CGI::CommandBufferPtr cb;
	cb.InitOneTimeSubmit();

	const int frameSide = 512;
	const int framePad = 16;
	const int frameCountH = 16;
	const int frameCountV = 16;
	const int texWidth = frameCountH * frameSide;
	const int texHeight = frameCountV * frameSide;
	const float stepH = VERUS_2PI / frameCountH;
	const float stepV = VERUS_PI / 2 / frameCountV;

	CGI::RPHandle rph = renderer->CreateRenderPass(
		{
			CGI::RP::Attachment("GBuffer0", CGI::Format::srgbR8G8B8A8).LoadOpClear().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("GBuffer1", CGI::Format::unormR10G10B10A2).LoadOpClear().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("GBuffer2", CGI::Format::unormR8G8B8A8).LoadOpClear().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("Depth", CGI::Format::unormD24uintS8).LoadOpClear().Layout(CGI::ImageLayout::depthStencilAttachment, CGI::ImageLayout::depthStencilReadOnly),
		},
		{
			CGI::RP::Subpass("Sp0").Color(
				{
					CGI::RP::Ref("GBuffer0", CGI::ImageLayout::colorAttachment),
					CGI::RP::Ref("GBuffer1", CGI::ImageLayout::colorAttachment),
					CGI::RP::Ref("GBuffer2", CGI::ImageLayout::colorAttachment)
				}).DepthStencil(CGI::RP::Ref("Depth", CGI::ImageLayout::depthStencilAttachment))
		},
		{});

	CGI::TexturePwn texGB[3], texDepth;
	CGI::TextureDesc texDesc;
	texDesc._width = texWidth;
	texDesc._height = texHeight;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;

	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer0();
	texDesc._format = CGI::Format::srgbR8G8B8A8;
	texGB[0].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer1();
	texDesc._format = CGI::Format::unormR10G10B10A2;
	texGB[1].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer2();
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texGB[2].Init(texDesc);
	texDesc.Reset();
	texDesc._clearValue = Vector4(1);
	texDesc._format = CGI::Format::unormD24uintS8;
	texDesc._width = texWidth;
	texDesc._height = texHeight;
	texDepth.Init(texDesc);

	CGI::FBHandle fbh = renderer->CreateFramebuffer(rph,
		{
			texGB[0],
			texGB[1],
			texGB[2],
			texDepth
		},
		texWidth,
		texHeight);

	cb->BeginRenderPass(rph, fbh,
		{
			texGB[0]->GetClearValue(),
			texGB[1]->GetClearValue(),
			texGB[2]->GetClearValue(),
			texDepth->GetClearValue()
		});

	plant._mesh.BindPipeline(cb, false);
	plant._mesh.BindGeo(cb);
	Mesh::GetShader()->BeginBindDescriptors();
	plant._material->UpdateMeshUniformBuffer();
	cb->BindDescriptors(Scene::Mesh::GetShader(), 1, plant._material->GetComplexSetHandle());
	plant._mesh.UpdateUniformBufferPerMeshVS();
	cb->BindDescriptors(Scene::Mesh::GetShader(), 2);
	plant._mesh.UpdateUniformBufferSkeletonVS();
	cb->BindDescriptors(Scene::Mesh::GetShader(), 3);
	plant._mesh.UpdateUniformBufferPerObject(Transform3::identity());
	cb->BindDescriptors(Scene::Mesh::GetShader(), 4);
	VERUS_FOR(i, frameCountV)
	{
		const Matrix3 matV = Matrix3::rotationX(-stepV * i);
		VERUS_FOR(j, frameCountH)
		{
			const Vector4 vp(
				static_cast<float>(j * frameSide + framePad),
				static_cast<float>(i * frameSide + framePad),
				static_cast<float>(frameSide - framePad * 2),
				static_cast<float>(frameSide - framePad * 2));
			cb->SetViewport({ vp });

			const float size = plant.GetSize();
			const Matrix3 matH = Matrix3::rotationY(stepH * j);
			const Matrix3 matR = matH * matV;
			const Vector3 offset = matR * Vector3(0, 0, size);

			Camera cam;
			cam.MoveAtTo(Vector3(0, size * 0.5f / _margin, 0));
			cam.MoveEyeTo(Vector3(0, size * 0.5f / _margin, 0) + offset);
			cam.SetFovY(0);
			cam.SetWidth(size * _margin);
			cam.SetHeight(size * _margin);
			cam.SetAspectRatio(1);
			cam.SetZNear(0);
			cam.SetZFar(size * 2);
			cam.Update();
			PCamera pPrevCamera = sm.SetCamera(&cam);

			plant._mesh.UpdateUniformBufferPerFrame();
			cb->BindDescriptors(Scene::Mesh::GetShader(), 0);
			cb->DrawIndexed(plant._mesh.GetIndexCount());

			sm.SetCamera(pPrevCamera);
		}
	}
	Mesh::GetShader()->EndBindDescriptors();

	cb->EndRenderPass();

	CGI::TexturePwn texFinalGB[3];
	texDesc.Reset();
	texDesc._width = texWidth;
	texDesc._height = texHeight;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::generateMips;
	texDesc._readbackMip = 2;

	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer0();
	texDesc._format = CGI::Format::srgbR8G8B8A8;
	texFinalGB[0].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer1();
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texFinalGB[1].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer2();
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texFinalGB[2].Init(texDesc);

	renderer.GetDS().BakeSprites(texGB, texFinalGB, cb.Get());

	VERUS_FOR(i, 3)
	{
		texFinalGB[i]->GenerateMips(cb.Get());
		texFinalGB[i]->ReadbackSubresource(nullptr, true, cb.Get());
	}

	cb->DoneOneTimeSubmit();

	const int mipWidth = texWidth / 4;
	const int mipHeight = texHeight / 4;
	Vector<UINT32> vData;
	vData.resize(mipWidth * mipHeight);
	const CSZ ext[] = { "", ".FX", ".FX" };
	VERUS_FOR(i, 3)
	{
		texFinalGB[i]->ReadbackSubresource(vData.data(), false);

		String pathname = _C(plant._mesh.GetUrl());
		char filename[20];
		sprintf_s(filename, "GB%d%s.psd", i, ext[i]);
		Str::ReplaceFilename(pathname, filename);
		pathname = Str::ToPakFriendlyUrl(_C(pathname));
		pathname = String(url) + '/' + pathname;
		IO::FileSystem::SaveImage(_C(pathname), vData.data(), mipWidth, mipHeight);
	}

	renderer.GetDS().BakeSpritesCleanup();
	renderer->DeleteFramebuffer(fbh);
	renderer->DeleteRenderPass(rph);
	renderer->DeleteCommandBuffer(cb.Detach());
}

bool Forest::BakeSprites(CSZ url)
{
	for (auto& plant : _vPlants)
	{
		if (plant._mesh.IsLoaded() && plant._material->IsLoaded())
			BakeSprite(plant, url);
		else
			return false;
	}
	return true;
}

void Forest::Scatter_AddInstance(const int ij[2], int type, float x, float z, float scale, float angle, UINT32 r)
{
	if (_visibleCount == _vDrawPlants.size())
		return;

	VERUS_QREF_ATMO;

	const int layer = _pTerrain->GetMainLayerAt(ij);
	if (layer >= _vLayerPlants.size())
		return;
	const int plantIndex = _vLayerPlants[layer]._plants[type];
	if (plantIndex < 0)
		return;

	const float h = _pTerrain->GetHeightAt(ij);
	Point3 pos(x, h, z);
	RcPoint3 eyePos = atmo.GetEyePosition();
	const float distSq = VMath::distSqr(eyePos, pos);
	const float maxDistSq = _maxDist * _maxDist;
	if (distSq >= maxDistSq)
		return;

	RPlant plant = _vPlants[plantIndex];

	if (_pTerrain->GetNormalAt(ij)[1] < plant._allowedNormal)
		return;

	int ij4[2] = { ij[0], ij[1] };
	float h4[4];
	h4[0] = h;
	ij4[0]++; h4[1] = _pTerrain->GetHeightAt(ij4);
	ij4[1]++; h4[2] = _pTerrain->GetHeightAt(ij4);
	ij4[0]--; h4[3] = _pTerrain->GetHeightAt(ij4);
	pos.setY(*std::min_element(h4 + 0, h4 + 4));

	const float distFractionSq = distSq / maxDistSq;
	const float alignToNormal = (1 - distFractionSq) * plant._alignToNormal;
	const float t = Math::Clamp<float>((alignToNormal - 0.1f) / 0.8f, 0, 1);

	Vector3 pushBack(0);
	Matrix3 matScale = Matrix3::identity();
	if (Scene::SceneManager::IsDrawingDepth(DrawDepth::automatic))
	{
		const float strength = Math::Clamp<float>((1 - distFractionSq) * 1.25f, 0, 1);
		matScale = Matrix3::scale(Vector3(strength, 0.5f + 0.5f * strength, strength));
	}
	else
	{
		const float strength = Math::Clamp<float>((distFractionSq - 0.5f) * 2, 0, 1);
		const Point3 center = pos + Vector3(0, plant._maxSize * 0.5f, 0);
		pushBack = VMath::normalizeApprox(center - eyePos) * plant._maxSize * strength;
	}

	DrawPlant drawPlant;
	drawPlant._basis = Matrix3::Lerp(Matrix3::identity(), _pTerrain->GetBasisAt(ij), t) * matScale;
	drawPlant._pos = pos;
	drawPlant._pushBack = pushBack;
	drawPlant._scale = plant._vScales[r % plant._vScales.size()];
	drawPlant._angle = angle;
	drawPlant._distToEyeSq = distSq;
	drawPlant._plantIndex = plantIndex;
	_vDrawPlants[_visibleCount++] = drawPlant;
}

Continue Forest::Octree_ProcessNode(void* pToken, void* pUser)
{
	PBakedChunk pBakedChunk = static_cast<PBakedChunk>(pToken);
	pBakedChunk->_visible = true;
	return Continue::yes;
}
