// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
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

			enum class InstanceBufferFormat : int
			{
				pid0_mataff_float4,
				pid0_mataff_float4_pid1_float4,
				pid0_mataff_float4_pid1_mataff_float4,
				pid0_mataff_float4_pid1_matrix_float4
			};

		private:
			static CGI::ShaderPwns<SHADER_COUNT> s_shader;
			static CGI::PipelinePwns<PIPE_COUNT> s_pipe;

			static UB_View                       s_ubView;
			static UB_MaterialFS                 s_ubMaterialFS;
			static UB_MeshVS                     s_ubMeshVS;
			static UB_SkeletonVS                 s_ubSkeletonVS;
			static UB_Object                     s_ubObject;

			static UB_SimpleView                 s_ubSimpleView;
			static UB_SimpleMaterialFS           s_ubSimpleMaterialFS;
			static UB_SimpleMeshVS               s_ubSimpleMeshVS;
			static UB_SimpleSkeletonVS           s_ubSimpleSkeletonVS;
			static UB_SimpleObject               s_ubSimpleObject;

			CGI::GeometryPwn     _geo;
			Vector<BYTE>         _vInstanceBuffer;
			Vector<Vector<BYTE>> _vStorageBuffers;
			int                  _instanceCapacity = 0;
			int                  _instanceCount = 0;
			int                  _markedInstance = 0;
			InstanceBufferFormat _instanceBufferFormat = InstanceBufferFormat::pid0_mataff_float4;
			UINT32               _bindingsMask = 0;

		public:
			struct Desc
			{
				CSZ                  _url = nullptr;
				CSZ                  _warpURL = nullptr;
				int                  _instanceCapacity = 0;
				InstanceBufferFormat _instanceBufferFormat = InstanceBufferFormat::pid0_mataff_float4;
				bool                 _initShape = false;

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
			void Init(RcSourceBuffers sourceBuffers, RcDesc desc = Desc());
			void Done();

			void Draw(RcDrawDesc drawDesc, CGI::CommandBufferPtr cb);
			void DrawSimple(RcDrawDesc drawDesc, CGI::CommandBufferPtr cb);

			void BindPipeline(PIPE pipe, CGI::CommandBufferPtr cb);
			void BindPipeline(CGI::CommandBufferPtr cb, bool allowTess = true);
			void BindPipelineInstanced(CGI::CommandBufferPtr cb, bool allowTess = true, bool plant = false);
			void BindGeo(CGI::CommandBufferPtr cb, UINT32 bindingsFilter = 0);

			static CGI::ShaderPtr GetShader() { return s_shader[SHADER_MAIN]; }
			static UB_MaterialFS& GetUbMaterialFS() { return s_ubMaterialFS; }
			void UpdateUniformBuffer_View(float invTessDist = 0);
			void UpdateUniformBuffer_MeshVS();
			void UpdateUniformBuffer_SkeletonVS();
			void UpdateUniformBuffer_Object(RcTransform3 tr, RcVector4 color = Vector4(0.5f, 0.5f, 0.5f, 1));

			static CGI::ShaderPtr GetSimpleShader() { return s_shader[SHADER_SIMPLE]; }
			static UB_SimpleMaterialFS& GetUbSimpleMaterialFS() { return s_ubSimpleMaterialFS; }
			void UpdateUniformBuffer_SimpleView(DrawSimpleMode mode);
			void UpdateUniformBuffer_SimpleSkeletonVS();

			CGI::GeometryPtr GetGeometry() const { return _geo; }
			virtual void CreateDeviceBuffers() override;
			virtual void UpdateVertexBuffer(const void* p, int binding) override;
			void CreateStorageBuffer(int count, int structSize, int sbIndex, CGI::ShaderStageFlags stageFlags);

			// <Instancing>
			void ResetInstanceCount();
			void MarkInstance() { _markedInstance = _instanceCount; }
			int GetMarkedInstance() const { return _markedInstance; }
			int GetInstanceSize() const;
			int GetInstanceCount(bool fromMarkedInstance = false) const { return fromMarkedInstance ? _instanceCount - _markedInstance : _instanceCount; }
			int GetInstanceCapacity() const { return _instanceCapacity; }
			bool IsInstanceBufferEmpty(bool fromMarkedInstance = false) const;
			bool IsInstanceBufferFull() const;
			VERUS_P(bool PushInstanceCheck() const);
			void PushStorageBufferData(int sbIndex, const void* p, bool incInstanceCount = false);
			void PushInstance(RcTransform3 matW, RcVector4 instData);
			void PushInstance(RcTransform3 matW, RcVector4 instData, RcVector4 pid1_instData);
			void PushInstance(RcTransform3 matW, RcVector4 instData, RcTransform3 pid1_mat, RcVector4 pid1_instData);
			void PushInstance(RcTransform3 matW, RcVector4 instData, RcMatrix4 pid1_mat, RcVector4 pid1_instData);
			void UpdateStorageBuffer(int sbIndex);
			void UpdateInstanceBuffer();
			// </Instancing>

			UINT32 GetBindingsMask() const { return _bindingsMask; }
		};
		VERUS_TYPEDEFS(Mesh);
	}
}
