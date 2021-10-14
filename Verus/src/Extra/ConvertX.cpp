// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Extra;

ConvertX::ConvertX()
{
	_trRoot = Transform3::identity();
}

ConvertX::~ConvertX()
{
	Done();
}

void ConvertX::Init(BaseConvertDelegate* p, RcDesc desc)
{
	VERUS_INIT();
	_pDelegate = p;
	_desc = desc;
}

void ConvertX::Done()
{
	DeleteAll();
	VERUS_DONE(ConvertX);
}

bool ConvertX::UseAreaBasedNormals()
{
	return _desc._areaBasedNormals;
}

bool ConvertX::UseRigidBones()
{
	return _desc._useRigidBones;
}

Str ConvertX::GetPathname()
{
	return _C(_pathname);
}

void ConvertX::ParseData(CSZ pathname)
{
	VERUS_RT_ASSERT(IsInitialized());

	LoadFromFile(pathname);

	char prev[256] = {};
	char buffer[256] = {};
	_pData = _pDataBegin = _vData.data();

	StreamSkipWhitespace();
	StreamReadUntil(buffer, sizeof(buffer), VERUS_WHITESPACE); // xof.
	if (strcmp(buffer, "xof"))
		throw VERUS_RECOVERABLE << "ParseData(); Invalid file format (no xof)";
	StreamSkipWhitespace();
	StreamReadUntil(buffer, sizeof(buffer), VERUS_WHITESPACE); // 0303txt.
	StreamSkipWhitespace();
	StreamReadUntil(buffer, sizeof(buffer), VERUS_WHITESPACE); // 0032.

	StreamSkipWhitespace();
	while (*_pData) // Top-level blocks:
	{
		StreamReadUntil(buffer, sizeof(buffer), VERUS_WHITESPACE); // Type.
		StreamSkipWhitespace();
		const bool temp = !strcmp(prev, "template");

		if (!temp && !strcmp(buffer, "Mesh")) // Unusual top-level mesh.
		{
			if ('{' == *_pData)
			{
				_currentMesh = "Unnamed";
			}
			else
			{
				char buffer[256] = {};
				StreamReadUntil(buffer, sizeof(buffer), VERUS_WHITESPACE);
				_currentMesh = buffer;
				StreamSkipWhitespace();
			}
			ParseBlockRecursive(buffer, 0);
		}
		else if (!temp && !strcmp(buffer, "Frame")) // Similar to node in glTF.
		{
			char name[256] = {};
			StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
			StreamSkipWhitespace();
			ParseBlockRecursive(buffer, name);
		}
		else if (!temp && !strcmp(buffer, "AnimationSet"))
		{
			FixBones();

			char name[256] = {};
			StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
			StreamSkipWhitespace();
			ParseBlockRecursive(buffer, name);
		}
		else if (!temp && !strcmp(buffer, "Material"))
		{
			char name[256] = {};
			StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
			StreamSkipWhitespace();
			ParseBlockRecursive(buffer, name);
		}

		strcpy(prev, buffer);
		StreamSkipWhitespace();
	}
}

void ConvertX::SerializeAll(CSZ pathname)
{
	char path[256] = {};
	strcpy(path, pathname);

	pathname = path;

	DetectMaterialCopies();

	StringStream ssLevel;
	ssLevel << "<?xml version=\"1.0\" encoding=\"utf-8\"?>" VERUS_CRNL;
	ssLevel << "<scene>" VERUS_CRNL;

	for (const auto& mat : _vMaterials)
	{
		if (!mat._copyOf.empty())
			continue;

		ssLevel << "<material ";
		ssLevel << "name=\"" << Str::XmlEscape(_C(mat._name)) << "\" ";
		if (_desc._useDefaultMaterial)
		{
			ssLevel << "baseColorFactor=\"1 1 1 1\" ";
			ssLevel << "power=\"10\" ";
			ssLevel << "specularFactor=\"0.25 0.25 0.25\" ";
			ssLevel << "emissiveFactor=\"0 0 0\">";
		}
		else
		{
			ssLevel << "baseColorFactor=\"" << Vector4(mat._faceColor).ToString() << "\" ";
			ssLevel << "power=\"" << mat._power << "\" ";
			ssLevel << "specularFactor=\"" << Vector3(mat._specularColor).ToString() << "\" ";
			ssLevel << "emissiveFactor=\"" << Vector3(mat._emissiveColor).ToString() << "\">";
		}
		ssLevel << Str::XmlEscape(_C(mat._textureFilename));
		ssLevel << "</material>" VERUS_CRNL;
	}

	for (const auto& pMesh : _vMeshes)
	{
		strcpy(strrchr(path, '\\') + 1, _C(pMesh->GetName()));
		strcat(path, ".x3d");

		if (_pDelegate && !_pDelegate->BaseConvert_CanOverwriteFile(path))
		{
			StringStream ss;
			ss << pMesh->GetName() << ": Cancelled, file exists";
			OnProgressText(_C(ss.str()));
			continue;
		}

		Transform3 matW = pMesh->GetWorldSpaceMatrix();
		Vector3 pos = matW.getTranslation();
		pos *= _desc._scaleFactor;
		if (_desc._rightHanded)
			pos.setZ(-pos.getZ());
		matW.setTranslation(pos);

		ssLevel << "<node ";
		ssLevel << "copyOf=\"" << Str::XmlEscape(_C(pMesh->GetCopyOfName())) << "\" ";
		ssLevel << "tr=\"" << matW.ToString() << "\" ";
		ssLevel << "mat=\"" << GetXmlMaterial(pMesh->GetMaterialIndex()) << "\"";
		ssLevel << ">" << Str::XmlEscape(_C(pMesh->GetName())) << "</node>" VERUS_CRNL;

		if (!_desc._convertAsScene || !pMesh->IsCopy())
		{
			IO::File file;
			if (file.Open(path, "wb"))
				pMesh->SerializeX3D3(file);
			else
				throw VERUS_RECOVERABLE << "SerializeAll(); Failed to create file: " << path;
		}
	}

	ssLevel << "</scene>" VERUS_CRNL;

	if (_desc._convertAsScene)
	{
		strcpy(strrchr(path, '\\') + 1, "Scene.xml");
		if (!_pDelegate || _pDelegate->BaseConvert_CanOverwriteFile(path))
		{
			IO::File file;
			if (file.Open(path, "wb"))
				file.Write(_C(ssLevel.str()), ssLevel.str().length());
		}
	}

	SerializeMotions(path);

	OnProgress(100);
}

