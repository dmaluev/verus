// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class PipelineD3D12 : public BasePipeline
		{
			ComPtr<ID3D12PipelineState> _pPipelineState;
			ID3D12RootSignature* _pRootSignature = nullptr;
			D3D_PRIMITIVE_TOPOLOGY      _topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
			bool                        _compute = false;

		public:
			PipelineD3D12();
			virtual ~PipelineD3D12() override;

			virtual void Init(RcPipelineDesc desc) override;
			virtual void Done() override;

			//
			// D3D12
			//

			VERUS_P(void InitCompute(RcPipelineDesc desc));
			VERUS_P(void InitMeshShading(RcPipelineDesc desc));
			D3D12_SHADER_BYTECODE ToBytecode(ID3DBlob* pBlob);
			bool IsCompute() const { return _compute; }
			ID3D12PipelineState* GetD3DPipelineState() const { return _pPipelineState.Get(); }
			ID3D12RootSignature* GetD3DRootSignature() const { return _pRootSignature; }
			D3D_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const { return _topology; }
			void FillBlendStateRenderTargets(RcPipelineDesc desc, int attachmentCount, D3D12_BLEND_DESC& blendDesc);
		};
		VERUS_TYPEDEFS(PipelineD3D12);
	}
}
