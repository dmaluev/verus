#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CGI::ShaderPwn            Terrain::s_shader;
Terrain::UB_DrawDepth     Terrain::s_ubDrawDepth;
Terrain::UB_PerMaterialFS Terrain::s_ubPerMaterialFS;

// TerrainPhysics:

TerrainPhysics::TerrainPhysics()
{
}

TerrainPhysics::~TerrainPhysics()
{
	Done();
}

void TerrainPhysics::Init(Physics::PUserPtr p, int w, int h, const void* pData, float heightScale)
{
	VERUS_QREF_BULLET;

	_pShape = new btHeightfieldTerrainShape(
		w,
		h,
		pData,
		heightScale,
		-SHRT_MAX * heightScale,
		SHRT_MAX * heightScale,
		1,
		PHY_SHORT,
		false);

	_pShape->setLocalScaling(btVector3(1, 1, 1));

	btTransform tr;
	tr.setIdentity();
	tr.setOrigin(btVector3(-0.5f, 0, -0.5f));
	_pRigidBody = bullet.AddNewRigidBody(0, tr, _pShape, +Physics::Group::terrain);
	_pRigidBody->setFriction(Physics::Bullet::GetFriction(Physics::Material::wood));
	_pRigidBody->setRestitution(Physics::Bullet::GetRestitution(Physics::Material::wood));
	_pRigidBody->setUserPointer(p);

	EnableDebugDraw(false);
}

void TerrainPhysics::Done()
{
	if (_pRigidBody)
	{
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pRigidBody);
		delete _pRigidBody->getMotionState();
		delete _pRigidBody;
		_pRigidBody = nullptr;
	}
	VERUS_SMART_DELETE(_pShape);
}

void TerrainPhysics::EnableDebugDraw(bool b)
{
	_debugDraw = b;
	if (_debugDraw)
	{
		const int f = _pRigidBody->getCollisionFlags();
		_pRigidBody->setCollisionFlags(f & ~btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT);
	}
	else
	{
		const int f = _pRigidBody->getCollisionFlags();
		_pRigidBody->setCollisionFlags(f | btCollisionObject::CF_DISABLE_VISUALIZE_OBJECT);
	}
}

// TerrainLOD:

void TerrainLOD::Init(int sidePoly, int step)
{
	_side = sidePoly + 1;
	_vertCount = _side * _side;
	if (1 == step)
		Math::BuildListGrid(sidePoly, sidePoly, _vIB);
	else
		Math::BuildStripGrid(sidePoly, sidePoly, _vIB);
	_indexCount = Utils::Cast32(_vIB.size());
	_firstIndex = 0;
}

void TerrainLOD::InitGeo(short* pV, UINT16* pI, int vertexOffset)
{
	const int edge = _side - 1;
	const int step = 16 / edge;
	VERUS_FOR(i, _side)
	{
		const int offset = i * _side;
		VERUS_FOR(j, _side)
		{
			// X and Z:
			pV[((offset + j) << 2) + 0] = j * step;
			pV[((offset + j) << 2) + 2] = i * step;

			// Y and W, edge correction:
			if (i == 0)         pV[((offset + j) << 2) + 3] = 1;
			else if (i == edge) pV[((offset + j) << 2) + 3] = -1;
			else                pV[((offset + j) << 2) + 3] = 0;
			if (j == 0)         pV[((offset + j) << 2) + 1] = 1;
			else if (j == edge) pV[((offset + j) << 2) + 1] = -1;
			else                pV[((offset + j) << 2) + 1] = 0;
		}
	}
	VERUS_FOR(i, _vIB.size())
	{
		if (_vIB[i] != 0xFFFF)
			_vIB[i] += vertexOffset;
	}
	memcpy(pI, _vIB.data(), _indexCount * sizeof(UINT16));
	VERUS_FOR(i, _vIB.size())
	{
		if (_vIB[i] != 0xFFFF)
			_vIB[i] -= vertexOffset;
	}
}

// TerrainPatch:

TerrainPatch::TerrainPatch()
{
	VERUS_ZERO_MEM(_ijCoord);
	VERUS_ZERO_MEM(_layerForChannel);
	VERUS_ZERO_MEM(_height);
	VERUS_ZERO_MEM(_mainLayer);
}

void TerrainPatch::BindTBN(PTBN p)
{
	_pTBN = p;
}

void TerrainPatch::InitFlat(short height, int mainLayer)
{
	char tan[3] = { 127, 0, 0 };
	char bin[3] = { 0, 0, 127 };
	char nrm[3] = { 0, 127, 0 };

	VERUS_FOR(i, 16 * 16)
	{
		_height[i] = height;
		_mainLayer[i] = mainLayer;
		memcpy(&_pTBN[0]._normals0[i][0], tan, 3);
		memcpy(&_pTBN[1]._normals0[i][0], bin, 3);
		memcpy(&_pTBN[2]._normals0[i][0], nrm, 3);
	}
	VERUS_FOR(i, 8 * 8)
	{
		memcpy(&_pTBN[0]._normals1[i][0], tan, 3);
		memcpy(&_pTBN[1]._normals1[i][0], bin, 3);
		memcpy(&_pTBN[2]._normals1[i][0], nrm, 3);
	}
	VERUS_FOR(i, 4 * 4)
	{
		memcpy(&_pTBN[0]._normals2[i][0], tan, 3);
		memcpy(&_pTBN[1]._normals2[i][0], bin, 3);
		memcpy(&_pTBN[2]._normals2[i][0], nrm, 3);
	}
	VERUS_FOR(i, 2 * 2)
	{
		memcpy(&_pTBN[0]._normals3[i][0], tan, 3);
		memcpy(&_pTBN[1]._normals3[i][0], bin, 3);
		memcpy(&_pTBN[2]._normals3[i][0], nrm, 3);
	}
	VERUS_FOR(i, 1 * 1)
	{
		memcpy(&_pTBN[0]._normals4[i][0], tan, 3);
		memcpy(&_pTBN[1]._normals4[i][0], bin, 3);
		memcpy(&_pTBN[2]._normals4[i][0], nrm, 3);
	}
}

