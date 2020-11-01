// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Scene;

BaseMesh::BaseMesh()
{
	VERUS_ZERO_MEM(_posDeq);
	VERUS_ZERO_MEM(_tc0Deq);
	VERUS_ZERO_MEM(_tc1Deq);
	VERUS_CT_ASSERT(16 == sizeof(VertexInputBinding0));
	VERUS_CT_ASSERT(16 == sizeof(VertexInputBinding1));
	VERUS_CT_ASSERT(16 == sizeof(VertexInputBinding2));
	VERUS_CT_ASSERT(8 == sizeof(VertexInputBinding3));
}

BaseMesh::~BaseMesh()
{
	Done();
}

void BaseMesh::Init(CSZ url)
{
	VERUS_INIT();
	_url = url;
	IO::Async::I().Load(url, this);
}

void BaseMesh::Done()
{
	IO::Async::Cancel(this);
	DoneShape();
	VERUS_DONE(BaseMesh);
}

void BaseMesh::Async_Run(CSZ url, RcBlob blob)
{
	if (_url == url)
	{
		Load(blob);
		LoadPrimaryBones();
		LoadRig();
		LoadWarp();
		return;
	}
}

void BaseMesh::Load(RcBlob blob)
{
	IO::StreamPtr sp(blob);
	UINT32 magic = 0;
	sp >> magic;
	if (magic != ' D3X')
	{
		if (magic == 'D3X<')
		{
			return LoadX3D3(blob);
		}
		else
		{
			throw VERUS_RECOVERABLE << "Load(), invalid magic number";
		}
	}
	VERUS_LOG_WARN("Load(), Old X3D version");
}