void ConvertX::AsyncRun(CSZ pathname)
{
	_future = Async([this, pathname]()
		{
			ParseData(pathname);
			SerializeAll(pathname);
		});
}

void ConvertX::AsyncJoin()
{
	_future.get();
}

bool ConvertX::IsAsyncStarted() const
{
	return _future.valid();
}

bool ConvertX::IsAsyncFinished() const
{
	return _future.valid() && _future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void ConvertX::LoadFromFile(CSZ pathname)
{
	IO::File file;
	if (file.Open(pathname, "rb"))
	{
		const INT64 size = file.GetSize();
		_vData.resize(size + 1);
		file.Read(_vData.data(), size);
		_pData = _vData.data();
	}
	else
		throw VERUS_RECOVERABLE << "LoadFromFile(); " << pathname;
}

void ConvertX::StreamReadUntil(SZ dest, int destSize, CSZ separator)
{
	memset(dest, 0, destSize);
	size_t len = strcspn(_pData, separator);
	if (int(len) >= destSize)
		throw VERUS_RECOVERABLE << "StreamReadUntil(); dest=" << destSize << ", length=" << len;
	strncpy(dest, _pData, len);
	_pData += len;
}

void ConvertX::StreamSkipWhitespace()
{
	const char whitespace[] = " \r\n\t";
	const size_t length = strspn(_pData, whitespace);
	_pData += length;

	// Also skip comments:
	if (_pData[0] == '/' && _pData[1] == '/')
	{
		const char* pEnd = strstr(_pData, "\r\n");
		if (pEnd)
			_pData = pEnd;
		else
			_pData += strlen(_pData);
		StreamSkipWhitespace();
	}
}

void ConvertX::StreamSkipUntil(char c)
{
	while (*_pData && *_pData != c)
		_pData++;
}

void ConvertX::FixBones()
{
	// Same as in ParseBlockData_Mesh:
	const float s = _desc._scaleFactor;
	const Transform3 tr = VMath::appendScale(
		Transform3(Matrix3::rotationY(_desc._angle), Vector3(_desc._bias)), Vector3(s, s, _desc._rightHanded ? -s : s));
	const glm::mat4 matW = tr.GLM();

	for (const auto& pMesh : _vMeshes)
	{
		bool boneAdded = false;
		do
		{
			boneAdded = false;
			VERUS_FOREACH_CONST(TMapFrames, _mapFrames, itFrame)
			{
				RcFrame frame = itFrame->second;
				Mesh::PBone pBone = pMesh->FindBone(_C(frame._name));
				if (pBone) // Is this frame a bone?
				{
					pBone->_trLocal = frame._trLocal;
					pBone->_trGlobal = frame._trGlobal;

					pBone->_trGlobal = glm::inverse(pBone->_trGlobal); // Turn to Offset matrix.
					// Good time to adjust bone matrix (combined matrix is already included):
					pBone->_trGlobal = pMesh->GetBoneAxisMatrix() * pBone->_trGlobal;
					pBone->_trGlobal *= glm::inverse(matW);
					pBone->_trGlobal = Transform3(pBone->_trGlobal).Normalize(true).GLM();
					pBone->_trToBoneSpace = pBone->_trGlobal; // Use Frames in rest position for offsets.
					pBone->_trGlobal = glm::inverse(pBone->_trGlobal); // Back to Global matrix.

					// Add missing parent bones, which are not part of skinning info:
					Mesh::PBone pParent = pMesh->FindBone(_C(frame._parentName));
					if (!pParent && _mapFrames.find(frame._parentName) != _mapFrames.end())
					{
						const String name = RenameBone(_C(frame._parentName));
						pBone->_parentName = name;

						Mesh::Bone bone;
						bone._name = name;
						pMesh->AddBone(bone);
						pMesh->SetBoneCount(pMesh->GetBoneCount() + 1);
						boneAdded = true;

						StringStream ss;
						ss << "Added: " << name << " (parent of " << frame._name << ")";
						ConvertX::I().OnProgressText(_C(ss.str()));
					}
					else
						pBone->_parentName = pParent ? pParent->_name : Anim::Skeleton::RootName();
				}
			}
		} while (boneAdded);
	}
}

void ConvertX::ParseBlockRecursive(CSZ type, CSZ blockName)
{
	_depth++;
	_pData++; // Skip {
	StreamSkipWhitespace();

	if (!strcmp(type, "Mesh"))
	{
		_pCurrentMesh.reset(new Mesh(this));
		_pCurrentMesh->SetName(_C(_currentMesh));
		ParseBlockData_Mesh();
		bool loop = true;
		while (loop)
		{
			switch (*_pData)
			{
			case '{':
			{
				throw VERUS_RECOVERABLE << "ParseBlockRecursive(); Unexpected symbol \"{\" found in Mesh block";
			}
			break;
			case '}':
			{
				AddMesh(_pCurrentMesh.release());
				loop = false;
			}
			break;
			default:
			{
				char buffer[256] = {};
				StreamReadUntil(buffer, sizeof(buffer), VERUS_WHITESPACE);
				StreamSkipWhitespace();
				char name[256] = {};
				if ('{' != *_pData)
				{
					StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
					StreamSkipWhitespace();
				}
				ParseBlockRecursive(buffer, name);
			}
			}
		}
	}
	else if (!strcmp(type, "MeshTextureCoords"))
	{
		ParseBlockData_MeshTextureCoords();
	}
	else if (!strcmp(type, "MeshNormals"))
	{
		ParseBlockData_MeshNormals();
	}
	else if (!strcmp(type, "FVFData") || !strcmp(type, "DeclData"))
	{
		ParseBlockData_FVFData(!strcmp(type, "DeclData"));
	}
	else if (!strcmp(type, "XSkinMeshHeader"))
	{
		ParseBlockData_XSkinMeshHeader();
	}
	else if (!strcmp(type, "SkinWeights"))
	{
		ParseBlockData_SkinWeights();
	}
	else if (!strcmp(type, "Frame"))
	{
		ParseBlockData_Frame(blockName);
	}
	else if (!strcmp(type, "FrameTransformMatrix"))
	{
		ParseBlockData_FrameTransformMatrix();
	}
	else if (!strcmp(type, "AnimationSet"))
	{
		ParseBlockData_AnimationSet(blockName);
	}
	else if (!strcmp(type, "Animation"))
	{
		ParseBlockData_Animation(blockName);
	}
	else if (!strcmp(type, "AnimationKey"))
	{
		ParseBlockData_AnimationKey();
	}
	else if (!strcmp(type, "Material"))
	{
		Material mat;
		mat._name = blockName;
		mat._power = 10;
		_vMaterials.push_back(mat);
		ParseBlockData_Material();

		// TextureFilename (kW X-port):
		if ('}' != *_pData)
		{
			char buffer[256] = {};
			StreamReadUntil(buffer, sizeof(buffer), VERUS_WHITESPACE);
			StreamSkipWhitespace();
			char name[256] = {};
			if ('{' != *_pData)
			{
				StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
				StreamSkipWhitespace();
			}
			ParseBlockRecursive(buffer, name);
		}
	}
	else if (!strcmp(type, "TextureFilename"))
	{
		ParseBlockData_TextureFilename();
	}
	else if (!strcmp(type, "MeshMaterialList"))
	{
		ParseBlockData_MeshMaterialList();

		// Material (kW X-port):
		if ('}' != *_pData)
		{
			char buffer[256] = {};
			StreamReadUntil(buffer, sizeof(buffer), VERUS_WHITESPACE);
			StreamSkipWhitespace();
			char name[256] = {};
			if ('{' != *_pData)
			{
				StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
				StreamSkipWhitespace();
			}
			ParseBlockRecursive(buffer, name);
			_pCurrentMesh->SetMaterialIndex(Utils::Cast32(_vMaterials.size()) - 1);
		}
	}
	else // Unknown block:
	{
		Debug("UNKNOWN");

		const char control[] = "{}";
		const size_t length = strcspn(_pData, control);
		_pData += length;
		switch (*_pData)
		{
		case '{': ParseBlockRecursive("", 0); break;
		case '}': break;
		}
	}

	_depth--;
	_pData++; // Skip }
	StreamSkipWhitespace();
}

void ConvertX::ParseBlockData_Mesh()
{
	Debug("Mesh");

	const float s = _desc._scaleFactor;
	const Matrix4 matFix = Matrix4::scale(Vector3(s, s, _desc._rightHanded ? -s : s));

	const int frameCount = Utils::Cast32(_stackFrames.size());
	if (_desc._convertAsScene)
	{
		// World matrix for scene nodes:
		VERUS_FOR(i, frameCount)
		{
			if (!i && _stackFrames[i]._name == "Root")
				continue; // Blender's coord system matrix.
			_pCurrentMesh->SetWorldSpaceMatrix(_pCurrentMesh->GetWorldSpaceMatrix() * _stackFrames[i]._trLocal);
		}
	}
	else
	{
		// Collapse all frame matrices (Frame0 * Frame1 * Frame2):
		VERUS_FOR(i, frameCount)
			_pCurrentMesh->SetGlobalMatrix(_pCurrentMesh->GetGlobalMatrix() * _stackFrames[i]._trLocal);

		const float s = _desc._scaleFactor;
		const Transform3 tr = VMath::appendScale(
			Transform3(Matrix3::rotationY(_desc._angle), Vector3(_desc._bias)), Vector3(s, s, _desc._rightHanded ? -s : s));
		const glm::mat4 matW = tr.GLM();
		// Scale, flip, turn & offset after frame collapse (matW * Frame0 * Frame1 * Frame2):
		_pCurrentMesh->SetGlobalMatrix(matW * _pCurrentMesh->GetGlobalMatrix());
	}

	Transform3 mat;
	if (_desc._convertAsScene)
	{
		const Transform3 matFromBlender = Transform3(_stackFrames[0]._trLocal);
		const Transform3 matToBlender = VMath::inverse(matFromBlender);
		const Transform3 matS = Transform3::scale(Vector3::Replicate(_desc._scaleFactor));
		const Transform3 matRH = _desc._rightHanded ? Transform3::scale(Vector3(1, 1, -1)) : Transform3::identity();
		// The matrix to scale and flip mesh in world space:
		mat = Transform3((matRH * matS * matFromBlender).getUpper3x3(), Vector3(0));

		if (frameCount)
		{
			const glm::mat4 matFromBlender = matFix.GLM() * _stackFrames[0]._trLocal;
			const glm::mat4 matToBlender = glm::inverse(matFromBlender);
			_pCurrentMesh->SetWorldSpaceMatrix(matFromBlender * _pCurrentMesh->GetWorldSpaceMatrix() * matToBlender);
		}
	}

	char buffer[256] = {};

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": Mesh";
		OnProgressText(_C(ss.str()));
	}

	// <VertexPositions>
	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++; // Skip ";"
	_pCurrentMesh->SetVertCount(atoi(buffer));
	StreamSkipWhitespace();
	const int vertCount = _pCurrentMesh->GetVertCount();
	const int vertEdge = vertCount - 1;
	_pCurrentMesh->ResizeVertsArray(vertCount);
	VERUS_FOR(i, vertCount)
	{
		OnProgress(float(i) / vertCount * 50);
		glm::vec3 v;
		v.x = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';'); _pData++;
		v.y = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';'); _pData++;
		v.z = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';');
		_pData = strstr(_pData, (i == vertEdge) ? ";;" : ";,") + 2;
		StreamSkipWhitespace();

		if (Math::IsNaN(v.x) || Math::IsNaN(v.y) || Math::IsNaN(v.z))
			throw VERUS_RECOVERABLE << "ParseBlockData_Mesh(); NaN found";

		// Collapse transformations:
		if (_desc._convertAsScene)
		{
			Point3 pos = v;
			pos = mat * pos;
			v = pos.GLM();
		}
		else
		{
			v = glm::vec3(_pCurrentMesh->GetGlobalMatrix() * glm::vec4(v, 1));
		}

		_pCurrentMesh->GetSetVertexAt(i)._pos = v;
	}

	if (!vertCount)
	{
		_pData++;
		StreamSkipWhitespace();
	}
	// </VertexPositions>

	// <FaceIndices>
	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++; // Skip ";"
	_pCurrentMesh->SetFaceCount(atoi(buffer));
	StreamSkipWhitespace();
	const int faceCount = _pCurrentMesh->GetFaceCount();
	const int faceEdge = faceCount - 1;
	_pCurrentMesh->ResizeFacesArray(faceCount);
	VERUS_FOR(i, faceCount)
	{
		OnProgress(float(i) / faceCount * 50 + 50);

		int indices[3];
		int order[3] = { 0, 1, 2 };
		if (_desc._rightHanded)
			std::swap(order[1], order[2]);
		const int count = atoi(_pData); StreamSkipUntil(';'); _pData++;
		if (3 != count)
			throw VERUS_RECOVERABLE << "ParseBlockData_Mesh(); Only triangles are supported, triangulate the quads";
		indices[order[0]] = atoi(_pData); StreamSkipUntil(','); _pData++;
		indices[order[1]] = atoi(_pData); StreamSkipUntil(','); _pData++;
		indices[order[2]] = atoi(_pData); StreamSkipUntil(';');

		Mesh::Face face;
		VERUS_FOR(j, 3)
		{
			VERUS_RT_ASSERT(indices[j] < vertCount);
			face._indices[j] = indices[j];
		}
		_pData = strstr(_pData, (i == faceEdge) ? ";;" : ";,") + 2;
		StreamSkipWhitespace();
		_pCurrentMesh->GetSetFaceAt(i) = face;
	}

	if (!faceCount)
	{
		_pData++;
		StreamSkipWhitespace();
	}
	// </FaceIndices>

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": (" << vertCount << " vertices, " << faceCount << " faces)";
		OnProgressText(_C(ss.str()));
	}

	_pCurrentMesh->AddFoundFlag(Mesh::Found::faces);
}

