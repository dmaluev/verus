// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Extra;

ConvertGLTF::ConvertGLTF()
{
}

ConvertGLTF::~ConvertGLTF()
{
	Done();
}

void ConvertGLTF::Init(PBaseConvertDelegate p, RcDesc desc)
{
	VERUS_INIT();
	_pDelegate = p;
	_desc = desc;
}

void ConvertGLTF::Done()
{
	DeleteAll();
	VERUS_DONE(ConvertGLTF);
}

bool ConvertGLTF::UseAreaBasedNormals()
{
	return _desc._areaBasedNormals;
}

bool ConvertGLTF::UseRigidBones()
{
	return _desc._useRigidBones;
}

Str ConvertGLTF::GetPathname()
{
	return _C(_pathname);
}

void ConvertGLTF::ParseData(CSZ pathname)
{
	VERUS_RT_ASSERT(IsInitialized());

	LoadFromFile(pathname);

	const String baseDir = IO::FileSystem::ReplaceFilename(pathname, "");
	std::string err, warn;
	if (!_context.LoadASCIIFromString(&_model, &err, &warn, _vData.data(), Utils::Cast32(_vData.size()), _C(baseDir)))
		throw VERUS_RECOVERABLE << "LoadASCIIFromString(); err=" << err << ", warn=" << warn;

	_vNodeExtraData.resize(_model.nodes.size());
	const auto& scene = _model.scenes[_model.defaultScene];
	for (int nodeIndex : scene.nodes) // Root nodes:
	{
		if (nodeIndex < 0 || nodeIndex >= _model.nodes.size())
			throw VERUS_RECOVERABLE << "Invalid root node index: " << nodeIndex;
		const auto& node = _model.nodes[nodeIndex];
		ProcessNodeRecursive(node, nodeIndex, true);
		ProcessNodeRecursive(node, nodeIndex, false);
	}
	_vAnimSets.reserve(_model.animations.size());
	for (const auto& anim : _model.animations)
	{
		AnimationSet animSet;
		animSet._name = _C(anim.name);
		_vAnimSets.push_back(animSet);
		ProcessAnimation(anim);
	}
}

void ConvertGLTF::SerializeAll(CSZ pathname)
{
	char path[256] = {};
	strcpy(path, pathname);

	pathname = path;

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

		if (!pMesh->IsCopy())
		{
			IO::File file;
			if (file.Open(path, "wb"))
				pMesh->SerializeX3D3(file);
			else
				throw VERUS_RECOVERABLE << "SerializeAll(); Failed to create file: " << path;
		}
	}

	SerializeMotions(path);

	OnProgress(100);
}

void ConvertGLTF::AsyncRun(CSZ pathname)
{
	_future = Async([this, pathname]()
		{
			ParseData(pathname);
			SerializeAll(pathname);
		});
}

void ConvertGLTF::AsyncJoin()
{
	_future.get();
}

bool ConvertGLTF::IsAsyncStarted() const
{
	return _future.valid();
}

