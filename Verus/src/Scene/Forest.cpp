#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CGI::ShaderPwn							Forest::s_shader;
//CGI::CStateBlockPwns<Forest::SB_MAX>	Forest::ms_sb;
//Forest::CB_PerFrame					Forest::ms_cbPerFrame;

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
	//VERUS_OUTPUT_DEBUG_STRING(__FUNCTION__);
	//
	//CGI::CShaderDesc sd;
	//sd._url = "Shaders:DS_Forest.cg";
	//s_shader.Init(sd);
	//s_shader->BindBufferSource(&ms_cbPerFrame, sizeof(ms_cbPerFrame), 0, "PerFrame");
	//
	//CGI::CStateBlockDesc sbd;
	//sbd.B();
	//sbd.R();
	//sbd.Z().depthEnable = true;
	//sbd.Z().depthWriteEnable = true;
	//sbd.S(0).Set("a", "ww");
	//sbd.S(1).Set("a", "ww");
	//sbd.S(2).Set("a", "ww");
	//ms_sb[SB_MASTER].Init(sbd);
	//
	//sbd.Reset();
	//sbd.B().rtWriteMasks[0] = CShadowMap::GetWriteMask();
	//sbd.R().depthBias = CShadowMap::GetDepthBias() * 48;
	//sbd.R().slopeScaledDepthBias = CShadowMap::GetSlopeScaledDepthBias();
	//sbd.Z().depthEnable = true;
	//sbd.Z().depthWriteEnable = true;
	//sbd.S(0).Set("a", "ww");
	//ms_sb[SB_DEPTH].Init(sbd);
}

void Forest::DoneStatic()
{
	//VERUS_OUTPUT_DEBUG_STRING(__FUNCTION__);
	//ms_sb.Done();
	//s_shader.Done();
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
		}
	}

	if (false && !_geo)
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
					vertCount += bc._vSprites.size();
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
					_octree.BindEntity(Math::Octree::Entity(bounds, &bc));

					if (!bc._vSprites.empty())
						memcpy(&vVB[vertCount], bc._vSprites.data(), bc._vSprites.size() * sizeof(Vertex));
					vertCount += bc._vSprites.size();
				}
			}

			//CGI::CVertexElement ve[] =
			//{
			//	{0, offsetof(Vertex, _pos),	/**/CGI::VeType::_float, 3, CGI::VeUsage::position, 0},
			//	{0, offsetof(Vertex, _tc),	/**/CGI::VeType::_short, 2, CGI::VeUsage::texCoord, 0},
			//	VERUS_END_DECL
			//};
			//CGI::GeometryDesc gd;
			//gd._pVertDecl = ve;
			//gd._pShader = &(*s_shader);
			//_geo.Init(gd);
			//_geo->DefineVertexStream(0, sizeof(Vertex), vertCount * sizeof(Vertex));
			//_geo->BufferDataVB(vVB.data());
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

	const float zFarWas = sm.GetCamera()->GetZFar();
	if (Atmosphere::I().GetShadowMap().IsRendering())
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
	sm.GetCamera()->SetFrustumFar(zFarWas);

	const Point3 eyePos = sm.GetCamera()->GetEyePosition();
	std::sort(_vDrawPlants.begin(), _vDrawPlants.begin() + _visibleCount, [&eyePos](RcDrawPlant plantA, RcDrawPlant plantB)
		{
			if (plantA._plantIndex != plantB._plantIndex)
				return plantA._plantIndex < plantB._plantIndex;
			const float distA = VMath::distSqr(eyePos, plantA._pos);
			const float distB = VMath::distSqr(eyePos, plantB._pos);
			return distA < distB;
		});
}

void Forest::Draw()
{
	if (!IsInitialized())
		return;
	VERUS_UPDATE_ONCE_CHECK_DRAW;

	//for (auto& plant : _vPlants)
	//{
	//	if (!plant._tex[0] && plant._mesh.IsLoaded() && plant._material->IsLoaded())
	//	{
	//		if (!LoadSprite(plant))
	//			BakeSprite(plant);
	//	}
	//}

	DrawModels();
	DrawSprites();
}

