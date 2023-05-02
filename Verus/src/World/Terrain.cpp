// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::World;

CGI::ShaderPwns<Terrain::SHADER_COUNT> Terrain::s_shader;
Terrain::UB_TerrainVS                  Terrain::s_ubTerrainVS;
Terrain::UB_TerrainFS                  Terrain::s_ubTerrainFS;
Terrain::UB_SimpleTerrainVS            Terrain::s_ubSimpleTerrainVS;
Terrain::UB_SimpleTerrainFS            Terrain::s_ubSimpleTerrainFS;

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

	_pShape = new(_pShape.GetData()) btHeightfieldTerrainShape(
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
	_pRigidBody = bullet.AddNewRigidBody(_pRigidBody, 0, tr, _pShape.Get(), +Physics::Group::terrain, +bullet.GetNonStaticMask());
	_pRigidBody->setFriction(Physics::Bullet::GetFriction(Physics::Material::stone));
	_pRigidBody->setRestitution(Physics::Bullet::GetRestitution(Physics::Material::stone));
	_pRigidBody->setUserPointer(p);

	EnableDebugDraw(false);
}

void TerrainPhysics::Done()
{
	if (_pRigidBody.Get())
	{
		VERUS_QREF_BULLET;
		bullet.GetWorld()->removeRigidBody(_pRigidBody.Get());
		_pRigidBody.Delete();
	}
	_pShape.Delete();
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

void TerrainLOD::Init(int sidePoly, int step, bool addEdgeTess)
{
	_side = sidePoly + 1;
	_vertCount = _side * _side;
	if (1 == step)
		Math::CreateListGrid(sidePoly, sidePoly, _vIB);
	else
		Math::CreateStripGrid(sidePoly, sidePoly, _vIB);
	_firstIndex = 0;

	if (addEdgeTess)
	{
		const int extraVerts = sidePoly * 4;
		_vIB.reserve(_vIB.capacity() + extraVerts * 4);
		_vIB.push_back(0xFFFF);
		auto ComputeIndex = [this](int i, int j)
		{
			return i * _side + j;
		};

		// Bottom, forwards:
		VERUS_FOR(i, sidePoly)
		{
			_vIB.push_back(ComputeIndex(sidePoly, i));
			_vIB.push_back(_vertCount + i);
			_vIB.push_back(ComputeIndex(sidePoly, i + 1));
			_vIB.push_back(0xFFFF);
		}
		// Right, backwards:
		VERUS_FOR(i, sidePoly)
		{
			_vIB.push_back(ComputeIndex(sidePoly - i, sidePoly));
			_vIB.push_back(_vertCount + sidePoly + i);
			_vIB.push_back(ComputeIndex(sidePoly - i - 1, sidePoly));
			_vIB.push_back(0xFFFF);
		}
		// Top, backwards:
		VERUS_FOR(i, sidePoly)
		{
			_vIB.push_back(ComputeIndex(0, sidePoly - i));
			_vIB.push_back(_vertCount + sidePoly * 2 + i);
			_vIB.push_back(ComputeIndex(0, sidePoly - i - 1));
			_vIB.push_back(0xFFFF);
		}
		// Left, forwards:
		VERUS_FOR(i, sidePoly)
		{
			_vIB.push_back(ComputeIndex(i, 0));
			_vIB.push_back(_vertCount + sidePoly * 3 + i);
			_vIB.push_back(ComputeIndex(i + 1, 0));
			if (_vIB.size() < _vIB.capacity())
				_vIB.push_back(0xFFFF);
		}

		_vertCount += extraVerts;
		VERUS_RT_ASSERT(Math::CheckIndexBuffer(_vIB, _vertCount - 1));
	}
	_indexCount = Utils::Cast32(_vIB.size());
}

void TerrainLOD::InitGeo(short* pV, UINT16* pI, int vertexOffset, bool addEdgeTess)
{
	const int edge = _side - 1;
	const int step = 16 / edge;
	VERUS_FOR(i, _side)
	{
		const int rowOffset = i * _side;
		VERUS_FOR(j, _side)
		{
			// X and Z:
			pV[((rowOffset + j) << 2) + 0] = j * step;
			pV[((rowOffset + j) << 2) + 2] = i * step;

			// Y and W, edge correction:
			if (i == 0)         pV[((rowOffset + j) << 2) + 3] = 1;
			else if (i == edge) pV[((rowOffset + j) << 2) + 3] = -1;
			else                pV[((rowOffset + j) << 2) + 3] = 0;
			if (j == 0)         pV[((rowOffset + j) << 2) + 1] = 1;
			else if (j == edge) pV[((rowOffset + j) << 2) + 1] = -1;
			else                pV[((rowOffset + j) << 2) + 1] = 0;
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

	if (addEdgeTess)
	{
		const int halfStep = step / 2;
		const int vertCount = _side * _side;
		const int sidePoly = _side - 1;
		const short minPos = 0;
		const short maxPos = 16;
		VERUS_FOR(i, sidePoly)
		{
			pV[((vertCount + i) << 2) + 0] = halfStep + i * step;
			pV[((vertCount + i) << 2) + 2] = maxPos;
			pV[((vertCount + i) << 2) + 3] = -1;
		}
		VERUS_FOR(i, sidePoly)
		{
			pV[((vertCount + sidePoly + i) << 2) + 0] = maxPos;
			pV[((vertCount + sidePoly + i) << 2) + 2] = halfStep + (sidePoly - i - 1) * step;
			pV[((vertCount + sidePoly + i) << 2) + 1] = -1;
		}
		VERUS_FOR(i, sidePoly)
		{
			pV[((vertCount + sidePoly * 2 + i) << 2) + 0] = halfStep + (sidePoly - i - 1) * step;
			pV[((vertCount + sidePoly * 2 + i) << 2) + 2] = minPos;
			pV[((vertCount + sidePoly * 2 + i) << 2) + 3] = 1;
		}
		VERUS_FOR(i, sidePoly)
		{
			pV[((vertCount + sidePoly * 3 + i) << 2) + 0] = minPos;
			pV[((vertCount + sidePoly * 3 + i) << 2) + 2] = halfStep + i * step;
			pV[((vertCount + sidePoly * 3 + i) << 2) + 1] = 1;
		}
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

	s_shader[SHADER_MAIN].Init("[Shaders]:DS_Terrain.hlsl");
	s_shader[SHADER_MAIN]->CreateDescriptorSet(0, &s_ubTerrainVS, sizeof(s_ubTerrainVS), settings.GetLimits()._terrain_ubVSCapacity,
		{
			CGI::Sampler::linearClampMipN, // Height
			CGI::Sampler::linearClampMipN // Normal
		}, CGI::ShaderStageFlags::vs_hs_ds);
	s_shader[SHADER_MAIN]->CreateDescriptorSet(1, &s_ubTerrainFS, sizeof(s_ubTerrainFS), settings.GetLimits()._terrain_ubFSCapacity,
		{
			CGI::Sampler::anisoClamp, // Normal
			CGI::Sampler::anisoClamp, // Blend
			CGI::Sampler::aniso, // Layers
			CGI::Sampler::aniso, // LayersN
			CGI::Sampler::aniso, // LayersX
			CGI::Sampler::aniso, // Detail
			CGI::Sampler::aniso, // DetailN
			CGI::Sampler::lodBias // Strass
		}, CGI::ShaderStageFlags::fs);
	s_shader[SHADER_MAIN]->CreatePipelineLayout();

	s_shader[SHADER_SIMPLE].Init("[Shaders]:SimpleTerrain.hlsl");
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(0, &s_ubSimpleTerrainVS, sizeof(s_ubSimpleTerrainVS), settings.GetLimits()._terrain_ubVSCapacity,
		{
			CGI::Sampler::linearClampMipN, // Height
			CGI::Sampler::linearClampMipN // Normal
		}, CGI::ShaderStageFlags::vs);
	s_shader[SHADER_SIMPLE]->CreateDescriptorSet(1, &s_ubSimpleTerrainFS, sizeof(s_ubSimpleTerrainFS), settings.GetLimits()._terrain_ubFSCapacity,
		{
			CGI::Sampler::linearClampMipN, // Blend
			CGI::Sampler::linearMipN, // Layers
			CGI::Sampler::linearMipN, // LayersX
			CGI::Sampler::shadow
		}, CGI::ShaderStageFlags::fs);
	s_shader[SHADER_SIMPLE]->CreatePipelineLayout();
}

void Terrain::DoneStatic()
{
	s_shader.Done();
}

void Terrain::Init(RcDesc desc)
{
	VERUS_INIT();
	VERUS_QREF_ATMO;
	VERUS_QREF_CSMB;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;

	_mapSide = desc._mapSide;
	IO::Image image;
	if (desc._heightmapUrl)
	{
		image.Init(desc._heightmapUrl);
		_mapSide = image._width;
	}
	_mapShift = Math::HighestBit(_mapSide);

	if (!Math::IsPowerOfTwo(_mapSide))
		throw VERUS_RECOVERABLE << "Init(); mapSide must be power of two";

	const int patchSide = _mapSide >> 4;
	const int patchShift = _mapShift - 4;
	const int patchCount = patchSide * patchSide;
	const int maxInstances = patchCount * 16;

	_vPatches.resize(patchCount);
	_vPatchTBNs.resize(patchCount * 3);
	VERUS_P_FOR(i, patchSide)
	{
		const int rowOffset = i << patchShift;
		VERUS_FOR(j, patchSide)
		{
			RTerrainPatch patch = _vPatches[rowOffset + j];
			patch.BindTBN(&_vPatchTBNs[3 * (rowOffset + j)]);
			patch.InitFlat(desc._height, desc._layer);
			patch._ijCoord[0] = i << 4;
			patch._ijCoord[1] = j << 4;
			patch._layerForChannel[0] = desc._layer;
		}
	});

	if (desc._heightmapUrl)
	{
		VERUS_P_FOR(i, _mapSide)
		{
			VERUS_FOR(j, _mapSide)
			{
				const BYTE pix = image._p[image._pixelStride * ((i << _mapShift) + j)];
				const float h = Convert::Uint8ToUnorm(pix) * desc._heightmapScale + desc._heightmapBias;
				const short hs = Math::Clamp(ConvertHeight(h), -SHRT_MAX, SHRT_MAX);
				const int ij[] = { i, j };
				SetHeightAt(ij, hs);
			}
		});

		VERUS_FOR(lod, 5)
		{
			VERUS_P_FOR(i, Utils::Cast32(_vPatches.size()))
			{
				_vPatches[i].UpdateNormals(this, lod);
			});
		}
	}
	else if (desc._debugHills)
	{
		if (desc._debugHills < 0)
		{
			VERUS_P_FOR(i, _mapSide)
			{
				VERUS_FOR(j, _mapSide)
				{
					const int ij[] = { i, j };
					if (((i >> 4) + (j >> 4)) & 0x1)
						SetHeightAt(ij, (i & 0xF) * 100);
					else
						SetHeightAt(ij, (j & 0xF) * 100);
				}
			});
		}
		else
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
		}

		VERUS_FOR(lod, 5)
		{
			VERUS_P_FOR(i, Utils::Cast32(_vPatches.size()))
			{
				_vPatches[i].UpdateNormals(this, lod);
			});
		}
	}
	_vSortedPatchIndices.resize(patchCount);
	_vRandomPatchIndices.resize(patchCount);

	// Init LODs:
	VERUS_FOR(i, VERUS_COUNT_OF(_lods))
		_lods[i].Init(16 >> i, 1 << i, i > 0);

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
		_lods[i].InitGeo(&vVB[vbSize * 4], &vIB[ibSize], vbSize, i > 0);
		_lods[i]._firstIndex = ibSize;
		vbSize += _lods[i]._vertCount;
		ibSize += _lods[i]._indexCount;
	}
	_vInstanceBuffer.resize(maxInstances);

	CGI::GeometryDesc geoDesc;
	geoDesc._name = "Terrain.Geo";
	const CGI::VertexInputAttrDesc viaDesc[] =
	{
		{ 0, 0,                                    CGI::ViaType::shorts, 4, CGI::ViaUsage::position, 0},
		{-1, offsetof(PerInstanceData, _posPatch), CGI::ViaType::shorts, 4, CGI::ViaUsage::instData, 0},
		{-1, offsetof(PerInstanceData, _layers),   CGI::ViaType::shorts, 4, CGI::ViaUsage::instData, 1},
		CGI::VertexInputAttrDesc::End()
	};
	geoDesc._pVertexInputAttrDesc = viaDesc;
	const int strides[] = { sizeof(short) * 4, sizeof(PerInstanceData), 0 };
	geoDesc._pStrides = strides;
	_geo.Init(geoDesc);
	_geo->CreateVertexBuffer(vbSize, 0);
	_geo->CreateVertexBuffer(maxInstances, 1);
	_geo->CreateIndexBuffer(ibSize);
	_geo->UpdateVertexBuffer(vVB.data(), 0);
	_geo->UpdateIndexBuffer(vIB.data());

	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#", renderer.GetDS().GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
		_pipe[PIPE_LIST].Init(pipeDesc);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_STRIP].Init(pipeDesc);
	}
	if (settings._gpuTessellation)
	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#Tess", renderer.GetDS().GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._topology = CGI::PrimitiveTopology::patchList3;
		_pipe[PIPE_TESS].Init(pipeDesc);
	}

	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#Depth", csmb.GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = "";
		_pipe[PIPE_DEPTH_LIST].Init(pipeDesc);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_DEPTH_STRIP].Init(pipeDesc);
	}
	if (settings._gpuTessellation)
	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#DepthTess", csmb.GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = "";
		pipeDesc._topology = CGI::PrimitiveTopology::patchList3;
		_pipe[PIPE_DEPTH_TESS].Init(pipeDesc);
	}

	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_MAIN], "#SolidColor", renderer.GetDS().GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[1] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[2] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._colorAttachBlendEqs[3] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._rasterizationState._polygonMode = CGI::PolygonMode::line;
		pipeDesc._rasterizationState._depthBiasEnable = true;
		pipeDesc._rasterizationState._depthBiasConstantFactor = -50;
		pipeDesc._depthCompareOp = CGI::CompareOp::lessOrEqual;
		_pipe[PIPE_WIREFRAME_LIST].Init(pipeDesc);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_WIREFRAME_STRIP].Init(pipeDesc);
	}

	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], "#", atmo.GetCubeMapBaker().GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._rasterizationState._cullMode = CGI::CullMode::front;
		_pipe[PIPE_SIMPLE_ENV_MAP_LIST].Init(pipeDesc);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_SIMPLE_ENV_MAP_STRIP].Init(pipeDesc);
	}

	CGI::TextureDesc texDesc;
	texDesc._name = "Terrain.Heightmap";
	texDesc._format = CGI::Format::floatR16;
	texDesc._width = _mapSide;
	texDesc._height = _mapSide;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::anyShaderResource;
	_tex[TEX_HEIGHTMAP].Init(texDesc);

	texDesc._name = "Terrain.Normals";
	texDesc._format = CGI::Format::unormR8G8B8A8;
	texDesc._width = _mapSide;
	texDesc._height = _mapSide;
	texDesc._mipLevels = 0;
	texDesc._flags = CGI::TextureDesc::Flags::anyShaderResource | CGI::TextureDesc::Flags::generateMips;
	_tex[TEX_NORMALS].Init(texDesc);
	texDesc._name = "Terrain.Blend";
	texDesc._flags = CGI::TextureDesc::Flags::generateMips;
	_tex[TEX_BLEND].Init(texDesc);

	texDesc.Reset();
	texDesc._name = "Terrain.MainLayer";
	texDesc._format = CGI::Format::unormR8;
	texDesc._width = _mapSide;
	texDesc._height = _mapSide;
	texDesc._flags = CGI::TextureDesc::Flags::anyShaderResource;
	_tex[TEX_MAIN_LAYER].Init(texDesc);

	s_shader[SHADER_MAIN]->FreeDescriptorSet(_cshVS);
	_cshVS = s_shader[SHADER_MAIN]->BindDescriptorSetTextures(0, { _tex[TEX_HEIGHTMAP], _tex[TEX_NORMALS] });

	s_shader[SHADER_SIMPLE]->FreeDescriptorSet(_cshSimpleVS);
	_cshSimpleVS = s_shader[SHADER_SIMPLE]->BindDescriptorSetTextures(0, { _tex[TEX_HEIGHTMAP], _tex[TEX_NORMALS] });

	_vBlendBuffer.resize(_mapSide * _mapSide);
	VERUS_P_FOR(i, Utils::Cast32(_vBlendBuffer.size()))
	{
		_vBlendBuffer[i] = VERUS_COLOR_RGBA(255, 0, 0, 255);
	});
	UpdateBlendTexture();
	UpdateMainLayerTexture();

	OnHeightModified();
	AddNewRigidBody();

	_vLayerUrls.reserve(s_maxLayers);

	renderer.GetDS().InitByTerrain(_tex[TEX_HEIGHTMAP], _tex[TEX_BLEND], _mapSide);
}

