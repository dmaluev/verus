// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Extra;

// ConvertX::AnimationKey:

void ConvertX::AnimationKey::DetectRedundantFrames(float threshold)
{
	for (auto& subkey : _vFrame)
		subkey._redundant = false;

	const size_t size = _vFrame.size();
	const size_t edge = size - 1;

	_logicFrameCount = size > 1 ? 2 : 1;

	if (size <= 1)
		return;

	Quat base(
		_vFrame[1]._q.getX() - _vFrame[0]._q.getX(),
		_vFrame[1]._q.getY() - _vFrame[0]._q.getY(),
		_vFrame[1]._q.getZ() - _vFrame[0]._q.getZ(),
		_vFrame[1]._q.getW() - _vFrame[0]._q.getW());
	for (size_t i = 1; i < edge; ++i)
	{
		const Quat test(
			_vFrame[i]._q.getX() - _vFrame[i - 1]._q.getX(),
			_vFrame[i]._q.getY() - _vFrame[i - 1]._q.getY(),
			_vFrame[i]._q.getZ() - _vFrame[i - 1]._q.getZ(),
			_vFrame[i]._q.getW() - _vFrame[i - 1]._q.getW());
		if (base.IsEqual(test, threshold))
		{
			_vFrame[i]._redundant = true;
		}
		else
		{
			base = test;
			_logicFrameCount++;
			if (_vFrame[i - 1]._redundant)
			{
				_vFrame[i - 1]._redundant = false;
				_logicFrameCount++;
			}
		}
	}
}

// ConvertX::AnimationSet:

void ConvertX::AnimationSet::CleanUp()
{
	VERUS_WHILE(Vector<Animation>, _vAnimations, it)
	{
		const String name = (*it)._name;
		if (String::npos != name.find("OgreMax") ||
			String::npos != name.find("Omni") ||
			String::npos != name.find("Camera"))
			it = _vAnimations.erase(it);
		else
			it++;
	}
}

// ConvertX:

