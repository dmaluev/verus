// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

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
	s_shader[SHADER_MAIN].Init("[Shaders]:DS_Forest.hlsl");
	s_shader[SHADER_MAIN]->CreateDescriptorSet(0, &s_ubForestVS, sizeof(s_ubForestVS), 100, {}, CGI::ShaderStageFlags::vs);
	s_shader[SHADER_MAIN]->CreateDescriptorSet(1, &s_ubForestFS, sizeof(s_ubForestFS), 100,
		{
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipL,
			CGI::Sampler::linearMipL
		}, CGI::ShaderStageFlags::fs);
	s_shader[SHADER_MAIN]->CreatePipelineLayout();

	s_shader[SHADER_SIMPLE].Init("[Shaders]:SimpleForest.hlsl");
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(0, &s_ubSimpleForestVS, sizeof(s_ubSimpleForestVS), 100, {}, CGI::ShaderStageFlags::vs);
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(1, &s_ubSimpleForestFS, sizeof(s_ubSimpleForestFS), 100,
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

void Forest::Init(PTerrain pTerrain)
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
	_scatter.Init(32, SCATTER_TYPE_COUNT, id, 19201);
	_scatter.SetDelegate(this);
	_scatter.SetMaxDist(_maxDist * 1.25f);

	_vPlants.reserve(16);
	_vLayerPlants.reserve(16);
	_vDrawPlants.resize(_capacity);

	_vCollisionPool.Resize(1000);
}

void Forest::Done()
{
	_vCollisionPool.ForEachReserved([](RCollisionPoolBlock block)
		{
			VERUS_QREF_BULLET;
			bullet.GetWorld()->removeRigidBody(block.GetRigidBody());
			block.GetDefaultMotionState()->~btDefaultMotionState();
			block.GetRigidBody()->~btRigidBody();
			block.GetScaledBvhTriangleMeshShape()->~btScaledBvhTriangleMeshShape();
			block.Free();
		});
	for (auto& plant : _vPlants)
	{
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
			plant._aoSize = plant._mesh.GetBounds().GetAverageSize() * 1.5f;
		}

		if (!plant._tex[0] && plant._mesh.IsLoaded() && plant._material->IsLoaded())
			LoadSprite(plant);

		if (!plant._csh.IsSet())
		{
			if (plant._tex[Plant::TEX_GBUFFER_0] && plant._tex[Plant::TEX_GBUFFER_0]->IsLoaded() &&
				plant._tex[Plant::TEX_GBUFFER_1] && plant._tex[Plant::TEX_GBUFFER_1]->IsLoaded() &&
				plant._tex[Plant::TEX_GBUFFER_2] && plant._tex[Plant::TEX_GBUFFER_2]->IsLoaded())
			{
				plant._csh = s_shader[SHADER_MAIN]->BindDescriptorSetTextures(1,
					{
						plant._tex[Plant::TEX_GBUFFER_0],
						plant._tex[Plant::TEX_GBUFFER_1],
						plant._tex[Plant::TEX_GBUFFER_2]
					});
				VERUS_QREF_ATMO;
				plant._cshSimple = s_shader[SHADER_SIMPLE]->BindDescriptorSetTextures(1,
					{
						plant._tex[Plant::TEX_GBUFFER_0],
						plant._tex[Plant::TEX_GBUFFER_1],
						plant._tex[Plant::TEX_GBUFFER_2],
						atmo.GetShadowMap().GetTexture()
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
					for (auto& s : bc._vSprites)
						bc._bounds.Include(s._pos);
					bc._bounds.FattenBy(plant.GetSize());
					_octree.BindClient(Math::Octree::Client(bc._bounds, &bc));
					bc._bounds.FattenBy(10); // For shadow map.

					if (!bc._vSprites.empty())
						memcpy(&vVB[vertCount], bc._vSprites.data(), bc._vSprites.size() * sizeof(Vertex));
					vertCount += Utils::Cast32(bc._vSprites.size());
				}
			}

			CGI::GeometryDesc geoDesc;
			geoDesc._name = "Forest.Geo";
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

			VERUS_QREF_ATMO;
			VERUS_QREF_RENDERER;
			VERUS_QREF_WATER;

			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#", renderer.GetDS().GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_MAIN].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#Depth", atmo.GetShadowMap().GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = "";
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_DEPTH].Init(pipeDesc);
			}
			{
				CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], "#", water.GetRenderPassHandle());
				pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
				pipeDesc._rasterizationState._cullMode = CGI::CullMode::none;
				pipeDesc._topology = CGI::PrimitiveTopology::pointList;
				_pipe[PIPE_REFLECTION].Init(pipeDesc);
			}
		}
	}
}