void Terrain::InitByWater()
{
	VERUS_QREF_WATER;

	if (_pipe[PIPE_SIMPLE_PLANAR_REF_LIST].Get())
		return;

	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], "#", water.GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		pipeDesc._rasterizationState._cullMode = CGI::CullMode::front;
		_pipe[PIPE_SIMPLE_PLANAR_REF_LIST].Init(pipeDesc);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_SIMPLE_PLANAR_REF_STRIP].Init(pipeDesc);
	}
	{
		CGI::PipelineDesc pipeDesc(_geo, s_shader[SHADER_SIMPLE], "#", water.GetRenderPassHandle());
		pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
		_pipe[PIPE_SIMPLE_UNDERWATER_LIST].Init(pipeDesc);
		pipeDesc._topology = CGI::PrimitiveTopology::triangleStrip;
		pipeDesc._primitiveRestartEnable = true;
		_pipe[PIPE_SIMPLE_UNDERWATER_STRIP].Init(pipeDesc);
	}
}

void Terrain::Done()
{
	if (s_shader[SHADER_SIMPLE])
	{
		s_shader[SHADER_SIMPLE]->FreeDescriptorSet(_cshSimpleVS);
		s_shader[SHADER_SIMPLE]->FreeDescriptorSet(_cshSimpleFS);
	}
	if (s_shader[SHADER_MAIN])
	{
		s_shader[SHADER_MAIN]->FreeDescriptorSet(_cshVS);
		s_shader[SHADER_MAIN]->FreeDescriptorSet(_cshFS);
	}

	_tex.Done();
	_pipe.Done();
	_geo.Done();

	_physics.Done();

	VERUS_DONE(Terrain);
}

