// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

CGI::ShaderPwns<Forest::SHADER_COUNT> Forest::s_shader;
Forest::UB_ForestVS                   Forest::s_ubForestVS;
Forest::UB_ForestFS                   Forest::s_ubForestFS;
Forest::UB_SimpleForestVS             Forest::s_ubSimpleForestVS;
Forest::UB_SimpleForestFS             Forest::s_ubSimpleForestFS;

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
	VERUS_QREF_CONST_SETTINGS;

	s_shader[SHADER_MAIN].Init("[Shaders]:DS_Forest.hlsl");
	s_shader[SHADER_MAIN]->CreateDescriptorSet(0, &s_ubForestVS, sizeof(s_ubForestVS), settings.GetLimits()._forest_ubVSCapacity, {}, CGI::ShaderStageFlags::vs | CGI::ShaderStageFlags::gs);
	s_shader[SHADER_MAIN]->CreateDescriptorSet(1, &s_ubForestFS, sizeof(s_ubForestFS), settings.GetLimits()._forest_ubFSCapacity,
		{
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipL
		}, CGI::ShaderStageFlags::fs);
	s_shader[SHADER_MAIN]->CreatePipelineLayout();

	s_shader[SHADER_SIMPLE].Init("[Shaders]:SimpleForest.hlsl");
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(0, &s_ubSimpleForestVS, sizeof(s_ubSimpleForestVS), settings.GetLimits()._forest_ubVSCapacity, {}, CGI::ShaderStageFlags::vs);
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(1, &s_ubSimpleForestFS, sizeof(s_ubSimpleForestFS), settings.GetLimits()._forest_ubFSCapacity,
		{
			CGI::Sampler::linearMipN,
			CGI::Sampler::linearMipN,
			CGI::Sampler::linearMipN,
			CGI::Sampler::shadow
		}, CGI::ShaderStageFlags::fs);
	s_shader[SHADER_SIMPLE]->CreatePipelineLayout();
}

void Forest::DoneStatic()
{
	s_shader.Done();
}

void Forest::Init(PTerrain pTerrain, CSZ url)
{
	VERUS_INIT();

	_pTerrain = pTerrain;

	OnTerrainModified();

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
	_scatter.Init(s_scatterSide, SCATTER_TYPE_COUNT, id, 19201);
	_scatter.SetDelegate(this);
	_scatter.SetMaxDist(_maxDistForModels * 1.25f);

	_vPlants.reserve(16);
	_vLayerData.reserve(16);
	_vDrawPlants.resize(_capacity);

	_vCollisionPlants.Reserve(400);
	_vCollisionPool.Resize(400);

	if (url)
	{
		Vector<BYTE> vData;
		IO::FileSystem::LoadResource(url, vData, IO::FileSystem::LoadDesc(true));

		pugi::xml_document doc;
		const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size());
		if (!result)
			throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
		pugi::xml_node root = doc.first_child();

		for (auto layerNode : root.children("layer"))
		{
			const int id = layerNode.attribute("id").as_int();
			LayerDesc layerDesc;
			for (auto typeNode : layerNode.children("type"))
			{
				const int typeID = typeNode.attribute("id").as_int();
				SCATTER_TYPE scatterType = SCATTER_TYPE_COUNT;
				switch (typeID)
				{
				case 100: scatterType = SCATTER_TYPE_100; break;
				case 50:  scatterType = SCATTER_TYPE_50; break;
				case 25:  scatterType = SCATTER_TYPE_25; break;
				case 20:  scatterType = SCATTER_TYPE_20; break;
				case 15:  scatterType = SCATTER_TYPE_15; break;
				case 10:  scatterType = SCATTER_TYPE_10; break;
				case 5:   scatterType = SCATTER_TYPE_5; break;
				case 3:   scatterType = SCATTER_TYPE_3; break;
				}
				if (SCATTER_TYPE_COUNT == scatterType)
					throw VERUS_RECOVERABLE << "Invalid scatterType";

				layerDesc._forType[scatterType].Set(
					typeNode.text().as_string(),
					typeNode.attribute("alignToNormal").as_float(1),
					typeNode.attribute("minScale").as_float(0.5f),
					typeNode.attribute("maxScale").as_float(1.5f),
					typeNode.attribute("windBending").as_float(1),
					typeNode.attribute("hullSplitU").as_float(-1),
					typeNode.attribute("allowedNormal").as_int(116));
			}

			SetLayer(id, layerDesc);
		}
	}
}