void Forest::Layout(bool reflection)
{
	if (!IsInitialized())
		return;

	_visibleCount = 0;

	VERUS_QREF_SM;

	for (auto& plant : _vPlants)
		for (auto& bc : plant._vBakedChunks)
			bc._visible = false;

	{
		const float zFarWas = sm.GetMainCamera()->GetZFar();

		if (!reflection)
		{
			sm.GetMainCamera()->SetFrustumFar(_maxDist);
			Math::RQuadtreeIntegral qt = _pTerrain->GetQuadtree();
			qt.SetDelegate(&_scatter);
			qt.TraverseVisible();
			qt.SetDelegate(_pTerrain);
		}

		sm.GetMainCamera()->SetFrustumFar(1000);
		_octree.TraverseVisible(sm.GetMainCamera()->GetFrustum());

		sm.GetMainCamera()->SetFrustumFar(zFarWas);

		if (reflection)
			return;
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
					pMesh->UpdateUniformBufferPerObject(trBending, Vector4(_phaseY, _phaseXZ));
					cb->BindDescriptors(shader, 4);
				}
				else if (1 == bindPipelineStage && !nextTess)
				{
					bindPipelineStage = 0;
					pMesh->BindPipelineInstanced(cb, false, true);
					pMesh->UpdateUniformBufferPerFrame();
					cb->BindDescriptors(shader, 0);
					pMesh->UpdateUniformBufferPerObject(trBending, Vector4(_phaseY, _phaseXZ));
					cb->BindDescriptors(shader, 4);
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
			pMesh->PushInstance(matW, Vector4(Vector3(drawPlant._pos), drawPlant._windBending));
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

	const bool drawingDepth = Scene::SceneManager::IsDrawingDepth(DrawDepth::automatic);

	auto cb = renderer.GetCommandBuffer();

	s_ubForestVS._matP = sm.GetCamera()->GetMatrixP().UniformBufferFormat();
	s_ubForestVS._matWVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubForestVS._viewportSize = cb->GetViewportSize().GLM();
	s_ubForestVS._eyePos = float4(sm.GetCamera()->GetEyePosition().GLM(), 0);
	s_ubForestVS._eyePosScreen = float4(sm.GetMainCamera()->GetEyePosition().GLM(), 0);

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
			if (bc._visible)
				cb->Draw(Utils::Cast32(bc._vSprites.size()), 1, bc._vbOffset);
		}
	}
	s_shader[SHADER_MAIN]->EndBindDescriptors();
}