void ConvertX::ParseBlockData_MeshTextureCoords()
{
	Debug("MeshTextureCoords");

	char buffer[256] = {};

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": MeshTextureCoords";
		OnProgressText(_C(ss.str()));
	}

	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++; // Skip ";"
	StreamSkipWhitespace();
	const int vertCount = atoi(buffer);
	const int vertEdge = vertCount - 1;
	if (_pCurrentMesh->GetVertCount() != vertCount)
		throw VERUS_RECOVERABLE << "ParseBlockData_MeshTextureCoords(); Different vertex count (" << vertCount << ")";
	VERUS_FOR(i, vertCount)
	{
		OnProgress(float(i) / vertCount * 100);
		glm::vec2 tc(0);
		tc.x = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';'); _pData++;
		tc.y = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';');
		_pData = strstr(_pData, (i == vertEdge) ? ";;" : ";,") + 2;
		StreamSkipWhitespace();

		if (Math::IsNaN(tc.x) || Math::IsNaN(tc.y))
			throw VERUS_RECOVERABLE << "ParseBlockData_MeshTextureCoords(); NaN found";

		_pCurrentMesh->GetSetVertexAt(i)._tc0 = tc;
	}

	if (_desc._convertAsScene)
	{
		for (const auto& pMesh : _vMeshes)
		{
			if (_pCurrentMesh->IsCopyOf(*pMesh))
				break;
		}
	}

	_pCurrentMesh->AddFoundFlag(Mesh::Found::posAndTexCoord0);
}