void Forest::Done()
{
	_vCollisionPool.ForEachReserved([](RCollisionPoolBlock block)
		{
			VERUS_QREF_BULLET;
			bullet.GetWorld()->removeRigidBody(block.GetRigidBody());
			block.GetDefaultMotionState()->~btDefaultMotionState();
			block.GetRigidBody()->~btRigidBody();
			block.GetUniformScalingShapeForBranches()->~btUniformScalingShape();
			block.GetUniformScalingShapeForTrunk()->~btUniformScalingShape();
			block.GetCompoundShape()->~btCompoundShape();
			block.Free();
		});
	for (auto& plant : _vPlants)
	{
		plant._pConvexHullShapeForBranches.Delete();
		plant._pConvexHullShapeForTrunk.Delete();
		s_shader[SHADER_SIMPLE]->FreeDescriptorSet(plant._cshSimple);
		s_shader[SHADER_MAIN]->FreeDescriptorSet(plant._csh);
	}
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

	VERUS_QREF_TIMER;
	_phaseY = glm::fract(_phaseY + dt * 0.37f);
	_phaseXZ = glm::fract(_phaseXZ + dt * 1.49f);

	if (!_async_initPlants)
	{
		_async_initPlants = true;
		for (auto& plant : _vPlants)
		{
			Mesh::Desc meshDesc;
			meshDesc._url = _C(plant._url);
			meshDesc._instanceCapacity = _capacity * 2 / 3;
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
			BakeChunksFor(plant);
			UpdateOcclusionFor(plant);
			if (plant._maxSize > _maxSizeAll)
			{
				_maxSizeAll = plant._maxSize;
				_pTerrain->FattenQuadtreeNodesBy(_maxSizeAll);
			}

			// Time to create convex hulls:
			plant._pConvexHullShapeForTrunk = new(plant._pConvexHullShapeForTrunk.GetData()) btConvexHullShape();
			plant._pConvexHullShapeForBranches = new(plant._pConvexHullShapeForBranches.GetData()) btConvexHullShape();
			plant._mesh.ForEachVertex([&plant](int index, RcPoint3 pos, RcVector3 nrm, RcPoint3 tc)
				{
					if (tc.getX() >= plant._hullSplitU)
						plant._pConvexHullShapeForBranches->addPoint(pos.Bullet(), false);
					else
						plant._pConvexHullShapeForTrunk->addPoint(pos.Bullet(), false);
					return Continue::yes;
				});
			plant._pConvexHullShapeForTrunk->recalcLocalAabb();
			plant._pConvexHullShapeForBranches->recalcLocalAabb();
			if (plant._pConvexHullShapeForTrunk->getNumPoints())
				plant._pConvexHullShapeForTrunk->optimizeConvexHull();
			if (plant._pConvexHullShapeForBranches->getNumPoints())
				plant._pConvexHullShapeForBranches->optimizeConvexHull();
		}

		if (!plant._tex[0] && plant._mesh.IsLoaded() && plant._material->IsLoaded())
			LoadSprite(plant);

		if (!plant._csh.IsSet())
		{
			if (plant._tex[Plant::TEX_GBUFFER_0] && plant._tex[Plant::TEX_GBUFFER_0]->IsLoaded() &&
				plant._tex[Plant::TEX_GBUFFER_1] && plant._tex[Plant::TEX_GBUFFER_1]->IsLoaded() &&
				plant._tex[Plant::TEX_GBUFFER_2] && plant._tex[Plant::TEX_GBUFFER_2]->IsLoaded() &&
				plant._tex[Plant::TEX_GBUFFER_3] && plant._tex[Plant::TEX_GBUFFER_3]->IsLoaded())
			{
				plant._csh = s_shader[SHADER_MAIN]->BindDescriptorSetTextures(1,
					{
						plant._tex[Plant::TEX_GBUFFER_0],
						plant._tex[Plant::TEX_GBUFFER_1],
						plant._tex[Plant::TEX_GBUFFER_2],
						plant._tex[Plant::TEX_GBUFFER_3]
					});
				VERUS_QREF_CSMB;
				plant._cshSimple = s_shader[SHADER_SIMPLE]->BindDescriptorSetTextures(1,
					{
						plant._tex[Plant::TEX_GBUFFER_0],
						plant._tex[Plant::TEX_GBUFFER_1],
						plant._tex[Plant::TEX_GBUFFER_2],
						csmb.GetTexture()
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
			_pTerrain->UpdateOcclusion(this);

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
					if (!bc._vSprites.empty())
					{
						for (auto& s : bc._vSprites)
							bc._bounds.Include(s._pos);
						bc._bounds.FattenBy(plant.GetSize());
						_octree.BindElement(Math::Octree::Element(bc._bounds, &bc));
						bc._bounds.FattenBy(10); // For shadow map.

						memcpy(&vVB[vertCount], bc._vSprites.data(), bc._vSprites.size() * sizeof(Vertex));
					}
					vertCount += Utils::Cast32(bc._vSprites.size());
				}
			}

			CGI::GeometryDesc geoDesc;
			geoDesc._name = "Forest.Geo";
			const CGI::VertexInputAttrDesc viaDesc[] =
			{
				{0, offsetof(Vertex, _pos), CGI::ViaType::floats, 3, CGI::ViaUsage::position, 0},
				{0, offsetof(Vertex, _tc),  CGI::ViaType::halfs, 2, CGI::ViaUsage::texCoord, 0},
				CGI::VertexInputAttrDesc::End()
			};
			geoDesc._pVertexInputAttrDesc = viaDesc;
			const int strides[] = { sizeof(Vertex), 0 };
			geoDesc._pStrides = strides;
			_geo.Init(geoDesc);
			if (vertCount)
			{
				_geo->CreateVertexBuffer(vertCount, 0);
				_geo->UpdateVertexBuffer(vVB.data(), 0);
			}

			VERUS_QREF_ATMO;
			VERUS_QREF_CSMB;
			VERUS_QREF_RENDERER;
			VERUS_QREF_WATER;

			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#", renderer.GetDS().GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_MAIN].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#Depth", csmb.GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = "";
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_DEPTH].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], "#", atmo.GetCubeMapBaker().GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_SIMPLE_ENV_MAP].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], "#", water.GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_SIMPLE_PLANAR_REF].Init(pipeDesc);
			}
		}
	}
}