ConvertX::ConvertX()
{
	_matRoot = Transform3::identity();
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

void ConvertX::LoadBoneNames(CSZ pathname)
{
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(pathname, vData, IO::FileSystem::LoadDesc(true));
	Vector<String> vLines;
	Str::ReadLines(reinterpret_cast<CSZ>(vData.data()), vLines);
	for (const auto& line : vLines)
	{
		Vector<String> vPair;
		Str::Explode(_C(line), " ", vPair);
		Str::ToLower(const_cast<SZ>(_C(vPair[0])));
		_mapBoneNames[vPair[0]] = vPair[1];
	}
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
		throw VERUS_RECOVERABLE << "ParseData(), invalid file format (no xof)";
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

		if (!temp && !strcmp(buffer, "Mesh"))
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
		else if (!temp && !strcmp(buffer, "Frame"))
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
	ssLevel << "<level>" VERUS_CRNL;

	for (const auto& mat : _vMaterials)
	{
		if (!mat._copyOf.empty())
			continue;

		ssLevel << "<material ";
		ssLevel << "name=\"" << Str::XmlEscape(_C(mat._name)) << "\" ";
		if (_desc._useDefaultMaterial)
		{
			ssLevel << "faceColor=\"1 1 1 1\" ";
			ssLevel << "power=\"10\" ";
			ssLevel << "specularColor=\"0.25 0.25 0.25\" ";
			ssLevel << "emissiveColor=\"0 0 0\">";
		}
		else
		{
			ssLevel << "faceColor=\"" << Vector4(mat._faceColor).ToString() << "\" ";
			ssLevel << "power=\"" << mat._power << "\" ";
			ssLevel << "specularColor=\"" << Vector3(mat._specularColor).ToString() << "\" ";
			ssLevel << "emissiveColor=\"" << Vector3(mat._emissiveColor).ToString() << "\">";
		}
		ssLevel << Str::XmlEscape(_C(mat._textureFilename));
		ssLevel << "</material>" VERUS_CRNL;
	}

	for (const auto& pMesh : _vMesh)
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

		ssLevel << "<object ";
		ssLevel << "copyOf=\"" << Str::XmlEscape(_C(pMesh->GetCopyOfName())) << "\" ";
		ssLevel << "matrix=\"" << matW.ToString() << "\" ";
		ssLevel << "mat=\"" << GetXmlMaterial(pMesh->GetMaterialIndex()) << "\"";
		ssLevel << ">" << Str::XmlEscape(_C(pMesh->GetName())) << "</object>" VERUS_CRNL;

		if (!_desc._convertAsLevel || !pMesh->IsCopy())
		{
			IO::File file;
			if (file.Open(path, "wb"))
				pMesh->SerializeX3D3(file);
			else
				throw VERUS_RECOVERABLE << "SerializeAll(), failed to create file: " << path;
		}
	}

	ssLevel << "</level>" VERUS_CRNL;

	if (_desc._convertAsLevel)
	{
		strcpy(strrchr(path, '\\') + 1, "Level.xml");
		if (!_pDelegate || _pDelegate->BaseConvert_CanOverwriteFile(path))
		{
			IO::File file;
			if (file.Open(path, "wb"))
				file.Write(_C(ssLevel.str()), ssLevel.str().length());
		}
	}

	int animTotal = 0, animCount = 0;
	if (!_vAnimSets.empty())
	{
		OnProgressText("Saving motions");
		VERUS_FOREACH(Vector<AnimationSet>, _vAnimSets, itSet)
		{
			AnimationSet& set = *itSet;
			set.CleanUp();
			animTotal += Utils::Cast32(set._vAnimations.size());
		}
	}

	VERUS_FOREACH(Vector<AnimationSet>, _vAnimSets, itSet) // Each animation set:
	{
		AnimationSet& set = *itSet;

		int maxFrames = 0;
		VERUS_FOREACH_CONST(Vector<Animation>, set._vAnimations, it)
		{
			maxFrames = Math::Max(maxFrames, static_cast<int>((*it)._vAnimKeys[0]._vFrame.size()));
			maxFrames = Math::Max(maxFrames, static_cast<int>((*it)._vAnimKeys[1]._vFrame.size()));
			maxFrames = Math::Max(maxFrames, static_cast<int>((*it)._vAnimKeys[2]._vFrame.size()));
		}

		strcpy(strrchr(path, '\\') + 1, _C(set._name));
		strcat(path, ".xan");
		if (_pDelegate && !_pDelegate->BaseConvert_CanOverwriteFile(path))
			continue;

		IO::File file;
		if (file.Open(path, "wb"))
		{
			const UINT32 magic = '2NAX';
			file << magic;

			const UINT16 version = 0x0101;
			file << version;

			const UINT32 frameCount = maxFrames;
			file << frameCount;

			const UINT32 fps = 10;
			file << fps;

			const int boneCount = Utils::Cast32(set._vAnimations.size());
			file << boneCount;

			VERUS_FOREACH(Vector<Animation>, set._vAnimations, itAnim) // Each bone:
			{
				Animation& anim = *itAnim;

				String name;
				const size_t startAt = anim._name.rfind("-");
				if (startAt != String::npos)
				{
					name = anim._name.substr(startAt + 1);
				}
				else
				{
					name = anim._name;
				}

				if (name == "Pelvis0")
					name = Anim::Skeleton::RootName();

				name = RenameBone(_C(name));

				file.WriteString(_C(name)); // Bone's name.
				anim._vAnimKeys[0].DetectRedundantFrames();
				const int keyframeCount = anim._vAnimKeys[0]._logicFrameCount;
				file << keyframeCount;
				VERUS_FOREACH(Vector<AnimationKey>, anim._vAnimKeys, itKey)
				{
					AnimationKey& key = *itKey;
					if (key._type == 0) // Rotation:
					{
						int frame = 0;
						VERUS_FOREACH(Vector<SubKey>, key._vFrame, itSK)
						{
							SubKey& sk = *itSK;
							if (!sk._redundant)
							{
								file << frame;
								file.Write(sk._q.ToPointer(), 16);
							}
							frame++;
						}
					}
				}

				anim._vAnimKeys[2].DetectRedundantFrames();
				const int keyframePCount = anim._vAnimKeys[2]._logicFrameCount;
				file << keyframePCount;
				VERUS_FOREACH(Vector<AnimationKey>, anim._vAnimKeys, itKey)
				{
					AnimationKey& key = *itKey;
					if (key._type == 2) // Position:
					{
						int frame = 0;
						VERUS_FOREACH(Vector<SubKey>, key._vFrame, itSK)
						{
							SubKey& sk = *itSK;
							if (!sk._redundant)
							{
								file << frame;
								file.Write(sk._q.ToPointer(), 12);
							}
							frame++;
						}
					}
				}

				// Scale/trigger:
				const int keyframeSTCount = 0;
				file << keyframeSTCount;
				file << keyframeSTCount;

				animCount++;
				OnProgress(float(animCount) / animTotal * 100);
			}
		}
	}

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
		_vData.resize((size_t)size + 1);
		file.Read(_vData.data(), size);
		_pData = _vData.data();
	}
	else
		throw VERUS_RECOVERABLE << "LoadFromFile() failed, " << pathname;
}