bool ConvertGLTF::IsAsyncFinished() const
{
	return _future.valid() && _future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void ConvertGLTF::LoadFromFile(CSZ pathname)
{
	IO::File file;
	if (file.Open(pathname, "rb"))
	{
		const INT64 size = file.GetSize();
		_vData.resize(size);
		file.Read(_vData.data(), size);
	}
	else
		throw VERUS_RECOVERABLE << "LoadFromFile(); " << pathname;
}

void ConvertGLTF::ProcessNodeRecursive(const tinygltf::Node& node, int nodeIndex, bool computeExtraData)
{
	if (computeExtraData)
	{
		_vNodeExtraData[nodeIndex]._trLocal = GetNodeMatrix(node);
		_vNodeExtraData[nodeIndex]._trGlobal = _vNodeExtraData[nodeIndex]._trLocal;
		int parent = _vNodeExtraData[nodeIndex]._parent;
		while (parent >= 0)
		{
			_vNodeExtraData[nodeIndex]._trGlobal = _vNodeExtraData[parent]._trLocal * _vNodeExtraData[nodeIndex]._trGlobal;
			parent = _vNodeExtraData[parent]._parent;
		}
	}
	else
	{
		if (node.mesh >= 0 && node.mesh < _model.meshes.size()) // Node with mesh?
		{
			const auto& mesh = _model.meshes[node.mesh];
			ProcessMesh(mesh, nodeIndex, node.skin);
		}
		if (node.skin >= 0 && node.skin < _model.skins.size()) // Add skin to current mesh?
		{
			const auto& skin = _model.skins[node.skin];
			ProcessSkin(skin);
		}
		_pCurrentMesh.release();
	}

	// Process children:
	for (int childIndex : node.children)
	{
		if (childIndex < 0 || childIndex >= _model.nodes.size())
			throw VERUS_RECOVERABLE << "Invalid child node index: " << childIndex;
		const auto& node = _model.nodes[childIndex];
		_vNodeExtraData[childIndex]._parent = nodeIndex;
		ProcessNodeRecursive(node, childIndex, computeExtraData);
	}
}

void ConvertGLTF::ProcessMesh(const tinygltf::Mesh& mesh, int nodeIndex, int skinIndex)
{
	_pCurrentMesh.reset(new Mesh(this));
	_pCurrentMesh->SetName(_C(mesh.name));

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": Mesh";
		OnProgressText(_C(ss.str()));
	}

	const float s = _desc._scaleFactor;
	const Transform3 tr = VMath::appendScale(
		Transform3(Matrix3::rotationY(_desc._angle), Vector3(_desc._bias)), Vector3(s, s, s));
	const glm::mat4 matW = tr.GLM();
	if (skinIndex >= 0 && skinIndex < _model.skins.size())
	{
		// Skinned meshes are already transformed.
		_pCurrentMesh->SetGlobalMatrix(matW);

		// Add "Armature" bone, which is not included in skin joints:
		const int parent = _vNodeExtraData[nodeIndex]._parent;
		if (parent >= 0 && parent < _model.nodes.size())
		{
			const auto& node = _model.nodes[parent];

			Mesh::Bone bone;
			bone._name = RenameBone(_C(node.name));
			bone._parentName = Anim::Skeleton::RootName();
			bone._trLocal = _vNodeExtraData[nodeIndex]._trLocal;
			bone._trGlobal = _vNodeExtraData[nodeIndex]._trGlobal;
			bone._trToBoneSpace = glm::inverse(_vNodeExtraData[nodeIndex]._trGlobal);

			// Adjust this bone matrix:
			bone._trToBoneSpace = _pCurrentMesh->GetBoneAxisMatrix() * bone._trToBoneSpace;
			bone._trToBoneSpace *= glm::inverse(_pCurrentMesh->GetGlobalMatrix());
			bone._trToBoneSpace = Transform3(bone._trToBoneSpace).Normalize(true).GLM();
			// Fix negative zeros:
			VERUS_FOR(i, 4)
			{
				VERUS_FOR(j, 4)
				{
					if (bone._trToBoneSpace[i][j] == 0)
						bone._trToBoneSpace[i][j] = 0;
				}
			}

			_pCurrentMesh->AddBone(bone);
			_pCurrentMesh->SetBoneCount(1);
		}
	}
	else
	{
		_pCurrentMesh->SetGlobalMatrix(matW * _vNodeExtraData[nodeIndex]._trGlobal);
	}

	Vector<glm::ivec4> vBlendIndices;
	Vector<glm::vec4> vBlendWeights;

	for (const auto& primitive : mesh.primitives)
	{
		{
			if (primitive.indices < 0 || primitive.indices >= _model.accessors.size())
				throw VERUS_RECOVERABLE << "Invalid primitive indices: " << primitive.indices;

			const auto& accessor = _model.accessors[primitive.indices];
			const auto& bufferView = _model.bufferViews[accessor.bufferView];
			const auto& buffer = _model.buffers[bufferView.buffer];
			const int byteStride = accessor.ByteStride(bufferView);

			if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
				throw VERUS_RECOVERABLE << "Invalid componentType for indices: " << accessor.componentType;
			if (accessor.type != TINYGLTF_TYPE_SCALAR)
				throw VERUS_RECOVERABLE << "Invalid type for indices: " << accessor.type;
			if (byteStride != 2)
				throw VERUS_RECOVERABLE << "Invalid byteStride for indices: " << byteStride;

			{
				StringStream ss;
				ss << _pCurrentMesh->GetName() << ": indices";
				OnProgressText(_C(ss.str()));
			}

			const BYTE* p = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

			const int faceCount = static_cast<int>(accessor.count / 3);
			_pCurrentMesh->SetFaceCount(faceCount);
			_pCurrentMesh->ResizeFacesArray(faceCount);
			VERUS_FOR(faceIndex, faceCount)
			{
				Mesh::Face face;
				VERUS_FOR(i, 3)
				{
					const UINT16* pSrc = reinterpret_cast<const UINT16*>(p + byteStride * (faceIndex * 3 + i));
					face._indices[i] = *pSrc;
				}
				if (_desc._flipFaces)
					std::swap(face._indices[1], face._indices[2]);
				_pCurrentMesh->GetSetFaceAt(faceIndex) = face;
			}
			_pCurrentMesh->AddFoundFlag(Mesh::Found::faces);
		}
		for (const auto& attr : primitive.attributes)
		{
			const auto& accessor = _model.accessors[attr.second];
			const auto& bufferView = _model.bufferViews[accessor.bufferView];
			const auto& buffer = _model.buffers[bufferView.buffer];
			const int byteStride = accessor.ByteStride(bufferView);

			if (attr.first == "POSITION")
			{
				if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
					throw VERUS_RECOVERABLE << "Invalid componentType for POSITION: " << accessor.componentType;
				if (accessor.type != TINYGLTF_TYPE_VEC3)
					throw VERUS_RECOVERABLE << "Invalid type for POSITION: " << accessor.type;
				if (byteStride != 12)
					throw VERUS_RECOVERABLE << "Invalid byteStride for POSITION: " << byteStride;

				{
					StringStream ss;
					ss << _pCurrentMesh->GetName() << ": POSITION";
					OnProgressText(_C(ss.str()));
				}

				const BYTE* p = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

				const int vertCount = Utils::Cast32(accessor.count);
				TryResizeVertsArray(vertCount);
				VERUS_FOR(vertIndex, vertCount)
				{
					const float* pSrc = reinterpret_cast<const float*>(p + byteStride * vertIndex);
					glm::vec3 pos = glm::make_vec3(pSrc);
					pos = glm::vec3(_pCurrentMesh->GetGlobalMatrix() * glm::vec4(pos, 1));
					_pCurrentMesh->GetSetVertexAt(vertIndex)._pos = pos;
				}
			}
			if (attr.first == "NORMAL")
			{
				if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
					throw VERUS_RECOVERABLE << "Invalid componentType for NORMAL: " << accessor.componentType;
				if (accessor.type != TINYGLTF_TYPE_VEC3)
					throw VERUS_RECOVERABLE << "Invalid type for NORMAL: " << accessor.type;
				if (byteStride != 12)
					throw VERUS_RECOVERABLE << "Invalid byteStride for NORMAL: " << byteStride;

				{
					StringStream ss;
					ss << _pCurrentMesh->GetName() << ": NORMAL";
					OnProgressText(_C(ss.str()));
				}

				const BYTE* p = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

				const int vertCount = Utils::Cast32(accessor.count);
				TryResizeVertsArray(vertCount);
				VERUS_FOR(vertIndex, vertCount)
				{
					const float* pSrc = reinterpret_cast<const float*>(p + byteStride * vertIndex);
					glm::vec3 nrm = glm::make_vec3(pSrc);
					nrm = glm::vec3(_pCurrentMesh->GetGlobalMatrix() * glm::vec4(nrm, 0));
					nrm = glm::normalize(nrm);
					if (_desc._flipNormals)
						nrm = -nrm;
					_pCurrentMesh->GetSetVertexAt(vertIndex)._nrm = nrm;
				}
				_pCurrentMesh->AddFoundFlag(Mesh::Found::normals);
			}
			if (attr.first == "TEXCOORD_0")
			{
				if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
					throw VERUS_RECOVERABLE << "Invalid componentType for TEXCOORD_0: " << accessor.componentType;
				if (accessor.type != TINYGLTF_TYPE_VEC2)
					throw VERUS_RECOVERABLE << "Invalid type for TEXCOORD_0: " << accessor.type;
				if (byteStride != 8)
					throw VERUS_RECOVERABLE << "Invalid byteStride for TEXCOORD_0: " << byteStride;

				{
					StringStream ss;
					ss << _pCurrentMesh->GetName() << ": TEXCOORD_0";
					OnProgressText(_C(ss.str()));
				}

				const BYTE* p = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

				const int vertCount = Utils::Cast32(accessor.count);
				TryResizeVertsArray(vertCount);
				VERUS_FOR(vertIndex, vertCount)
				{
					const float* pSrc = reinterpret_cast<const float*>(p + byteStride * vertIndex);
					const glm::vec2 tc = glm::make_vec2(pSrc);
					_pCurrentMesh->GetSetVertexAt(vertIndex)._tc0 = tc;
				}
				_pCurrentMesh->AddFoundFlag(Mesh::Found::posAndTexCoord0);
			}
			if (attr.first == "JOINTS_0")
			{
				if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
					throw VERUS_RECOVERABLE << "Invalid componentType for JOINTS_0: " << accessor.componentType;
				if (accessor.type != TINYGLTF_TYPE_VEC4)
					throw VERUS_RECOVERABLE << "Invalid type for JOINTS_0: " << accessor.type;
				if (byteStride != 4)
					throw VERUS_RECOVERABLE << "Invalid byteStride for JOINTS_0: " << byteStride;

				{
					StringStream ss;
					ss << _pCurrentMesh->GetName() << ": JOINTS_0";
					OnProgressText(_C(ss.str()));
				}

				const BYTE* p = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

				const int vertCount = Utils::Cast32(accessor.count);
				TryResizeVertsArray(vertCount);
				vBlendIndices.resize(vertCount);
				VERUS_FOR(vertIndex, vertCount)
				{
					VERUS_FOR(i, 4)
						vBlendIndices[vertIndex][i] = p[byteStride * vertIndex + i] + _pCurrentMesh->GetBoneCount();
				}
			}
			if (attr.first == "WEIGHTS_0")
			{
				if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
					throw VERUS_RECOVERABLE << "Invalid componentType for WEIGHTS_0: " << accessor.componentType;
				if (accessor.type != TINYGLTF_TYPE_VEC4)
					throw VERUS_RECOVERABLE << "Invalid type for WEIGHTS_0: " << accessor.type;
				if (byteStride != 16)
					throw VERUS_RECOVERABLE << "Invalid byteStride for WEIGHTS_0: " << byteStride;

				{
					StringStream ss;
					ss << _pCurrentMesh->GetName() << ": WEIGHTS_0";
					OnProgressText(_C(ss.str()));
				}

				const BYTE* p = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

				const int vertCount = Utils::Cast32(accessor.count);
				TryResizeVertsArray(vertCount);
				vBlendWeights.resize(vertCount);
				VERUS_FOR(vertIndex, vertCount)
				{
					const float* pSrc = reinterpret_cast<const float*>(p + byteStride * vertIndex);
					vBlendWeights[vertIndex] = glm::make_vec4(pSrc);
				}
			}

			if (!vBlendIndices.empty() && !vBlendWeights.empty())
			{
				VERUS_FOR(vertIndex, vBlendWeights.size())
				{
					VERUS_FOR(i, 4)
						_pCurrentMesh->GetSetVertexAt(vertIndex).Add(vBlendIndices[vertIndex][i], vBlendWeights[vertIndex][i]);
				}
				_pCurrentMesh->AddFoundFlag(Mesh::Found::skinning);
				vBlendIndices.clear();
				vBlendWeights.clear();
			}
		}
	}

	{
		StringStream ss;
		ss << _pCurrentMesh->GetName() << ": (" << _pCurrentMesh->GetVertCount() << " vertices, " << _pCurrentMesh->GetFaceCount() << " faces)";
		OnProgressText(_C(ss.str()));
	}

	AddMesh(_pCurrentMesh.get());
}

