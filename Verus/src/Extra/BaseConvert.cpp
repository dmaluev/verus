// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Extra;

// BaseConvert::Mesh::UberVertex:

BaseConvert::Mesh::UberVertex::UberVertex()
{
	VERUS_ZERO_MEM(_bw);
	VERUS_ZERO_MEM(_bi);
}

bool BaseConvert::Mesh::UberVertex::operator==(RcUberVertex that) const
{
	const float e = 0.001f;
	return
		glm::all(glm::epsilonEqual(_pos, that._pos, e)) &&
		glm::all(glm::epsilonEqual(_nrm, that._nrm, e)) &&
		glm::all(glm::epsilonEqual(_tc0, that._tc0, e)) &&
		glm::all(glm::epsilonEqual(_tc1, that._tc1, e));
}

void BaseConvert::Mesh::UberVertex::Add(float weight, UINT32 index)
{
	if (_currentWeight < 4) // Space for one more?
	{
		_bw[_currentWeight] = weight;
		_bi[_currentWeight] = index;
		_currentWeight++;
	}
	else // No space left - check the smallest weight:
	{
		float* pMin = std::min_element(_bw, _bw + 4);
		if (weight > *pMin)
		{
			const int i = Utils::Cast32(std::distance(_bw, pMin));
			_bw[i] = weight;
			_bi[i] = index;
		}
	}
}

void BaseConvert::Mesh::UberVertex::CompileBits(UINT32& ww, UINT32& ii) const
{
	ww = 0;
	ii = 0;

	int ibw[4] = {};
	VERUS_FOR(i, _currentWeight)
		ibw[i] = glm::clamp(int(_bw[i] * 255 + 0.5f), 0, 255);
	ibw[0] = glm::clamp(255 - ibw[1] - ibw[2] - ibw[3], 0, 255);

	// Make sure that the sum is 255:
	int at = 0;
	int sum = ibw[0] + ibw[1] + ibw[2] + ibw[3];
	while (sum > 255)
	{
		if (ibw[at] > 0)
		{
			sum--;
			ibw[at]--;
		}
		at = (at + 1) & 0x3;
	}

	VERUS_FOR(i, _currentWeight)
	{
		ww |= ibw[i] << (i << 3);
		ii |= _bi[i] << (i << 3);
	}
}

UINT32 BaseConvert::Mesh::UberVertex::GetDominantIndex()
{
	_bw[0] = 1 - _bw[1] - _bw[2] - _bw[3];
	const int i = Utils::Cast32(std::distance(_bw, std::max_element(_bw, _bw + 4)));
	return _bi[i];
}

// BaseConvert::Mesh::Aabb:

void BaseConvert::Mesh::Aabb::Reset()
{
	_mn = glm::vec3(+FLT_MAX);
	_mx = glm::vec3(-FLT_MAX);
}

void BaseConvert::Mesh::Aabb::Include(const glm::vec3& point)
{
	_mn = glm::min(_mn, point);
	_mx = glm::max(_mx, point);
}

glm::vec3 BaseConvert::Mesh::Aabb::GetExtents() const
{
	return (_mx - _mn) * 0.5f;
}

// BaseConvert::Mesh:

BaseConvert::Mesh::Mesh(BaseConvert* pBaseConvert) : _pBaseConvert(pBaseConvert)
{
	_matBoneAxis = glm::rotate(glm::radians(-90.f), glm::vec3(0, 0, 1));
	VERUS_FOR(i, 4)
	{
		VERUS_FOR(j, 4)
			_matBoneAxis[i][j] = glm::round(_matBoneAxis[i][j]);
	}
}

BaseConvert::Mesh::~Mesh()
{
}

BaseConvert::Mesh::PBone BaseConvert::Mesh::FindBone(CSZ name)
{
	for (auto& bone : _vBones)
	{
		if (bone._name == name)
			return &bone;
	}
	return nullptr;
}

