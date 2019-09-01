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
			int                        _renderPassID = -1;
			int                        _subpass = 0;
			UINT32                     _vertexInputBindingsFilter = UINT32_MAX;
			CompareOp                  _depthCompareOp = CompareOp::less;
			bool                       _depthTestEnable = true;
			bool                       _depthWriteEnable = true;
			bool                       _stencilTestEnable = false;
			bool                       _primitiveRestartEnable = false;
			bool                       _compute = false;

			PipelineDesc(GeometryPtr geo, ShaderPtr shader, CSZ branch, int renderPassID) :
				_geometry(geo), _shader(shader), _shaderBranch(branch), _renderPassID(renderPassID) {}
			PipelineDesc(ShaderPtr shader, CSZ branch) :
				_shader(shader), _shaderBranch(branch), _compute(true) {}
		};
		VERUS_TYPEDEFS(PipelineDesc);

		class BasePipeline : public Object
		{
		protected:
			UINT32 _vertexInputBindingsFilter = UINT32_MAX;

			BasePipeline() = default;
			virtual ~BasePipeline() = default;

		public:
			virtual void Init(RcPipelineDesc desc) = 0;
			virtual void Done() = 0;

			UINT32 GetVertexInputBindingsFilter() const { return _vertexInputBindingsFilter; }
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