void BaseMesh::LoadX3D3(RcBlob blob)
{
	IO::StreamPtr sp(blob);

	bool hasNormals = false;
	bool hasTBN = false;
	UINT32 temp;
	UINT32 blockType = 0;
	INT64 blockSize = 0;
	char buffer[IO::Stream::s_bufferSize] = {};
	sp.Read(buffer, 5);
	sp >> temp;
	sp >> temp;
	sp.ReadString(buffer);
	VERUS_RT_ASSERT(!strcmp(buffer, "3.0"));

	while (!sp.IsEnd())
	{
		sp >> temp;
		sp >> blockType;
		sp >> blockSize;
		switch (blockType)
		{
		case '>XI<':
		{
			sp.ReadString(buffer);
			_faceCount = atoi(buffer);
			_indexCount = _faceCount * 3;
			VERUS_RT_ASSERT(_vIndices.empty());
			_vIndices.resize(_indexCount);
			UINT16* pIB = _vIndices.data();
			VERUS_FOR(i, _faceCount)
				sp.Read(&pIB[i * 3], 6);
		}
		break;
		case '>XV<':
		{
			sp.ReadString(buffer);
			_vertCount = atoi(buffer);
#ifdef VERUS_RELEASE_DEBUG
			Vector<UINT64> vPolyCheck;
			vPolyCheck.reserve(_faceCount);
			VERUS_FOR(i, _faceCount)
			{
				UINT16 indices[3];
				memcpy(indices, &_vIndices[i * 3], sizeof(indices));
				std::sort(indices, indices + 3);
				const UINT64 tag = UINT64(indices[0]) | UINT64(indices[1]) << 16 | UINT64(indices[2]) << 32;
				vPolyCheck.push_back(tag);
			}
			std::sort(vPolyCheck.begin(), vPolyCheck.end());
			if (vPolyCheck.end() != std::adjacent_find(vPolyCheck.begin(), vPolyCheck.end()))
				VERUS_LOG_WARN("Duplicate triangle in " << _url);
			Vector<UINT16> vSorted;
			vSorted.assign(_vIndices.begin(), _vIndices.end());
			std::sort(vSorted.begin(), vSorted.end());
			bool isolatedVertex = false;
			if (vSorted.end() != std::adjacent_find(vSorted.begin(), vSorted.end(), [](const UINT16& a, const UINT16& b) {return b - a > 1; }))
				isolatedVertex = true;
			if (isolatedVertex)
				VERUS_LOG_WARN("Isolated vertex in " << _url);
			if (_vertCount <= vSorted.back())
				VERUS_LOG_WARN("Index out of bounds in " << _url);
#endif
			sp.Read(&_posDeq[0], 12);
			sp.Read(&_posDeq[3], 12);
			VERUS_RT_ASSERT(_vBinding0.empty());
			_vBinding0.resize(_vertCount);
			VertexInputBinding0* pVB = _vBinding0.data();
			short mn = 0, mx = 0;
			VERUS_FOR(i, _vertCount)
			{
				sp.Read(pVB[i]._pos, 6);
#ifdef VERUS_RELEASE_DEBUG
				const short* pMin = std::min_element(pVB[i]._pos, pVB[i]._pos + 3);
				const short* pMax = std::max_element(pVB[i]._pos, pVB[i]._pos + 3);
				mn = Math::Min(mn, *pMin);
				mx = Math::Max(mx, *pMax);
#endif
			}
#ifdef VERUS_RELEASE_DEBUG
			VERUS_RT_ASSERT(mn < -32700);
			VERUS_RT_ASSERT(mx > +32700);
#endif
		}
		break;
		case '>LN<':
		{
			hasNormals = true;
			VertexInputBinding0* pVB = _vBinding0.data();
			VERUS_FOR(i, _vertCount)
			{
				sp.Read(pVB[i]._nrm, 3);
#ifdef VERUS_RELEASE_DEBUG
				const int len =
					pVB[i]._nrm[0] * pVB[i]._nrm[0] +
					pVB[i]._nrm[1] * pVB[i]._nrm[1] +
					pVB[i]._nrm[2] * pVB[i]._nrm[2];
				VERUS_RT_ASSERT(len > 125 * 125);
#endif
				Convert::ToDeviceNormal(pVB[i]._nrm, pVB[i]._nrm);
			}
		}
		break;
		case '>ST<':
		{
			hasTBN = true;
			VERUS_RT_ASSERT(_vBinding2.empty());
			_vBinding2.resize(_vertCount);
			VertexInputBinding2* pVB = _vBinding2.data();
			char temp[3];
			VERUS_FOR(i, _vertCount)
			{
				sp.Read(temp, 3);
				Convert::Sint8ToSint16(temp, pVB[i]._tan, 3); // Single byte is not supported in OpenGL.
				sp.Read(temp, 3);
				Convert::Sint8ToSint16(temp, pVB[i]._bin, 3);
#ifdef VERUS_RELEASE_DEBUG
				const int lenT =
					pVB[i]._tan[0] * pVB[i]._tan[0] +
					pVB[i]._tan[1] * pVB[i]._tan[1] +
					pVB[i]._tan[2] * pVB[i]._tan[2];
				VERUS_RT_ASSERT(lenT > 32100 * 32100);
				const int lenB =
					pVB[i]._bin[0] * pVB[i]._bin[0] +
					pVB[i]._bin[1] * pVB[i]._bin[1] +
					pVB[i]._bin[2] * pVB[i]._bin[2];
				VERUS_RT_ASSERT(lenB > 32100 * 32100);
#endif
			}
		}
		break;
		case '>0T<':
		{
			sp.Read(&_tc0Deq[0], 8);
			sp.Read(&_tc0Deq[2], 8);
			VertexInputBinding0* pVB = _vBinding0.data();
			short mn = 0, mx = 0;
			VERUS_FOR(i, _vertCount)
			{
				sp.Read(pVB[i]._tc0, 4);
#ifdef VERUS_RELEASE_DEBUG
				const short* pMin = std::min_element(pVB[i]._tc0, pVB[i]._tc0 + 2);
				const short* pMax = std::max_element(pVB[i]._tc0, pVB[i]._tc0 + 2);
				mn = Math::Min(mn, *pMin);
				mx = Math::Max(mx, *pMax);
#endif
			}
#ifdef VERUS_RELEASE_DEBUG
			VERUS_RT_ASSERT(mn < -32700);
			VERUS_RT_ASSERT(mx > +32700);
#endif
		}
		break;
		case '>1T<':
		{
			sp.Read(&_tc1Deq[0], 8);
			sp.Read(&_tc1Deq[2], 8);
			VERUS_RT_ASSERT(_vBinding3.empty());
			_vBinding3.resize(_vertCount);
			VertexInputBinding3* pVB = _vBinding3.data();
			VERUS_FOR(i, _vertCount)
				sp.Read(&pVB[i]._tc1, 4);
		}
		break;
		case '>HB<':
		{
			BYTE boneCount = 0;
			sp >> boneCount;
			VERUS_RT_ASSERT(boneCount <= VERUS_MAX_BONES);
			_boneCount = boneCount;
			_skeleton.Init();
			VERUS_FOR(i, _boneCount)
			{
				Anim::Skeleton::Bone bone;
				sp.ReadString(buffer); bone._name = buffer;
				sp.ReadString(buffer); bone._parentName = buffer;
				Transform3 matOffset;
				sp.Read(&matOffset, 64);
				bone._matToBoneSpace = matOffset;
				bone._matFromBoneSpace = VMath::orthoInverse(matOffset);
				_skeleton.InsertBone(bone);
			}
		}
		break;
		case '>SS<':
		{
			VERUS_RT_ASSERT(_vBinding1.empty());
			_vBinding1.resize(_vertCount);
			VertexInputBinding1* pVB = _vBinding1.data();
			bool abnormal = false;
			VERUS_FOR(i, _vertCount)
			{
				BYTE data[8];
				sp.Read(data, 8);
				VERUS_FOR(j, 4)
				{
					pVB[i]._bw[j] = data[j];
					pVB[i]._bi[j] = data[j + 4];
				}
#ifdef VERUS_RELEASE_DEBUG
				const int sum =
					pVB[i]._bw[0] +
					pVB[i]._bw[1] +
					pVB[i]._bw[2] +
					pVB[i]._bw[3];
				if (sum != 255)
					abnormal = true;
#endif
			}
			if (abnormal)
				VERUS_LOG_WARN(_C("WARNING: Abnormal skin weights in " + _url));
		}
		break;
		case '>RS<':
		{
			VertexInputBinding0* pVB = _vBinding0.data();
			BYTE index = 0;
			VERUS_FOR(i, _vertCount)
			{
				sp >> index;
				pVB[i]._pos[3] = index;
			}
			_rigidSkeleton = true;
		}
		break;
		default:
			sp.Advance(blockSize);
		}
	}

	if (_loadOnly)
		return;

	if (!hasTBN)
	{
		_vBinding2.resize(_vertCount);
		ComputeTangentSpace();
	}

	if (_initShape)
		InitShape(Transform3::identity());

	CreateDeviceBuffers();
}