void ConvertGLTF::ProcessSkin(const tinygltf::Skin& skin)
{
	const auto& accessor = _model.accessors[skin.inverseBindMatrices];
	const auto& bufferView = _model.bufferViews[accessor.bufferView];
	const auto& buffer = _model.buffers[bufferView.buffer];
	const int byteStride = accessor.ByteStride(bufferView);

	if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
		throw VERUS_RECOVERABLE << "Invalid componentType for inverseBindMatrices: " << accessor.componentType;
	if (accessor.type != TINYGLTF_TYPE_MAT4)
		throw VERUS_RECOVERABLE << "Invalid type for inverseBindMatrices: " << accessor.type;
	if (byteStride != 64)
		throw VERUS_RECOVERABLE << "Invalid byteStride for inverseBindMatrices: " << byteStride;
	if (skin.joints.size() != accessor.count)
		throw VERUS_RECOVERABLE << "Joint count and inverseBindMatrix count dont's match: " << skin.joints.size() << " != " << accessor.count;

	const float* p = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset]);

	Vector<glm::mat4> vInverseBindMatrices;
	vInverseBindMatrices.resize(accessor.count);
	VERUS_FOR(index, accessor.count)
	{
		vInverseBindMatrices[index] = glm::make_mat4(p + index * 16);

		// Good time to adjust bone matrix:
		vInverseBindMatrices[index] = _pCurrentMesh->GetBoneAxisMatrix() * vInverseBindMatrices[index];
		vInverseBindMatrices[index] *= glm::inverse(_pCurrentMesh->GetGlobalMatrix());
		vInverseBindMatrices[index] = Transform3(vInverseBindMatrices[index]).Normalize(true).GLM();
	}

	VERUS_FOR(index, accessor.count)
	{
		const int nodeIndex = skin.joints[index];
		if (nodeIndex < 0 || nodeIndex >= _model.nodes.size())
			throw VERUS_RECOVERABLE << "Invalid joint node index: " << nodeIndex;
		const auto& node = _model.nodes[nodeIndex];

		Mesh::Bone bone;
		bone._name = RenameBone(_C(node.name));
		const int parent = _vNodeExtraData[nodeIndex]._parent;
		if (parent >= 0 && parent < _model.nodes.size())
			bone._parentName = RenameBone(_C(_model.nodes[parent].name));
		else
			bone._parentName = Anim::Skeleton::RootName();
		bone._trLocal = _vNodeExtraData[nodeIndex]._trLocal;
		bone._trGlobal = _vNodeExtraData[nodeIndex]._trGlobal;
		bone._trToBoneSpace = vInverseBindMatrices[index];
		_pCurrentMesh->AddBone(bone);
	}
	_pCurrentMesh->SetBoneCount(_pCurrentMesh->GetBoneCount() + Utils::Cast32(accessor.count));
}