void BaseConvert::Mesh::Optimize()
{
	{
		StringStream ss;
		ss << _name << ": Optimize";
		_pBaseConvert->OnProgressText(_C(ss.str()));
	}

	// Weld vertices, remove duplicates:
	int similarVertCount = 0;
	int degenerateFaceCount = 0;
	Vector<UberVertex> vVbOpt; // This will not contain equal vertices.
	Vector<Face> vIbOpt; // This can get smaller than current IB, if there are zero-area triangles.
	vVbOpt.reserve(_vertCount);
	vIbOpt.reserve(_faceCount);
	VERUS_FOR(face, _faceCount) // For each triangle:
	{
		_pBaseConvert->OnProgress(float(face) / _faceCount * 100);
		Face newFace;
		int pushedCount = 0;
		VERUS_FOR(i, 3) // For each vertex in triangle:
		{
			RcUberVertex test = _vUberVerts[_vFaces[face]._indices[i]]; // Fetch vertex.
			const auto similar = std::find(vVbOpt.begin(), vVbOpt.end(), test);
			const int index = (similar == vVbOpt.end()) ? -1 : Utils::Cast32(std::distance(vVbOpt.begin(), similar));
			if (index < 0) // No similar vertex found.
			{
				newFace._indices[i] = Utils::Cast32(vVbOpt.size());
				vVbOpt.push_back(test);
				pushedCount++;
			}
			else
			{
				newFace._indices[i] = index;
				similarVertCount++;
			}
		}
		if (newFace._indices[0] != newFace._indices[1] &&
			newFace._indices[1] != newFace._indices[2] &&
			newFace._indices[2] != newFace._indices[0])
		{
			vIbOpt.push_back(newFace); // Add triangle, if it's area is more than zero.
		}
		else
		{
			degenerateFaceCount++;
			vVbOpt.resize(vVbOpt.size() - pushedCount); // Rollback.
		}
	}
	_vUberVerts.assign(vVbOpt.begin(), vVbOpt.end());
	_vFaces.assign(vIbOpt.begin(), vIbOpt.end());
	_vertCount = Utils::Cast32(_vUberVerts.size());
	_faceCount = Utils::Cast32(_vFaces.size());
	StringStream ssWeldReport;
	ssWeldReport << "Weld report: " << similarVertCount << " similar vertices removed, " << degenerateFaceCount << " degenerate faces removed";
	_pBaseConvert->OnProgressText(_C(ssWeldReport.str()));

	{
		StringStream ss;
		ss << _name << ": (" << _vertCount << " vertices, " << _faceCount << " faces)";
		_pBaseConvert->OnProgressText(_C(ss.str()));
	}

	// Optimize (using AMD Tootle):
	StringStream ss2;
	ss2 << _name << ": Optimize (Tootle)";
	_pBaseConvert->OnProgressText(_C(ss2.str()));
	if (_pBaseConvert->_pDelegate)
		_pBaseConvert->_pDelegate->BaseConvert_Optimize(_vUberVerts, _vFaces);
}

void BaseConvert::Mesh::ComputeTangentSpace()
{
	StringStream ss;
	ss << _name << ": ComputeTangentSpace";
	_pBaseConvert->OnProgressText(_C(ss.str()));

	Vector<glm::vec3> vPos, vN, vTan, vBin;
	vPos.resize(_vertCount);

	Vector<glm::vec2> vTc0;
	vTc0.resize(_vertCount);
	VERUS_FOR(i, _vertCount)
	{
		vPos[i] = _vUberVerts[i]._pos;
		vTc0[i] = _vUberVerts[i]._tc0;
	}

	Vector<UINT16> vIndices;
	vIndices.resize(_faceCount * 3);
	VERUS_FOR(i, _faceCount)
	{
		vIndices[i * 3 + 0] = _vFaces[i]._indices[0];
		vIndices[i * 3 + 1] = _vFaces[i]._indices[1];
		vIndices[i * 3 + 2] = _vFaces[i]._indices[2];
	}

	if (_found & Found::normals)
	{
		vN.resize(_vertCount);
		VERUS_FOR(i, _vertCount)
			vN[i] = _vUberVerts[i]._nrm;
	}
	else
	{
		Math::NormalComputer::ComputeNormals(vIndices, vPos, vN, _pBaseConvert->UseAreaBasedNormals());
	}

	_pBaseConvert->OnProgress(50);
	Math::NormalComputer::ComputeTangentSpace(vIndices, vPos, vN, vTc0, vTan, vBin);

	_vZipNormal.resize(_vertCount);
	_vZipTan.resize(_vertCount);
	_vZipBin.resize(_vertCount);
	VERUS_FOR(i, _vertCount) // Compress them all:
	{
		Convert::SnormToSint8(&vN[i].x, &_vZipNormal[i]._x, 3);
		Convert::SnormToSint8(&vTan[i].x, &_vZipTan[i]._x, 3);
		Convert::SnormToSint8(&vBin[i].x, &_vZipBin[i]._x, 3);
	}
}