void Forest::DrawModels()
{
	if (!_visibleCount)
		return;

	VERUS_QREF_RENDERER;

	PMesh pMesh = nullptr;
	MaterialPtr material;
	bool bindPipeline = true;

	auto cb = renderer.GetCommandBuffer();
	auto shader = Scene::Mesh::GetShader();

	// Draw all visible assets:
	shader->BeginBindDescriptors();
	for (int i = 0; i <= _visibleCount; ++i)
	{
		if (i == _visibleCount) // The end?
		{
			if (pMesh && !pMesh->IsInstanceBufferEmpty(true))
			{
				pMesh->UpdateInstanceBuffer();
				cb->DrawIndexed(pMesh->GetIndexCount(), pMesh->GetInstanceCount(true), 0, 0, pMesh->GetFirstInstance());
			}
			break;
		}

		RcDrawPlant drawPlant = _vDrawPlants[i];
		RPlant plant = _vPlants[drawPlant._plantIndex];
		PMesh pNewMesh = &plant._mesh;
		MaterialPtr newMaterial = plant._material;

		if (!pNewMesh->IsLoaded() || !newMaterial->IsLoaded())
			continue;

		if (pNewMesh != pMesh) // New pMesh?
		{
			if (pMesh && !pMesh->IsInstanceBufferEmpty(true))
			{
				pMesh->UpdateInstanceBuffer();
				cb->DrawIndexed(pMesh->GetIndexCount(), pMesh->GetInstanceCount(true), 0, 0, pMesh->GetFirstInstance());
			}
			pMesh = pNewMesh;
			pMesh->MarkFirstInstance();
			if (bindPipeline)
			{
				bindPipeline = false;
				pMesh->BindPipelineInstanced(cb, false);
			}
			pMesh->BindGeo(cb);

			pMesh->UpdateUniformBufferPerFrame();
			cb->BindDescriptors(shader, 0);
			pMesh->UpdateUniformBufferPerMeshVS();
			cb->BindDescriptors(shader, 2);
		}
		if (newMaterial != material) // Switch material?
		{
			material = newMaterial;
			material->UpdateMeshUniformBuffer();
			cb->BindDescriptors(shader, 1, material->GetComplexSetHandle());
		}
		if (pMesh) // Draw this pMesh:
		{
			const Transform3 matW = VMath::appendScale(Transform3(drawPlant._basis * Matrix3::rotationY(drawPlant._angle),
				Vector3(drawPlant._pos + drawPlant._pushBack)), Vector3::Replicate(drawPlant._scale));
			pMesh->PushInstance(matW, Vector4::Replicate(1));
		}
	}
	shader->EndBindDescriptors();
}