void Terrain::ResetInstanceCount()
{
	_instanceCount = 0;
}

void Terrain::Layout()
{
	VERUS_QREF_ATMO;
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_WM;

	_visiblePatchCount = 0;
	_visibleSortedPatchCount = 0;
	_visibleRandomPatchCount = 0;

	_quadtree.DetectElements();

	SortVisible();
}

void Terrain::SortVisible()
{
	_visiblePatchCount = _visibleSortedPatchCount + _visibleRandomPatchCount;
	std::sort(_vSortedPatchIndices.begin(), _vSortedPatchIndices.begin() + _visibleSortedPatchCount, [this](int a, int b)
		{
			RcTerrainPatch patchA = GetPatch(a);
			RcTerrainPatch patchB = GetPatch(b);

			if (patchA._quadtreeLOD != patchB._quadtreeLOD)
				return patchA._quadtreeLOD < patchB._quadtreeLOD;

			return patchA._distToHeadSq < patchB._distToHeadSq;
		});
	if (_visibleRandomPatchCount)
		memcpy(&_vSortedPatchIndices[_visibleSortedPatchCount], _vRandomPatchIndices.data(), _visibleRandomPatchCount * sizeof(UINT16));
}

void Terrain::Draw(RcDrawDesc dd)
{
	VERUS_QREF_CONST_SETTINGS;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WM;

	if (!_visiblePatchCount)
		return;

	const bool drawingDepth = WorldManager::IsDrawingDepth(DrawDepth::automatic);

	const Transform3 matW = Transform3::identity();

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(160, 255, 96, 255), "Terrain/Draw");

	s_ubTerrainVS._matW = matW.UniformBufferFormat();
	s_ubTerrainVS._matWV = Transform3(wm.GetPassCamera()->GetMatrixV() * matW).UniformBufferFormat();
	s_ubTerrainVS._matV = wm.GetPassCamera()->GetMatrixV().UniformBufferFormat();
	s_ubTerrainVS._matVP = wm.GetPassCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubTerrainVS._matP = wm.GetPassCamera()->GetMatrixP().UniformBufferFormat();
	s_ubTerrainVS._headPos_invMapSide = float4(wm.GetHeadCamera()->GetEyePosition().GLM(), 0);
	s_ubTerrainVS._headPos_invMapSide.w = 1.f / _mapSide;
	s_ubTerrainVS._viewportSize = cb->GetViewportSize().GLM();
	s_ubTerrainFS._matWV = s_ubTerrainVS._matWV;
	VERUS_FOR(i, VERUS_COUNT_OF(_layerData))
	{
		s_ubTerrainFS._vDetailStrength[i >> 2][i & 0x3] = _layerData[i]._detailStrength;
		s_ubTerrainFS._vRoughStrength[i >> 2][i & 0x3] = _layerData[i]._roughStrength;
	}
	s_ubTerrainFS._lamScaleBias.x = _lamScale;
	s_ubTerrainFS._lamScaleBias.y = _lamBias;

	cb->BindVertexBuffers(_geo);
	cb->BindIndexBuffer(_geo);

	bool bindStrip = true;
	const int half = _mapSide >> 1;
	int firstInstance = 0;
	int edge = _visiblePatchCount - 1;
	int lod = _vPatches[_vSortedPatchIndices.front()]._quadtreeLOD;
	s_shader[SHADER_MAIN]->BeginBindDescriptors();
	VERUS_FOR(i, _visiblePatchCount)
	{
		RcTerrainPatch patch = _vPatches[_vSortedPatchIndices[i]];
		if (patch._quadtreeLOD != lod || i == edge)
		{
			if (i == edge)
				i++; // Drawing patches [firstInstance, i).

			if (!lod)
			{
				const bool tess = dd._allowTess && settings._gpuTessellation;
				if (dd._wireframe)
					cb->BindPipeline(_pipe[PIPE_WIREFRAME_LIST]);
				else if (drawingDepth)
					cb->BindPipeline(_pipe[tess ? PIPE_DEPTH_TESS : PIPE_DEPTH_LIST]);
				else
					cb->BindPipeline(_pipe[tess ? PIPE_TESS : PIPE_LIST]);
				cb->BindDescriptors(s_shader[SHADER_MAIN], 0, _cshVS);
				cb->BindDescriptors(s_shader[SHADER_MAIN], 1, _cshFS);
			}
			else if (bindStrip)
			{
				bindStrip = false;
				if (dd._wireframe)
					cb->BindPipeline(_pipe[PIPE_WIREFRAME_STRIP]);
				else if (drawingDepth)
					cb->BindPipeline(_pipe[PIPE_DEPTH_STRIP]);
				else
					cb->BindPipeline(_pipe[PIPE_STRIP]);
				cb->BindDescriptors(s_shader[SHADER_MAIN], 0, _cshVS);
				cb->BindDescriptors(s_shader[SHADER_MAIN], 1, _cshFS);
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
				VERUS_ZERO_MEM(_vInstanceBuffer[at]._layers);
				if (dd._wireframe)
				{
					switch (patchToDraw._usedChannelCount)
					{
					case 1: _vInstanceBuffer[at]._layers[0] = 0; _vInstanceBuffer[at]._layers[1] = 0; _vInstanceBuffer[at]._layers[2] = 1; break;
					case 2: _vInstanceBuffer[at]._layers[0] = 0; _vInstanceBuffer[at]._layers[1] = 1; _vInstanceBuffer[at]._layers[2] = 0; break;
					case 3: _vInstanceBuffer[at]._layers[0] = 1; _vInstanceBuffer[at]._layers[1] = 1; _vInstanceBuffer[at]._layers[2] = 0; break;
					case 4: _vInstanceBuffer[at]._layers[0] = 1; _vInstanceBuffer[at]._layers[1] = 0; _vInstanceBuffer[at]._layers[2] = 0; break;
					}
				}
				else
				{
					VERUS_FOR(ch, 4)
						_vInstanceBuffer[at]._layers[ch] = patchToDraw._layerForChannel[ch];
				}
			}

			cb->DrawIndexed(_lods[lod]._indexCount, instanceCount, _lods[lod]._firstIndex, 0, _instanceCount + firstInstance);

			lod = patch._quadtreeLOD;
			firstInstance = i;
		}
	}
	s_shader[SHADER_MAIN]->EndBindDescriptors();

	_geo->UpdateVertexBuffer(&_vInstanceBuffer[_instanceCount], 1, cb.Get(), _visiblePatchCount, _instanceCount);
	_instanceCount += _visiblePatchCount;

	VERUS_PROFILER_END_EVENT(cb);
}