void BaseConvert::Mesh::Compress()
{
	StringStream ss;
	ss << _name << ": Compress";
	_pBaseConvert->OnProgressText(_C(ss.str()));

	Mesh::Aabb aabb;
	glm::vec3 extents, scale, bias;

	// Compress position:
	VERUS_FOR(i, _vertCount)
		aabb.Include(_vUberVerts[i]._pos);
	extents = aabb.GetExtents();
	Scene::BaseMesh::ComputeDeq(_posScale, _posBias, extents, aabb._mn);
	_vZipPos.reserve(_vertCount);
	VERUS_FOR(i, _vertCount)
	{
		Mesh::Vec3Short pos;
		glm::vec3 v(_vUberVerts[i]._pos);
		Scene::BaseMesh::QuantizeV(v, extents, aabb._mn);
		pos._x = short(v.x);
		pos._y = short(v.y);
		pos._z = short(v.z);
		_vZipPos.push_back(pos);
	}

	// Compress tc0:
	_pBaseConvert->OnProgress(50);
	aabb.Reset();
	VERUS_FOR(i, _vertCount)
		aabb.Include(glm::vec3(_vUberVerts[i]._tc0, 0));
	extents = aabb.GetExtents();
	Scene::BaseMesh::ComputeDeq(scale, bias, extents, aabb._mn);
	_tc0Scale = glm::vec2(scale);
	_tc0Bias = glm::vec2(bias);
	_vZipTc0.reserve(_vertCount);
	VERUS_FOR(i, _vertCount)
	{
		Mesh::Vec2Short tc;
		glm::vec3 v(_vUberVerts[i]._tc0, 0);
		Scene::BaseMesh::QuantizeV(v, extents, aabb._mn);
		tc._u = short(v.x);
		tc._v = short(v.y);
		_vZipTc0.push_back(tc);
	}

	if (!(_found & Found::texcoordExtra))
		return;

	// Compress tc1:
	_pBaseConvert->OnProgress(75);
	aabb.Reset();
	VERUS_FOR(i, _vertCount)
		aabb.Include(glm::vec3(_vUberVerts[i]._tc1, 0));
	glm::vec2 normBias(-aabb._mn);
	glm::vec2 normScale(aabb._mx - aabb._mn);
	normScale.x = 1 / normScale.x;
	normScale.y = 1 / normScale.y;
	aabb.Reset();
	VERUS_FOR(i, _vertCount)
	{
		_vUberVerts[i]._tc1 = (_vUberVerts[i]._tc1 + normBias) * normScale;
		aabb.Include(glm::vec3(_vUberVerts[i]._tc1, 0));
	}
	extents = aabb.GetExtents();
	Scene::BaseMesh::ComputeDeq(scale, bias, extents, aabb._mn);
	_tc1Scale = glm::vec2(scale);
	_tc1Bias = glm::vec2(bias);
	_vZipTc1.reserve(_vertCount);
	VERUS_FOR(i, _vertCount)
	{
		Mesh::Vec2Short tc;
		glm::vec3 v((_vUberVerts[i]._tc1), 0);
		Scene::BaseMesh::QuantizeV(v, extents, aabb._mn);
		tc._u = short(v.x);
		tc._v = short(v.y);
		_vZipTc1.push_back(tc);
	}
}