void Forest::DrawSprites()
{
	//if (!_geo)
	//	return;
	//
	//VERUS_QREF_RENDER;
	//VERUS_QREF_SM;
	//
	//const bool depth = Utils::IsDrawingDepth(DrawDepth::automatic);
	//
	//if (depth)
	//	ms_sb[SB_DEPTH]->Apply();
	//else
	//	ms_sb[SB_MASTER]->Apply();
	//
	//ms_cbPerFrame.matP = sm.GetCamera()->GetMatrixP().ConstBufferFormat();
	//ms_cbPerFrame.matWVP = sm.GetCamera()->GetMatrixVP().ConstBufferFormat();
	//ms_cbPerFrame.viewportSize = render.GetViewportSize();
	//ms_cbPerFrame.eyePos = sm.GetCamera()->GetPositionEye();
	//ms_cbPerFrame.posEyeScreen = Atmosphere::I().GetEyePosition();
	//
	//s_shader->Bind(depth ? "TDepth" : "T");
	//s_shader->UpdateBuffer(0);
	//
	//_geo->BeginDraw(0x1);
	//for (auto& plant : _vPlants)
	//{
	//	if (depth)
	//	{
	//		render->SetTextures({ plant._tex[Plant::TEX_GBUFFER_0] });
	//	}
	//	else
	//	{
	//		render->SetTextures(
	//			{
	//				plant._tex[Plant::TEX_GBUFFER_0],
	//				plant._tex[Plant::TEX_GBUFFER_2],
	//				plant._tex[Plant::TEX_GBUFFER_3]
	//			});
	//	}
	//	for (auto& bc : plant._vBakedChunks)
	//	{
	//		if (bc._visible)
	//			render->DrawPrimitive(CGI::PT_POINTLIST, bc._vbOffset, bc._vSprites.size());
	//	}
	//}
	//_geo->EndDraw(0x1);
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
				_vLayerPlants[layer]._plants[type] = _vPlants.size();
				_vPlants.resize(_vPlants.size() + 1);
				RPlant plant = _vPlants[_vPlants.size() - 1];
				plant._url = plantDesc._url;
				plant._normal = plantDesc._normal;
				const float ds = plantDesc._maxScale - plantDesc._minScale;
				const int count = 64;
				plant._vScales.resize(count);
				VERUS_FOR(i, count)
					plant._vScales[i] = plantDesc._minScale + ds * (i * i * i) / (count * count * count);
				std::shuffle(plant._vScales.begin(), plant._vScales.end(), random.GetGenerator());
				plant._maxScale = plantDesc._maxScale;
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

				if (_pTerrain->GetNormalAt(ij)[1] < plant._normal)
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
							v._tc[0] = Math::Clamp<int>(psize * 500, 0, SHRT_MAX);
							v._tc[1] = Math::Clamp<int>(instance._angle * SHRT_MAX / VERUS_2PI, 0, SHRT_MAX);
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
	const CSZ ext2[] = { "", ".NM", ".FX" };
	VERUS_FOR(gb, 3)
	{
		const int gbIndices[3] = { 0, 2, 3 };
		const int gbIndex = gbIndices[gb];

		String path = _C(plant._mesh.GetUrl());
		char name[20];
		sprintf_s(name, "GB%d%s.dds", gbIndex, ext2[gb]);
		Str::ReplaceFilename(path, name);
		Str::ReplaceAll(path, ":", "-");
		Str::ReplaceAll(path, "/", ".");

		const String url = "Textures:Forest/" + path;

		if (IO::FileSystem::FileExist(_C(url)))
		{
			count++;
			CGI::TextureDesc td;
			td._url = _C(url);
			plant._tex[gb].Init(td);
		}
	}
	return 3 == count;
}

void Forest::BakeSprite(RPlant plant)
{
	//#ifdef _DEBUG
	//	const bool depth = Utils::IsDrawingDepth(DrawDepth::automatic);
	//	if (depth)
	//		return;
	//
	//	VERUS_QREF_RENDER;
	//	VERUS_QREF_SM;
	//	VERUS_RT_ASSERT(render.GetDS().IsActiveGeometryPass());
	//
	//	const int frameSide = 512;
	//	const int framePad = 16;
	//	const int numFramesH = 8;
	//	const int numFramesV = 4;
	//	const int texW = numFramesH * frameSide;
	//	const int texH = numFramesV * frameSide;
	//	const float stepH = VERUS_2PI / numFramesH;
	//	const float stepV = VERUS_PI / 2 / numFramesV;
	//
	//	CGI::DeferredShading ds;
	//	ds.InitGBuffers(texW, texH, true, true);
	//
	//	ds.BeginGeometryPass(false, true);
	//	VERUS_FOR(i, numFramesV)
	//	{
	//		const Matrix3 matV = Matrix3::rotationX(-stepV * i);
	//		VERUS_FOR(j, numFramesH)
	//		{
	//			int vp[4] =
	//			{
	//				j * frameSide + framePad,
	//				i * frameSide + framePad,
	//				frameSide - framePad * 2,
	//				frameSide - framePad * 2,
	//			};
	//			render->SetViewport(vp);
	//
	//			const float size = plant.GetSize();
	//			const Matrix3 matH = Matrix3::rotationY(stepH * j);
	//			const Matrix3 matR = matH * matV;
	//			const Vector3 offset = matR * Vector3(0, 0, size);
	//
	//			Camera cam;
	//			cam.MoveAtTo(Vector3(0, size * 0.5f / m_margin, 0));
	//			cam.MoveEyeTo(Vector3(0, size * 0.5f / m_margin, 0) + offset);
	//			cam.SetFOV(0);
	//			cam.SetWidth(size * m_margin);
	//			cam.SetHeight(size * m_margin);
	//			cam.SetAspectRatio(1);
	//			cam.SetZNear(0);
	//			cam.SetZFar(size * 2);
	//			cam.Update();
	//			PCamera pPrevCamera = sm.SetCamera(&cam);
	//
	//			Mesh::CDrawDesc dd;
	//			dd._material = plant._material;
	//			dd._spriteBaking = true;
	//			plant._mesh.Draw(dd);
	//
	//			sm.SetCamera(pPrevCamera);
	//		}
	//	}
	//	ds.EndGeometryPass();
	//
	//	Vector<UINT32> vData;
	//	vData.resize(texW * texH);
	//	VERUS_FOR(gb, 3)
	//	{
	//		const int gbIndices[3] = { 0, 2, 3 };
	//		const int gbIndex = gbIndices[gb];
	//
	//		ds.GetGBuffer(gbIndex)->GetTexImage(reinterpret_cast<BYTE*>(vData.data()));
	//
	//		String path = _C(plant._mesh.GetUrl());
	//		char name[20];
	//		sprintf_s(name, "GB%d.psd", gbIndex);
	//		Str::ReplaceFilename(path, name);
	//		Str::ReplaceAll(path, ":", "-");
	//		Str::ReplaceAll(path, "/", ".");
	//		path = "D:\\Stuff\\Gulman\\Data\\Textures\\Forest/" + path;
	//		IO::FileSystem::SaveImage(_C(path), vData.data(), texW, texH);
	//	}
	//
	//	render.GetDS().BeginGeometryPass(true);
	//	VERUS_RT_ASSERT(false);
	//#endif
}