void Terrain::DrawSimple(DrawSimpleMode mode)
{
	VERUS_QREF_ATMO;
	VERUS_QREF_CSMB;
	VERUS_QREF_RENDERER;
	VERUS_QREF_WATER;
	VERUS_QREF_WM;

	if (!_visiblePatchCount)
		return;

	const Transform3 matW = Transform3::identity();
	const float clipDistanceOffset = (water.IsUnderwater() || DrawSimpleMode::envMap == mode) ? static_cast<float>(USHRT_MAX) : 0.f;

	auto cb = renderer.GetCommandBuffer();

	VERUS_PROFILER_BEGIN_EVENT(cb, VERUS_COLOR_RGBA(160, 255, 64, 255), "Terrain/DrawSimple");

	const float occlusion = 0.1f;

	s_ubSimpleTerrainVS._matW = matW.UniformBufferFormat();
	s_ubSimpleTerrainVS._matVP = wm.GetPassCamera()->GetMatrixVP().UniformBufferFormat();
	s_ubSimpleTerrainVS._headPos = float4(wm.GetHeadCamera()->GetEyePosition().GLM(), 0);
	s_ubSimpleTerrainVS._eyePos = float4(wm.GetPassCamera()->GetEyePosition().GLM(), 0);
	s_ubSimpleTerrainVS._invMapSide_clipDistanceOffset.x = 1.f / _mapSide;
	s_ubSimpleTerrainVS._invMapSide_clipDistanceOffset.y = clipDistanceOffset;
	s_ubSimpleTerrainFS._ambientColor = float4(atmo.GetAmbientColor().GLM(), 0) * occlusion;
	s_ubSimpleTerrainFS._fogColor = Vector4(atmo.GetFogColor(), atmo.GetFogDensity()).GLM();
	s_ubSimpleTerrainFS._dirToSun = float4(atmo.GetDirToSun().GLM(), 0);
	s_ubSimpleTerrainFS._sunColor = float4(atmo.GetSunColor().GLM(), 0);
	s_ubSimpleTerrainFS._matShadow = csmb.GetShadowMatrix(0).UniformBufferFormat();
	s_ubSimpleTerrainFS._matShadowCSM1 = csmb.GetShadowMatrix(1).UniformBufferFormat();
	s_ubSimpleTerrainFS._matShadowCSM2 = csmb.GetShadowMatrix(2).UniformBufferFormat();
	s_ubSimpleTerrainFS._matShadowCSM3 = csmb.GetShadowMatrix(3).UniformBufferFormat();
	s_ubSimpleTerrainFS._matScreenCSM = csmb.GetScreenMatrixVP().UniformBufferFormat();
	s_ubSimpleTerrainFS._csmSliceBounds = csmb.GetSliceBounds().GLM();
	memcpy(&s_ubSimpleTerrainFS._shadowConfig, &csmb.GetConfig(), sizeof(s_ubSimpleTerrainFS._shadowConfig));

	cb->BindVertexBuffers(_geo);
	cb->BindIndexBuffer(_geo);

	bool bindStrip = true;
	const int half = _mapSide >> 1;
	int firstInstance = 0;
	int edge = _visiblePatchCount - 1;
	int lod = _vPatches[_vSortedPatchIndices.front()]._quadtreeLOD;
	s_shader[SHADER_SIMPLE]->BeginBindDescriptors();
	VERUS_FOR(i, _visiblePatchCount)
	{
		RcTerrainPatch patch = _vPatches[_vSortedPatchIndices[i]];
		if (patch._quadtreeLOD != lod || i == edge)
		{
			if (i == edge)
				i++; // Drawing patches [firstInstance, i).

			if (!lod)
			{
				if (DrawSimpleMode::envMap == mode)
					cb->BindPipeline(_pipe[PIPE_SIMPLE_ENV_MAP_LIST]);
				else if (water.IsUnderwater())
					cb->BindPipeline(_pipe[PIPE_SIMPLE_UNDERWATER_LIST]);
				else
					cb->BindPipeline(_pipe[PIPE_SIMPLE_PLANAR_REF_LIST]);
				cb->BindDescriptors(s_shader[SHADER_SIMPLE], 0, _cshSimpleVS);
				cb->BindDescriptors(s_shader[SHADER_SIMPLE], 1, _cshSimpleFS);
			}
			else if (bindStrip)
			{
				bindStrip = false;
				if (DrawSimpleMode::envMap == mode)
					cb->BindPipeline(_pipe[PIPE_SIMPLE_ENV_MAP_STRIP]);
				else if (water.IsUnderwater())
					cb->BindPipeline(_pipe[PIPE_SIMPLE_UNDERWATER_STRIP]);
				else
					cb->BindPipeline(_pipe[PIPE_SIMPLE_PLANAR_REF_STRIP]);
				cb->BindDescriptors(s_shader[SHADER_SIMPLE], 0, _cshSimpleVS);
				cb->BindDescriptors(s_shader[SHADER_SIMPLE], 1, _cshSimpleFS);
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
				VERUS_ZERO_MEM(_vInstanceBuffer[at]._layers);
				VERUS_FOR(ch, 4)
					_vInstanceBuffer[at]._layers[ch] = patchToDraw._layerForChannel[ch];
			}

			cb->DrawIndexed(_lods[lod]._indexCount, instanceCount, _lods[lod]._firstIndex, 0, _instanceCount + firstInstance);

			lod = patch._quadtreeLOD;
			firstInstance = i;
		}
	}
	s_shader[SHADER_SIMPLE]->EndBindDescriptors();

	_geo->UpdateVertexBuffer(&_vInstanceBuffer[_instanceCount], 1, cb.Get(), _visiblePatchCount, _instanceCount);
	_instanceCount += _visiblePatchCount;

	VERUS_PROFILER_END_EVENT(cb);
}