void BaseMesh::LoadPrimaryBones()
{
#if 0
	if (!m_skeleton.IsInitialized())
		return;

	String url(_url);
	Str::ReplaceExtension(url, ".primary");
	Vector<BYTE> vData;
	IO::FileSystem::I().LoadResourceFromCache(_C(url), vData, false);
	if (vData.size() <= 1)
		return;
	Vector<String> vPrimaryBones;
	Str::ReadLines(reinterpret_cast<CSZ>(vData.data()), vPrimaryBones);
	if (!vPrimaryBones.empty())
	{
		_skeleton.AdjustPrimaryBones(vPrimaryBones);
		VERUS_FOREACH(Vector<VertexInputBinding1>, _vBinding1, it)
		{
			VERUS_FOR(i, 4)
				it->bi[i] = _skeleton.RemapBoneIndex(it->bi[i]);
		}
		UpdateVertexBuffer(_vBinding1.data(), 1);
	}
#endif
}

void BaseMesh::LoadRig()
{
	if (!_skeleton.IsInitialized())
		return;

	String url(_url);
	Str::ReplaceExtension(url, ".rig");
	Vector<BYTE> vData;
	IO::FileSystem::I().LoadResourceFromCache(_C(url), vData, false);
	if (vData.size() <= 1)
		return;
	_skeleton.LoadRigInfoFromPtr(reinterpret_cast<SZ>(vData.data()));
}