void ConvertX::ParseBlockData_MeshNormals()
{
	Debug("MeshNormals");

	Transform3 mat;
	if (_desc._convertAsScene)
	{
		const Transform3 matFromBlender = Transform3(_stackFrames[0]._trLocal);
		const Transform3 matToBlender = VMath::inverse(matFromBlender);
		const Transform3 matS = Transform3::scale(Vector3::Replicate(_desc._scaleFactor));
		const Transform3 matRH = _desc._rightHanded ? Transform3::scale(Vector3(1, 1, -1)) : Transform3::identity();
		// The matrix to scale and flip mesh in world space:
		mat = Transform3((matRH * matS * matFromBlender).getUpper3x3(), Vector3(0));
	}

	char buffer[256] = {};

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": MeshNormals";
		OnProgressText(_C(ss.str()));
	}

	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++; // Skip ";"
	StreamSkipWhitespace();
	const int normCount = atoi(buffer);
	const int normEdge = normCount - 1;
	Vector<glm::vec3> vNormals;
	vNormals.resize(normCount);
	VERUS_FOR(i, normCount)
	{
		OnProgress(float(i) / normCount * 50);
		vNormals[i].x = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';'); _pData++;
		vNormals[i].y = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';'); _pData++;
		vNormals[i].z = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';');

		_pData = strstr(_pData, (i == normEdge) ? ";;" : ";,") + 2;
		StreamSkipWhitespace();

		if (Math::IsNaN(vNormals[i].x) || Math::IsNaN(vNormals[i].y) || Math::IsNaN(vNormals[i].z))
			throw VERUS_RECOVERABLE << "ParseBlockData_MeshNormals(); NaN found";

		// Collapse transformations (no translation here):
		if (_desc._convertAsScene)
		{
			Vector3 n = vNormals[i];
			n = mat * n;
			vNormals[i] = n.GLM();
			vNormals[i] = glm::normalize(vNormals[i]);
		}
		else
		{
			vNormals[i] = glm::vec3(_pCurrentMesh->GetGlobalMatrix() * glm::vec4(vNormals[i], 0));
			vNormals[i] = glm::normalize(vNormals[i]);
		}
	}

	if (!normCount)
	{
		_pData++;
		StreamSkipWhitespace();
	}

	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++; // Skip ";"
	StreamSkipWhitespace();
	const int faceCount = atoi(buffer);
	const int faceEdge = faceCount - 1;
	if (_pCurrentMesh->GetFaceCount() != faceCount)
		throw VERUS_RECOVERABLE << "ParseBlockData_MeshNormals(); Different face count (" << faceCount << ")";
	VERUS_FOR(i, faceCount)
	{
		OnProgress(float(i) / faceCount * 50 + 50);

		int indices[3];
		int order[3] = { 0, 1, 2 };
		if (_desc._rightHanded)
			std::swap(order[1], order[2]);
		atoi(_pData); StreamSkipUntil(';'); _pData++;
		indices[order[0]] = atoi(_pData); StreamSkipUntil(','); _pData++;
		indices[order[1]] = atoi(_pData); StreamSkipUntil(','); _pData++;
		indices[order[2]] = atoi(_pData); StreamSkipUntil(';');

		_pCurrentMesh->GetSetVertexAt(_pCurrentMesh->GetSetFaceAt(i)._indices[0])._nrm = vNormals[indices[0]];
		_pCurrentMesh->GetSetVertexAt(_pCurrentMesh->GetSetFaceAt(i)._indices[1])._nrm = vNormals[indices[1]];
		_pCurrentMesh->GetSetVertexAt(_pCurrentMesh->GetSetFaceAt(i)._indices[2])._nrm = vNormals[indices[2]];
		_pData = strstr(_pData, (i == faceEdge) ? ";;" : ";,") + 2;
		StreamSkipWhitespace();
	}

	if (!faceCount)
	{
		_pData++;
		StreamSkipWhitespace();
	}

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": (" << normCount << " normals, " << faceCount << " faces)";
		OnProgressText(_C(ss.str()));
	}

	_pCurrentMesh->AddFoundFlag(Mesh::Found::normals);
}