int Terrain::UserPtr_GetType()
{
	return +NodeType::terrain;
}

void Terrain::QuadtreeIntegral_OnElementDetected(const short ij[2], RcPoint3 center)
{
	VERUS_QREF_WM;

	const RcPoint3 headPos = wm.GetHeadCamera()->GetEyePosition();

	const Vector3 toCenter = center - headPos;
	const float dist = VMath::length(toCenter);

	const float lodF = log2(Math::Clamp((dist - 12) * (2 / 100.f), 1.f, 18.f));
	const int lod = Math::Clamp<int>(int(lodF), 0, 4);

	if (4 == lod && center.getY() <= -20) // Cull underwater patches:
		return;

	// Locate this patch:
	const int shiftPatch = _mapShift - 4;
	const int iPatch = ij[0] >> 4;
	const int jPatch = ij[1] >> 4;
	const int offsetPatch = (iPatch << shiftPatch) + jPatch;

	// Update this patch:
	RTerrainPatch patch = _vPatches[offsetPatch];
	patch._distToHeadSq = int(dist * dist);
	patch._patchHeight = ConvertHeight(center.getY());
	patch._quadtreeLOD = lod;

	if (4 == lod)
		_vRandomPatchIndices[_visibleRandomPatchCount++] = offsetPatch;
	else
		_vSortedPatchIndices[_visibleSortedPatchCount++] = offsetPatch;
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
	_quadtree.SetDistCoarseMode(true);
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
		const int rowOffset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			const int ij[] = { i, j };
			short h;
			GetHeightAt(ij, 0, &h);
			_vHeightBuffer[rowOffset + j] = h;
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
		throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
	pugi::xml_node root = doc.first_child();
	for (auto node : root.children("layer"))
	{
		const int id = node.attribute("id").as_int();
		InsertLayerUrl(id, node.attribute("url").value());
	}
	LoadLayerTextures();
}