void BaseMesh::LoadWarp()
{
	if (_warpUrl.empty())
		return;

	Vector<BYTE> vData;
	IO::FileSystem::I().LoadResourceFromCache(_C(_warpUrl), vData, false);
	if (vData.size() > 1)
	{
		VERUS_RT_ASSERT(!(_vBinding0.empty() || _vBinding2.empty()));
		VERUS_RT_ASSERT(vData.size() - 1 == _vertCount * 6);
		CSZ ps = reinterpret_cast<CSZ>(vData.data());
		VERUS_FOR(i, _vertCount)
		{
			int r, g, b;
			sscanf(ps + i * 6, "%d %d %d", &r, &g, &b);
			_vBinding0[i]._pos[3] = r * SHRT_MAX / 9;
			_vBinding2[i]._tan[3] = g * SHRT_MAX / 9;
			_vBinding2[i]._bin[3] = b * SHRT_MAX / 9;
		}
		UpdateVertexBuffer(_vBinding0.data(), 0);
		UpdateVertexBuffer(_vBinding2.data(), 2);
	}

	vData.clear();
	String url(_warpUrl);
	Str::ReplaceExtension(url, ".xwa");
	IO::FileSystem::I().LoadResourceFromCache(_C(url), vData, false);
	if (vData.size() > 1)
	{
		_warp.Init();
		_warp.LoadFromPtr(reinterpret_cast<SZ>(vData.data()));
	}
}

void BaseMesh::ComputeDeq(glm::vec3& scale, glm::vec3& bias, const glm::vec3& extents, const glm::vec3& minPos)
{
	scale = extents * (1.f / SHRT_MAX);
	bias = minPos + extents;
}

void BaseMesh::QuantizeV(glm::vec3& v, const glm::vec3& extents, const glm::vec3& minPos)
{
	v = ((v - minPos - extents) / extents) * float(SHRT_MAX);
	v.x = (v.x >= 0) ? v.x + 0.5f : v.x - 0.5f;
	v.y = (v.y >= 0) ? v.y + 0.5f : v.y - 0.5f;
	v.z = (v.z >= 0) ? v.z + 0.5f : v.z - 0.5f;
	v = glm::clamp(v, glm::vec3(-SHRT_MAX), glm::vec3(SHRT_MAX));
}

void BaseMesh::DequantizeUsingDeq2D(const short* in, const float* deq, glm::vec2& out)
{
	out.x = in[0] * deq[0] + deq[2];
	out.y = in[1] * deq[1] + deq[3];
}

void BaseMesh::DequantizeUsingDeq2D(const short* in, const float* deq, RPoint3 out)
{
	out = Point3(
		in[0] * deq[0] + deq[2],
		in[1] * deq[1] + deq[3],
		0);
}

void BaseMesh::DequantizeUsingDeq3D(const short* in, const float* deq, glm::vec3& out)
{
	out.x = in[0] * deq[0] + deq[3];
	out.y = in[1] * deq[1] + deq[4];
	out.z = in[2] * deq[2] + deq[5];
}

void BaseMesh::DequantizeUsingDeq3D(const short* in, const float* deq, RPoint3 out)
{
	out = Point3(
		in[0] * deq[0] + deq[3],
		in[1] * deq[1] + deq[4],
		in[2] * deq[2] + deq[5]);
}

void BaseMesh::DequantizeNrm(const char* in, glm::vec3& out)
{
	out.x = Convert::Sint8ToSnorm(in[0]);
	out.y = Convert::Sint8ToSnorm(in[1]);
	out.z = Convert::Sint8ToSnorm(in[2]);
}

void BaseMesh::DequantizeNrm(const char* in, RVector3 out)
{
	out = Vector3(
		Convert::Sint8ToSnorm(in[0]),
		Convert::Sint8ToSnorm(in[1]),
		Convert::Sint8ToSnorm(in[2]));
}

void BaseMesh::DequantizeTanBin(const short* in, RVector3 out)
{
	out = Vector3(
		Convert::Sint16ToSnorm(in[0]),
		Convert::Sint16ToSnorm(in[1]),
		Convert::Sint16ToSnorm(in[2]));
}