void Forest::Layout(bool reflection)
{
	if (!IsInitialized())
		return;
	VERUS_QREF_ATMO;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_WM;

	_visibleCount = 0;
	for (auto& plant : _vPlants)
	{
		for (auto& bc : plant._vBakedChunks)
			bc._visible = false;
	}

	_octree.DetectElements(wm.GetPassCamera()->GetFrustum());
	if (!reflection)
	{
		Math::RQuadtreeIntegral quadtree = _pTerrain->GetQuadtree();
		quadtree.SetDelegate(&_scatter);
		quadtree.DetectElements();
		quadtree.SetDelegate(_pTerrain);

		SortVisible();
	}
}

void Forest::SortVisible()
{
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

	VERUS_QREF_ATMO;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	const Transform3 trBending = Transform3(atmo.GetPlantBendingMatrix(), Vector3(0));

	PMesh pMesh = nullptr;
	MaterialPtr material;
	int bindPipelineStage = -1;
	bool tess = true;
	const float tessDistSq = _tessDist * _tessDist;

	auto cb = renderer.GetCommandBuffer();
	auto shader = Mesh::GetShader();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(96, 255, 160, 255), "Forest/DrawModels");

	auto DrawMesh = [cb](PMesh pMesh)
	{
		if (pMesh && !pMesh->IsInstanceBufferEmpty(true))
		{
			pMesh->UpdateInstanceBuffer();
			cb->DrawIndexed(pMesh->GetIndexCount(), pMesh->GetInstanceCount(true), 0, 0, pMesh->GetMarkedInstance());
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
			pMesh->MarkInstance();
			if (bindPipelineStage)
			{
				if (-1 == bindPipelineStage)
				{
					bindPipelineStage = (nextTess && allowTess && settings._gpuTessellation) ? 1 : 0;
					pMesh->BindPipelineInstanced(cb, 1 == bindPipelineStage, true);
					pMesh->UpdateUniformBuffer_View(1 / (_tessDist - 10));
					cb->BindDescriptors(shader, 0);
					pMesh->UpdateUniformBuffer_Object(trBending, Vector4(_phaseY, _phaseXZ));
					cb->BindDescriptors(shader, 4);
				}
				else if (1 == bindPipelineStage && !nextTess)
				{
					bindPipelineStage = 0;
					pMesh->BindPipelineInstanced(cb, false, true);
					pMesh->UpdateUniformBuffer_View();
					cb->BindDescriptors(shader, 0);
					pMesh->UpdateUniformBuffer_Object(trBending, Vector4(_phaseY, _phaseXZ));
					cb->BindDescriptors(shader, 4);
				}
			}
			tess = nextTess;
			pMesh->BindGeo(cb);
			pMesh->UpdateUniformBuffer_MeshVS();
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
			pMesh->PushInstance(matW, Vector4(Vector3(drawPlant._pos), drawPlant._windBending));
		}
	}
	shader->EndBindDescriptors();

	VERUS_PROFILER_END_EVENT(cb);
}

void Forest::DrawSprites()
{
	if (!_geo)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	const bool drawingDepth = WorldManager::IsDrawingDepth(DrawDepth::automatic);

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(64, 255, 160, 255), "Forest/DrawSprites");

	s_ubForestVS._matP = wm.GetPassCamera()->GetMatrixP().UniformBufferFormat();
	s_ubForestVS._matWVP = wm.GetPassCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubForestVS._viewportSize = cb->GetViewportSize().GLM();
	s_ubForestVS._eyePos = float4(wm.GetPassCamera()->GetEyePosition().GLM(), 0);
	s_ubForestVS._headPos = float4(wm.GetHeadCamera()->GetEyePosition().GLM(), 0);
	s_ubForestVS._matRoll = wm.GetPassCamera()->GetMatrixV().ToSpriteRollMatrix();

	cb->BindPipeline(_pipe[drawingDepth ? PIPE_DEPTH : PIPE_MAIN]);
	cb->BindVertexBuffers(_geo);
	s_shader[SHADER_MAIN]->BeginBindDescriptors();
	cb->BindDescriptors(s_shader[SHADER_MAIN], 0);
	for (auto& plant : _vPlants)
	{
		if (!plant._csh.IsSet())
			continue;
		cb->BindDescriptors(s_shader[SHADER_MAIN], 1, plant._csh);
		for (auto& bc : plant._vBakedChunks)
		{
			if (bc._visible && !bc._vSprites.empty())
				cb->Draw(Utils::Cast32(bc._vSprites.size()), 1, bc._vbOffset);
		}
	}
	s_shader[SHADER_MAIN]->EndBindDescriptors();

	VERUS_PROFILER_END_EVENT(cb);
}

