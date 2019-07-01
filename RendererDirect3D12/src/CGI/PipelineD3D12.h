#pragma once

namespace verus
{
	namespace CGI
	{
		class PipelineD3D12 : public BasePipeline
		{
			ComPtr<ID3D12PipelineState> _pPipelineState;
			ID3D12RootSignature*        _pRootSignature = nullptr;
			D3D_PRIMITIVE_TOPOLOGY      _topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

		public:
			PipelineD3D12();
			virtual ~PipelineD3D12() override;

			virtual void Init(RcPipelineDesc desc) override;
			virtual void Done() override;

			//
			// D3D12
			//

			ID3D12PipelineState* GetD3DPipelineState() const { return _pPipelineState.Get(); }
			ID3D12RootSignature* GetD3DRootSignature() const { return _pRootSignature; }
			D3D_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const { return _topology; }
		};
		VERUS_TYPEDEFS(PipelineD3D12);
	}
}