void ConvertX::ParseBlockData_FVFData(bool declData)
{
	Debug("FVFData"); // Actually lightmap's texture coords.

	char buffer[256] = {};

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": FVFData";
		OnProgressText(_C(ss.str()));
	}

	if (declData) // (kW X-port):
	{
		_pData = strstr(_pData, ";;") + 2;
	}
	else
	{
		StreamReadUntil(buffer, sizeof(buffer), ";");
		_pData++; // Skip ";"
		if (atoi(buffer) != 258)
			throw VERUS_RECOVERABLE << "ParseBlockData_FVFData(); FVFData is not extra texture coords";
	}
	StreamSkipWhitespace();

	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++; // Skip ";"
	StreamSkipWhitespace();
	const int vertCount = _pCurrentMesh->GetVertCount();
	const int vertEdge = vertCount - 1;
	if (atoi(buffer) != _pCurrentMesh->GetVertCount() * 2)
		throw VERUS_RECOVERABLE << "ParseBlockData_FVFData(); Different vertex count";
	glm::vec2 tcPrev;
	bool trash = true;
	VERUS_FOR(i, vertCount)
	{
		OnProgress(float(i) / vertCount * 100);
		glm::vec2 tc;
		UINT32 temp;

		StreamReadUntil(buffer, sizeof(buffer), ",;");
		_pData++;
		StreamSkipWhitespace();
		temp = static_cast<UINT32>(atol(buffer));
		tc.x = *(float*)&temp;

		StreamReadUntil(buffer, sizeof(buffer), (i == vertEdge) ? ";" : ",");
		_pData++;
		StreamSkipWhitespace();
		temp = static_cast<UINT32>(atol(buffer));
		tc.y = *(float*)&temp;

		if (!i)
		{
			tcPrev = tc;
		}
		else
		{
			if (!glm::all(glm::equal(tc, tcPrev)))
				trash = false;
		}

		_pCurrentMesh->GetSetVertexAt(i)._tc1 = tc;
	}

	if (!trash)
		_pCurrentMesh->AddFoundFlag(Mesh::Found::texCoord1);
}

void ConvertX::ParseBlockData_XSkinMeshHeader()
{
	Debug("XSkinMeshHeader");

	int boneCount = 0;
	sscanf(_pData, "%*d;%*d;%d;", &boneCount);
	_pCurrentMesh->SetBoneCount(boneCount);
	_pData = strchr(_pData, '}');

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": (" << boneCount << " bones)";
		OnProgressText(_C(ss.str()));
	}
}