void BaseMesh::ComputeTangentSpace()
{
	Vector<glm::vec3> vV, vN, vTan, vBin;
	Vector<glm::vec2> vTex;
	vV.resize(_vertCount);
	vTex.resize(_vertCount);

	VERUS_FOR(i, _vertCount)
	{
		DequantizeUsingDeq3D(_vBinding0[i]._pos, _posDeq, vV[i]);
		DequantizeUsingDeq2D(_vBinding0[i]._tc0, _tc0Deq, vTex[i]);
	}

	Math::NormalComputer::ComputeNormals(_vIndices, vV, vN, 1);
	VERUS_FOR(i, _vertCount)
		Convert::SnormToSint8(&vN[i].x, _vBinding0[i]._nrm, 3);

	if (!_vBinding2.empty())
	{
		Math::NormalComputer::ComputeTangentSpace(_vIndices, vV, vN, vTex, vTan, vBin);
		VERUS_FOR(i, _vertCount)
		{
			Convert::SnormToSint16(&vTan[i].x, _vBinding2[i]._tan, 3);
			Convert::SnormToSint16(&vBin[i].x, _vBinding2[i]._bin, 3);
		}
	}
}

btBvhTriangleMeshShape* BaseMesh::InitShape(RcTransform3 tr, CSZ url)
{
	if (!_vertCount)
		return nullptr;

	DoneShape();

	String finalUrl = url ? url : _url;
	String cacheFilename;
	if (!finalUrl.empty())
	{
		String filename = Str::ToPakFriendlyUrl(_C(finalUrl));
		if (url)
			filename += ".INST";
		filename = "[Models]:PhyCache/" + filename + ".bullet";
		cacheFilename = IO::FileSystem::ConvertRelativePathToAbsolute(filename, true);
		if (IO::FileSystem::FileExist(_C(filename)))
		{
			Vector<BYTE> vMesh;
			IO::FileSystem::LoadResource(_C(filename), vMesh);

			btBulletWorldImporter bwi(0);
			if (!vMesh.empty() && bwi.loadFileFromMemory(reinterpret_cast<char*>(vMesh.data()), Utils::Cast32(vMesh.size())))
			{
				const int count = bwi.getNumCollisionShapes();
				if (count)
					_pShape = static_cast<btBvhTriangleMeshShape*>(bwi.getCollisionShapeByIndex(0));
			}

			return _pShape;
		}
	}

	btTriangleMesh* pTriangleMesh = new btTriangleMesh(_vIndices.empty());
	pTriangleMesh->preallocateVertices(_vertCount);
	pTriangleMesh->preallocateIndices(_indexCount);
	VERUS_FOR(i, _vertCount)
	{
		Point3 pos;
		DequantizeUsingDeq3D(_vBinding0[i]._pos, _posDeq, pos);
		pos = tr * pos;
		pTriangleMesh->findOrAddVertex(pos.Bullet(), false);
	}
	VERUS_FOR(i, _faceCount)
	{
		const int i0 = _vIndices.empty() ? _vIndices32[i * 3 + 0] : _vIndices[i * 3 + 0];
		const int i1 = _vIndices.empty() ? _vIndices32[i * 3 + 1] : _vIndices[i * 3 + 1];
		const int i2 = _vIndices.empty() ? _vIndices32[i * 3 + 2] : _vIndices[i * 3 + 2];
		pTriangleMesh->addTriangleIndices(i0, i1, i2);
	}

	_pShape = new btBvhTriangleMeshShape(pTriangleMesh, true);

	if (!cacheFilename.empty())
	{
		btDefaultSerializer s;
		s.startSerialization();
		_pShape->serializeSingleShape(&s);
		s.finishSerialization();
		IO::File file;
		if (file.Open(_C(cacheFilename), "wb"))
			file.Write(s.getBufferPointer(), s.getCurrentBufferSize());
	}

	return _pShape;
}

void BaseMesh::DoneShape()
{
	if (_pShape)
	{
		delete _pShape->getMeshInterface();
		delete _pShape;
		_pShape = nullptr;
	}
}

