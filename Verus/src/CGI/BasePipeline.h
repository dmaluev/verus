// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
			String                     _colorAttachBlendEqs[VERUS_MAX_RT];
			String                     _colorAttachWriteMasks[VERUS_MAX_RT];
			PipelineRasterizationState _rasterizationState;
			PrimitiveTopology          _topology = PrimitiveTopology::triangleList;
			int                        _sampleCount = 1;
			RPHandle                   _renderPassHandle;
			int                        _subpass = 0;
			int                        _multiViewport = 1; // See SV_ViewportArrayIndex semantic output by a geometry shader.
			UINT32                     _vertexInputBindingsFilter = UINT32_MAX;
			CompareOp                  _depthCompareOp = CompareOp::less;
			bool                       _depthTestEnable = true;
			bool                       _depthWriteEnable = true;
			bool                       _stencilTestEnable = false;
			bool                       _primitiveRestartEnable = false; // Special index value is 0xFFFFFFFF for 32-bit and 0xFFFF for 16-bit indices.
			bool                       _compute = false; // Compute pipeline, use PipelineDesc(ShaderPtr, CSZ) to set this to true.

			// What to draw (geo)? How to draw (shader)? Where to draw (render pass)?
			PipelineDesc(GeometryPtr geo, ShaderPtr shader, CSZ branch, RPHandle renderPassHandle, int subpass = 0) :
				_geometry(geo), _shader(shader), _shaderBranch(branch), _renderPassHandle(renderPassHandle), _subpass(subpass)
			{
				_colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_OFF;
				VERUS_FOR(i, VERUS_COUNT_OF(_colorAttachWriteMasks))
					_colorAttachWriteMasks[i] = "rgba";
			}
			PipelineDesc(ShaderPtr shader, CSZ branch) :
				_shader(shader), _shaderBranch(branch), _compute(true) {}

			void DisableDepthTest()
			{
				_depthTestEnable = false;
				_depthWriteEnable = false;
			}

			void EnableDepthBias()
			{
				_rasterizationState._depthBiasEnable = true;
				_rasterizationState._depthBiasConstantFactor = 14;
				_rasterizationState._depthBiasSlopeFactor = 1.1f;
				if (App::Settings::I()._sceneShadowQuality >= App::Settings::ShadowQuality::cascaded)
					_rasterizationState._depthBiasConstantFactor = 50;
			}
		};
		VERUS_TYPEDEFS(PipelineDesc);

		class BasePipeline : public Object, public Scheduled
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

		template<int COUNT>
		class PipelinePwns : public Pwns<PipelinePwn, COUNT>
		{
		};
	}
}
