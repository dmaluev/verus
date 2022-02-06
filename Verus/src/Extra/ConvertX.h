// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Extra
	{
		class ConvertX : public Singleton<ConvertX>, public Object, public BaseConvert
		{
		public:
			struct Desc
			{
				glm::vec3 _bias = glm::vec3(0);
				float     _scaleFactor = 1;
				float     _angle = 0;
				float     _areaBasedNormals = 0;
				bool      _useRigidBones = false;
				bool      _convertAsScene = false;
				bool      _useDefaultMaterial = false;
				bool      _rightHanded = true;
			};
			VERUS_TYPEDEFS(Desc);

		private:
			struct Frame
			{
				String    _name;
				String    _parentName;
				glm::mat4 _trLocal;
				glm::mat4 _trGlobal;
			};
			VERUS_TYPEDEFS(Frame);

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

			typedef Map<String, Frame> TMapFrames;

			Transform3            _trRoot;
			String                _currentMesh;
			String                _pathname;
			Vector<char>          _vData;
			const char* _pData;
			const char* _pDataBegin;
			TMapFrames            _mapFrames;
			Vector<Frame>         _stackFrames;
			Vector<Material>      _vMaterials;
			std::future<void>     _future;
			Desc                  _desc;
			int                   _depth = 0;
			StringStream          _ssDebug;

		public:
			ConvertX();
			~ConvertX();

			void Init(PBaseConvertDelegate p, RcDesc desc);
			void Done();

			virtual bool UseAreaBasedNormals() override;
			virtual bool UseRigidBones() override;
			virtual Str GetPathname() override;

			void ParseData(CSZ pathname);
			void SerializeAll(CSZ pathname);

			void AsyncRun(CSZ pathname);
			void AsyncJoin();
			bool IsAsyncStarted() const;
			bool IsAsyncFinished() const;

		private:
			void LoadFromFile(CSZ pathname);

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

			void OnProgress(float percent);
			void OnProgressText(CSZ txt);

			void Debug(CSZ txt);

			void DetectMaterialCopies();
			String GetXmlMaterial(int i);
		};
		VERUS_TYPEDEFS(ConvertX);
	}
}
