#pragma once

namespace verus
{
	namespace Extra
	{
		struct FileParserDelegate;

		class FileParser : public Singleton<FileParser>, public Object
		{
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
				Mesh();
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

			struct Desc
			{
				glm::vec3 _bias = glm::vec3(0);
				float     _scaleFactor = 1;
				float     _angle = 0;
				float     _areaBasedNormals = 0;
				bool      _useRigidBones = false;
				bool      _convertAsLevel = false;
				bool      _useDefaultMaterial = false;
				bool      _rightHanded = true;
			};
			VERUS_TYPEDEFS(Desc);

		private:
			struct Frame
			{
				String    _name;
				String    _parent;
				glm::mat4 _mat;
				glm::mat4 _matAcc;
			};
			VERUS_TYPEDEFS(Frame);

			struct SubKey
			{
				Quat _q;
				bool _redundant = false;
			};

			struct AnimationKey
			{
				Vector<SubKey> _vFrame;
				int            _type = 0;
				int            _logicFrameCount = 0;

				void DetectRedundantFrames(float threshold = 0.001f);
			};

			struct Animation
			{
				String               _name;
				Vector<AnimationKey> _vAnimKeys;
			};

			struct AnimationSet
			{
				String            _name;
				Vector<Animation> _vAnimations;

				void CleanUp();
			};

			struct Material
			{
				String    _name;
				String    _copyOf;
				String    _textureFilename;
				glm::vec4 _faceColor;
				glm::vec3 _specularColor;
				glm::vec3 _emissiveColor;
				float     _power = 1;

				bool IsCopyOf(const Material& that)
				{
					const float e = 0.01f;
					return
						_textureFilename == that._textureFilename &&
						glm::all(glm::epsilonEqual(_faceColor, that._faceColor, e)) &&
						glm::all(glm::epsilonEqual(_specularColor, that._specularColor, e)) &&
						glm::all(glm::epsilonEqual(_emissiveColor, that._emissiveColor, e)) &&
						glm::epsilonEqual(_power, that._power, e);
				}
			};
			VERUS_TYPEDEFS(Material);

			typedef Map<String, String> TMapBoneNames;
			typedef Map<String, Frame> TMapFrames;

			Transform3            _matRoot;
			TMapBoneNames         _mapBoneNames;
			String                _currentMesh;
			String                _pathName;
			Vector<char>          _vData;
			const char* _pData;
			const char* _pDataBegin;
			Vector<PMesh>         _vMesh;
			std::unique_ptr<Mesh> _pCurrentMesh;
			TMapFrames            _mapFrames;
			Vector<Frame>         _stackFrames;
			Vector<AnimationSet> _vAnimSets;
			Vector<Material>      _vMaterials;
			FileParserDelegate* _pDelegate;
			std::future<void>     _future;
			Desc                  _desc;
			int                   _depth = 0;
			StringStream          _ssDebug;

		public:
			FileParser();
			~FileParser();

			void Init(FileParserDelegate* p, RcDesc desc);
			void Done();

			void LoadBoneNames(CSZ pathName);
			void ParseData(CSZ pathName);
			void SerializeAll(CSZ pathName);

			void AsyncRun(CSZ pathName);
			void AsyncJoin();
			bool IsAsyncStarted() const;
			bool IsAsyncFinished() const;

		private:
			void LoadFromFile(CSZ pathName);

			void StreamReadUntil(SZ dest, int destSize, CSZ separator);
			void StreamSkipWhitespace();
			void StreamSkipUntil(char c);

			void FixBones();

			void ParseBlockRecursive(CSZ type, CSZ blockName);
			void ParseBlockData_Mesh();
			void ParseBlockData_MeshTextureCoords();
			void ParseBlockData_MeshNormals();
			void ParseBlockData_FVFData(bool declData);
			void ParseBlockData_XSkinMeshHeader();
			void ParseBlockData_SkinWeights();
			void ParseBlockData_Frame(CSZ blockName);
			void ParseBlockData_FrameTransformMatrix();
			void ParseBlockData_AnimationSet(CSZ blockName);
			void ParseBlockData_Animation(CSZ blockName);
			void ParseBlockData_AnimationKey();
			void ParseBlockData_Material();
			void ParseBlockData_TextureFilename();
			void ParseBlockData_MeshMaterialList();

			PMesh AddMesh(PMesh pMesh);
			PMesh FindMesh(CSZ name);
			void DeleteAll();

			void OnProgress(float percent);
			void OnProgressText(CSZ txt);

			void Debug(CSZ txt);

			void DetectMaterialCopies();
			String GetXmlMaterial(int i);
			String RenameBone(CSZ name);
		};
		VERUS_TYPEDEFS(FileParser);

		struct FileParserDelegate
		{
			virtual void FileParser_OnProgress(float percent) = 0;
			virtual void FileParser_OnProgressText(CSZ txt) = 0;
			virtual void FileParser_Optimize(
				Vector<FileParser::Mesh::UberVertex>& vVb,
				Vector<FileParser::Mesh::Face>& vIb) = 0;
			virtual bool FileParser_CanOverwriteFile(CSZ filename) { return true; }
		};
	}
}
