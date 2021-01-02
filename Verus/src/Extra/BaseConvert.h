// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Extra
	{
		struct BaseConvertDelegate;

		class BaseConvert
		{
		protected:
			BaseConvertDelegate* _pDelegate = nullptr;

		public:
			class Mesh
			{
			public:
				enum class Found : int
				{
					null = 0,
					faces = (1 << 0),
					vertsAndUVs = (1 << 1),
					normals = (1 << 2),
					boneHierarchy = (1 << 3),
					skinning = (1 << 4),
					tangentSpace = (1 << 5),
					texcoordExtra = (1 << 6)
				};

				struct UberVertex
				{
					glm::vec3 _pos;
					glm::vec3 _nrm;
					glm::vec2 _tc0;
					glm::vec2 _tc1;
					glm::vec3 _tan;
					glm::vec3 _bin;
					float     _bw[4];
					UINT32    _bi[4];
					int       _currentWeight = 0;

					UberVertex();
					bool operator==(const UberVertex& that) const;
					void Add(float weight, UINT32 index);
					void CompileBits(UINT32& ww, UINT32& ii) const;
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
					glm::mat4 _matOffset;
					glm::mat4 _matFrame;
					glm::mat4 _matFrameAcc;
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
				glm::mat4          _matCombined;
				glm::mat4          _worldSpace;
				Found              _found = Found::null;
				int                _materialIndex = 0;

			public:
				Mesh(BaseConvert* pBaseConvert);
				~Mesh();

				PBone FindBone(CSZ name);

				VERUS_P(void Optimize());
				VERUS_P(void ComputeTangentSpace());
				VERUS_P(void Compress());
				void SerializeX3D3(IO::RFile file);

				RcString GetName() const { return _name; }
				void     SetName(CSZ name) { _name = name; }

				const glm::mat4& GetBoneAxisMatrix() const { return _matBoneAxis; }

				const glm::mat4& GetCombinedMatrix() const { return _matCombined; }
				void             SetCombinedMatrix(const glm::mat4& mat) { _matCombined = mat; }

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

		public:
			BaseConvert();
			~BaseConvert();

			void OnProgress(float percent);
			void OnProgressText(CSZ txt);

			virtual bool UseAreaBasedNormals() { return false; }
			virtual bool UseRigidBones() { return false; }
			virtual Str GetPathname() { return ""; }
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
}
