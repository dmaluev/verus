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
			static UB_Skinning          s_ubSkinning;
			static UB_PerObject         s_ubPerObject;

			CGI::GeometryPwn        _geo;
			Vector<PerInstanceData> _vInstanceBuffer;
			int                     _maxNumInstances = 0;
			int                     _numInstances = 0;
			UINT32                  _bindingsMask = 0;

		public:
			struct Desc
			{
				CSZ _url = nullptr;
				int _maxNumInstances = 1;

				Desc(CSZ url = nullptr) : _url(url) {}
			};
			VERUS_TYPEDEFS(Desc);

			Mesh();
			virtual ~Mesh();

			static void InitStatic();
			static void DoneStatic();

			void Init(RcDesc desc = Desc());
			void Done();

			void Bind(CGI::CommandBufferPtr cb, UINT32 bindingsFilter);

			static CGI::ShaderPtr GetShader() { return s_shader; }
			void UpdateUniformBufferPerFrame(Scene::RcCamera cam);
			void UpdateUniformBufferPerMaterial();
			void UpdateUniformBufferPerMesh();
			void UpdateUniformBufferSkinning();
			void UpdateUniformBufferPerObject(Point3 pos);

			CGI::GeometryPtr GetGeometry() const { return _geo; }
			virtual void CreateDeviceBuffers() override;
			virtual void BufferDataVB(const void* p, int binding) override;

			// Instancing:
			void PushInstance(RcTransform3 matW, RcVector4 instData);
			bool IsInstanceBufferFull();
			bool IsInstanceBufferEmpty();
			void ResetNumInstances();
			void UpdateInstanceBuffer();
			int GetNumInstances() const { return _numInstances; }
			int GetMaxNumInstances() const { return _maxNumInstances; }
		};
		VERUS_TYPEDEFS(Mesh);
	}
}