void Forest::DrawAO()
{
	if (!_visibleCount)
		return;

	VERUS_QREF_HELPERS;
	VERUS_QREF_RENDERER;

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

void Forest::DrawReflection()
{
	if (!_geo)
		return;

	VERUS_QREF_ATMO;
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_WATER;

	auto cb = renderer.GetCommandBuffer();

	s_ubSimpleForestVS._matP = sm.GetCamera()->GetMatrixP().UniformBufferFormat();
	s_ubSimpleForestVS._matWVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubSimpleForestVS._viewportSize = renderer.GetCommandBuffer()->GetViewportSize().GLM();
	s_ubSimpleForestVS._eyePos = float4(sm.GetCamera()->GetEyePosition().GLM(), 0);
	s_ubSimpleForestVS._eyePosScreen = float4(sm.GetMainCamera()->GetEyePosition().GLM(), 0);
	s_ubSimpleForestVS._pointSpriteScale = float4(1, water.IsUnderwater() ? 1 : -1, 0, 0);
	s_ubSimpleForestFS._matInvV = sm.GetCamera()->GetMatrixVi().UniformBufferFormat();
	s_ubSimpleForestFS._ambientColor = float4(atmo.GetAmbientColor().GLM(), 0);
	s_ubSimpleForestFS._fogColor = Vector4(atmo.GetFogColor(), atmo.GetFogDensity()).GLM();
	s_ubSimpleForestFS._dirToSun = float4(atmo.GetDirToSun().GLM(), 0);
	s_ubSimpleForestFS._sunColor = float4(atmo.GetSunColor().GLM(), 0);
	s_ubSimpleForestFS._matSunShadow = atmo.GetShadowMap().GetShadowMatrix(0).UniformBufferFormat();
	s_ubSimpleForestFS._matSunShadowCSM1 = atmo.GetShadowMap().GetShadowMatrix(1).UniformBufferFormat();
	s_ubSimpleForestFS._matSunShadowCSM2 = atmo.GetShadowMap().GetShadowMatrix(2).UniformBufferFormat();
	s_ubSimpleForestFS._matSunShadowCSM3 = atmo.GetShadowMap().GetShadowMatrix(3).UniformBufferFormat();
	memcpy(&s_ubSimpleForestFS._shadowConfig, &atmo.GetShadowMap().GetConfig(), sizeof(s_ubSimpleForestFS._shadowConfig));
	s_ubSimpleForestFS._splitRanges = atmo.GetShadowMap().GetSplitRanges().GLM();

	cb->BindPipeline(_pipe[PIPE_REFLECTION]);
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
			if (bc._visible)
				cb->Draw(Utils::Cast32(bc._vSprites.size()), 1, bc._vbOffset);
		}
	}
	s_shader[SHADER_SIMPLE]->EndBindDescriptors();
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
		const int iRange[2] = { Math::Clamp(centerIJ[0] - r, 0, mapSide), Math::Clamp(centerIJ[0] + r, 0, mapSide) };
		const int jRange[2] = { Math::Clamp(centerIJ[1] - r, 0, mapSide), Math::Clamp(centerIJ[1] + r, 0, mapSide) };
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
					VERUS_QREF_SM;

					const int layer = _pTerrain->GetMainLayerAt(ij);
					if (layer >= _vLayerPlants.size())
						continue;
					const int plantIndex = _vLayerPlants[layer]._plants[type];
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
			const int plantIndex = _vLayerPlants[layer]._plants[type];
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
			btScaledBvhTriangleMeshShape* pShape = new(block.GetScaledBvhTriangleMeshShape())
				btScaledBvhTriangleMeshShape(plant._mesh.GetShape(), btVector3(scale, scale, scale));
			btRigidBody* pRigidBody = bullet.AddNewRigidBody(0, matW.Bullet(), pShape, Physics::Group::immovable, Physics::Group::all, nullptr,
				block.GetDefaultMotionState(),
				block.GetRigidBody());
			pRigidBody->setFriction(Physics::Bullet::GetFriction(Physics::Material::stone));
			pRigidBody->setRestitution(Physics::Bullet::GetRestitution(Physics::Material::stone));
		},
		[this](RCollisionPlant cp)
		{
			VERUS_QREF_BULLET;
			RCollisionPoolBlock block = _vCollisionPool.GetBlockAt(cp._poolBlockIndex);
			bullet.GetWorld()->removeRigidBody(block.GetRigidBody());
			block.GetDefaultMotionState()->~btDefaultMotionState();
			block.GetRigidBody()->~btRigidBody();
			block.GetScaledBvhTriangleMeshShape()->~btScaledBvhTriangleMeshShape();
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
				plant._windBending = plantDesc._windBending;
				plant._allowedNormal = plantDesc._allowedNormal;
				const float ds = plantDesc._maxScale - plantDesc._minScale;
				const int count = 64;
				plant._vScales.resize(count);
				VERUS_FOR(i, count)
					plant._vScales[i] = plantDesc._minScale + ds * (i * i * i) / (count * count * count);
				std::shuffle(plant._vScales.begin(), plant._vScales.end(), random.GetGenerator());

				//plant._vShapePool.resize(2000 * sizeof(btScaledBvhTriangleMeshShape));
				//plant._vMotionStatePool.resize(2000 * sizeof(btDefaultMotionState));
				//plant._vRigidBodyPool.resize(2000 * sizeof(btRigidBody));
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
							v._tc[0] = Math::Clamp<int>(static_cast<int>(psize * 500), 0, SHRT_MAX);
							v._tc[1] = Math::Clamp<int>(static_cast<int>(instance._angle * SHRT_MAX / VERUS_2PI), 0, SHRT_MAX);
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
			CGI::RP::Attachment("Depth", CGI::Format::unormD24uintS8).LoadOpClear().Layout(CGI::ImageLayout::depthStencilAttachment),
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

	VERUS_QREF_SM;

	const int layer = _pTerrain->GetMainLayerAt(ij);
	if (layer >= _vLayerPlants.size())
		return;
	const int plantIndex = _vLayerPlants[layer]._plants[type];
	if (plantIndex < 0)
		return;

	const float h = _pTerrain->GetHeightAt(ij);
	Point3 pos(x, h, z);
	RcPoint3 eyePos = sm.GetMainCamera()->GetEyePosition();
	const float distSq = VMath::distSqr(eyePos, pos);
	const float maxDistSq = _maxDist * _maxDist;
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
	if (Scene::SceneManager::IsDrawingDepth(DrawDepth::automatic))
	{
		const float strength = Math::Clamp<float>((1 - distFractionSq) * 1.25f, 0, 1);
		matScale = Matrix3::scale(Vector3::Replicate(0.5f + 0.5f * strength));
	}
	else
	{
		const float strength = Math::Clamp<float>((distFractionSq - 0.5f) * 2, 0, 1);
		const float maxOffset = Math::Min(plant._maxSize, 5.f);
		const Point3 center = pos + Vector3(0, maxOffset * 0.5f, 0);
		pushBack = VMath::normalizeApprox(center - eyePos) * maxOffset * strength;
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

Continue Forest::Octree_ProcessNode(void* pToken, void* pUser)
{
	VERUS_QREF_SM;
	PBakedChunk pBakedChunk = static_cast<PBakedChunk>(pToken);
	pBakedChunk->_visible = sm.GetMainCamera()->GetFrustum().ContainsAabb(pBakedChunk->_bounds) != Relation::outside;
	return Continue::yes;
}