void TerrainPatch::UpdateNormals(PTerrain p, int lod)
{
	if (!lod) // First LOD, full normal map calculation:
	{
		VERUS_FOR(i, 16)
		{
			VERUS_FOR(j, 16)
			{
				const int ijTL[] = { i + _ijCoord[0] - 1, j + _ijCoord[1] - 1 };
				const int  ijL[] = { i + _ijCoord[0] + 0, j + _ijCoord[1] - 1 };
				const int ijBL[] = { i + _ijCoord[0] + 1, j + _ijCoord[1] - 1 };
				const int  ijT[] = { i + _ijCoord[0] - 1, j + _ijCoord[1] + 0 };
				const int  ijB[] = { i + _ijCoord[0] + 1, j + _ijCoord[1] + 0 };
				const int ijTR[] = { i + _ijCoord[0] - 1, j + _ijCoord[1] + 1 };
				const int  ijR[] = { i + _ijCoord[0] + 0, j + _ijCoord[1] + 1 };
				const int ijBR[] = { i + _ijCoord[0] + 1, j + _ijCoord[1] + 1 };

				const float dampF = 0.125f; // 1/8 gives correct normal in 1 meter case.

				const float tl = dampF * p->GetHeightAt(ijTL);
				const float l = dampF * p->GetHeightAt(ijL);
				const float bl = dampF * p->GetHeightAt(ijBL);
				const float t = dampF * p->GetHeightAt(ijT);
				const float b = dampF * p->GetHeightAt(ijB);
				const float tr = dampF * p->GetHeightAt(ijTR);
				const float r = dampF * p->GetHeightAt(ijR);
				const float br = dampF * p->GetHeightAt(ijBR);

				const float dX = -tl - 2 * l - bl + tr + 2 * r + br;
				const float dY = -tl - 2 * t - tr + bl + 2 * b + br;

				const Vector3 normal = VMath::normalize(Vector3(-dX, 1, -dY));

				Convert::SnormToSint8(normal.ToPointer(), _pTBN[+TerrainTBN::normal]._normals0[(i << 4) + j], 3);

				// TBN:
				glm::vec3 nreal = normal.GLM(), nrm(0, 1, 0), tan(1, 0, 0), bin(0, 0, 1);
				const float angle = glm::angle(nrm, nreal);
				VERUS_RT_ASSERT(angle <= VERUS_PI * 0.5f);
				if (angle >= Math::ToRadians(1))
				{
					const glm::vec3 axis = glm::normalize(glm::cross(nrm, nreal));
					const glm::quat q = glm::angleAxis(angle, axis);
					const glm::mat3 mat = glm::mat3_cast(q);
					tan = mat * tan;
					bin = mat * bin;
				}
				Convert::SnormToSint8(Vector3(tan).ToPointer(), _pTBN[+TerrainTBN::tangent]._normals0[(i << 4) + j], 3);
				Convert::SnormToSint8(Vector3(bin).ToPointer(), _pTBN[+TerrainTBN::binormal]._normals0[(i << 4) + j], 3);
			}
		}
	}
	else // Lower LOD, some 'average' value:
	{
		const int step = 1 << lod;
		const int side = 16 / step;
		const int e = step >> 1;

		VERUS_FOR(i, side)
		{
			VERUS_FOR(j, side)
			{
				const int ijA[] = { _ijCoord[0] + i * step + 0, _ijCoord[1] + j * step + 0 };
				const int ijB[] = { _ijCoord[0] + i * step + 0, _ijCoord[1] + j * step + e };
				const int ijC[] = { _ijCoord[0] + i * step + e, _ijCoord[1] + j * step + 0 };
				const int ijD[] = { _ijCoord[0] + i * step + e, _ijCoord[1] + j * step + e };

				VERUS_FOR(tbn, 3)
				{
					char* normals[] =
					{
						nullptr,
						_pTBN[tbn]._normals1[0],
						_pTBN[tbn]._normals2[0],
						_pTBN[tbn]._normals3[0],
						_pTBN[tbn]._normals4[0]
					};

					float temp[3];
					Vector3 a, b, c, d;
					Convert::Sint8ToSnorm(p->GetNormalAt(ijA, lod - 1, static_cast<TerrainTBN>(tbn)), temp, 3); a = Vector3::MakeFromPointer(temp);
					Convert::Sint8ToSnorm(p->GetNormalAt(ijB, lod - 1, static_cast<TerrainTBN>(tbn)), temp, 3); b = Vector3::MakeFromPointer(temp);
					Convert::Sint8ToSnorm(p->GetNormalAt(ijC, lod - 1, static_cast<TerrainTBN>(tbn)), temp, 3); c = Vector3::MakeFromPointer(temp);
					Convert::Sint8ToSnorm(p->GetNormalAt(ijD, lod - 1, static_cast<TerrainTBN>(tbn)), temp, 3); d = Vector3::MakeFromPointer(temp);
					const float ratio = 1.f + (4 - lod) * 4;
					const Vector3 normal = VMath::normalize(a * ratio + b + c + d);

					Convert::SnormToSint8(normal.ToPointer(), &normals[lod][((i << (4 - lod)) + j) << 2], 3);
				}
			}
		}
	}
}