void BaseConvert::Mesh::SerializeX3D3(IO::RFile file)
{
	if (!(_found & Found::vertsAndUVs))
		return;

	Optimize();
	ComputeTangentSpace();
	Compress();

	file.WriteText("<X3D>");

	file.WriteText(VERUS_CRNL VERUS_CRNL "<VN>");
	file.WriteString("3.0");

	file.WriteText(VERUS_CRNL VERUS_CRNL "<IX>");
	file.BeginBlock();
	file.WriteString(_C(std::to_string(_faceCount)));
	VERUS_FOR(i, _faceCount)
		file.Write(&_vFaces[i], 6);
	file.EndBlock();

	file.WriteText(VERUS_CRNL VERUS_CRNL "<VX>");
	file.BeginBlock();
	file.WriteString(_C(std::to_string(_vertCount)));
	file.Write(&_posScale, 12);
	file.Write(&_posBias, 12);
	VERUS_FOR(i, _vertCount)
		file.Write(&_vZipPos[i], 6);
	file.EndBlock();

	file.WriteText(VERUS_CRNL VERUS_CRNL "<NL>");
	file.BeginBlock();
	VERUS_FOR(i, _vertCount)
		file.Write(&_vZipNormal[i], 3);
	file.EndBlock();

	file.WriteText(VERUS_CRNL VERUS_CRNL "<TS>");
	file.BeginBlock();
	VERUS_FOR(i, _vertCount)
	{
		file.Write(&_vZipTan[i], 3);
		file.Write(&_vZipBin[i], 3);
	}
	file.EndBlock();

	file.WriteText(VERUS_CRNL VERUS_CRNL "<T0>");
	file.BeginBlock();
	file.Write(&_tc0Scale, 8);
	file.Write(&_tc0Bias, 8);
	VERUS_FOR(i, _vertCount)
		file.Write(&_vZipTc0[i], 4);
	file.EndBlock();

	if (_found & Found::texcoordExtra)
	{
		file.WriteText(VERUS_CRNL VERUS_CRNL "<T1>");
		file.BeginBlock();
		file.Write(&_tc1Scale, 8);
		file.Write(&_tc1Bias, 8);
		VERUS_FOR(i, _vertCount)
			file.Write(&_vZipTc1[i], 4);
		file.EndBlock();
	}

	if (_boneCount > 0)
	{
		// Must be the same order as in X file! Do not sort.
		file.WriteText(VERUS_CRNL VERUS_CRNL "<BH>");
		file.BeginBlock();
		const BYTE boneInfo = BYTE(_boneCount);
		file << boneInfo;
		VERUS_FOR(i, _boneCount)
		{
			file.WriteString(_C(_vBones[i]._name));
			file.WriteString(_C(_vBones[i]._parentName));
			file.Write(&_vBones[i]._matOffset, 64);
		}
		file.EndBlock();

		StringStream ssDebug;
		VERUS_FOR(i, _boneCount)
			ssDebug << "Bone: " << _vBones[i]._name << "; Parent: " << _vBones[i]._parentName << ";" VERUS_CRNL;
		String pathname = _C(_pBaseConvert->GetPathname());
		Str::ReplaceExtension(pathname, "_Bones.txt");
		IO::File fileDebug;
		if (fileDebug.Open(_C(pathname), "wb"))
			fileDebug.Write(_C(ssDebug.str()), ssDebug.str().length());

		if (!_pBaseConvert->UseRigidBones())
		{
			file.WriteText(VERUS_CRNL VERUS_CRNL "<SS>");
			file.BeginBlock();
			UINT32 ww, ii;
			VERUS_FOR(i, _vertCount)
			{
				_vUberVerts[i].CompileBits(ww, ii);
				file << ww << ii;
			}
			file.EndBlock();
		}
		else
		{
			file.WriteText(VERUS_CRNL VERUS_CRNL "<SR>");
			file.BeginBlock();
			VERUS_FOR(i, _vertCount)
				file << BYTE(_vUberVerts[i].GetDominantIndex());
			file.EndBlock();
		}
	}

	StringStream ss;
	ss << _name << ": Done";
	_pBaseConvert->OnProgressText(_C(ss.str()));
}

bool BaseConvert::Mesh::IsCopyOf(RMesh that)
{
	if (_vertCount != that._vertCount)
		return false;
	if (!std::equal(_vUberVerts.begin(), _vUberVerts.end(), that._vUberVerts.begin(), [](RcUberVertex a, RcUberVertex b)
		{
			const glm::vec3 dpos = a._pos - b._pos;
			const glm::vec2 dtc0 = a._tc0 - b._tc0;
			const float d = 0.01f * 0.01f;
			return glm::dot(dpos, dpos) < d && glm::dot(dtc0, dtc0) < d;
		}))
		return false;
		_copyOf = that._name;
		return true;
}

// BaseConvert:

BaseConvert::BaseConvert()
{
}

BaseConvert::~BaseConvert()
{
}

void BaseConvert::OnProgress(float percent)
{
	if (_pDelegate)
		_pDelegate->BaseConvert_OnProgress(percent);
}

void BaseConvert::OnProgressText(CSZ txt)
{
	if (_pDelegate)
		_pDelegate->BaseConvert_OnProgressText(txt);
}
