// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	class PipelineD3D11 : public BasePipeline
	{
		ComPtr<ID3D11VertexShader>      _pVS;
		ComPtr<ID3D11HullShader>        _pHS;
		ComPtr<ID3D11DomainShader>      _pDS;
		ComPtr<ID3D11GeometryShader>    _pGS;
		ComPtr<ID3D11PixelShader>       _pPS;
		ComPtr<ID3D11ComputeShader>     _pCS;
		ComPtr<ID3D11BlendState>        _pBlendState;
		ComPtr<ID3D11RasterizerState>   _pRasterizerState;
		ComPtr<ID3D11DepthStencilState> _pDepthStencilState;
		ComPtr<ID3D11InputLayout>       _pInputLayout;
		UINT                            _sampleMask = UINT_MAX;
		UINT                            _stencilRef = 0;
		D3D_PRIMITIVE_TOPOLOGY          _topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
		bool                            _compute = false;

	public:
		PipelineD3D11();
		virtual ~PipelineD3D11() override;

		virtual void Init(RcPipelineDesc desc) override;
		virtual void Done() override;

		//
		// D3D11
		//

		VERUS_P(void InitCompute(RcPipelineDesc desc));
		bool IsCompute() const { return _compute; }
		ID3D11VertexShader* GetD3DVS() const { return _pVS.Get(); }
		ID3D11HullShader* GetD3DHS() const { return _pHS.Get(); }
		ID3D11DomainShader* GetD3DDS() const { return _pDS.Get(); }
		ID3D11GeometryShader* GetD3DGS() const { return _pGS.Get(); }
		ID3D11PixelShader* GetD3DPS() const { return _pPS.Get(); }
		ID3D11ComputeShader* GetD3DCS() const { return _pCS.Get(); }
		ID3D11BlendState* GetD3DBlendState() const { return _pBlendState.Get(); }
		ID3D11RasterizerState* GetD3DRasterizerState() const { return _pRasterizerState.Get(); }
		ID3D11DepthStencilState* GetD3DDepthStencilState() const { return _pDepthStencilState.Get(); }
		ID3D11InputLayout* GetD3DInputLayout() const { return _pInputLayout.Get(); }
		UINT GetSampleMask() const { return _sampleMask; }
		UINT GetStencilRef() const { return _stencilRef; }
		D3D_PRIMITIVE_TOPOLOGY GetD3DPrimitiveTopology() const { return _topology; }
		void FillBlendStateRenderTargets(RcPipelineDesc desc, int attachmentCount, D3D11_BLEND_DESC& blendDesc);
	};
	VERUS_TYPEDEFS(PipelineD3D11);
}