int TerrainPatch::GetSplatChannelForLayer(int layer) const
{
	VERUS_FOR(i, _usedChannelCount)
	{
		if (_layerForChannel[i] == layer)
			return i;
	}
	return -1;
}

// Terrain:

Terrain::Terrain()
{
}

Terrain::~Terrain()
{
	Done();
}

void Terrain::InitStatic()
{
	VERUS_QREF_CONST_SETTINGS;

	s_shader.Init("[Shaders]:DS_Terrain.hlsl");
	s_shader->CreateDescriptorSet(0, &s_ubDrawDepth, sizeof(s_ubDrawDepth), settings.GetLimits()._terrain_ubDrawDepthCapacity,
		{
			CGI::Sampler::linear2D
		});
	s_shader->CreateDescriptorSet(1, &s_ubPerMaterialFS, sizeof(s_ubPerMaterialFS), 1000,
		{
			CGI::Sampler::aniso, CGI::Sampler::aniso,
			CGI::Sampler::aniso, CGI::Sampler::aniso
		}, CGI::ShaderStageFlags::fs);
	s_shader->CreatePipelineLayout();
}

void Terrain::DoneStatic()
{
	s_shader.Done();
}

void Terrain::Init(RcDesc desc)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	if (!Math::IsPowerOfTwo(desc._mapSide))
		throw VERUS_RECOVERABLE << "Init(), mapSide must be power of two";

	_mapSide = desc._mapSide;
	_mapShift = Math::HighestBit(desc._mapSide);

	const int patchSide = _mapSide >> 4;
	const int patchShift = _mapShift - 4;
	const int patchCount = patchSide * patchSide;
	_vPatches.resize(patchCount);
	_vPatchTBNs.resize(patchCount * 3);
	VERUS_P_FOR(i, patchSide)
	{
		const int offset = i << patchShift;
		VERUS_FOR(j, patchSide)
		{
			RTerrainPatch patch = _vPatches[offset + j];
			patch.BindTBN(&_vPatchTBNs[(offset + j) * 3]);
			patch.InitFlat(desc._height, desc._layer);
			patch._ijCoord[0] = i << 4;
			patch._ijCoord[1] = j << 4;
			patch._layerForChannel[0] = desc._layer;
		}
	});
	if (desc._debugHills)
	{
		const float scale = 1.f / desc._debugHills;
		VERUS_P_FOR(i, _mapSide)
		{
			const float z = i * scale;
			VERUS_FOR(j, _mapSide)
			{
				const float x = j * scale;
				const float res = sin(x * VERUS_2PI) * sin(z * VERUS_2PI);
				const int ij[] = { i, j };
				SetHeightAt(ij, short(res * 1000.f));
			}
		});

		VERUS_FOR(lod, 5)
		{
			VERUS_P_FOR(i, _vPatches.size())
			{
				_vPatches[i].UpdateNormals(this, lod);
			});
		}
	}
	_vSortedPatchIndices.resize(patchCount);
	const int maxInstances = patchCount * 8;

	// Init LODs:
	VERUS_FOR(i, VERUS_COUNT_OF(_lods))
		_lods[i].Init(16 >> i, 1 << i);

	Vector<short> vVB;
	Vector<UINT16> vIB;
	int vbSize = 0, ibSize = 0;
	VERUS_FOR(i, VERUS_COUNT_OF(_lods))
	{
		vbSize += _lods[i]._vertCount * 4;
		ibSize += _lods[i]._indexCount;
	}
	vVB.resize(vbSize);
	vIB.resize(ibSize);
	vbSize = 0, ibSize = 0;
	VERUS_FOR(i, VERUS_COUNT_OF(_lods))
	{
		_lods[i].InitGeo(&vVB[vbSize * 4], &vIB[ibSize], vbSize);
		_lods[i]._firstIndex = ibSize;
		vbSize += _lods[i]._vertCount;
		ibSize += _lods[i]._indexCount;
	}

	CGI::GeometryDesc geoDesc;
	const CGI::InputElementDesc ied[] =
	{
		{ 0, 0,                                    CGI::IeType::shorts, 4, CGI::IeUsage::position, 0},
		{-1, offsetof(PerInstanceData, _posPatch), CGI::IeType::shorts, 4, CGI::IeUsage::texCoord, 8},
		{-1, offsetof(PerInstanceData, _layers),   CGI::IeType::shorts, 4, CGI::IeUsage::texCoord, 9},
		CGI::InputElementDesc::End()
	};
	geoDesc._pInputElementDesc = ied;
	const int strides[] = { sizeof(short) * 4, sizeof(PerInstanceData), 0 };
	geoDesc._pStrides = strides;
	_geo.Init(geoDesc);
	_geo->CreateVertexBuffer(vbSize, 0);
	_geo->CreateVertexBuffer(maxInstances, 1);
	_geo->CreateIndexBuffer(ibSize);
	_geo->UpdateVertexBuffer(vVB.data(), 0);
	_geo->UpdateIndexBuffer(vIB.data());

	_vInstanceBuffer.resize(maxInstances);

	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader, "#", renderer.GetDS().GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
		_pipe[PIPE_LIST].Init(pipeDesc);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_STRIP].Init(pipeDesc);
	}
	{
		VERUS_QREF_ATMO;
		CGI::PipelineDesc pipeDesc(_geo, s_shader, "#Depth", atmo.GetShadowMap().GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = "";
		pipeDesc.EnableDepthBias();
		_pipe[PIPE_DEPTH_LIST].Init(pipeDesc);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_DEPTH_STRIP].Init(pipeDesc);
	}

	CGI::TextureDesc texDesc;
	texDesc._format = CGI::Format::floatR16;
	texDesc._width = _mapSide;
	texDesc._height = _mapSide;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::anyShaderResource;
	_tex[TEX_HEIGHTMAP].Init(texDesc);

	texDesc._format = CGI::Format::unormR8G8B8A8;
	texDesc._width = _mapSide;
	texDesc._height = _mapSide;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::generateMips;
	_tex[TEX_NORMALS].Init(texDesc);
	_tex[TEX_BLEND].Init(texDesc);

	s_shader->FreeDescriptorSet(_cshVS);
	_cshVS = s_shader->BindDescriptorSetTextures(0, { _tex[TEX_HEIGHTMAP] });

	_vBlendBuffer.resize(_mapSide * _mapSide);
	VERUS_P_FOR(i, _vBlendBuffer.size())
	{
		_vBlendBuffer[i] = VERUS_COLOR_RGBA(255, 0, 0, 255);
	});
	UpdateBlendTexture();

	OnHeightModified();
	AddNewRigidBody();

	_vLayerUrls.reserve(s_maxLayers);
}

