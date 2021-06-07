// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Extra
	{
		class ConvertGLTF : public Singleton<ConvertGLTF>, public Object, public BaseConvert
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
			};
			VERUS_TYPEDEFS(Desc);

		private:
			struct NodeExtraData
			{
				int       _parent = -1;
				glm::mat4 _trLocal;
				glm::mat4 _trGlobal;
			};

			tinygltf::TinyGLTF    _context;
			tinygltf::Model       _model;
			String                _pathname;
			Vector<char>          _vData;
			Vector<NodeExtraData> _vNodeExtraData;
			std::future<void>     _future;
			Desc                  _desc;

		public:
			ConvertGLTF();
			~ConvertGLTF();

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

			void ProcessNodeRecursive(const tinygltf::Node& node, int nodeIndex, bool computeExtraData);
			void ProcessMesh(const tinygltf::Mesh& mesh, int nodeIndex, int skinIndex);
			void ProcessSkin(const tinygltf::Skin& skin);
			void ProcessAnimation(const tinygltf::Animation& anim);

			void TryResizeVertsArray(int vertCount);

			static glm::vec3 ToVec3(const double* p);
			static glm::quat ToQuat(const double* p);
			static glm::mat4 ToMat4(const double* p);
			static glm::mat4 GetNodeMatrix(const tinygltf::Node& node);
		};
		VERUS_TYPEDEFS(ConvertGLTF);
	}
}