void Terrain::LoadLayerTextures()
{
	if (_vLayerUrls.empty())
		return;

	VERUS_QREF_CSMB;
	VERUS_QREF_MM;
	VERUS_QREF_RENDERER;

	renderer->WaitIdle();

	_tex[TEX_LAYERS_X].Done();
	_tex[TEX_LAYERS_N].Done();
	_tex[TEX_LAYERS].Done();

	Vector<CSZ> vLayerUrls;
	vLayerUrls.resize(_vLayerUrls.size() + 1);
	VERUS_FOR(i, _vLayerUrls.size())
		vLayerUrls[i] = _C(_vLayerUrls[i]);

	CGI::TextureDesc texDesc;
	texDesc._urls = vLayerUrls.data();
	_tex[TEX_LAYERS].Init(texDesc);

	Vector<String> vLayerUrlsN, vLayerUrlsX;
	Vector<CSZ> vLayerUrlsNPtr, vLayerUrlsXPtr;
	vLayerUrlsN.reserve(vLayerUrls.size());
	vLayerUrlsX.reserve(vLayerUrls.size());
	vLayerUrlsNPtr.reserve(vLayerUrls.size());
	vLayerUrlsXPtr.reserve(vLayerUrls.size());
	for (CSZ name : vLayerUrls)
	{
		if (name)
		{
			String nameN = name;
			Str::ReplaceExtension(nameN, ".N.dds");
			vLayerUrlsN.push_back(nameN);
			vLayerUrlsNPtr.push_back(_C(vLayerUrlsN.back()));

			String nameX = name;
			Str::ReplaceExtension(nameX, ".X.dds");
			vLayerUrlsX.push_back(nameX);
			vLayerUrlsXPtr.push_back(_C(vLayerUrlsX.back()));
		}
	}
	vLayerUrlsNPtr.push_back(nullptr);
	vLayerUrlsXPtr.push_back(nullptr);

	texDesc._urls = vLayerUrlsNPtr.data();
	_tex[TEX_LAYERS_N].Init(texDesc);
	texDesc._urls = vLayerUrlsXPtr.data();
	_tex[TEX_LAYERS_X].Init(texDesc);

	s_shader[SHADER_MAIN]->FreeDescriptorSet(_cshFS);
	_cshFS = s_shader[SHADER_MAIN]->BindDescriptorSetTextures(1,
		{
			_tex[TEX_NORMALS],
			_tex[TEX_BLEND],
			_tex[TEX_LAYERS],
			_tex[TEX_LAYERS_N],
			_tex[TEX_LAYERS_X],
			mm.GetDetailTexture(),
			mm.GetDetailNTexture(),
			mm.GetStrassTexture()
		});
	s_shader[SHADER_SIMPLE]->FreeDescriptorSet(_cshSimpleFS);
	_cshSimpleFS = s_shader[SHADER_SIMPLE]->BindDescriptorSetTextures(1,
		{
			_tex[TEX_BLEND],
			_tex[TEX_LAYERS],
			_tex[TEX_LAYERS_X],
			csmb.GetTexture()
		});
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
	VERUS_FOR(ch, 4)
	{
		const int weight = (ch != 3) ? rgba[ch] : 0xFF - rgba[0] - rgba[1] - rgba[2];
		if (weight > maxSplat)
		{
			maxSplat = weight;
			maxSplatChannel = ch;
		}
	}

	_vPatches[offsetPatch]._mainLayer[(iLocal << 4) + jLocal] =
		_vPatches[offsetPatch]._layerForChannel[maxSplatChannel];
}

