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

	RcShaderD3D12 shader = static_cast<RcShaderD3D12>(desc._shader);
	auto ToBytecode = [](ID3DBlob* pBlob) -> D3D12_SHADER_BYTECODE
	{
		D3D12_SHADER_BYTECODE bytecode;
		bytecode.pShaderBytecode = pBlob->GetBufferPointer();
		bytecode.BytecodeLength = pBlob->GetBufferSize();
		return bytecode;
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.pRootSignature = nullptr;
	gpsDesc.VS = ToBytecode(shader.GetID3DBlob(desc._shaderBranch, BaseShader::Stage::vs).Get());
	gpsDesc.PS = ToBytecode(shader.GetID3DBlob(desc._shaderBranch, BaseShader::Stage::fs).Get());
	gpsDesc.DS = ToBytecode(shader.GetID3DBlob(desc._shaderBranch, BaseShader::Stage::ds).Get());
	gpsDesc.HS = ToBytecode(shader.GetID3DBlob(desc._shaderBranch, BaseShader::Stage::hs).Get());
	gpsDesc.GS = ToBytecode(shader.GetID3DBlob(desc._shaderBranch, BaseShader::Stage::gs).Get());
	gpsDesc.StreamOutput = {};
	gpsDesc.BlendState = {};
	gpsDesc.SampleMask = UINT_MAX;

	gpsDesc.RasterizerState.FillMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	gpsDesc.RasterizerState.CullMode = ToNativeCullMode(desc._rasterizationState._cullMode);
	gpsDesc.RasterizerState.FrontCounterClockwise = FALSE;
	gpsDesc.RasterizerState.DepthBias = desc._rasterizationState._depthBiasConstantFactor;
	gpsDesc.RasterizerState.DepthBiasClamp = desc._rasterizationState._depthBiasClamp;
	gpsDesc.RasterizerState.SlopeScaledDepthBias = desc._rasterizationState._depthBiasSlopeFactor;
	gpsDesc.RasterizerState.DepthClipEnable = desc._rasterizationState._depthClampEnable;
	gpsDesc.RasterizerState.MultisampleEnable = FALSE;
	gpsDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	gpsDesc.RasterizerState.ForcedSampleCount = 0;
	gpsDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	gpsDesc.DepthStencilState = {};
	//gpsDesc.InputLayout = nullptr;
	gpsDesc.IBStripCutValue = desc._primitiveRestartEnable ? D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpsDesc.PrimitiveTopologyType = ToNativePrimitiveTopologyType(desc._topology);
	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	gpsDesc.SampleDesc.Count = desc._sampleCount;
	gpsDesc.NodeMask = 0;
	gpsDesc.CachedPSO = { nullptr, 0 };
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (FAILED(hr = pRendererD3D12->GetDevice()->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(&_pPipelineState))))
		throw VERUS_RUNTIME_ERROR << "CreateGraphicsPipelineState(), hr=" << VERUS_HR(hr);
}

void PipelineD3D12::Done()
{
	VERUS_DONE(PipelineD3D12);
}