void BaseMesh::GetBounds(RPoint3 mn, RPoint3 mx) const
{
	const short smin[] = { -SHRT_MAX, -SHRT_MAX, -SHRT_MAX };
	const short smax[] = { +SHRT_MAX, +SHRT_MAX, +SHRT_MAX };
	DequantizeUsingDeq3D(smin, _posDeq, mn);
	DequantizeUsingDeq3D(smax, _posDeq, mx);
}

Math::Bounds BaseMesh::GetBounds() const
{
	Point3 mn, mx;
	GetBounds(mn, mx);
	return Math::Bounds(mn, mx);
}

String BaseMesh::ToXmlString() const
{
	Point3 mn, mx;
	GetBounds(mn, mx);

	StringStream ss;
	ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" << VERUS_CRNL;
	ss << "<x3d>" << VERUS_CRNL;
	ss << "<version v=\"1.1\" />" << VERUS_CRNL;
	ss << "<aabb min=\"" << mn.ToString() << "\" max=\"" << mx.ToString() << "\" />" << VERUS_CRNL;

	// IB:
	ss << "<i>";
	if (!_vIndices.empty())
	{
		VERUS_FOR(i, _indexCount)
		{
			if (i)
				ss << ' ';
			ss << _vIndices[i];
		}
	}
	else
	{
		VERUS_FOR(i, _indexCount)
		{
			if (i)
				ss << ' ';
			ss << _vIndices32[i];
		}
	}

	ss << "</i>" << VERUS_CRNL;

	// Position:
	ss << "<p>";
	VERUS_FOR(i, _vertCount)
	{
		Point3 pos;
		DequantizeUsingDeq3D(_vBinding0[i]._pos, _posDeq, pos);
		if (i)
			ss << ' ';
		ss << pos.ToString();
	}
	ss << "</p>" << VERUS_CRNL;

	// Normal:
	ss << "<n>";
	VERUS_FOR(i, _vertCount)
	{
		Vector3 norm;
		DequantizeNrm(_vBinding0[i]._nrm, norm);
		if (i)
			ss << ' ';
		ss << norm.ToString();
	}
	ss << "</n>" << VERUS_CRNL;

	// TC 0:
	ss << "<tc0>";
	VERUS_FOR(i, _vertCount)
	{
		Point3 tc;
		DequantizeUsingDeq2D(_vBinding0[i]._tc0, _tc0Deq, tc);
		if (i)
			ss << ' ';
		ss << tc.ToString2();
	}
	ss << "</tc0>" << VERUS_CRNL;

	// TC 1:
	if (false)
	{
		ss << "<tc1>";
		VERUS_FOR(i, _vertCount)
		{
			Point3 tc;
			DequantizeUsingDeq2D(_vBinding3[i]._tc1, _tc1Deq, tc);
			if (i)
				ss << ' ';
			ss << tc.ToString2();
		}
		ss << "</tc1>" << VERUS_CRNL;
	}

	ss << "</x3d>";

	return ss.str();
}

String BaseMesh::ToObjString() const
{
	StringStream ss;

	VERUS_FOR(i, _vertCount)
	{
		Point3 pos;
		DequantizeUsingDeq3D(_vBinding0[i]._pos, _posDeq, pos);
		ss << "v " << Point3(VMath::scale(pos, 100)).ToString() << VERUS_CRNL;
	}

	ss << VERUS_CRNL;

	if (!_vIndices.empty())
	{
		VERUS_FOR(i, _faceCount)
		{
			ss << "f ";
			ss << _vIndices[i * 3 + 0] + 1 << ' ';
			ss << _vIndices[i * 3 + 1] + 1 << ' ';
			ss << _vIndices[i * 3 + 2] + 1 << VERUS_CRNL;
		}
	}
	else
	{
		VERUS_FOR(i, _faceCount)
		{
			ss << "f ";
			ss << _vIndices32[i * 3 + 0] + 1 << ' ';
			ss << _vIndices32[i * 3 + 1] + 1 << ' ';
			ss << _vIndices32[i * 3 + 2] + 1 << VERUS_CRNL;
		}
	}

	return ss.str();
}