void ConvertX::StreamReadUntil(SZ dest, int destSize, CSZ separator)
{
	memset(dest, 0, destSize);
	size_t len = strcspn(_pData, separator);
	if (int(len) >= destSize)
		throw VERUS_RECOVERABLE << "StreamReadUntil() failed, dest=" << destSize << ", length=" << len;
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
	const Transform3 m = VMath::appendScale(
		Transform3(Matrix3::rotationY(_desc._angle), Vector3(_desc._bias)), Vector3(s, s, _desc._rightHanded ? -s : s));
	const glm::mat4 matW = m.GLM();

	for (const auto& pMesh : _vMesh)
	{
		bool boneAdded = false;
		do
		{
			boneAdded = false;
			VERUS_FOREACH_CONST(TMapFrames, _mapFrames, itFrame)
			{
				RcFrame frame = itFrame->second;
				Mesh::PBone pBone = pMesh->FindBone(_C(frame._name));
				if (pBone)
				{
					pBone->_matFrame = frame._mat;
					pBone->_matFrameAcc = frame._matAcc;

					pBone->_matFrameAcc = glm::inverse(pBone->_matFrameAcc); // Turn to Offset matrix.
					// Good time to adjust bone matrix (combined matrix is already included):
					pBone->_matFrameAcc = pMesh->GetBoneAxisMatrix() * pBone->_matFrameAcc;
					pBone->_matFrameAcc *= glm::inverse(matW);
					pBone->_matFrameAcc = Transform3(pBone->_matFrameAcc).Normalize(true).GLM();
					pBone->_matOffset = pBone->_matFrameAcc; // Use Frames in rest position for offsets.
					pBone->_matFrameAcc = glm::inverse(pBone->_matFrameAcc); // Back to FrameAcc matrix.

					// Find a good parent for this bone:
					Mesh::PBone pParent = pMesh->FindBone(_C(frame._parent));
					if (!pParent && _mapFrames.find(frame._parent) != _mapFrames.end())
					{
						const String name = RenameBone(_C(frame._parent));
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
				throw VERUS_RECOVERABLE << "ParseBlockRecursive(), unexpected symbol \"{\" found in Mesh block";
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
	if (_desc._convertAsLevel)
	{
		// World matrix for level objects:
		VERUS_FOR(i, frameCount)
		{
			if (!i && _stackFrames[i]._name == "Root")
				continue; // Blender's coord system matrix.
			_pCurrentMesh->SetWorldSpaceMatrix(_pCurrentMesh->GetWorldSpaceMatrix() * _stackFrames[i]._mat);
		}
	}
	else
	{
		// Collapse all frame matrices (Frame0*Frame1*Frame2):
		VERUS_FOR(i, frameCount)
			_pCurrentMesh->SetCombinedMatrix(_pCurrentMesh->GetCombinedMatrix() * _stackFrames[i]._mat);

		const float s = _desc._scaleFactor;
		const Transform3 m = VMath::appendScale(
			Transform3(Matrix3::rotationY(_desc._angle), Vector3(_desc._bias)), Vector3(s, s, _desc._rightHanded ? -s : s));
		const glm::mat4 matW = m.GLM();
		// Scale, flip, turn & offset after frame collapse (matW*Frame0*Frame1*Frame2):
		_pCurrentMesh->SetCombinedMatrix(matW * _pCurrentMesh->GetCombinedMatrix());
	}

	Transform3 mat;
	if (_desc._convertAsLevel)
	{
		const Transform3 matFromBlender = Transform3(_stackFrames[0]._mat);
		const Transform3 matToBlender = VMath::inverse(matFromBlender);
		const Transform3 matS = Transform3::scale(Vector3::Replicate(_desc._scaleFactor));
		const Transform3 matRH = _desc._rightHanded ? Transform3::scale(Vector3(1, 1, -1)) : Transform3::identity();
		// The matrix to scale and flip mesh in world space:
		mat = Transform3((matRH * matS * matFromBlender).getUpper3x3(), Vector3(0));

		if (frameCount)
		{
			const glm::mat4 matFromBlender = matFix.GLM() * _stackFrames[0]._mat;
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
			throw VERUS_RECOVERABLE << "ParseBlockData_Mesh(), NaN found";

		// Collapse transformations:
		if (_desc._convertAsLevel)
		{
			Point3 pos = v;
			pos = mat * pos;
			v = pos.GLM();
		}
		else
		{
			v = glm::vec3(_pCurrentMesh->GetCombinedMatrix() * glm::vec4(v, 1));
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
			throw VERUS_RECOVERABLE << "ParseBlockData_Mesh(), only triangles are supported, triangulate the quads";
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
		throw VERUS_RECOVERABLE << "ParseBlockData_MeshTextureCoords(), different vertex count (" << vertCount << ")";
	VERUS_FOR(i, vertCount)
	{
		OnProgress(float(i) / vertCount * 100);
		glm::vec2 tc(0);
		tc.x = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';'); _pData++;
		tc.y = static_cast<float>(fast_atof(_pData)); StreamSkipUntil(';');
		_pData = strstr(_pData, (i == vertEdge) ? ";;" : ";,") + 2;
		StreamSkipWhitespace();

		if (Math::IsNaN(tc.x) || Math::IsNaN(tc.y))
			throw VERUS_RECOVERABLE << "ParseBlockData_MeshTextureCoords(), NaN found";

		_pCurrentMesh->GetSetVertexAt(i)._tc0 = tc;
	}

	if (_desc._convertAsLevel)
	{
		for (const auto& pMesh : _vMesh)
		{
			if (_pCurrentMesh->IsCopyOf(*pMesh))
				break;
		}
	}

	_pCurrentMesh->AddFoundFlag(Mesh::Found::vertsAndUVs);
}

void ConvertX::ParseBlockData_MeshNormals()
{
	Debug("MeshNormals");

	Transform3 mat;
	if (_desc._convertAsLevel)
	{
		const Transform3 matFromBlender = Transform3(_stackFrames[0]._mat);
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
			throw VERUS_RECOVERABLE << "ParseBlockData_MeshNormals(), NaN found";

		// Collapse transformations (no translation here):
		if (_desc._convertAsLevel)
		{
			Vector3 n = vNormals[i];
			n = mat * n;
			vNormals[i] = n.GLM();
			vNormals[i] = glm::normalize(vNormals[i]);
		}
		else
		{
			vNormals[i] = glm::vec3(_pCurrentMesh->GetCombinedMatrix() * glm::vec4(vNormals[i], 0));
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
		throw VERUS_RECOVERABLE << "ParseBlockData_MeshNormals(), different face count (" << faceCount << ")";
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
			throw VERUS_RECOVERABLE << "ParseBlockData_FVFData(), FVFData is not extra texture coords";
	}
	StreamSkipWhitespace();

	StreamReadUntil(buffer, sizeof(buffer), ";");
	_pData++; // Skip ";"
	StreamSkipWhitespace();
	const int vertCount = _pCurrentMesh->GetVertCount();
	const int vertEdge = vertCount - 1;
	if (atoi(buffer) != _pCurrentMesh->GetVertCount() * 2)
		throw VERUS_RECOVERABLE << "ParseBlockData_FVFData(), different vertex count";
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
		_pCurrentMesh->AddFoundFlag(Mesh::Found::texcoordExtra);
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
	bone._matOffset = glm::mat4(1);
	bone._matFrame = glm::mat4(1);
	_pData++; // Skip "
	StreamSkipWhitespace();
	if (_stackFrames.empty())
	{
		StreamReadUntil(buffer, sizeof(buffer), "_");	bone._name = RenameBone(buffer);
		StreamReadUntil(buffer, sizeof(buffer), "\"");	bone._parentName = RenameBone(buffer + 1);
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
		_pCurrentMesh->GetSetVertexAt(vVertexIndices[i]).Add(weight, boneIndex);
	}

	VERUS_FOR(i, 4)
	{
		VERUS_FOR(j, 4)
		{
			StreamReadUntil(buffer, sizeof(buffer), ",;");
			_pData++; // Skip "; or ,"
			StreamSkipWhitespace();
			bone._matOffset[i][j] = (float)atof(buffer);
			if (Math::IsNaN(bone._matOffset[i][j]))
				throw VERUS_RECOVERABLE << "ParseBlockData_SkinWeights(), NaN found";
		}
	}

	// Good time to adjust bone matrix:
	bone._matOffset = _pCurrentMesh->GetBoneAxisMatrix() * bone._matOffset;
	bone._matOffset *= glm::inverse(_pCurrentMesh->GetCombinedMatrix());
	bone._matOffset = Transform3(bone._matOffset).Normalize(true).GLM();

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
			throw VERUS_RECOVERABLE << "ParseBlockData_Frame(), unexpected symbol \"{\" found in Frame block";
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
		frame._parent = Anim::Skeleton::RootName();
		_matRoot = frame._mat;
	}
	else
	{
		do
		{
			stackTemp.push_back(_stackFrames.back());
			_stackFrames.pop_back();
		} while (!_stackFrames.empty() && _stackFrames.back()._name.empty());

		frame._parent = RenameBone(_C(_stackFrames.back()._name));

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
	glm::mat4& mat = _stackFrames.back()._mat;
	VERUS_FOR(i, 4)
	{
		VERUS_FOR(j, 4)
		{
			StreamReadUntil(buffer, sizeof(buffer), ",;");
			_pData++; // Skip "; or ,"
			StreamSkipWhitespace();
			mat[i][j] = (float)atof(buffer);
			if (Math::IsNaN(mat[i][j]))
				throw VERUS_RECOVERABLE << "ParseBlockData_FrameTransformMatrix(), NaN found";
		}
	}
	_pData++; // Skip ";"
	StreamSkipWhitespace();

	// Good time to compute accumulated frame matrix:
	VERUS_FOREACH_REVERSE_CONST(Vector<Frame>, _stackFrames, it)
		_stackFrames.back()._matAcc = (*it)._mat * _stackFrames.back()._matAcc;
}

void ConvertX::ParseBlockData_AnimationSet(CSZ blockName)
{
	AnimationSet set;
	set._name = blockName;
	_vAnimSets.push_back(set);
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
	const int indexSet = int(_vAnimSets.size() - 1);
	Animation an;
	an._name = blockName;
	if ('{' == *_pData) // Animation { {FrameName} AnimationKey {...
	{
		_pData++;
		CSZ pEnd = strchr(_pData, '}');
		if (an._name.empty())
			an._name.assign(_pData, pEnd);
		_pData = strchr(_pData, '}') + 1;
	}
	StreamSkipWhitespace();
	_vAnimSets[indexSet]._vAnimations.push_back(an);

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
	Animation& anFilled = _vAnimSets[indexSet]._vAnimations.back();
	CSZ name = _C(anFilled._name);
	if (strrchr(name, '-'))
		name = strrchr(name, '-') + 1;
	String xname = RenameBone(name);
	Mesh::Bone* pBone = _vMesh[0]->FindBone(_C(xname));
	Mesh::Bone* pParentBone = nullptr;
	if (pBone)
	{
		Transform3 matParentSpaceOffset;
		pParentBone = _vMesh[0]->FindBone(_C(pBone->_parentName));
		if (!pParentBone)
			matParentSpaceOffset = Transform3::identity();
		else
			matParentSpaceOffset = pParentBone->_matOffset;

		int maxFrames = 0;
		maxFrames = Math::Max(maxFrames, static_cast<int>(anFilled._vAnimKeys[0]._vFrame.size()));
		maxFrames = Math::Max(maxFrames, static_cast<int>(anFilled._vAnimKeys[1]._vFrame.size()));
		maxFrames = Math::Max(maxFrames, static_cast<int>(anFilled._vAnimKeys[2]._vFrame.size()));

		Transform3 matBoneAxis = _vMesh[0]->GetBoneAxisMatrix();
		Transform3 matBoneAxisInv = VMath::inverse(matBoneAxis);
		VERUS_FOR(i, maxFrames)
		{
			const int index0 = Math::Min(i, static_cast<int>(anFilled._vAnimKeys[0]._vFrame.size() - 1));
			const int index2 = Math::Min(i, static_cast<int>(anFilled._vAnimKeys[2]._vFrame.size() - 1));

			const glm::quat q = glm::inverse(anFilled._vAnimKeys[0]._vFrame[index0]._q.GLM());
			Transform3 matSRT = Transform3(Quat(q), anFilled._vAnimKeys[2]._vFrame[index2]._q.getXYZ());

			Transform3 matFrameAnim = matBoneAxis * matSRT * matBoneAxisInv;
			matFrameAnim.setTranslation(matFrameAnim.getTranslation() * _desc._scaleFactor);

			const glm::mat4 matNew = Transform3(Transform3(pBone->_matOffset) * VMath::inverse(matParentSpaceOffset) * matFrameAnim).GLM();

			if (index0 == i)
			{
				anFilled._vAnimKeys[0]._vFrame[i]._q = glm::quat_cast(matNew);
				anFilled._vAnimKeys[0]._vFrame[i]._q = VMath::normalize(anFilled._vAnimKeys[0]._vFrame[i]._q);
			}

			if (index2 == i)
			{
				Point3 pos(0);
				pos = Transform3(matNew) * pos;
				anFilled._vAnimKeys[2]._vFrame[i]._q.setXYZ(Vector3(pos));
			}
		}
	}
	else
	{
		const Transform3 matRootInv = VMath::inverse(_matRoot);
		int i = 0;
		for (auto& subkey : anFilled._vAnimKeys[2]._vFrame)
		{
			const int index = Math::Min(i, static_cast<int>(anFilled._vAnimKeys[0]._vFrame.size() - 1));

			const Transform3 matR(anFilled._vAnimKeys[0]._vFrame[index]._q, Vector3(0));
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
	const int indexSet = int(_vAnimSets.size() - 1);
	const int indexAnim = int(_vAnimSets[indexSet]._vAnimations.size() - 1);
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
			Matrix4 m;
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
			memcpy(&m, data, sizeof(m));
			SubKey skR;
			skR._redundant = false;
			skR._q = Quat(VMath::inverse(m.getUpper3x3()));
			SubKey skT;
			skT._redundant = false;
			skT._q = Quat(m.getTranslation(), 0);
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
		_vAnimSets[indexSet]._vAnimations[indexAnim]._vAnimKeys.push_back(akR);
		_vAnimSets[indexSet]._vAnimations[indexAnim]._vAnimKeys.push_back(akS);
		_vAnimSets[indexSet]._vAnimations[indexAnim]._vAnimKeys.push_back(akT);
	}
	else
	{
		_vAnimSets[indexSet]._vAnimations[indexAnim]._vAnimKeys.push_back(ak);
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

ConvertX::PMesh ConvertX::AddMesh(PMesh pMesh)
{
	VERUS_QREF_UTILS;
	VERUS_RT_ASSERT(pMesh);
	if (FindMesh(_C(pMesh->GetName())))
	{
		{
			StringStream ss;
			ss << "Mesh with this name (" << pMesh->GetName() << ") already exist FAIL.";
		}

		do
		{
			StringStream ssName;
			ssName << pMesh->GetName() << "_" << utils.GetRandom().Next();
			pMesh->SetName(_C(ssName.str()));
		} while (FindMesh(_C(pMesh->GetName())));

		{
			StringStream ss;
			ss << "Mesh renamed to " << pMesh->GetName() << ".";
		}
	}
	_vMesh.push_back(pMesh);
	return pMesh;
}

ConvertX::PMesh ConvertX::FindMesh(CSZ name)
{
	for (const auto& pMesh : _vMesh)
	{
		if (pMesh->GetName() == name)
			return pMesh;
	}
	return nullptr;
}

void ConvertX::DeleteAll()
{
	_vData.clear();
	for (const auto& pMesh : _vMesh)
		delete pMesh;
	_vMesh.clear();
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

String ConvertX::RenameBone(CSZ name)
{
	char lowName[256];
	strcpy(lowName, name);
	Str::ToLower(lowName);
	VERUS_IF_FOUND_IN(TMapBoneNames, _mapBoneNames, lowName, it)
	{
		return it->second;
	}
	if (strlen(name) >= 6 && !strncmp(lowName, "bip", 3))
	{
		return name + 6;
	}
	return name;
}