void Terrain::Done()
{
	s_shader->FreeDescriptorSet(_cshVS);
	s_shader->FreeDescriptorSet(_cshFS);

	_tex.Done();
	_pipe.Done();
	_geo.Done();

	_physics.Done();

	VERUS_DONE(Terrain);
}

int Terrain::UserPtr_GetType()
{
	return 0;
	//return +NodeType::terrain;
}

void Terrain::ResetInstanceCount()
{
	_instanceCount = 0;
}

void Terrain::Layout()
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_ATMO;
	VERUS_QREF_SM;

	// Reset LOD data:
	std::for_each(_vPatches.begin(), _vPatches.end(), [](RTerrainPatch patch) {patch._quadtreeLOD = -1; });

	PCamera pPrevCamera = nullptr;
	Camera cam;
	// For CSM we need to create geometry beyond the view frustum (1st slice):
	if (settings._sceneShadowQuality >= App::Settings::ShadowQuality::cascaded && atmo.GetShadowMap().IsRendering())
	{
		PCamera pCameraCSM = atmo.GetShadowMap().GetCameraCSM();
		if (pCameraCSM)
			pPrevCamera = sm.SetCamera(pCameraCSM);
	}

	_visiblePatchCount = 0;
	_quadtree.TraverseVisible();
	SortVisiblePatches();

	// Back to original camera:
	if (pPrevCamera)
		sm.SetCamera(pPrevCamera);
}

