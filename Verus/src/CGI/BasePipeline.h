#pragma once

namespace verus
{
	namespace CGI
	{
		struct PipelineDesc
		{
			GeometryPtr                _geometry;
			ShaderPtr                  _shader;
			CSZ                        _shaderBranch = nullptr;
			PipelineRasterizationState _rasterizationState;
			PrimitiveTopology          _topology = PrimitiveTopology::triangleList;
			int                        _sampleCount = 1;
			bool                       _primitiveRestartEnable = false;
			int                        _renderPassID = -1;

			PipelineDesc(GeometryPtr geo, ShaderPtr shader, CSZ branch, int renderPassID) :
				_geometry(geo), _shader(shader), _shaderBranch(branch), _renderPassID(renderPassID) {}
		};
		VERUS_TYPEDEFS(PipelineDesc);

		class BasePipeline : public Object
		{
		protected:
			BasePipeline() = default;
			virtual ~BasePipeline() = default;

		public:
			virtual void Init(RcPipelineDesc desc) = 0;
			virtual void Done() = 0;
		};
		VERUS_TYPEDEFS(BasePipeline);

		class PipelinePtr : public Ptr<BasePipeline>
		{
		public:
			void Init(RcPipelineDesc desc);
		};
		VERUS_TYPEDEFS(PipelinePtr);

		class PipelinePwn : public PipelinePtr
		{
		public:
			~PipelinePwn() { Done(); }
			void Done();
		};
		VERUS_TYPEDEFS(PipelinePwn);

		template<int NUM>
		class PipelinePwns : public Pwns<PipelinePwn, NUM>
		{
		};
	}
}