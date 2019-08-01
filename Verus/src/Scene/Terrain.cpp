#include "verus.h"

using namespace verus;
using namespace verus::Scene;

CGI::ShaderPwn        Terrain::s_shader;
Terrain::UB_DrawDepth Terrain::s_ubDrawDepth;

// TerrainLOD:

void TerrainLOD::Init(int sidePoly, int step)
{
	_side = sidePoly + 1;
	_numVerts = _side * _side;
	if (1 == step)
		Math::BuildListGrid(sidePoly, sidePoly, _vIB);
	else
		Math::BuildStripGrid(sidePoly, sidePoly, _vIB);
	_numIndices = Utils::Cast32(_vIB.size());
	_startIndex = 0;
}

void TerrainLOD::InitGeo(short* pV, UINT16* pI, int baseVertex)
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
			pV[((offset + j) << 2) + 1] = 0;
			pV[((offset + j) << 2) + 2] = i * step;
			pV[((offset + j) << 2) + 3] = 0;

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
		_vIB[i] += baseVertex;
	memcpy(pI, _vIB.data(), _numIndices * sizeof(UINT16));
	VERUS_FOR(i, _vIB.size())
		_vIB[i] -= baseVertex;
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
	CGI::ShaderDesc shaderDesc;
	shaderDesc._url = "[Shaders]:DS_Terrain.hlsl";
	s_shader.Init(shaderDesc);
	s_shader->CreateDescriptorSet(0, &s_ubDrawDepth, sizeof(s_ubDrawDepth));
	s_shader->CreatePipelineLayout();
}

void Terrain::DoneStatic()
{
	s_shader.Done();
}

void Terrain::Init()
{
	VERUS_INIT();

	// Init LODs:
	VERUS_FOR(i, VERUS_ARRAY_LENGTH(_lods))
		_lods[i].Init(16 >> i, 1 << i);

	Vector<short> vVB;
	Vector<UINT16> vIB;
	int vbSize = 0, ibSize = 0;
	VERUS_FOR(i, VERUS_ARRAY_LENGTH(_lods))
	{
		vbSize += _lods[i]._numVerts * 4;
		ibSize += _lods[i]._numIndices;
	}
	vVB.resize(vbSize);
	vIB.resize(ibSize);
	vbSize = 0, ibSize = 0;
	VERUS_FOR(i, VERUS_ARRAY_LENGTH(_lods))
	{
		_lods[i].InitGeo(&vVB[vbSize * 4], &vIB[ibSize], vbSize);
		_lods[i]._startIndex = ibSize;
		vbSize += _lods[i]._numVerts;
		ibSize += _lods[i]._numIndices;
	}

	const CGI::InputElementDesc ied[] =
	{
		{0, 0,                                     CGI::IeType::_short, 4, CGI::IeUsage::position, 0},
		{-1, offsetof(PerInstanceData, _posPatch), CGI::IeType::_short, 4, CGI::IeUsage::texCoord, 8},
		{-1, offsetof(PerInstanceData, _layers),   CGI::IeType::_short, 4, CGI::IeUsage::texCoord, 9},
		CGI::InputElementDesc::End()
	};
	CGI::GeometryDesc geoDesc;
	geoDesc._pInputElementDesc = ied;
	const int strides[] = { sizeof(short) * 4, sizeof(PerInstanceData), 0 };
	geoDesc._pStrides = strides;
	_geo.Init(geoDesc);
	_geo->CreateVertexBuffer(vbSize, 0);
	_geo->CreateVertexBuffer(1000, 1);
	_geo->CreateIndexBuffer(ibSize);
	_geo->UpdateVertexBuffer(vVB.data(), 0);
	_geo->UpdateIndexBuffer(vIB.data());
}

void Terrain::Done()
{
	VERUS_DONE(Terrain);
}

void Terrain::Layout()
{
}

void Terrain::Draw(Scene::RcCamera cam)
{
	s_ubDrawDepth._matW = mataff();
	s_ubDrawDepth._matWV = cam.GetMatrixV().UniformBufferFormat();
	s_ubDrawDepth._matVP = cam.GetMatrixVP().UniformBufferFormat();
}
