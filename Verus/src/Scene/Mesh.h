#pragma once

namespace verus
{
	namespace Scene
	{
		class Mesh : public BaseMesh
		{
		public:
#include "../Shaders/DS_Mesh.inc.hlsl"

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_ROBOTIC,
				PIPE_SKINNED,

				PIPE_INSTANCED,

				PIPE_DEPTH,
				PIPE_DEPTH_ROBOTIC,
				PIPE_DEPTH_SKINNED,

				PIPE_TESS,
				PIPE_TESS_ROBOTIC,
				PIPE_TESS_SKINNED,

				PIPE_WIREFRAME,
				PIPE_WIREFRAME_ROBOTIC,
				PIPE_WIREFRAME_SKINNED,

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
			static CGI::ShaderPwn                s_shader;
			static CGI::PipelinePwns<PIPE_COUNT> s_pipe;
			static UB_PerFrame                   s_ubPerFrame;
			static UB_PerMaterialFS              s_ubPerMaterialFS;
			static UB_PerMeshVS                  s_ubPerMeshVS;
			static UB_SkeletonVS                 s_ubSkeletonVS;
			static UB_PerObject                  s_ubPerObject;

			CGI::GeometryPwn        _geo;
			Vector<PerInstanceData> _vInstanceBuffer;
			int                     _instanceCapacity = 0;
			int                     _instanceCount = 0;
			UINT32                  _bindingsMask = 0;

		public:
			struct Desc
			{
				CSZ _url = nullptr;
				CSZ _warpUrl = nullptr;
				int _instanceCapacity = 1;

				Desc(CSZ url = nullptr) : _url(url) {}
			};
			VERUS_TYPEDEFS(Desc);

			Mesh();
			virtual ~Mesh();

			static void InitStatic();
			static void DoneStatic();

			void Init(RcDesc desc = Desc());
			void Done();

			void BindPipeline(PIPE pipe, CGI::CommandBufferPtr cb);
			void BindPipeline(CGI::CommandBufferPtr cb, bool allowTess = true);
			void BindGeo(CGI::CommandBufferPtr cb);
			void BindGeo(CGI::CommandBufferPtr cb, UINT32 bindingsFilter);

			static CGI::ShaderPtr GetShader() { return s_shader; }
			static UB_PerMaterialFS& GetUbPerMaterialFS() { return s_ubPerMaterialFS; }
			void UpdateUniformBufferPerFrame();
			void UpdateUniformBufferPerMaterialFS();
			void UpdateUniformBufferPerMeshVS();
			void UpdateUniformBufferSkeletonVS();
			void UpdateUniformBufferPerObject(RcTransform3 tr, RcVector4 color = Vector4(0.5f, 0.5f, 0.5f, 1));
			void UpdateUniformBufferPerObject(Point3 pos, RcVector4 color = Vector4(0.5f, 0.5f, 0.5f, 1));

			CGI::GeometryPtr GetGeometry() const { return _geo; }
			virtual void CreateDeviceBuffers() override;
			virtual void UpdateVertexBuffer(const void* p, int binding) override;

			// Instancing:
			void PushInstance(RcTransform3 matW, RcVector4 instData);
			bool IsInstanceBufferFull();
			bool IsInstanceBufferEmpty();
			void ResetInstanceCount();
			void UpdateInstanceBuffer();
			int GetInstanceCount() const { return _instanceCount; }
			int GetInstanceCapacity() const { return _instanceCapacity; }
		};
		VERUS_TYPEDEFS(Mesh);
	}
}