void Forest::DrawSimple(DrawSimpleMode mode, CGI::CubeMapFace cubeMapFace)
{
	if (!_geo)
		return;

	VERUS_QREF_ATMO;
	VERUS_QREF_CSMB;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;
	VERUS_QREF_WATER;

	const float clipDistanceOffset = (water.IsUnderwater() || DrawSimpleMode::envMap == mode) ? static_cast<float>(USHRT_MAX) : -4.f;
	const float pointSpriteFlipY = (water.IsUnderwater() || DrawSimpleMode::envMap == mode) ? 1.f : -1.f;

	Vector3 normalFlip(1, 1, 1);
	Vector3 tcFlip(1, 1, 0);
	if (DrawSimpleMode::envMap == mode)
	{
		switch (cubeMapFace)
		{
		case CGI::CubeMapFace::posX:
		case CGI::CubeMapFace::negX:
		{
			normalFlip = Vector3(-1, 1, -1);
			tcFlip = Vector3(-1, 1, 0);
		}
		break;
		case CGI::CubeMapFace::posY:
		case CGI::CubeMapFace::negY:
		{
			normalFlip = Vector3(1, 1, -1);
			tcFlip = Vector3(1, -1, 0);
		}
		break;
		case CGI::CubeMapFace::posZ:
		case CGI::CubeMapFace::negZ:
		{
			normalFlip = Vector3(-1, 1, -1);
			tcFlip = Vector3(-1, 1, 0);
		}
		break;
		}
	}
	else
	{
		normalFlip = Vector3(1, -1, 1);
	}

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(32, 255, 160, 255), "Forest/DrawSimple");

	const float occlusion = 0.1f;

	s_ubSimpleForestVS._matP = wm.GetPassCamera()->GetMatrixP().UniformBufferFormat();
	s_ubSimpleForestVS._matWVP = wm.GetPassCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubSimpleForestVS._viewportSize = cb->GetViewportSize().GLM();
	s_ubSimpleForestVS._eyePos_clipDistanceOffset = float4(wm.GetPassCamera()->GetEyePosition().GLM(), clipDistanceOffset);
	s_ubSimpleForestVS._mainCameraEyePos_pointSpriteFlipY = float4(wm.GetHeadCamera()->GetEyePosition().GLM(), pointSpriteFlipY);
	s_ubSimpleForestFS._matInvV = wm.GetPassCamera()->GetMatrixInvV().UniformBufferFormat();
	s_ubSimpleForestFS._normalFlip = float4(normalFlip.GLM(), 0);
	s_ubSimpleForestFS._tcFlip = float4(tcFlip.GLM(), 0);
	s_ubSimpleForestFS._ambientColor = float4(atmo.GetAmbientColor().GLM(), 0) * occlusion;
	s_ubSimpleForestFS._fogColor = Vector4(atmo.GetFogColor(), atmo.GetFogDensity()).GLM();
	s_ubSimpleForestFS._dirToSun = float4(atmo.GetDirToSun().GLM(), 0);
	s_ubSimpleForestFS._sunColor = float4(atmo.GetSunColor().GLM(), 0);
	s_ubSimpleForestFS._matShadow = csmb.GetShadowMatrix(0).UniformBufferFormat();
	s_ubSimpleForestFS._matShadowCSM1 = csmb.GetShadowMatrix(1).UniformBufferFormat();
	s_ubSimpleForestFS._matShadowCSM2 = csmb.GetShadowMatrix(2).UniformBufferFormat();
	s_ubSimpleForestFS._matShadowCSM3 = csmb.GetShadowMatrix(3).UniformBufferFormat();
	s_ubSimpleForestFS._matScreenCSM = csmb.GetScreenMatrixVP().UniformBufferFormat();
	s_ubSimpleForestFS._csmSliceBounds = csmb.GetSliceBounds().GLM();
	memcpy(&s_ubSimpleForestFS._shadowConfig, &csmb.GetConfig(), sizeof(s_ubSimpleForestFS._shadowConfig));

	switch (mode)
	{
	case DrawSimpleMode::envMap:           cb->BindPipeline(_pipe[PIPE_SIMPLE_ENV_MAP]); break;
	case DrawSimpleMode::planarReflection: cb->BindPipeline(_pipe[PIPE_SIMPLE_PLANAR_REF]); break;
	}
	cb->BindVertexBuffers(_geo);
	s_shader[SHADER_SIMPLE]->BeginBindDescriptors();
	cb->BindDescriptors(s_shader[SHADER_SIMPLE], 0);
	for (auto& plant : _vPlants)
	{
		if (!plant._cshSimple.IsSet())
			continue;
		cb->BindDescriptors(s_shader[SHADER_SIMPLE], 1, plant._cshSimple);
		for (auto& bc : plant._vBakedChunks)
		{
			if (bc._visible && !bc._vSprites.empty())
				cb->Draw(Utils::Cast32(bc._vSprites.size()), 1, bc._vbOffset);
		}
	}
	s_shader[SHADER_SIMPLE]->EndBindDescriptors();

	VERUS_PROFILER_END_EVENT(cb);
}