void Terrain::Draw()
{
	VERUS_QREF_RENDERER;
	VERUS_QREF_SM;
	VERUS_QREF_ATMO;

	if (!_visiblePatchCount)
		return;

	const bool drawingDepth = Scene::SceneManager::IsDrawingDepth(Scene::DrawDepth::automatic);

	s_ubDrawDepth._matW = mataff(1);
	s_ubDrawDepth._matWV = sm.GetCamera()->GetMatrixV().UniformBufferFormat();
	s_ubDrawDepth._matVP = sm.GetCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubDrawDepth._posEye_mapSideInv = float4(atmo.GetEyePosition().GLM(), 0);
	s_ubDrawDepth._posEye_mapSideInv.w = 1.f / _mapSide;

	VERUS_FOR(i, VERUS_COUNT_OF(_layerData))
	{
		s_ubPerMaterialFS._vSpecStrength[i >> 2][i & 0x3] = _layerData[i]._specStrength;
		s_ubPerMaterialFS._vDetailStrength[i >> 2][i & 0x3] = _layerData[i]._detailStrength;
	}

	renderer.GetCommandBuffer()->BindVertexBuffers(_geo);
	renderer.GetCommandBuffer()->BindIndexBuffer(_geo);

	s_shader->BeginBindDescriptors();

	bool strip = false;
	const int half = _mapSide >> 1;
	int firstInstance = 0;
	int edge = _visiblePatchCount - 1;
	int lod = _vPatches[_vSortedPatchIndices.front()]._quadtreeLOD;
	VERUS_FOR(i, _visiblePatchCount)
	{
		RcTerrainPatch patch = _vPatches[_vSortedPatchIndices[i]];
		if (patch._quadtreeLOD != lod || i == edge)
		{
			if (i == edge)
				i++; // Drawing patches [firstInstance, i).

			if (!lod)
			{
				if (drawingDepth)
				{
					renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_DEPTH_LIST]);
					renderer.GetCommandBuffer()->BindDescriptors(s_shader, 0, _cshVS);
					renderer.GetCommandBuffer()->BindDescriptors(s_shader, 1, _cshFS);
				}
				else
				{
					renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_LIST]);
					renderer.GetCommandBuffer()->BindDescriptors(s_shader, 0, _cshVS);
					renderer.GetCommandBuffer()->BindDescriptors(s_shader, 1, _cshFS);
				}
			}
			else if (!strip)
			{
				if (drawingDepth)
				{
					strip = true;
					renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_DEPTH_STRIP]);
					renderer.GetCommandBuffer()->BindDescriptors(s_shader, 0, _cshVS);
					renderer.GetCommandBuffer()->BindDescriptors(s_shader, 1, _cshFS);
				}
				else
				{
					strip = true;
					renderer.GetCommandBuffer()->BindPipeline(_pipe[PIPE_STRIP]);
					renderer.GetCommandBuffer()->BindDescriptors(s_shader, 0, _cshVS);
					renderer.GetCommandBuffer()->BindDescriptors(s_shader, 1, _cshFS);
				}
			}

			const int instanceCount = i - firstInstance; // Drawing patches [firstInstance, i).
			for (int inst = firstInstance; inst < i; ++inst)
			{
				const int at = _instanceCount + inst;
				RcTerrainPatch patchToDraw = _vPatches[_vSortedPatchIndices[inst]];
				_vInstanceBuffer[at]._posPatch[0] = patchToDraw._ijCoord[1] - half;
				_vInstanceBuffer[at]._posPatch[1] = patchToDraw._patchHeight;
				_vInstanceBuffer[at]._posPatch[2] = patchToDraw._ijCoord[0] - half;
				_vInstanceBuffer[at]._posPatch[3] = 0;
				VERUS_FOR(ch, 4)
					_vInstanceBuffer[at]._layers[ch] = patchToDraw._layerForChannel[ch];
			}

			renderer.GetCommandBuffer()->DrawIndexed(_lods[lod]._indexCount, instanceCount, _lods[lod]._firstIndex, 0, _instanceCount + firstInstance);

			lod = patch._quadtreeLOD;
			firstInstance = i;
		}
	}

	_instanceCount += _visiblePatchCount;

	_geo->UpdateVertexBuffer(_vInstanceBuffer.data(), 1);

	s_shader->EndBindDescriptors();
}

void Terrain::SortVisiblePatches()
{
	std::sort(_vSortedPatchIndices.begin(), _vSortedPatchIndices.begin() + _visiblePatchCount, [this](int a, int b)
		{
			RcTerrainPatch patchA = GetPatch(a);
			RcTerrainPatch patchB = GetPatch(b);

			if (patchA._quadtreeLOD != patchB._quadtreeLOD)
				return patchA._quadtreeLOD < patchB._quadtreeLOD;

			return patchA._distToCameraSq < patchB._distToCameraSq;
		});
}

void Terrain::QuadtreeIntegral_ProcessVisibleNode(const short ij[2], RcPoint3 center)
{
	VERUS_QREF_ATMO;

	const RcPoint3 posEye = atmo.GetEyePosition();

	const Vector3 toCenter = center - posEye;
	const float dist = VMath::length(toCenter);

	const float lodF = log2(Math::Clamp((dist - 12) * (2 / 100.f), 1.f, 18.f));
	const int lod = Math::Clamp<int>(int(lodF), 0, 4);

	// Locate this patch:
	const int shiftPatch = _mapShift - 4;
	const int iPatch = ij[0] >> 4;
	const int jPatch = ij[1] >> 4;
	const int offsetPatch = (iPatch << shiftPatch) + jPatch;

	// Update this patch:
	RTerrainPatch patch = _vPatches[offsetPatch];
	patch._distToCameraSq = int(dist * dist);
	patch._patchHeight = ConvertHeight(center.getY());
	patch._quadtreeLOD = lod;

	_vSortedPatchIndices[_visiblePatchCount++] = offsetPatch;
}

void Terrain::QuadtreeIntegral_GetHeights(const short ij[2], float height[2])
{
	float mn = FLT_MAX, mx = -FLT_MAX;
	VERUS_FOR(i, 17)
	{
		VERUS_FOR(j, 17)
		{
			const int ijTile[] = { ij[0] + i, ij[1] + j };
			mn = Math::Min(mn, GetHeightAt(ijTile));
			mx = Math::Max(mx, GetHeightAt(ijTile));
		}
	}
	height[0] = mn - 1;
	height[1] = mx + 1;
}

void Terrain::FattenQuadtreeNodesBy(float x)
{
	_quadtreeFatten = x;
	_quadtree.Done();
	_quadtree.Init(_mapSide, 16, this, _quadtreeFatten);
}

float Terrain::GetHeightAt(const float xz[2]) const
{
	VERUS_QREF_BULLET;

	const btVector3 from(xz[0], 500, xz[1]);
	const btVector3 to(xz[0], -500, xz[1]);
	btCollisionWorld::ClosestRayResultCallback crrc(from, to);
	crrc.m_collisionFilterMask = +Physics::Group::terrain;
	bullet.GetWorld()->rayTest(from, to, crrc);
	if (crrc.hasHit())
		return crrc.m_hitPointWorld.getY();
	return 0;
}

float Terrain::GetHeightAt(RcPoint3 pos) const
{
	const float xz[2] = { pos.getX(), pos.getZ() };
	return GetHeightAt(xz);
}

