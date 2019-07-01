#pragma once

namespace verus
{
	namespace Scene
	{
		class Mesh : public BaseMesh
		{
		public:
#include "../Shaders/DS_Mesh.inc.hlsl"

			struct PerInstanceData
			{
				Vector4 _matPart0 = Vector4(0);
				Vector4 _matPart1 = Vector4(0);
				Vector4 _matPart2 = Vector4(0);
				Vector4 _instData = Vector4(0);
			};

		private:
			static CGI::ShaderPwn       s_shader;
			static CGI::PipelinePwns<1> s_pipe;
			static UB_PerFrame          s_ubPerFrame;
			static UB_PerMaterial       s_ubPerMaterial;
			static UB_PerMesh           s_ubPerMesh;
			static UB_PerObject         s_ubPerObject;

			CGI::GeometryPwn _geo;
			UINT32           _bindingMask = 0;

		public:
			Mesh();
			virtual ~Mesh();

			static void InitStatic();
			static void DoneStatic();

			void Init();
			void Done();

			void Bind(CGI::CommandBufferPtr cb, UINT32 bindingsFilter);

			static CGI::ShaderPtr GetShader() { return s_shader; }
			void UpdateUniformBufferPerObject(Point3 pos);
			void UpdateUniformBufferPerMesh();
			void UpdateUniformBufferPerMaterial();
			void UpdateUniformBufferPerFrame(Scene::RcCamera cam);

			CGI::GeometryPtr GetGeometry() const { return _geo; }
			virtual void CreateDeviceBuffers() override;
			virtual void BufferDataVB(const void* p, int binding) override;
		};
	}
}