void Forest::UpdateCollision(const Vector<Vector4>& vZones)
{
	const int mapSide = _pTerrain->GetMapSide();
	const int mapHalf = _pTerrain->GetMapSide() / 2;

	for (auto& zone : vZones)
	{
		const int centerIJ[2] =
		{
			static_cast<int>(zone.getZ() + mapHalf + 0.5f),
			static_cast<int>(zone.getX() + mapHalf + 0.5f)
		};
		const int r = static_cast<int>(zone.getW() + 0.5f);
		const int r2 = r * r;
		const int iRange[2] = { Math::Clamp(centerIJ[0] - r, 0, mapSide), Math::Clamp(centerIJ[0] + r + 1, 0, mapSide) };
		const int jRange[2] = { Math::Clamp(centerIJ[1] - r, 0, mapSide), Math::Clamp(centerIJ[1] + r + 1, 0, mapSide) };
		for (int i = iRange[0]; i < iRange[1]; ++i)
		{
			const int offsetI = centerIJ[0] - i;
			const int offsetI2 = offsetI * offsetI;
			for (int j = jRange[0]; j < jRange[1]; ++j)
			{
				const int ij[] = { i, j };
				const int offsetJ = centerIJ[1] - j;
				const int offsetJ2 = offsetJ * offsetJ;
				if (offsetI2 + offsetJ2 > r2)
					continue;

				Scatter::RcInstance instance = _scatter.GetInstanceAt(ij);
				const int type = instance._type;

				if (type >= 0)
				{
					VERUS_QREF_WM;

					const int layer = _pTerrain->GetMainLayerAt(ij);
					if (layer >= _vLayerData.size())
						continue;
					const int plantIndex = _vLayerData[layer]._plants[type];
					if (plantIndex < 0)
						continue;
					RPlant plant = _vPlants[plantIndex];
					if (!plant._mesh.IsLoaded())
						continue;
					if (_pTerrain->GetNormalAt(ij)[1] < plant._allowedNormal)
						continue;

					CollisionPlant cp;
					cp._id = (ij[0] << 15) | ij[1];
					_vCollisionPlants.PushBack(cp);
				}
			}
		}
	}

	_vCollisionPlants.HandleDifference(
		[this, mapHalf](RCollisionPlant cp)
		{
			cp._poolBlockIndex = _vCollisionPool.Reserve();
			if (cp._poolBlockIndex < 0)
				return;

			const int ij[] = { (cp._id >> 15) & SHRT_MAX, cp._id & SHRT_MAX };

			Scatter::RcInstance instance = _scatter.GetInstanceAt(ij);
			const int type = instance._type;
			const int layer = _pTerrain->GetMainLayerAt(ij);
			const int plantIndex = _vLayerData[layer]._plants[type];
			RPlant plant = _vPlants[plantIndex];

			const float h = _pTerrain->GetHeightAt(ij);
			const float hMin = GetMinHeight(ij, h);

			const int xOffset = ij[1] & ~0xF;
			const int zOffset = ij[0] & ~0xF;
			const Point3 pos(
				xOffset - mapHalf + instance._x,
				hMin,
				zOffset - mapHalf + instance._z);

			const float scale = plant._vScales[instance._rand % plant._vScales.size()];

			VERUS_QREF_BULLET;
			RCollisionPoolBlock block = _vCollisionPool.GetBlockAt(cp._poolBlockIndex);
			const float t = (plant._alignToNormal - 0.1f) / 0.8f;
			const Matrix3 matBasis = Matrix3::Lerp(Matrix3::identity(), _pTerrain->GetBasisAt(ij), t);
			const Transform3 matW = Transform3(matBasis * Matrix3::rotationY(instance._angle), Vector3(pos));
			btCompoundShape* pCompoundShape = new(block.GetCompoundShape()) btCompoundShape();
			btUniformScalingShape* pUniformScalingShapeForTrunk = new(block.GetUniformScalingShapeForTrunk())
				btUniformScalingShape(plant._pConvexHullShapeForTrunk.Get(), scale);
			btUniformScalingShape* pUniformScalingShapeForBranches = new(block.GetUniformScalingShapeForBranches())
				btUniformScalingShape(plant._pConvexHullShapeForBranches.Get(), scale);
			// DebugDraw will not show UniformScalingShape, add ConvexHullShape instead.
			btTransform tr;
			tr.setIdentity();
			if (plant._pConvexHullShapeForTrunk->getNumPoints())
				pCompoundShape->addChildShape(tr, pUniformScalingShapeForTrunk);
			if (plant._pConvexHullShapeForBranches->getNumPoints())
				pCompoundShape->addChildShape(tr, pUniformScalingShapeForBranches);
			btRigidBody* pRigidBody = bullet.AddNewRigidBody(0, matW.Bullet(), pCompoundShape, +Physics::Group::forest, +bullet.GetNonStaticMask(), nullptr,
				block.GetDefaultMotionState(),
				block.GetRigidBody());
			pRigidBody->setFriction(Physics::Bullet::GetFriction(Physics::Material::wood));
			pRigidBody->setRestitution(Physics::Bullet::GetRestitution(Physics::Material::wood));
		},
		[this](RCollisionPlant cp)
		{
			if (cp._poolBlockIndex < 0)
				return;
			VERUS_QREF_BULLET;
			RCollisionPoolBlock block = _vCollisionPool.GetBlockAt(cp._poolBlockIndex);
			bullet.GetWorld()->removeRigidBody(block.GetRigidBody());
			block.GetDefaultMotionState()->~btDefaultMotionState();
			block.GetRigidBody()->~btRigidBody();
			block.GetUniformScalingShapeForBranches()->~btUniformScalingShape();
			block.GetUniformScalingShapeForTrunk()->~btUniformScalingShape();
			block.GetCompoundShape()->~btCompoundShape();
			block.Free();
		});
}