void ConvertGLTF::ProcessAnimation(const tinygltf::Animation& an)
{
	const int animSetIndex = static_cast<int>(_vAnimSets.size() - 1);
	_vAnimSets[animSetIndex]._vAnimations.reserve(an.channels.size() / 3);

	StringStream ss;
	ss << _vAnimSets[animSetIndex]._name << ": Animation";
	OnProgressText(_C(ss.str()));

	int progress = 0;
	for (const auto& channel : an.channels)
	{
		OnProgress(float(progress) / an.channels.size() * 50);

		const int nodeIndex = channel.target_node;
		if (_vAnimSets[animSetIndex]._vAnimations.size() <= nodeIndex)
			_vAnimSets[animSetIndex]._vAnimations.resize(nodeIndex + 1);

		Animation& anim = _vAnimSets[animSetIndex]._vAnimations[nodeIndex];
		anim._name = _model.nodes[nodeIndex].name;

		if (!anim._vAnimKeys.capacity())
			anim._vAnimKeys.reserve(3);

		AnimationKey ak;
		if (channel.target_path == "translation")
			ak._type = 2;
		else if (channel.target_path == "rotation")
			ak._type = 0;
		else if (channel.target_path == "scale")
			ak._type = 1;

		const auto& sampler = an.samplers[channel.sampler];

		const auto& accessor = _model.accessors[sampler.output];
		const auto& bufferView = _model.bufferViews[accessor.bufferView];
		const auto& buffer = _model.buffers[bufferView.buffer];
		const int byteStride = accessor.ByteStride(bufferView);

		if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT)
			throw VERUS_RECOVERABLE << "Invalid componentType for sampler output: " << accessor.componentType;
		if (accessor.type != TINYGLTF_TYPE_VEC3 && accessor.type != TINYGLTF_TYPE_VEC4)
			throw VERUS_RECOVERABLE << "Invalid type for sampler output: " << accessor.type;
		if (byteStride != 12 && byteStride != 16)
			throw VERUS_RECOVERABLE << "Invalid byteStride for sampler output: " << byteStride;

		const float* p = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset]);

		ak._vFrame.resize(accessor.count);
		if (ak._type)
		{
			VERUS_FOR(index, accessor.count)
			{
				const glm::vec3 v = glm::make_vec3(p + index * 3);
				ak._vFrame[index]._q.setX(v.x);
				ak._vFrame[index]._q.setY(v.y);
				ak._vFrame[index]._q.setZ(v.z);
			}
		}
		else
		{
			VERUS_FOR(index, accessor.count)
			{
				const glm::quat q = glm::make_quat(p + index * 4);
				ak._vFrame[index]._q = q;
			}
		}

		if (anim._vAnimKeys.size() <= ak._type)
			anim._vAnimKeys.resize(ak._type + 1);
		anim._vAnimKeys[ak._type] = std::move(ak);

		progress++;
	}

	// Remove empty:
	_vAnimSets[animSetIndex]._vAnimations.erase(std::remove_if(
		_vAnimSets[animSetIndex]._vAnimations.begin(),
		_vAnimSets[animSetIndex]._vAnimations.end(),
		[](const Animation& anim)
		{
			return anim._vAnimKeys.empty();
		}),
		_vAnimSets[animSetIndex]._vAnimations.end());

	// Convert data to bone space:
	progress = 0;
	for (Animation& animWithKeys : _vAnimSets[animSetIndex]._vAnimations)
	{
		OnProgress(float(progress) / _vAnimSets[animSetIndex]._vAnimations.size() * 50 + 50);

		String boneName = RenameBone(_C(animWithKeys._name));
		Mesh::PBone pBone = _vMeshes[0]->FindBone(_C(boneName));
		Mesh::PBone pParentBone = nullptr;
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

				const glm::quat q = animWithKeys._vAnimKeys[0]._vFrame[index0]._q.GLM();
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
		progress++;
	}
}

