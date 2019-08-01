#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

PipelineD3D12::PipelineD3D12()
{
}

PipelineD3D12::~PipelineD3D12()
{
	Done();
}

void PipelineD3D12::Init(RcPipelineDesc desc)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	_vertexInputBindingsFilter = desc._vertexInputBindingsFilter;

	RcGeometryD3D12 geo = static_cast<RcGeometryD3D12>(*desc._geometry);
	RcShaderD3D12 shader = static_cast<RcShaderD3D12>(*desc._shader);

	_pRootSignature = shader.GetD3DRootSignature();
	_topology = ToNativePrimitiveTopology(desc._topology);

	auto ToBytecode = [](ID3DBlob* pBlob) -> D3D12_SHADER_BYTECODE
	{
		D3D12_SHADER_BYTECODE bytecode = {};
		if (pBlob)
		{
			bytecode.pShaderBytecode = pBlob->GetBufferPointer();
			bytecode.BytecodeLength = pBlob->GetBufferSize();
		}
		return bytecode;
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.pRootSignature = _pRootSignature;
	gpsDesc.VS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::vs));
	gpsDesc.PS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::fs));
	gpsDesc.DS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::ds));
	gpsDesc.HS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::hs));
	gpsDesc.GS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::gs));
	gpsDesc.StreamOutput = {};
	gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsDesc.SampleMask = UINT_MAX;

	gpsDesc.RasterizerState.FillMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	gpsDesc.RasterizerState.CullMode = ToNativeCullMode(desc._rasterizationState._cullMode);
	gpsDesc.RasterizerState.FrontCounterClockwise = TRUE;
	gpsDesc.RasterizerState.DepthBias = static_cast<INT>(desc._rasterizationState._depthBiasConstantFactor);
	gpsDesc.RasterizerState.DepthBiasClamp = desc._rasterizationState._depthBiasClamp;
	gpsDesc.RasterizerState.SlopeScaledDepthBias = desc._rasterizationState._depthBiasSlopeFactor;
	gpsDesc.RasterizerState.DepthClipEnable = desc._rasterizationState._depthClampEnable;
	gpsDesc.RasterizerState.MultisampleEnable = FALSE;
	gpsDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	gpsDesc.RasterizerState.ForcedSampleCount = 0;
	gpsDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	gpsDesc.DepthStencilState.DepthEnable = desc._depthTestEnable;
	gpsDesc.DepthStencilState.DepthWriteMask = desc._depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	gpsDesc.DepthStencilState.DepthFunc = ToNativeCompareOp(desc._depthCompareOp);
	gpsDesc.DepthStencilState.StencilEnable = desc._stencilTestEnable;

	Vector<D3D12_INPUT_ELEMENT_DESC> vInputElementDesc;
	gpsDesc.InputLayout = geo.GetD3DInputLayoutDesc(_vertexInputBindingsFilter, vInputElementDesc);
	gpsDesc.IBStripCutValue = desc._primitiveRestartEnable ? D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpsDesc.PrimitiveTopologyType = ToNativePrimitiveTopologyType(desc._topology);

	RP::RcD3DRenderPass renderPass = pRendererD3D12->GetRenderPassByID(desc._renderPassID);
	RP::RcD3DSubpass subpass = renderPass._vSubpasses[desc._subpass];
	gpsDesc.NumRenderTargets = Utils::Cast32(subpass._vColor.size());
	VERUS_U_FOR(i, gpsDesc.NumRenderTargets)
	{
		const int index = subpass._vColor[i]._index;
		RP::RcD3DAttachment attachment = renderPass._vAttachments[index];
		gpsDesc.RTVFormats[i] = ToNativeFormat(attachment._format);
	}
	if (subpass._depthStencil._index >= 0)
	{
		RP::RcD3DAttachment attachment = renderPass._vAttachments[subpass._depthStencil._index];
		gpsDesc.DSVFormat = ToNativeFormat(attachment._format);
	}

	gpsDesc.SampleDesc.Count = desc._sampleCount;
	gpsDesc.NodeMask = 0;
	gpsDesc.CachedPSO = { nullptr, 0 };
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(&_pPipelineState))))
		throw VERUS_RUNTIME_ERROR << "CreateGraphicsPipelineState(), hr=" << VERUS_HR(hr);
}

void PipelineD3D12::Done()
{
	VERUS_COM_RELEASE_CHECK(_pPipelineState.Get());
	_pPipelineState.Reset();

	VERUS_DONE(PipelineD3D12);
}
