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

	_topology = ToNativePrimitiveTopology(desc._topology);

	CreateRootSignature();

	RcGeometryD3D12 geo = static_cast<RcGeometryD3D12>(*desc._geometry);
	RcShaderD3D12 shader = static_cast<RcShaderD3D12>(*desc._shader);

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
	gpsDesc.pRootSignature = _pRootSignature.Get();
	gpsDesc.VS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::vs));
	gpsDesc.PS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::fs));
	gpsDesc.DS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::ds));
	gpsDesc.HS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::hs));
	gpsDesc.GS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::gs));
	gpsDesc.StreamOutput = {};
	gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsDesc.SampleMask = UINT_MAX;

	gpsDesc.RasterizerState.FillMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	gpsDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE /*ToNativeCullMode(desc._rasterizationState._cullMode)*/;
	gpsDesc.RasterizerState.FrontCounterClockwise = FALSE;
	gpsDesc.RasterizerState.DepthBias = static_cast<INT>(desc._rasterizationState._depthBiasConstantFactor);
	gpsDesc.RasterizerState.DepthBiasClamp = desc._rasterizationState._depthBiasClamp;
	gpsDesc.RasterizerState.SlopeScaledDepthBias = desc._rasterizationState._depthBiasSlopeFactor;
	gpsDesc.RasterizerState.DepthClipEnable = desc._rasterizationState._depthClampEnable;
	gpsDesc.RasterizerState.MultisampleEnable = FALSE;
	gpsDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	gpsDesc.RasterizerState.ForcedSampleCount = 0;
	gpsDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	gpsDesc.DepthStencilState = {};
	gpsDesc.InputLayout = geo.GetD3DInputLayoutDesc();
	gpsDesc.IBStripCutValue = desc._primitiveRestartEnable ? D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpsDesc.PrimitiveTopologyType = ToNativePrimitiveTopologyType(desc._topology);
	gpsDesc.NumRenderTargets = 1;
	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpsDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	gpsDesc.SampleDesc.Count = desc._sampleCount;
	gpsDesc.NodeMask = 0;
	gpsDesc.CachedPSO = { nullptr, 0 };
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(&_pPipelineState))))
		throw VERUS_RUNTIME_ERROR << "CreateGraphicsPipelineState(), hr=" << VERUS_HR(hr);
}

void PipelineD3D12::Done()
{
	VERUS_DONE(PipelineD3D12);
}

void PipelineD3D12::CreateRootSignature()
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureSupportData = {};
	featureSupportData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(pRendererD3D12->GetD3DDevice()->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureSupportData, sizeof(featureSupportData))))
		featureSupportData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

	const D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	CD3DX12_ROOT_PARAMETER1 rootParameters[1];
	rootParameters[0].InitAsConstants(sizeof(Matrix4) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(VERUS_ARRAY_LENGTH(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> pRootSignatureBlob;
	ComPtr<ID3DBlob> pErrorBlob;
	if (FAILED(hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureSupportData.HighestVersion, &pRootSignatureBlob, &pErrorBlob)))
		throw VERUS_RUNTIME_ERROR << "D3DX12SerializeVersionedRootSignature(), hr=" << VERUS_HR(hr);
	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateRootSignature(0, pRootSignatureBlob->GetBufferPointer(), pRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&_pRootSignature))))
		throw VERUS_RUNTIME_ERROR << "CreateRootSignature(), hr=" << VERUS_HR(hr);
}