void ConvertX::ParseBlockData_SkinWeights()
{
	Debug("SkinWeights");

	char buffer[256] = {};
	StringStream ss;
	ss << _pCurrentMesh->GetName() << ": SkinWeights";
	OnProgressText(_C(ss.str()));

	Mesh::Bone bone;
	bone._trToBoneSpace = glm::mat4(1);
	bone._trLocal = glm::mat4(1);
	_pData++; // Skip "
	StreamSkipWhitespace();
	if (_stackFrames.empty())
	{
		StreamReadUntil(buffer, sizeof(buffer), "_");  bone._name = RenameBone(buffer);
		StreamReadUntil(buffer, sizeof(buffer), "\""); bone._parentName = RenameBone(buffer + 1);
	}
	else
	{
		StreamReadUntil(buffer, sizeof(buffer), "\"");
		bone._name = RenameBone(buffer);
	}
	_pData++; // Skip "
	StreamSkipWhitespace();
	_pData++; // Skip ;
	StreamSkipWhitespace();

	const int boneIndex = _pCurrentMesh->GetNextBoneIndex();
	Vector<UINT32> vVertexIndices;
	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++; // Skip ";"
	StreamSkipWhitespace();
	const int count = atoi(buffer);

	vVertexIndices.reserve(count);

	VERUS_FOR(i, count)
	{
		OnProgress(float(i) / count * 50);
		StreamReadUntil(buffer, sizeof(buffer), ",;");
		_pData++; // Skip "; or ,"
		StreamSkipWhitespace();
		const int vi = atoi(buffer);
		vVertexIndices.push_back(vi);
	}

	VERUS_FOR(i, count)
	{
		OnProgress(float(i) / count * 50 + 50);
		StreamReadUntil(buffer, sizeof(buffer), ",;");
		_pData++; // Skip "; or ,"
		StreamSkipWhitespace();
		const float weight = float(atof(buffer));
		_pCurrentMesh->GetSetVertexAt(vVertexIndices[i]).Add(boneIndex, weight);
	}

	VERUS_FOR(i, 4)
	{
		VERUS_FOR(j, 4)
		{
			StreamReadUntil(buffer, sizeof(buffer), ",;");
			_pData++; // Skip "; or ,"
			StreamSkipWhitespace();
			bone._trToBoneSpace[i][j] = (float)atof(buffer);
			if (Math::IsNaN(bone._trToBoneSpace[i][j]))
				throw VERUS_RECOVERABLE << "ParseBlockData_SkinWeights(); NaN found";
		}
	}

	// Good time to adjust bone matrix:
	bone._trToBoneSpace = _pCurrentMesh->GetBoneAxisMatrix() * bone._trToBoneSpace;
	bone._trToBoneSpace *= glm::inverse(_pCurrentMesh->GetGlobalMatrix());
	bone._trToBoneSpace = Transform3(bone._trToBoneSpace).Normalize(true).GLM();

	_pData++; // Skip ";"
	StreamSkipWhitespace();

	_pCurrentMesh->AddBone(bone);
	_pCurrentMesh->AddFoundFlag(Mesh::Found::skinning);
}

void ConvertX::ParseBlockData_Frame(CSZ blockName)
{
	StringStream ss;
	ss << "Frame " << blockName;
	Debug(_C(ss.str()));

	// Frame is similar to node in glTF.
	Frame frame;
	frame._name = RenameBone(blockName);
	_stackFrames.push_back(frame);

	bool loop = true;
	while (loop)
	{
		switch (*_pData)
		{
		case '{':
		{
			throw VERUS_RECOVERABLE << "ParseBlockData_Frame(); Unexpected symbol \"{\" found in Frame block";
		}
		break;
		case '}':
		{
			loop = false;
		}
		break;
		default: // Frame can contain FrameTransformMatrix or Mesh (without name) blocks:
		{
			char type[256] = {};
			char name[256] = {};
			StreamReadUntil(type, sizeof(type), VERUS_WHITESPACE);
			StreamSkipWhitespace();
			if ('{' != *_pData)
			{
				StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
				StreamSkipWhitespace();
			}

			if (!strcmp(type, "Mesh")) // Find name for this mesh:
			{
				VERUS_FOREACH_REVERSE_CONST(Vector<Frame>, _stackFrames, it)
				{
					if (!(*it)._name.empty())
					{
						_currentMesh = (*it)._name;
						break;
					}
				}
			}

			ParseBlockRecursive(type, name);
		}
		}
	}

	// Get parent's name:
	frame = _stackFrames.back();
	Vector<Frame> stackTemp;
	if (_stackFrames.size() == 1)
	{
		frame._parentName = Anim::Skeleton::RootName();
		_trRoot = frame._trLocal;
	}
	else
	{
		do
		{
			stackTemp.push_back(_stackFrames.back());
			_stackFrames.pop_back();
		} while (!_stackFrames.empty() && _stackFrames.back()._name.empty());

		frame._parentName = RenameBone(_C(_stackFrames.back()._name));

		while (!stackTemp.empty())
		{
			_stackFrames.push_back(stackTemp.back());
			stackTemp.pop_back();
		}
	}

	_stackFrames.pop_back();

	_mapFrames[frame._name] = frame;
}

void ConvertX::ParseBlockData_FrameTransformMatrix()
{
	Debug("FrameTransformMatrix");

	char buffer[256] = {};
	glm::mat4& trLocal = _stackFrames.back()._trLocal;
	VERUS_FOR(i, 4)
	{
		VERUS_FOR(j, 4)
		{
			StreamReadUntil(buffer, sizeof(buffer), ",;");
			_pData++; // Skip "; or ,"
			StreamSkipWhitespace();
			trLocal[i][j] = static_cast<float>(atof(buffer));
			if (Math::IsNaN(trLocal[i][j]))
				throw VERUS_RECOVERABLE << "ParseBlockData_FrameTransformMatrix(); NaN found";
		}
	}
	_pData++; // Skip ";"
	StreamSkipWhitespace();

	// Good time to compute frame's global transformation matrix:
	VERUS_FOREACH_REVERSE_CONST(Vector<Frame>, _stackFrames, it)
		_stackFrames.back()._trGlobal = (*it)._trLocal * _stackFrames.back()._trGlobal;
}

void ConvertX::ParseBlockData_AnimationSet(CSZ blockName)
{
	AnimationSet animSet;
	animSet._name = blockName;
	_vAnimSets.push_back(animSet);
	bool loop = true;
	while (loop)
	{
		switch (*_pData)
		{
		case '}':
		{
			loop = false;
		}
		break;
		default:
		{
			char type[256] = {};
			char name[256] = {};
			StreamReadUntil(type, sizeof(type), VERUS_WHITESPACE);
			StreamSkipWhitespace();
			if ('{' != *_pData)
			{
				StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
				StreamSkipWhitespace();
			}
			StreamSkipWhitespace();
			ParseBlockRecursive(type, name);
		}
		}
	}
	StreamSkipWhitespace();
}