void ConvertGLTF::TryResizeVertsArray(int vertCount)
{
	if (_pCurrentMesh->GetVertCount() > 0)
	{
		if (_pCurrentMesh->GetVertCount() != vertCount)
			throw VERUS_RECOVERABLE << "Different vertex count: " << vertCount;
	}
	else
	{
		_pCurrentMesh->SetVertCount(vertCount);
		_pCurrentMesh->ResizeVertsArray(vertCount);
	}
}

glm::vec3 ConvertGLTF::ToVec3(const double* p)
{
	glm::vec3 ret;
	VERUS_FOR(i, 3)
		ret[i] = static_cast<float>(p[i]);
	return ret;
}

glm::quat ConvertGLTF::ToQuat(const double* p)
{
	glm::quat ret;
	VERUS_FOR(i, 4)
		ret[i] = static_cast<float>(p[i]);
	return ret;
}

glm::mat4 ConvertGLTF::ToMat4(const double* p)
{
	glm::mat4 ret;
	VERUS_FOR(i, 4)
	{
		VERUS_FOR(j, 4)
			ret[i][j] = static_cast<float>(p[(i << 2) + j]);
	}
	return ret;
}

glm::mat4 ConvertGLTF::GetNodeMatrix(const tinygltf::Node& node)
{
	if (16 == node.matrix.size())
		return ToMat4(node.matrix.data());

	glm::quat rotation(1, 0, 0, 0);
	glm::vec3 scale(1, 1, 1);
	glm::vec3 translation(0, 0, 0);
	if (4 == node.rotation.size())
		rotation = ToQuat(node.rotation.data());
	if (3 == node.scale.size())
		scale = ToVec3(node.scale.data());
	if (3 == node.translation.size())
		translation = ToVec3(node.translation.data());

	const glm::mat4 matR = glm::mat4_cast(rotation);
	const glm::mat4 matS = glm::scale(scale);
	const glm::mat4 matT = glm::translate(translation);
	return matT * matR * matS;
}