void Terrain::UpdateHeightmapTexture()
{
	_vHeightmapSubresData.resize(_mapSide * _mapSide);
	const int mipLevels = Math::ComputeMipLevels(_mapSide, _mapSide);
	VERUS_FOR(lod, mipLevels)
	{
		const int side = _mapSide >> lod;
		const int step = _mapSide / side;
		VERUS_P_FOR(i, side)
		{
			const int rowOffset = i * side;
			VERUS_FOR(j, side)
			{
				const int ij[] = { i * step, j * step };
				short h;
				GetHeightAt(ij, 0, &h);
				_vHeightmapSubresData[rowOffset + j] = Convert::FloatToHalf(static_cast<float>(h - 3));
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
				static_cast<BYTE>(nrm[0] + 127),
				static_cast<BYTE>(nrm[2] + 127),
				static_cast<BYTE>(tan[1] + 127),
				static_cast<BYTE>(tan[2] + 127)
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
		const int rowOffset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			const int ij[] = { i, j };
			_vMainLayerSubresData[rowOffset + j] = GetMainLayerAt(ij) * 16 + 4;
		}
	});
	_tex[TEX_MAIN_LAYER]->UpdateSubresource(_vMainLayerSubresData.data());
}

CGI::TexturePtr Terrain::GetMainLayerTexture() const
{
	return _tex[TEX_MAIN_LAYER];
}