void ConvertX::ParseBlockData_Animation(CSZ blockName)
{
	const int animSetIndex = static_cast<int>(_vAnimSets.size() - 1);
	Animation anim;
	anim._name = blockName;
	if ('{' == *_pData) // Animation { {FrameName} AnimationKey {...
	{
		_pData++;
		CSZ pEnd = strchr(_pData, '}');
		if (anim._name.empty())
			anim._name.assign(_pData, pEnd);
		_pData = strchr(_pData, '}') + 1;
	}
	StreamSkipWhitespace();
	_vAnimSets[animSetIndex]._vAnimations.push_back(anim);

	bool loop = true;
	while (loop)
	{
		switch (*_pData)
		{
		case '}':
		{
			loop = false;
		}
		break;
		default:
		{
			char type[256] = {};
			char name[256] = {};
			if ('{' != *_pData)
			{
				StreamReadUntil(type, sizeof(type), VERUS_WHITESPACE);
				StreamSkipWhitespace();
			}
			if ('{' != *_pData)
			{
				StreamReadUntil(name, sizeof(name), VERUS_WHITESPACE);
				StreamSkipWhitespace();
			}
			StreamSkipWhitespace();
			ParseBlockRecursive(type, name);
		}
		}
	}
	StreamSkipWhitespace();

	// Convert data to bone space:
	Animation& animWithKeys = _vAnimSets[animSetIndex]._vAnimations.back();
	CSZ name = _C(animWithKeys._name);
	if (strrchr(name, '-'))
		name = strrchr(name, '-') + 1;
	String boneName = RenameBone(name);
	Mesh::Bone* pBone = _vMeshes[0]->FindBone(_C(boneName));
	Mesh::Bone* pParentBone = nullptr;
	if (pBone)
	{
		Transform3 trToParentSpace;
		pParentBone = _vMeshes[0]->FindBone(_C(pBone->_parentName));
		if (!pParentBone)
			trToParentSpace = Transform3::identity();
		else
			trToParentSpace = pParentBone->_trToBoneSpace;
		const Transform3 trFromParentSpace = VMath::inverse(trToParentSpace);

		int maxFrames = 0;
		maxFrames = Math::Max(maxFrames, static_cast<int>(animWithKeys._vAnimKeys[0]._vFrame.size()));
		maxFrames = Math::Max(maxFrames, static_cast<int>(animWithKeys._vAnimKeys[1]._vFrame.size()));
		maxFrames = Math::Max(maxFrames, static_cast<int>(animWithKeys._vAnimKeys[2]._vFrame.size()));

		Transform3 matBoneAxis = _vMeshes[0]->GetBoneAxisMatrix();
		Transform3 matBoneAxisInv = VMath::inverse(matBoneAxis);
		VERUS_FOR(i, maxFrames)
		{
			const int index0 = Math::Min(i, static_cast<int>(animWithKeys._vAnimKeys[0]._vFrame.size() - 1));
			const int index2 = Math::Min(i, static_cast<int>(animWithKeys._vAnimKeys[2]._vFrame.size() - 1));

			const glm::quat q = glm::inverse(animWithKeys._vAnimKeys[0]._vFrame[index0]._q.GLM());
			Transform3 matSRT = Transform3(Quat(q), animWithKeys._vAnimKeys[2]._vFrame[index2]._q.getXYZ());

			Transform3 trNewLocal = matBoneAxis * matSRT * matBoneAxisInv;
			trNewLocal.setTranslation(trNewLocal.getTranslation() * _desc._scaleFactor);

			const glm::mat4 matNew = Transform3(Transform3(pBone->_trToBoneSpace) * trFromParentSpace * trNewLocal).GLM();

			if (index0 == i)
			{
				const glm::quat qNew = glm::quat_cast(matNew);
				animWithKeys._vAnimKeys[0]._vFrame[i]._q = glm::normalize(qNew);
			}

			if (index2 == i)
			{
				Point3 pos(0);
				pos = Transform3(matNew) * pos;
				animWithKeys._vAnimKeys[2]._vFrame[i]._q.setXYZ(Vector3(pos));
			}
		}
	}
	else
	{
		const Transform3 matRootInv = VMath::inverse(_trRoot);
		int i = 0;
		for (auto& subkey : animWithKeys._vAnimKeys[2]._vFrame)
		{
			const int index = Math::Min(i, static_cast<int>(animWithKeys._vAnimKeys[0]._vFrame.size() - 1));

			const Transform3 matR(animWithKeys._vAnimKeys[0]._vFrame[index]._q, Vector3(0));
			const Transform3 matT = Transform3::translation(subkey._q.getXYZ());
			const Transform3 mat = matT * VMath::inverse(matR) * matRootInv;

			Point3 pos(0);
			pos = mat * pos;
			pos = Vector3(pos) * _desc._scaleFactor;

			if (_desc._rightHanded)
				pos.setZ(-pos.getZ());

			subkey._q.setXYZ(Vector3(pos));
			i++;
		}
	}
}