float Terrain::GetHeightAt(const int ij[2], int lod, short* pRaw) const
{
	VERUS_RT_ASSERT(lod >= 0 && lod <= 4);

	const int mapEdge = _mapSide - 1;
	const int i = Math::Clamp(ij[0], 0, mapEdge);
	const int j = Math::Clamp(ij[1], 0, mapEdge);

	const int iLocal = i & 0xF;
	const int jLocal = j & 0xF;

	const int shiftPatch = _mapShift - 4;
	const int iPatch = i >> 4;
	const int jPatch = j >> 4;
	const int offsetPatch = (iPatch << shiftPatch) + jPatch;

	const short h = _vPatches[offsetPatch]._height[(iLocal << 4) + jLocal];
	if (pRaw)
		*pRaw = h;
	return ConvertHeight(h);
}

void Terrain::SetHeightAt(const int ij[2], short h)
{
	const int mapEdge = _mapSide - 1;
	const int i = Math::Clamp(ij[0], 0, mapEdge);
	const int j = Math::Clamp(ij[1], 0, mapEdge);

	const int iLocal = i & 0xF;
	const int jLocal = j & 0xF;

	const int shiftPatch = _mapShift - 4;
	const int iPatch = i >> 4;
	const int jPatch = j >> 4;
	const int offsetPatch = (iPatch << shiftPatch) + jPatch;

	_vPatches[offsetPatch]._height[(iLocal << 4) + jLocal] = h;
}

void Terrain::UpdateHeightBuffer()
{
	_vHeightBuffer.resize(_mapSide * _mapSide);
	VERUS_P_FOR(i, _mapSide)
	{
		const int offset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			const int ij[] = { i, j };
			short h;
			GetHeightAt(ij, 0, &h);
			_vHeightBuffer[offset + j] = h;
		}
	});
}

const char* Terrain::GetNormalAt(const int ij[2], int lod, TerrainTBN tbn) const
{
	VERUS_RT_ASSERT(lod >= 0 && lod <= 4);

	const int mapEdge = _mapSide - 1;
	const int i = Math::Clamp(ij[0], 0, mapEdge);
	const int j = Math::Clamp(ij[1], 0, mapEdge);

	const int iLocal = i & 0xF;
	const int jLocal = j & 0xF;

	const int shiftPatch = _mapShift - 4;
	const int iPatch = i >> 4;
	const int jPatch = j >> 4;
	const int offsetPatch = (iPatch << shiftPatch) + jPatch;

	if (lod)
	{
		const char* normals[] =
		{
			nullptr,
			_vPatches[offsetPatch]._pTBN[+tbn]._normals1[0],
			_vPatches[offsetPatch]._pTBN[+tbn]._normals2[0],
			_vPatches[offsetPatch]._pTBN[+tbn]._normals3[0],
			_vPatches[offsetPatch]._pTBN[+tbn]._normals4[0]
		};
		const int iLOD = iLocal >> lod;
		const int jLOD = jLocal >> lod;
		return &normals[lod][((iLOD << (4 - lod)) + jLOD) << 2];
	}

	return _vPatches[offsetPatch]._pTBN[+tbn]._normals0[(iLocal << 4) + jLocal];
}

Matrix3 Terrain::GetBasisAt(const int ij[2]) const
{
	glm::vec3 c0, c1, c2;
	Convert::Sint8ToSnorm(GetNormalAt(ij, 0, TerrainTBN::tangent), &c0.x, 3);
	Convert::Sint8ToSnorm(GetNormalAt(ij, 0, TerrainTBN::binormal), &c1.x, 3);
	Convert::Sint8ToSnorm(GetNormalAt(ij, 0, TerrainTBN::normal), &c2.x, 3);
	return Matrix3(Vector3(c0), Vector3(c2), Vector3(c1));
}

void Terrain::InsertLayerUrl(int layer, CSZ url)
{
	VERUS_RT_ASSERT(layer >= 0 && layer < s_maxLayers);
	VERUS_RT_ASSERT(url);
	if (_vLayerUrls.size() <= layer)
		_vLayerUrls.resize(layer + 1);
	_vLayerUrls[layer] = url;
}

void Terrain::DeleteAllLayerUrls()
{
	_vLayerUrls.clear();
}

void Terrain::GetLayerUrls(Vector<CSZ>& v)
{
	v.clear();
	v.reserve(_vLayerUrls.size());
	for (const auto& url : _vLayerUrls)
		v.push_back(_C(url));
}

void Terrain::LoadLayersFromFile(CSZ url)
{
	DeleteAllLayerUrls();
	if (!url)
		url = "[Misc]:TerrainLayers.xml";
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData);
	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size());
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
	pugi::xml_node root = doc.first_child();
	for (auto node : root.children("layer"))
	{
		const int id = node.attribute("id").as_int();
		InsertLayerUrl(id, node.attribute("url").as_string());
	}
	LoadLayerTextures();
}