void Forest::Scatter_AddInstance(const int ij[2], int type, float x, float z, float scale, float angle, UINT32 r)
{
	if (_visibleCount == _vDrawPlants.size())
		return;

	const int layer = _pTerrain->GetMainLayerAt(ij);
	if (layer >= _vLayerPlants.size())
		return;
	const int plantIndex = _vLayerPlants[layer]._plants[type];
	if (plantIndex < 0)
		return;

	const float h = _pTerrain->GetHeightAt(ij);
	Point3 pos(x, h, z);
	RcPoint3 eyePos = Atmosphere::I().GetEyePosition();
	const float distSq = VMath::distSqr(eyePos, pos);
	if (distSq >= _maxDist * _maxDist)
		return;

	RPlant plant = _vPlants[plantIndex];

	if (_pTerrain->GetNormalAt(ij)[1] < plant._normal)
		return;

	int ij4[2] = { ij[0], ij[1] };
	float h4[4];
	h4[0] = h;
	ij4[0]++; h4[1] = _pTerrain->GetHeightAt(ij4);
	ij4[1]++; h4[2] = _pTerrain->GetHeightAt(ij4);
	ij4[0]--; h4[3] = _pTerrain->GetHeightAt(ij4);
	pos.setY(*std::min_element(h4 + 0, h4 + 4));
	const float ratio = 1 - sqrt(distSq) / _maxDist;
	const float t = Math::Clamp<float>((ratio - 0.1f) / 0.8f, 0, 1);
	Vector3 pushBack(0);
	Matrix3 matScale = Matrix3::identity();
	if (ratio < 0.25f)
	{
		const float a = ratio * 4;
		//if (Utils::IsDrawingDepth(DrawDepth::automatic))
		//{
		//	const float b = Math::Clamp<float>((a - 0.5f) * 2, 0, 1);
		//	matScale = Matrix3::scale(Vector3(b, 1, b));
		//}
		pushBack = VMath::normalizeApprox(pos - eyePos) * plant._maxSize * (1 - a) * 0.4f;
	}
	DrawPlant drawPlant;
	drawPlant._basis = Matrix3::Lerp(Matrix3::identity(), _pTerrain->GetBasisAt(ij), t) * matScale;
	drawPlant._pos = pos;
	drawPlant._pushBack = pushBack;
	drawPlant._scale = plant._vScales[r % plant._vScales.size()];
	drawPlant._angle = angle;
	drawPlant._plantIndex = plantIndex;
	_vDrawPlants[_visibleCount++] = drawPlant;
}

Continue Forest::Octree_ProcessNode(void* pToken, void* pUser)
{
	PBakedChunk pBakedChunk = static_cast<PBakedChunk>(pToken);
	pBakedChunk->_visible = true;
	return Continue::yes;
}