void ConvertX::ParseBlockData_AnimationKey()
{
	const int animSetIndex = static_cast<int>(_vAnimSets.size() - 1);
	const int animIndex = static_cast<int>(_vAnimSets[animSetIndex]._vAnimations.size() - 1);
	char buffer[256] = {};
	AnimationKey ak;
	AnimationKey akR;
	AnimationKey akS;
	AnimationKey akT;
	akR._type = 0;
	akS._type = 1;
	akT._type = 2;
	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++;
	StreamSkipWhitespace();
	ak._type = atoi(buffer);
	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++;
	StreamSkipWhitespace();
	const int frameCount = atoi(buffer);
	ak._vFrame.reserve(frameCount);
	VERUS_FOR(i, frameCount)
	{
		glm::quat q;
		StreamReadUntil(buffer, sizeof(buffer), ";");
		_pData++;
		StreamSkipWhitespace();
		StreamReadUntil(buffer, sizeof(buffer), ";");
		_pData++;
		StreamSkipWhitespace();
		if (atoi(buffer) == 16)
		{
			Matrix4 tr;
			float data[16];
			sscanf(_pData,
				"%f,%f,%f,%f,"
				"%f,%f,%f,%f,"
				"%f,%f,%f,%f,"
				"%f,%f,%f,%f",
				data + 0, data + 1, data + 2, data + 3,
				data + 4, data + 5, data + 6, data + 7,
				data + 8, data + 9, data + 10, data + 11,
				data + 12, data + 13, data + 14, data + 15);
			memcpy(&tr, data, sizeof(tr));
			SubKey skR;
			skR._redundant = false;
			skR._q = Quat(VMath::inverse(tr.getUpper3x3()));
			SubKey skT;
			skT._redundant = false;
			skT._q = Quat(tr.getTranslation(), 0);
			akR._vFrame.push_back(skR);
			akT._vFrame.push_back(skT);

			_pData = strchr(_pData, ';') + 3;
			continue;
		}
		else if (atoi(buffer) == 4) // Quaternion data:
		{
			q.w = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(','); _pData++;
			q.x = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(','); _pData++;
			q.y = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(','); _pData++;
			q.z = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';');
		}
		else // Scale/position data:
		{
			q.w = 0;
			q.x = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(','); _pData++;
			q.y = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(','); _pData++;
			q.z = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';');
		}
		_pData += 3;

		if (1 == ak._type)
		{
			//if (glm::any(glm::epsilonNotEqual(glm::vec3(q.x, q.y, q.z), glm::vec3(1), glm::vec3(0.01f))))
			//	throw VERUS_RECOVERABLE << "AnimationKey scale detected {" << q.x << ", " << q.y << ", " << q.z << "}!";
		}

		SubKey sk;
		sk._q = q;
		ak._vFrame.push_back(sk);
	}
	StreamSkipWhitespace();
	if (!akR._vFrame.empty())
	{
		_vAnimSets[animSetIndex]._vAnimations[animIndex]._vAnimKeys.push_back(akR);
		_vAnimSets[animSetIndex]._vAnimations[animIndex]._vAnimKeys.push_back(akS);
		_vAnimSets[animSetIndex]._vAnimations[animIndex]._vAnimKeys.push_back(akT);
	}
	else
	{
		_vAnimSets[animSetIndex]._vAnimations[animIndex]._vAnimKeys.push_back(ak);
	}
}

void ConvertX::ParseBlockData_Material()
{
	Debug("Material");

	RMaterial mat = _vMaterials.back();

	sscanf(_pData, "%f;%f;%f;%f", &mat._faceColor.x, &mat._faceColor.y, &mat._faceColor.z, &mat._faceColor.w);
	_pData = strstr(_pData, ";;") + 2;
	StreamSkipWhitespace();

	sscanf(_pData, "%f", &mat._power);
	_pData = strstr(_pData, ";") + 1;
	StreamSkipWhitespace();

	sscanf(_pData, "%f;%f;%f", &mat._specularColor.x, &mat._specularColor.y, &mat._specularColor.z);
	_pData = strstr(_pData, ";;") + 2;
	StreamSkipWhitespace();

	sscanf(_pData, "%f;%f;%f", &mat._emissiveColor.x, &mat._emissiveColor.y, &mat._emissiveColor.z);
	_pData = strstr(_pData, ";;") + 2;
	StreamSkipWhitespace();
}

void ConvertX::ParseBlockData_TextureFilename()
{
	Debug("TextureFilename");

	RMaterial mat = _vMaterials.back();

	_pData++; // '"'.
	char filename[256];
	StreamReadUntil(filename, sizeof(filename), "\"");
	_pData++; // '"'.
	StreamSkipWhitespace();
	_pData++; // ';'.
	StreamSkipWhitespace();

	mat._textureFilename = filename;
}

void ConvertX::ParseBlockData_MeshMaterialList()
{
	Debug("MeshMaterialList");

	char buffer[256] = {};
	StringStream ss;
	ss << _pCurrentMesh->GetName() << ": MeshMaterialList";
	OnProgressText(_C(ss.str()));

	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++;
	const int nMaterials = atoi(buffer);
	StreamSkipWhitespace();

	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++;
	const int nFaceIndexes = atoi(buffer);
	StreamSkipWhitespace();

	const int edge = nFaceIndexes - 1;

	VERUS_FOR(i, nFaceIndexes)
	{
		StreamReadUntil(buffer, sizeof(buffer), (i == edge) ? ";" : ",");
		_pData++;
	}

	_pData++;
	StreamSkipWhitespace();

	while ('{' == *_pData)
	{
		_pData++;
		StreamSkipWhitespace();

		char buffer[256];
		StreamReadUntil(buffer, sizeof(buffer), " }");

		const int count = Utils::Cast32(_vMaterials.size());
		VERUS_FOR(i, count)
		{
			if (_vMaterials[i]._name == buffer)
			{
				_pCurrentMesh->SetMaterialIndex(i);
				break;
			}
		}

		StreamSkipWhitespace();
		_pData++;
		StreamSkipWhitespace();
	}

	StreamSkipWhitespace();
}

void ConvertX::OnProgress(float percent)
{
	if (_pDelegate)
		_pDelegate->BaseConvert_OnProgress(percent);
}

void ConvertX::OnProgressText(CSZ txt)
{
	if (_pDelegate)
		_pDelegate->BaseConvert_OnProgressText(txt);
}

void ConvertX::Debug(CSZ txt)
{
	StringStream ss;
	VERUS_FOR(i, _depth)
		ss << "  ";
	ss << txt << VERUS_CRNL;
	_ssDebug << ss.str();
}

void ConvertX::DetectMaterialCopies()
{
	const int count = Utils::Cast32(_vMaterials.size());
	VERUS_FOR(i, count)
	{
		VERUS_FOR(j, i)
		{
			if (_vMaterials[i].IsCopyOf(_vMaterials[j]))
			{
				_vMaterials[i]._copyOf = _vMaterials[j]._name;
				break;
			}
		}
	}
}

String ConvertX::GetXmlMaterial(int i)
{
	if (_vMaterials.empty())
		return "";
	if (!_vMaterials[i]._copyOf.empty())
		return Str::XmlEscape(_C(_vMaterials[i]._copyOf));
	return Str::XmlEscape(_C(_vMaterials[i]._name));
}