void Terrain::LoadLayerTextures()
{
	if (_vLayerUrls.empty())
		return;

	VERUS_QREF_RENDERER;
	renderer->WaitIdle();

	_tex[TEX_LAYERS].Done();
	_tex[TEX_LAYERS_NM].Done();

	Vector<CSZ> vLayerUrls;
	vLayerUrls.resize(_vLayerUrls.size() + 1);
	VERUS_FOR(i, _vLayerUrls.size())
		vLayerUrls[i] = _C(_vLayerUrls[i]);

	CGI::TextureDesc texDesc;
	texDesc._urls = vLayerUrls.data();
	_tex[TEX_LAYERS].Init(texDesc);

	Vector<String> vLayerUrlsNM;
	Vector<CSZ> vLayerUrlsPtrNM;
	vLayerUrlsNM.reserve(vLayerUrls.size());
	vLayerUrlsPtrNM.reserve(vLayerUrls.size());
	for (CSZ name : vLayerUrls)
	{
		if (name)
		{
			String nm = name;
			Str::ReplaceExtension(nm, ".NM.dds");
			vLayerUrlsNM.push_back(nm);
			vLayerUrlsPtrNM.push_back(_C(vLayerUrlsNM.back()));
		}
	}
	vLayerUrlsPtrNM.push_back(nullptr);

	texDesc._urls = vLayerUrlsPtrNM.data();
	_tex[TEX_LAYERS_NM].Init(texDesc);

	s_shader->FreeDescriptorSet(_cshFS);
	_cshFS = s_shader->BindDescriptorSetTextures(1, { _tex[TEX_NORMALS], _tex[TEX_BLEND], _tex[TEX_LAYERS], _tex[TEX_LAYERS_NM] });
}

int Terrain::GetMainLayerAt(const int ij[2]) const
{
	const int mapEdge = _mapSide - 1;
	const int i = Math::Clamp(ij[0], 0, mapEdge);
	const int j = Math::Clamp(ij[1], 0, mapEdge);

	const int iLocal = i & 0xF;
	const int jLocal = j & 0xF;

	const int shiftPatch = _mapShift - 4;
	const int iPatch = i >> 4;
	const int jPatch = j >> 4;
	const int offsetPatch = (iPatch << shiftPatch) + jPatch;

	return _vPatches[offsetPatch]._mainLayer[(iLocal << 4) + jLocal];
}

void Terrain::UpdateMainLayerAt(const int ij[2])
{
	const int mapEdge = _mapSide - 1;
	const int i = Math::Clamp(ij[0], 0, mapEdge);
	const int j = Math::Clamp(ij[1], 0, mapEdge);

	const int iLocal = i & 0xF;
	const int jLocal = j & 0xF;

	const int shiftPatch = _mapShift - 4;
	const int iPatch = i >> 4;
	const int jPatch = j >> 4;
	const int offsetPatch = (iPatch << shiftPatch) + jPatch;

	int maxSplat = 0;
	int maxSplatChannel = 0;
	const int offset = (i << _mapShift) + j;
	const BYTE* rgba = reinterpret_cast<const BYTE*>(&_vBlendBuffer[offset]);
	VERUS_FOR(i, 4)
	{
		const int value = (i != 3) ? rgba[i] : 255 - rgba[0] - rgba[1] - rgba[2];
		if (value > maxSplat)
		{
			maxSplat = value;
			maxSplatChannel = i;
		}
	}

	_vPatches[offsetPatch]._mainLayer[(iLocal << 4) + jLocal] =
		_vPatches[offsetPatch]._layerForChannel[maxSplatChannel];
}

void Terrain::UpdateHeightmapTexture()
{
	VERUS_QREF_RENDERER;
	_vHeightmapSubresData.resize(_mapSide * _mapSide);
	const int mipLevels = Math::ComputeMipLevels(_mapSide, _mapSide);
	VERUS_FOR(lod, mipLevels)
	{
		const int side = _mapSide >> lod;
		const int step = _mapSide / side;
		VERUS_P_FOR(i, side)
		{
			const int offset = i * side;
			VERUS_FOR(j, side)
			{
				const int ij[] = { i * step, j * step };
				const float bestPrecision = 50;
				_vHeightmapSubresData[offset + j] = glm::packHalf1x16(GetHeightAt(ij, 0) - bestPrecision);
			}
		});
		_tex[TEX_HEIGHTMAP]->UpdateSubresource(_vHeightmapSubresData.data(), lod);
	}
}

CGI::TexturePtr Terrain::GetHeightmapTexture() const
{
	return _tex[TEX_HEIGHTMAP];
}

void Terrain::UpdateNormalsTexture()
{
	_vNormalsSubresData.resize(_mapSide * _mapSide);
	VERUS_P_FOR(i, _mapSide)
	{
		VERUS_FOR(j, _mapSide)
		{
			const int ij[] = { i, j };
			char nrm[4];
			char tan[4];
			memcpy(nrm, GetNormalAt(ij, 0, TerrainTBN::normal), 3);
			memcpy(tan, GetNormalAt(ij, 0, TerrainTBN::tangent), 3);
			BYTE rgba[4] =
			{
				nrm[0] + 127,
				nrm[2] + 127,
				tan[1] + 127,
				tan[2] + 127
			};
			memcpy(&_vNormalsSubresData[(i << _mapShift) + j], rgba, sizeof(UINT32));
		}
	});
	_tex[TEX_NORMALS]->UpdateSubresource(_vNormalsSubresData.data());
	_tex[TEX_NORMALS]->GenerateMips();
}

CGI::TexturePtr Terrain::GetNormalsTexture() const
{
	return _tex[TEX_NORMALS];
}