void Forest::OnTerrainModified()
{
	VERUS_QREF_RENDERER;
	renderer->WaitIdle();

	for (auto& plant : _vPlants)
		plant._maxSize = 0;

	_octree.Done();
	_octree.SetDelegate(this);
	const float hf = _pTerrain->GetMapSide() * 0.5f;
	Math::Bounds bounds;
	bounds.Set(
		Vector3(-hf, -hf, -hf),
		Vector3(+hf, +hf, +hf));
	const Vector3 limit(hf / 6, hf / 6, hf / 6);
	_octree.Init(bounds, limit);

	_pipe.Done();
	_geo.Done();
}

float Forest::GetMinHeight(const int ij[2], float h) const
{
	int ij4[2] = { ij[0], ij[1] };
	float h4[4];
	h4[0] = h;
	ij4[0]++; h4[1] = _pTerrain->GetHeightAt(ij4);
	ij4[1]++; h4[2] = _pTerrain->GetHeightAt(ij4);
	ij4[0]--; h4[3] = _pTerrain->GetHeightAt(ij4);
	return *std::min_element(h4 + 0, h4 + 4);
}

void Forest::SetLayer(int layer, RcLayerDesc desc)
{
	if (_vLayerData.size() < layer + 1)
		_vLayerData.resize(layer + 1);

	Random random(2247 + layer);

	VERUS_FOR(type, SCATTER_TYPE_COUNT)
	{
		RcPlantDesc plantDesc = desc._forType[type];
		if (plantDesc._url)
		{
			const int plantIndex = FindPlant(plantDesc._url);
			if (plantIndex >= 0)
			{
				_vLayerData[layer]._plants[type] = plantIndex;
			}
			else // Add new plant?
			{
				_vLayerData[layer]._plants[type] = Utils::Cast32(_vPlants.size());
				_vPlants.resize(_vPlants.size() + 1);
				RPlant plant = _vPlants[_vPlants.size() - 1];
				plant._url = plantDesc._url;
				plant._alignToNormal = plantDesc._alignToNormal;
				plant._maxScale = plantDesc._maxScale;
				plant._windBending = plantDesc._windBending;
				plant._hullSplitU = plantDesc._hullSplitU;
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

void Forest::BakeChunksFor(RPlant plant)
{
	const int side = 8;
	const int mapHalf = _pTerrain->GetMapSide() / 2;
	const int chunkSide = _pTerrain->GetMapSide() / side;

	auto BakeChunk = [this, side, mapHalf, chunkSide](int ic, int jc, RPlant plant)
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
				if (layer >= _vLayerData.size())
					continue;

				if (_pTerrain->GetNormalAt(ij)[1] < plant._allowedNormal)
					continue;

				VERUS_FOR(type, SCATTER_TYPE_COUNT)
				{
					const int plantIndex = _vLayerData[layer]._plants[type];
					if (plantIndex >= 0 && &plant == &_vPlants[plantIndex]) // Coarse check.
					{
						Scatter::RcInstance instance = _scatter.GetInstanceAt(ij);
						if (type == instance._type) // Fine check.
						{
							const float h = _pTerrain->GetHeightAt(ij);
							const float hMin = GetMinHeight(ij, h);

							const int xOffset = ij[1] & ~0xF;
							const int zOffset = ij[0] & ~0xF;
							const Point3 pos(
								xOffset - mapHalf + instance._x,
								hMin,
								zOffset - mapHalf + instance._z);

							const float scale = plant._vScales[instance._rand % plant._vScales.size()];
							const float psize = plant.GetSize() * scale * _margin;

							Vertex v;
							v._pos = pos.GLM();
							v._tc[0] = Convert::FloatToHalf(psize);
							v._tc[1] = Convert::FloatToHalf(instance._angle / VERUS_2PI);
							v._pos.y += psize * 0.5f / _margin - psize * 0.05f;
							if (!bc._vSprites.capacity())
								bc._vSprites.reserve(256);
							bc._vSprites.push_back(v);
							_totalPlantCount++;
						}
					}
				}
			}
		}
	};

	plant._vBakedChunks.clear();
	plant._vBakedChunks.resize(side * side);
	VERUS_FOR(i, side)
	{
		VERUS_FOR(j, side)
		{
			BakeChunk(i, j, plant);
		}
	}
}

void Forest::UpdateOcclusionFor(RPlant plant)
{
	VERUS_FOR(layer, _vLayerData.size())
	{
		VERUS_FOR(type, SCATTER_TYPE_COUNT)
		{
			const int plantIndex = _vLayerData[layer]._plants[type];
			if (plantIndex >= 0 && &plant == &_vPlants[plantIndex])
			{
				VERUS_FOR(i, s_scatterSide)
				{
					VERUS_FOR(j, s_scatterSide)
					{
						const int ij[] = { i, j };
						Scatter::RcInstance instance = _scatter.GetInstanceAt(ij);
						if (type == instance._type)
						{
							const float scale = plant._vScales[instance._rand % plant._vScales.size()];
							const float size = plant.GetSize() * scale;
							AddOcclusionAt(ij, layer, 3 + static_cast<int>(size + 0.5f));
						}
					}
				}
			}
		}
	}
}

void Forest::AddOcclusionAt(const int ij[2], int layer, int radius)
{
	radius = Math::Clamp(radius, 1, 15);
	const int mask = s_scatterSide - 1;
	const int radiusSq = radius * radius;
	for (int di = -radius; di <= radius; ++di)
	{
		const int ki = ij[0] + di;
		const int mi = (ki + s_scatterSide) & mask;
		for (int dj = -radius; dj <= radius; ++dj)
		{
			const int kj = ij[1] + dj;
			const int mj = (kj + s_scatterSide) & mask;

			const int lenSq = di * di + dj * dj;
			if (lenSq > radiusSq)
				continue;

			const float a = static_cast<float>(lenSq) / radiusSq;
			const float b = 0xFF * pow(a, 0.3f);
			const float c = Math::Clamp(b, 160.f - radius * radius, 255.f);
			const BYTE value = static_cast<BYTE>(c);
			BYTE& dst = _vLayerData[layer]._occlusion[mi * s_scatterSide + mj];
			dst = Math::CombineOcclusion(dst, value);
		}
	}
}

bool Forest::LoadSprite(RPlant plant)
{
	int count = 0;
	const CSZ ext[] = { "", ".X", ".X", ".X" };
	VERUS_FOR(i, 4)
	{
		String pathname = _C(plant._mesh.GetURL());
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
	return 4 == count;
}

void Forest::BakeSprite(RPlant plant, CSZ url)
{
	const bool drawingDepth = WorldManager::IsDrawingDepth(DrawDepth::automatic);
	if (drawingDepth)
		return;

	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	CGI::CommandBufferPtr cb;
	cb.InitOneTimeSubmit();

	const int frameSide = 512;
	const int framePad = 4;
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
			CGI::RP::Attachment("GBuffer3", CGI::Format::unormR8G8B8A8).LoadOpClear().Layout(CGI::ImageLayout::fsReadOnly),
			CGI::RP::Attachment("Depth", CGI::Format::unormD24uintS8).LoadOpClear().Layout(CGI::ImageLayout::depthStencilAttachment),
		},
		{
			CGI::RP::Subpass("Sp0").Color(
				{
					CGI::RP::Ref("GBuffer0", CGI::ImageLayout::colorAttachment),
					CGI::RP::Ref("GBuffer1", CGI::ImageLayout::colorAttachment),
					CGI::RP::Ref("GBuffer2", CGI::ImageLayout::colorAttachment),
					CGI::RP::Ref("GBuffer3", CGI::ImageLayout::colorAttachment)
				}).DepthStencil(CGI::RP::Ref("Depth", CGI::ImageLayout::depthStencilAttachment))
		},
		{});

	if (!_pipe[PIPE_MESH_BAKE])
	{
		CGI::PipelineDesc pipeDesc(plant._mesh.GetGeometry(), Mesh::GetShader(), "#", rph);
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._vertexInputBindingsFilter = plant._mesh.GetBindingsMask();
		_pipe[PIPE_MESH_BAKE].Init(pipeDesc);
	}

	CGI::TexturePwn texGB[4], texDepth;
	CGI::TextureDesc texDesc;
	texDesc._width = texWidth;
	texDesc._height = texHeight;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment;

	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer0(0.5f);
	texDesc._format = CGI::Format::srgbR8G8B8A8;
	texGB[0].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer1(0.5f, 0);
	texDesc._format = CGI::Format::unormR10G10B10A2;
	texGB[1].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer2(0.5f);
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texGB[2].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer3(0.5f);
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texGB[3].Init(texDesc);
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
			texGB[3],
			texDepth
		},
		texWidth,
		texHeight);

	cb->BeginRenderPass(rph, fbh,
		{
			texGB[0]->GetClearValue(),
			texGB[1]->GetClearValue(),
			texGB[2]->GetClearValue(),
			texGB[3]->GetClearValue(),
			texDepth->GetClearValue()
		});

	cb->BindPipeline(_pipe[PIPE_MESH_BAKE]);
	plant._mesh.BindGeo(cb);
	Mesh::GetShader()->BeginBindDescriptors();
	plant._material->UpdateMeshUniformBuffer(1, false);
	cb->BindDescriptors(Mesh::GetShader(), 1, plant._material->GetComplexSetHandle());
	plant._mesh.UpdateUniformBuffer_MeshVS();
	cb->BindDescriptors(Mesh::GetShader(), 2);
	plant._mesh.UpdateUniformBuffer_SkeletonVS();
	cb->BindDescriptors(Mesh::GetShader(), 3);
	plant._mesh.UpdateUniformBuffer_Object(Transform3::identity());
	cb->BindDescriptors(Mesh::GetShader(), 4);
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
			cam.MoveEyeTo(Vector3(0, size * 0.5f / _margin, 0) + offset);
			cam.MoveAtTo(Vector3(0, size * 0.5f / _margin, 0));
			cam.SetYFov(0);
			cam.SetAspectRatio(1);
			cam.SetZNear(0);
			cam.SetZFar(size * 2);
			cam.SetXMag(size * _margin);
			cam.SetYMag(size * _margin);
			cam.Update();
			PCamera pPrevCamera = wm.SetPassCamera(&cam);

			plant._mesh.UpdateUniformBuffer_View();
			cb->BindDescriptors(Mesh::GetShader(), 0);
			cb->DrawIndexed(plant._mesh.GetIndexCount());

			wm.SetPassCamera(pPrevCamera);
		}
	}
	Mesh::GetShader()->EndBindDescriptors();

	cb->EndRenderPass();

	CGI::TexturePwn texFinalGB[4];
	texDesc.Reset();
	texDesc._width = texWidth;
	texDesc._height = texHeight;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::colorAttachment | CGI::TextureDesc::Flags::generateMips;
	texDesc._readbackMip = 2;

	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer0(0.5f);
	texDesc._format = CGI::Format::srgbR8G8B8A8;
	texFinalGB[0].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer1(0.5f, 0);
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texFinalGB[1].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer2(0.5f);
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texFinalGB[2].Init(texDesc);
	texDesc._clearValue = CGI::DeferredShading::GetClearValueForGBuffer3(0.5f);
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texFinalGB[3].Init(texDesc);

	renderer.GetDS().BakeSprites(texGB, texFinalGB, cb.Get());

	VERUS_FOR(i, 4)
	{
		texFinalGB[i]->GenerateMips(cb.Get());
		texFinalGB[i]->ReadbackSubresource(nullptr, true, cb.Get());
	}

	cb->DoneOneTimeSubmit();

	const int mipWidth = texWidth / 4;
	const int mipHeight = texHeight / 4;
	Vector<UINT32> vData;
	vData.resize(mipWidth * mipHeight);
	const CSZ ext[] = { "", ".X", ".X", ".X" };
	VERUS_FOR(i, 4)
	{
		texFinalGB[i]->ReadbackSubresource(vData.data(), false);

		String pathname = _C(plant._mesh.GetURL());
		char filename[20];
		sprintf_s(filename, "GB%d%s.tga", i, ext[i]);
		Str::ReplaceFilename(pathname, filename);
		pathname = Str::ToPakFriendlyUrl(_C(pathname));
		pathname = String(url) + '/' + pathname;
		IO::FileSystem::SaveImage(_C(pathname), vData.data(), mipWidth, mipHeight);
	}

	renderer.GetDS().BakeSpritesCleanup();
	renderer->DeleteFramebuffer(fbh);
	renderer->DeleteRenderPass(rph);
	renderer->DeleteCommandBuffer(cb.Detach());
	_pipe[PIPE_MESH_BAKE].Done();
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

	VERUS_QREF_WM;

	const int layer = _pTerrain->GetMainLayerAt(ij);
	if (layer >= _vLayerData.size())
		return;
	const int plantIndex = _vLayerData[layer]._plants[type];
	if (plantIndex < 0)
		return;

	const float h = _pTerrain->GetHeightAt(ij);
	Point3 pos(x, h, z);
	RcPoint3 headPos = wm.GetHeadCamera()->GetEyePosition();
	const float distSq = VMath::distSqr(headPos, pos);
	const float maxDistSq = _maxDistForModels * _maxDistForModels;
	if (distSq >= maxDistSq)
		return;

	RPlant plant = _vPlants[plantIndex];

	if (_pTerrain->GetNormalAt(ij)[1] < plant._allowedNormal)
		return;

	pos.setY(GetMinHeight(ij, h));

	const float distFractionSq = distSq / maxDistSq;
	const float alignToNormal = (1 - distFractionSq) * plant._alignToNormal;
	const float t = Math::Clamp<float>((alignToNormal - 0.1f) / 0.8f, 0, 1);

	Vector3 pushBack(0);
	Matrix3 matScale = Matrix3::identity();
	if (WorldManager::IsDrawingDepth(DrawDepth::automatic))
	{
		const float strength = Math::Clamp<float>((1 - distFractionSq) * 1.25f, 0, 1);
		matScale = Matrix3::scale(Vector3::Replicate(0.5f + 0.5f * strength));
	}
	else
	{
		const float strength = Math::Clamp<float>((distFractionSq - 0.5f) * 2, 0, 1);
		const float maxOffset = Math::Min(plant._maxSize, 5.f);
		const Point3 center = pos + Vector3(0, maxOffset * 0.5f, 0);
		pushBack = VMath::normalizeApprox(center - headPos) * maxOffset * strength;
	}

	DrawPlant drawPlant;
	drawPlant._basis = Matrix3::Lerp(Matrix3::identity(), _pTerrain->GetBasisAt(ij), t) * matScale;
	drawPlant._pos = pos;
	drawPlant._pushBack = pushBack;
	drawPlant._scale = plant._vScales[r % plant._vScales.size()];
	drawPlant._angle = angle;
	drawPlant._distToEyeSq = distSq;
	drawPlant._windBending = (1 - distFractionSq) * plant._windBending;
	drawPlant._plantIndex = plantIndex;
	_vDrawPlants[_visibleCount++] = drawPlant;
}

Continue Forest::Octree_OnElementDetected(void* pToken, void* pUser)
{
	VERUS_QREF_WM;
	PBakedChunk pBakedChunk = static_cast<PBakedChunk>(pToken);
	pBakedChunk->_visible = true;
	return Continue::yes;
}

BYTE Forest::GetOcclusionAt(const int ij[2], int layer) const
{
	if (layer >= _vLayerData.size())
		return 0xFF;
	const int mask = s_scatterSide - 1;
	const int i = ij[0] & mask;
	const int j = ij[1] & mask;
	return _vLayerData[layer]._occlusion[i * s_scatterSide + j];
}