void Terrain::ComputeOcclusion()
{
#ifdef _DEBUG
	return;
#endif

	const int mapEdge = _mapSide - 1;
	const int radius = 48;
	const int radiusSq = radius * radius;
	VERUS_P_FOR(i, _mapSide)
	{
		VERUS_FOR(j, _mapSide)
		{
			if (!(i & 0xF) || !(j & 0xF))
				continue;

			const int ij[] = { i, j };
			const float originHeight = GetHeightAt(ij);
			glm::vec3 originNormal;
			Convert::Sint8ToSnorm(GetNormalAt(ij), &originNormal.x, 3);

			const int cosSampleCount = 24;
			float cosSamples[cosSampleCount] = {};
			//std::fill(cosSamples, cosSamples + cosSampleCount, 1000.f); // For testing.

			// Kernel:
			for (int di = -radius; di <= radius; di += 6)
			{
				const int ki = i + di;
				if (ki < 0 || ki >= _mapSide)
					continue;
				for (int dj = -radius; dj <= radius; dj += 6)
				{
					const int kj = j + dj;
					if (kj < 0 || kj >= _mapSide)
						continue;

					if (!di && !dj)
						continue; // Skip origin.
					const int lenSq = di * di + dj * dj;
					if (lenSq > radiusSq)
						continue;

					const int ij[] = { ki, kj };
					const float kernelHeight = GetHeightAt(ij);
					const glm::vec3 to(dj, kernelHeight - originHeight, di);
					const glm::vec3 dir = glm::normalize(to);
					const int index = Math::Clamp<int>(static_cast<int>((atan2(dir.z, dir.x) + VERUS_PI) / VERUS_2PI * cosSampleCount), 0, cosSampleCount - 1);
					cosSamples[index] = Math::Max(cosSamples[index], Math::Clamp<float>(glm::dot(originNormal, dir), 0, 1));
				}
			}

			// Fake ambient occlusion for dramatic look:
			float sum = 0;
			VERUS_FOR(index, cosSampleCount)
				sum += cosSamples[index];
			const float averageCosine = sum / cosSampleCount;
			const float oneMinusAverageCosine = 1 - averageCosine;
			const float occlusion = oneMinusAverageCosine * oneMinusAverageCosine * oneMinusAverageCosine;

			const BYTE occlusion8 = Convert::UnormToUint8(occlusion);

			BYTE* rgba = reinterpret_cast<BYTE*>(&_vBlendBuffer[(i << _mapShift) + j]);
			rgba[3] = occlusion8;

			const bool iExtend = ((15 == (i & 0xF)) && (i != mapEdge));
			const bool jExtend = ((15 == (j & 0xF)) && (j != mapEdge));
			if (iExtend)
			{
				BYTE* rgba = reinterpret_cast<BYTE*>(&_vBlendBuffer[((i + 1) << _mapShift) + j]);
				rgba[3] = occlusion8;
			}
			if (jExtend)
			{
				BYTE* rgba = reinterpret_cast<BYTE*>(&_vBlendBuffer[(i << _mapShift) + (j + 1)]);
				rgba[3] = occlusion8;
			}
			if (iExtend && jExtend)
			{
				BYTE* rgba = reinterpret_cast<BYTE*>(&_vBlendBuffer[((i + 1) << _mapShift) + (j + 1)]);
				rgba[3] = occlusion8;
			}
		}
	});
}

void Terrain::UpdateOcclusion(Forest* pForest)
{
	const int patchSide = _mapSide >> 4;
	const int patchShift = _mapShift - 4;
	VERUS_P_FOR(ip, patchSide)
	{
		const int rowOffset = ip << patchShift;
		VERUS_FOR(jp, patchSide)
		{
			RcTerrainPatch patch = _vPatches[rowOffset + jp];
			VERUS_FOR(i, 16)
			{
				VERUS_FOR(j, 16)
				{
					const int ij[] = { (ip << 4) + i, (jp << 4) + j };
					const int fade = 0xFF - (Math::Max<int>(0, GetNormalAt(ij)[1]) << 1);
					BYTE* rgba = reinterpret_cast<BYTE*>(&_vBlendBuffer[(ij[0] << _mapShift) + ij[1]]);
					BYTE occlusion = 0;
					VERUS_FOR(ch, 4)
					{
						const int layer = patch._layerForChannel[ch];
						const int weight = (ch != 3) ? rgba[ch] : 0xFF - rgba[0] - rgba[1] - rgba[2];
						if (weight > 0)
							occlusion += pForest->GetOcclusionAt(ij, layer) * weight / 0xFF;
					}
					rgba[3] = Math::CombineOcclusion(rgba[3], Math::Max(occlusion, static_cast<BYTE>(fade)));
				}
			}
		}
	});
	UpdateBlendTexture();
}

void Terrain::OnHeightModified()
{
	_quadtree.Done();
	_quadtree.Init(_mapSide, 16, this, _quadtreeFatten);
	_quadtree.SetDistCoarseMode(true);

	UpdateHeightBuffer();
	UpdateHeightmapTexture();
	UpdateNormalsTexture();
}

void Terrain::AddNewRigidBody()
{
	_physics.Done();
	_physics.Init(this, _mapSide, _mapSide, _vHeightBuffer.data(), ConvertHeight(static_cast<short>(1)));
}

void Terrain::Serialize(IO::RSeekableStream stream)
{
	stream.WriteString(_C(std::to_string(_mapSide)));

	// Tiles (height):
	VERUS_FOR(i, _mapSide)
	{
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
	BYTE layerCount = Utils::Cast32(_vLayerUrls.size());
	stream << layerCount;
	VERUS_FOR(i, layerCount)
	{
		stream.WriteString(_C(_vLayerUrls[i]));
		stream << _layerData[i]._detailStrength;
		stream << _layerData[i]._roughStrength;
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
		const int rowOffset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			const UINT32 b = _vBlendBuffer[rowOffset + j];
			const UINT16 data = Convert::Uint8x4ToUint4x4(b);
			stream << data;
		}
	}
}

void Terrain::Deserialize(IO::RStream stream)
{
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
		stream >> _layerData[i]._detailStrength;
		stream >> _layerData[i]._roughStrength;
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
				_vPatches[i]._layerForChannel[j] = 0;
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
		const int rowOffset = i << _mapShift;
		VERUS_FOR(j, _mapSide)
		{
			UINT16 data;
			stream >> data;
			_vBlendBuffer[rowOffset + j] = Convert::Uint4x4ToUint8x4(data, true);
			const int ij[] = { i, j };
			UpdateMainLayerAt(ij);
		}
	}
	ComputeOcclusion();
	UpdateBlendTexture();
	UpdateMainLayerTexture();
	// </Blend>

	OnHeightModified();
	AddNewRigidBody();
}
