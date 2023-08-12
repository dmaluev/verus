// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Extra
{
	struct BaseConvertDelegate;

	class BaseConvert
	{
	public:
		class Mesh
		{
		public:
			enum class Found : int
			{
				null = 0,
				faces = (1 << 0),
				posAndTexCoord0 = (1 << 1),
				normals = (1 << 2),
				tangentSpace = (1 << 3),
				skinning = (1 << 4),
				texCoord1 = (1 << 5)
			};

			struct UberVertex
			{
				glm::vec3 _pos;
				glm::vec3 _nrm;
				glm::vec2 _tc0;
				glm::vec2 _tc1;
				glm::vec3 _tan;
				glm::vec3 _bin;
				UINT32    _bi[4];
				float     _bw[4];
				int       _currentWeight = 0;

				UberVertex();
				bool operator==(const UberVertex& that) const;
				void Add(UINT32 index, float weight);
				void CompileBits(UINT32& ii, UINT32& ww) const;
				UINT32 GetDominantIndex();
			};
			VERUS_TYPEDEFS(UberVertex);

			struct Face
			{
				UINT16 _indices[3];
			};
			VERUS_TYPEDEFS(Face);

			struct Bone
			{
				String    _name;
				String    _parentName;
				glm::mat4 _trLocal;
				glm::mat4 _trGlobal;
				glm::mat4 _trToBoneSpace; // Inverse of Global.
				bool      _usedInSkin = false;
			};
			VERUS_TYPEDEFS(Bone);

		private:
			struct Vec3Short
			{
				short _x;
				short _y;
				short _z;
			};

			struct Vec2Short
			{
				short _u;
				short _v;
			};

			struct Vec3Char
			{
				char _x;
				char _y;
				char _z;
			};

			struct Aabb
			{
				glm::vec3 _mn = glm::vec3(+FLT_MAX);
				glm::vec3 _mx = glm::vec3(-FLT_MAX);

				void Reset();
				void Include(const glm::vec3& point);
				glm::vec3 GetExtents() const;
			};

			BaseConvert* _pBaseConvert = nullptr;
			String             _name;
			String             _copyOf;
			Vector<UberVertex> _vUberVerts;
			Vector<Face>       _vFaces;
			Vector<Vec3Short>  _vZipPos;
			Vector<Vec3Char>   _vZipNormal;
			Vector<Vec3Char>   _vZipTan;
			Vector<Vec3Char>   _vZipBin;
			Vector<Vec2Short>  _vZipTc0;
			Vector<Vec2Short>  _vZipTc1;
			Vector<Bone>       _vBones;
			int                _vertCount = 0;
			int                _faceCount = 0;
			int                _boneCount = 0;
			glm::vec3          _posScale;
			glm::vec3          _posBias;
			glm::vec2          _tc0Scale;
			glm::vec2          _tc0Bias;
			glm::vec2          _tc1Scale;
			glm::vec2          _tc1Bias;
			glm::mat4          _matBoneAxis;
			glm::mat4          _matGlobal;
			glm::mat4          _worldSpace;
			Found              _found = Found::null;
			int                _materialIndex = 0;

		public:
			Mesh(BaseConvert* pBaseConvert);
			~Mesh();

			PBone FindBone(CSZ name);

			VERUS_P(void CleanBones());
			VERUS_P(void Optimize());
			VERUS_P(void RecalculateTangentSpace());
			VERUS_P(void Compress());
			void SerializeX3D3(IO::RFile file);

			RcString GetName() const { return _name; }
			void     SetName(CSZ name) { _name = name; }

			const glm::mat4& GetBoneAxisMatrix() const { return _matBoneAxis; }

			const glm::mat4& GetGlobalMatrix() const { return _matGlobal; }
			void             SetGlobalMatrix(const glm::mat4& mat) { _matGlobal = mat; }

			const glm::mat4& GetWorldSpaceMatrix() const { return _worldSpace; }
			void             SetWorldSpaceMatrix(const glm::mat4& mat) { _worldSpace = mat; }

			int GetVertCount() const { return _vertCount; }
			int GetFaceCount() const { return _faceCount; }
			int GetBoneCount() const { return _boneCount; }
			void SetVertCount(int count) { _vertCount = count; }
			void SetFaceCount(int count) { _faceCount = count; }
			void SetBoneCount(int count) { _boneCount = count; }

			void ResizeVertsArray(int size) { _vUberVerts.resize(size); }
			void ResizeFacesArray(int size) { _vFaces.resize(size); }

			RUberVertex GetSetVertexAt(int index) { return _vUberVerts[index]; }
			RFace       GetSetFaceAt(int index) { return _vFaces[index]; }

			void AddFoundFlag(Found flag) { _found |= flag; }

			int GetNextBoneIndex() const { return Utils::Cast32(_vBones.size()); }
			void AddBone(RcBone bone) { _vBones.push_back(bone); }

			bool IsCopyOf(Mesh& that);
			bool IsCopy() const { return !_copyOf.empty(); }
			RcString GetCopyOfName() const { return _copyOf; }

			int  GetMaterialIndex() const { return _materialIndex; }
			void SetMaterialIndex(int index) { _materialIndex = index; }
		};
		VERUS_TYPEDEFS(Mesh);

		struct SubKey
		{
			Quat _q;
			bool _redundant = false;
		};

		struct AnimationKey // Represents one bone's channel in motion file.
		{
			Vector<SubKey> _vFrame;
			int            _type = 0;
			int            _logicFrameCount = 0;

			void DetectRedundantFrames(float threshold = 0.0001f);
		};

		struct Animation // Represents one bone in motion file.
		{
			String               _name;
			Vector<AnimationKey> _vAnimKeys;
		};

		struct AnimationSet // Represents one motion file.
		{
			String            _name;
			Vector<Animation> _vAnimations;

			void CleanUp();
		};

	protected:
		typedef Map<String, String> TMapBoneNames;

		TMapBoneNames         _mapBoneNames;
		BaseConvertDelegate* _pDelegate = nullptr;
		Vector<PMesh>         _vMeshes;
		Vector<AnimationSet>  _vAnimSets;
		std::unique_ptr<Mesh> _pCurrentMesh;

	public:
		BaseConvert();
		~BaseConvert();

		void LoadBoneNames(CSZ pathname);
		String RenameBone(CSZ name);

	protected:
		PMesh AddMesh(PMesh pMesh);
		PMesh FindMesh(CSZ name);
		void DeleteAll();

		void OnProgress(float percent);
		void OnProgressText(CSZ txt);

		virtual bool UseAreaBasedNormals() { return false; }
		virtual bool UseRigidBones() { return false; }
		virtual Str GetPathname() { return ""; }

		void SerializeMotions(SZ pathname);
	};
	VERUS_TYPEDEFS(BaseConvert);

	struct BaseConvertDelegate
	{
		virtual void BaseConvert_OnProgress(float percent) = 0;
		virtual void BaseConvert_OnProgressText(CSZ txt) = 0;
		virtual void BaseConvert_Optimize(
			Vector<BaseConvert::Mesh::UberVertex>& vVB,
			Vector<BaseConvert::Mesh::Face>& vIB) = 0;
		virtual bool BaseConvert_CanOverwriteFile(CSZ filename) { return true; }
	};
	VERUS_TYPEDEFS(BaseConvertDelegate);
}
