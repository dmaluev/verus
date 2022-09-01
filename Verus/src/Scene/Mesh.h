// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class Mesh : public BaseMesh
		{
		public:
#include "../Shaders/DS_Mesh.inc.hlsl"
#include "../Shaders/SimpleMesh.inc.hlsl"

			enum SHADER
			{
				SHADER_MAIN,
				SHADER_SIMPLE,
				SHADER_COUNT
			};

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_INSTANCED,
				PIPE_ROBOTIC,
				PIPE_SKINNED,
				PIPE_PLANT,

				PIPE_TESS,
				PIPE_TESS_INSTANCED,
				PIPE_TESS_ROBOTIC,
				PIPE_TESS_SKINNED,
				PIPE_TESS_PLANT,

				PIPE_DEPTH,
				PIPE_DEPTH_INSTANCED,
				PIPE_DEPTH_ROBOTIC,
				PIPE_DEPTH_SKINNED,
				PIPE_DEPTH_PLANT,

				PIPE_DEPTH_TESS,
				PIPE_DEPTH_TESS_INSTANCED,
				PIPE_DEPTH_TESS_ROBOTIC,
				PIPE_DEPTH_TESS_SKINNED,
				PIPE_DEPTH_TESS_PLANT,

				PIPE_WIREFRAME,
				PIPE_WIREFRAME_INSTANCED,
				PIPE_WIREFRAME_ROBOTIC,
				PIPE_WIREFRAME_SKINNED,

				PIPE_SIMPLE_ENV_MAP,
				PIPE_SIMPLE_ENV_MAP_INSTANCED,
				PIPE_SIMPLE_ENV_MAP_ROBOTIC,
				PIPE_SIMPLE_ENV_MAP_SKINNED,

				PIPE_SIMPLE_PLANAR_REF,
				PIPE_SIMPLE_PLANAR_REF_INSTANCED,
				PIPE_SIMPLE_PLANAR_REF_ROBOTIC,
				PIPE_SIMPLE_PLANAR_REF_SKINNED,

				PIPE_SIMPLE_TEX_ADD,
				PIPE_SIMPLE_TEX_ADD_INSTANCED,
				PIPE_SIMPLE_TEX_ADD_ROBOTIC,
				PIPE_SIMPLE_TEX_ADD_SKINNED,

				PIPE_SIMPLE_PLASMA_FX,

				PIPE_COUNT
			};

			struct PerInstanceData
			{
				Vector4 _matPart0 = Vector4(0);
				Vector4 _matPart1 = Vector4(0);
				Vector4 _matPart2 = Vector4(0);
				Vector4 _instData = Vector4(0);
			};

		private:
			static CGI::ShaderPwns<SHADER_COUNT> s_shader;
			static CGI::PipelinePwns<PIPE_COUNT> s_pipe;
			static UB_PerFrame                   s_ubPerFrame;
			static UB_PerMaterialFS              s_ubPerMaterialFS;
			static UB_PerMeshVS                  s_ubPerMeshVS;
			static UB_SkeletonVS                 s_ubSkeletonVS;
			static UB_PerObject                  s_ubPerObject;
			static UB_SimplePerFrame             s_ubSimplePerFrame;
			static UB_SimplePerMaterialFS        s_ubSimplePerMaterialFS;
			static UB_SimplePerMeshVS            s_ubSimplePerMeshVS;
			static UB_SimpleSkeletonVS           s_ubSimpleSkeletonVS;
			static UB_SimplePerObject            s_ubSimplePerObject;

			CGI::GeometryPwn        _geo;
			Vector<PerInstanceData> _vInstanceBuffer;
			int                     _instanceCapacity = 0;
			int                     _instanceCount = 0;
			int                     _firstInstance = 0;
			UINT32                  _bindingsMask = 0;

		public:
			struct Desc
			{
				CSZ  _url = nullptr;
				CSZ  _warpUrl = nullptr;
				int  _instanceCapacity = 0;
				bool _initShape = false;

				Desc(CSZ url = nullptr) : _url(url) {}
			};
			VERUS_TYPEDEFS(Desc);

			struct DrawDesc
			{
				Transform3    _matW = Transform3::identity();
				Vector4       _userColor = Vector4(0);
				CGI::CSHandle _cshMaterial;
				PcVector4     _pOverrideFogColor = nullptr;
				PIPE          _pipe = PIPE_COUNT;
				bool          _allowTess = true;
				bool          _bindMaterial = true;
				bool          _bindPipeline = true;
				bool          _bindSkeleton = true;
			};
			VERUS_TYPEDEFS(DrawDesc);

			Mesh();
			virtual ~Mesh();

			static void InitStatic();
			static void DoneStatic();

			void Init(RcDesc desc = Desc());
			void Done();

			void Draw(RcDrawDesc drawDesc, CGI::CommandBufferPtr cb);
			void DrawSimple(RcDrawDesc drawDesc, CGI::CommandBufferPtr cb);

			void BindPipeline(PIPE pipe, CGI::CommandBufferPtr cb);
			void BindPipeline(CGI::CommandBufferPtr cb, bool allowTess = true);
			void BindPipelineInstanced(CGI::CommandBufferPtr cb, bool allowTess = true, bool plant = false);
			void BindGeo(CGI::CommandBufferPtr cb, UINT32 bindingsFilter = 0);

			static CGI::ShaderPtr GetShader() { return s_shader[SHADER_MAIN]; }
			static UB_PerMaterialFS& GetUbPerMaterialFS() { return s_ubPerMaterialFS; }
			void UpdateUniformBufferPerFrame(float invTessDist = 0);
			void UpdateUniformBufferPerMeshVS();
			void UpdateUniformBufferSkeletonVS();
			void UpdateUniformBufferPerObject(RcTransform3 tr, RcVector4 color = Vector4(0.5f, 0.5f, 0.5f, 1));

			static CGI::ShaderPtr GetSimpleShader() { return s_shader[SHADER_SIMPLE]; }
			static UB_SimplePerMaterialFS& GetUbSimplePerMaterialFS() { return s_ubSimplePerMaterialFS; }
			void UpdateUniformBufferSimplePerFrame(DrawSimpleMode mode);
			void UpdateUniformBufferSimpleSkeletonVS();

			CGI::GeometryPtr GetGeometry() const { return _geo; }
			virtual void CreateDeviceBuffers() override;
			virtual void UpdateVertexBuffer(const void* p, int binding) override;

			// Instancing:
			void ResetInstanceCount();
			void MarkFirstInstance() { _firstInstance = _instanceCount; }
			void PushInstance(RcTransform3 matW, RcVector4 instData);
			bool IsInstanceBufferFull();
			bool IsInstanceBufferEmpty(bool fromFirstInstance = false);

			void UpdateInstanceBuffer();
			int GetInstanceCount(bool fromFirstInstance = false) const { return fromFirstInstance ? _instanceCount - _firstInstance : _instanceCount; }
			int GetInstanceCapacity() const { return _instanceCapacity; }
			int GetFirstInstance() const { return _firstInstance; }

			UINT32 GetBindingsMask() const { return _bindingsMask; }
		};
		VERUS_TYPEDEFS(Mesh);
	}
}