void Terrain::UpdateBlendTexture()
{
	_tex[TEX_BLEND]->UpdateSubresource(_vBlendBuffer.data());
	_tex[TEX_BLEND]->GenerateMips();
}

CGI::TexturePtr Terrain::GetBlendTexture() const
{
	return _tex[TEX_BLEND];
}

void Terrain::UpdateMainLayerTexture()
{
	_vMainLayerSubresData.resize(_mapSide * _mapSide);
	VERUS_P_FOR(i, _mapSide)
	{
		const int offset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			const int ij[] = { i, j };
			_vMainLayerSubresData[offset + j] = GetMainLayerAt(ij) * 16 + 4;
		}
	});
	//_tex[TEX_MAIN_LAYER]->UpdateSubresource(_vMainLayerSubresData.data());
}

CGI::TexturePtr Terrain::GetMainLayerTexture() const
{
	return _tex[TEX_MAIN_LAYER];
}

void Terrain::OnHeightModified()
{
	_quadtree.Done();
	_quadtree.Init(_mapSide, 16, this, _quadtreeFatten);

	UpdateHeightBuffer();
	UpdateHeightmapTexture();
	UpdateNormalsTexture();
}

void Terrain::AddNewRigidBody()
{
	_physics.Init(this, _mapSide, _mapSide, _vHeightBuffer.data(), ConvertHeight(static_cast<short>(1)));
}

void Terrain::Serialize(IO::RSeekableStream stream)
{
	stream.WriteText(VERUS_CRNL VERUS_CRNL "<TR>");
	stream.BeginBlock();
	stream.WriteString(_C(std::to_string(_mapSide)));

	// Tiles (height):
	VERUS_FOR(i, _mapSide)
	{
		const int offset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			const int ij[] = { i, j };
			short h;
			GetHeightAt(ij, 0, &h);
			stream << h;
			stream << char(0);
		}
	}

	// Texture names:
	BYTE layerCount = _vLayerUrls.size();
	stream << layerCount;
	VERUS_FOR(i, layerCount)
	{
		stream.WriteString(_C(_vLayerUrls[i]));
		stream << _layerData[i]._specStrength;
		stream << _layerData[i]._detailStrength;
	}

	// Patches:
	const int patchesSide = _mapSide >> 4;
	const int patchCount = patchesSide * patchesSide;
	VERUS_FOR(i, patchCount)
	{
		RTerrainPatch patch = _vPatches[i];
		const BYTE count = patch._usedChannelCount;
		stream << count;
		VERUS_FOR(j, patch._usedChannelCount)
		{
			const char layer = patch._layerForChannel[j];
			stream << layer;
		}
	}

	// Blend channels:
	VERUS_FOR(i, _mapSide)
	{
		const int offset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			const UINT32 b = _vBlendBuffer[offset + j];
			const UINT16 data = Convert::Uint8x4ToUint4x4(b);
			stream << data;
		}
	}

	stream.EndBlock();
}

void Terrain::Deserialize(IO::RStream stream)
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_SM;

	char buffer[IO::Stream::s_bufferSize] = {};

	Done();
	stream.ReadString(buffer);
	Desc desc;
	desc._mapSide = atoi(buffer);
	Init(desc);

	const int patchSide = _mapSide >> 4;
	const int patchShift = _mapShift - 4;
	const int patchCount = patchSide * patchSide;

	// <Height>
	VERUS_FOR(i, _mapSide)
	{
		const int offset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			short h;
			char hole;
			stream >> h;
			stream >> hole;
			const int ij[] = { i, j };
			SetHeightAt(ij, h);
		}
	}
	// </Height>

	// <Layers>
	DeleteAllLayerUrls();
	BYTE layerCount = 0;
	stream >> layerCount;
	VERUS_FOR(i, layerCount)
	{
		char url[IO::Stream::s_bufferSize] = {};
		stream.ReadString(url);

		if (*url != '[')
		{
			String newUrl("[");
			newUrl += url;
			Str::ReplaceAll(newUrl, ":", "]:");
			strcpy_s(url, _C(newUrl));
		}

		InsertLayerUrl(i, url);
		stream >> _layerData[i]._specStrength;
		stream >> _layerData[i]._detailStrength;
	}
	LoadLayerTextures();
	// </Layers>

	// <Patches>
	VERUS_FOR(i, patchCount)
	{
		BYTE count = 0;
		stream >> count;
		_vPatches[i]._usedChannelCount = Math::Clamp<int>(count, 1, 4);
		VERUS_FOR(j, 4)
		{
			if (j < count)
			{
				BYTE layer = 0;
				stream >> layer;
				_vPatches[i]._layerForChannel[j] = Math::Clamp<int>(layer, 0, s_maxLayers - 1);
			}
			else
			{
				_vPatches[i]._layerForChannel[j] = -1;
			}
		}
	}
	VERUS_FOR(lod, 5)
	{
		VERUS_FOR(i, _vPatches.size())
			_vPatches[i].UpdateNormals(this, lod);
	}
	// </Patches>

	// <Blend>
	VERUS_FOR(i, _mapSide)
	{
		const int offset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			UINT16 data;
			stream >> data;
			_vBlendBuffer[offset + j] = Convert::Uint4x4ToUint8x4(data);
			const int ij[] = { i, j };
			UpdateMainLayerAt(ij);
		}
	}
	UpdateBlendTexture();
	// </Blend>

	OnHeightModified();
	AddNewRigidBody();
}
