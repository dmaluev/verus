// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

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

	if (desc._compute)
	{
		_compute = true;
		InitCompute(desc);
		return;
	}

	// Attachment count according to blend equations:
	int attachmentCount = 0;
	for (const auto& x : desc._colorAttachBlendEqs)
	{
		if (x.empty())
			break;
		attachmentCount++;
	}

	RcGeometryD3D12 geo = static_cast<RcGeometryD3D12>(*desc._geometry);
	RcShaderD3D12 shader = static_cast<RcShaderD3D12>(*desc._shader);

	_pRootSignature = shader.GetD3DRootSignature();
	_topology = ToNativePrimitiveTopology(desc._topology);
	_vertexInputBindingsFilter = desc._vertexInputBindingsFilter;

	RP::RcD3DRenderPass renderPass = pRendererD3D12->GetRenderPass(desc._renderPassHandle);
	RP::RcD3DSubpass subpass = renderPass._vSubpasses[desc._subpass];

	auto GetStripCutValue = [&geo]()
	{
		return geo.Has32BitIndices() ? D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.pRootSignature = _pRootSignature;
	ShaderD3D12::RcCompiled compiled = shader.GetCompiled(desc._shaderBranch);
	gpsDesc.VS = ToBytecode(compiled._pBlobs[+BaseShader::Stage::vs].Get());
	gpsDesc.PS = ToBytecode(compiled._pBlobs[+BaseShader::Stage::fs].Get());
	gpsDesc.DS = ToBytecode(compiled._pBlobs[+BaseShader::Stage::ds].Get());
	gpsDesc.HS = ToBytecode(compiled._pBlobs[+BaseShader::Stage::hs].Get());
	gpsDesc.GS = ToBytecode(compiled._pBlobs[+BaseShader::Stage::gs].Get());
	gpsDesc.StreamOutput = {};

	gpsDesc.BlendState.AlphaToCoverageEnable = FALSE;
	gpsDesc.BlendState.IndependentBlendEnable = TRUE;
	FillBlendStateRenderTargets(desc, attachmentCount, gpsDesc.BlendState);

	gpsDesc.SampleMask = UINT_MAX;

	gpsDesc.RasterizerState.FillMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	gpsDesc.RasterizerState.CullMode = ToNativeCullMode(desc._rasterizationState._cullMode);
	gpsDesc.RasterizerState.FrontCounterClockwise = TRUE;
	gpsDesc.RasterizerState.DepthBias = static_cast<INT>(desc._rasterizationState._depthBiasConstantFactor);
	gpsDesc.RasterizerState.DepthBiasClamp = desc._rasterizationState._depthBiasClamp;
	gpsDesc.RasterizerState.SlopeScaledDepthBias = desc._rasterizationState._depthBiasSlopeFactor;
	gpsDesc.RasterizerState.DepthClipEnable = desc._rasterizationState._depthClampEnable;
	gpsDesc.RasterizerState.MultisampleEnable = FALSE;
	gpsDesc.RasterizerState.AntialiasedLineEnable = (desc._colorAttachBlendEqs[0] == VERUS_COLOR_BLEND_ALPHA) ? TRUE : FALSE;
	gpsDesc.RasterizerState.ForcedSampleCount = 0;
	gpsDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	if (subpass._depthStencil._index >= 0)
	{
		gpsDesc.DepthStencilState.DepthEnable = desc._depthTestEnable;
		gpsDesc.DepthStencilState.DepthWriteMask = desc._depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		gpsDesc.DepthStencilState.DepthFunc = ToNativeCompareOp(desc._depthCompareOp);
		gpsDesc.DepthStencilState.StencilEnable = desc._stencilTestEnable;
	}

	Vector<D3D12_INPUT_ELEMENT_DESC> vInputElementDesc;
	gpsDesc.InputLayout = geo.GetD3DInputLayoutDesc(_vertexInputBindingsFilter, vInputElementDesc);
	gpsDesc.IBStripCutValue = desc._primitiveRestartEnable ? GetStripCutValue() : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpsDesc.PrimitiveTopologyType = ToNativePrimitiveTopologyType(desc._topology);

	gpsDesc.NumRenderTargets = Utils::Cast32(subpass._vColor.size()); // According to subpass.
	VERUS_RT_ASSERT(gpsDesc.NumRenderTargets == attachmentCount); // These two must match.
	VERUS_U_FOR(i, gpsDesc.NumRenderTargets)
	{
		RP::RcD3DAttachment attachment = renderPass._vAttachments[subpass._vColor[i]._index];
		gpsDesc.RTVFormats[i] = ToNativeFormat(attachment._format, false);
	}
	if (subpass._depthStencil._index >= 0)
	{
		RP::RcD3DAttachment attachment = renderPass._vAttachments[subpass._depthStencil._index];
		gpsDesc.DSVFormat = ToNativeFormat(attachment._format, false);
	}

	gpsDesc.SampleDesc.Count = desc._sampleCount;
	gpsDesc.NodeMask = 0;
	gpsDesc.CachedPSO = { nullptr, 0 };
	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(&_pPipelineState))))
		throw VERUS_RUNTIME_ERROR << "CreateGraphicsPipelineState(); hr=" << VERUS_HR(hr);
	_pPipelineState->SetName(_C(Str::Utf8ToWide(String("PipelineState (") + _C(shader.GetSourceName()) + ", " + desc._shaderBranch + ")")));
}

void PipelineD3D12::Done()
{
	VERUS_COM_RELEASE_CHECK(_pPipelineState.Get());
	_pPipelineState.Reset();

	VERUS_DONE(PipelineD3D12);
}

void PipelineD3D12::InitCompute(RcPipelineDesc desc)
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	RcShaderD3D12 shader = static_cast<RcShaderD3D12>(*desc._shader);

	_pRootSignature = shader.GetD3DRootSignature();

	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsDesc = {};
	cpsDesc.pRootSignature = _pRootSignature;
	ShaderD3D12::RcCompiled compiled = shader.GetCompiled(desc._shaderBranch);
	cpsDesc.CS = ToBytecode(compiled._pBlobs[+BaseShader::Stage::cs].Get());
	cpsDesc.NodeMask = 0;
	cpsDesc.CachedPSO = { nullptr, 0 };
	cpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateComputePipelineState(&cpsDesc, IID_PPV_ARGS(&_pPipelineState))))
		throw VERUS_RUNTIME_ERROR << "CreateComputePipelineState(); hr=" << VERUS_HR(hr);
	_pPipelineState->SetName(_C(Str::Utf8ToWide(String("PipelineState (") + _C(shader.GetSourceName()) + ", " + desc._shaderBranch + ")")));
}

void PipelineD3D12::InitMeshShading(RcPipelineDesc desc)
{
	VERUS_QREF_RENDERER_D3D12;
	HRESULT hr = 0;

	// Attachment count according to blend equations:
	int attachmentCount = 0;
	for (const auto& x : desc._colorAttachBlendEqs)
	{
		if (x.empty())
			break;
		attachmentCount++;
	}

	RcShaderD3D12 shader = static_cast<RcShaderD3D12>(*desc._shader);

	_pRootSignature = shader.GetD3DRootSignature();
	_topology = ToNativePrimitiveTopology(desc._topology);

	RP::RcD3DRenderPass renderPass = pRendererD3D12->GetRenderPass(desc._renderPassHandle);
	RP::RcD3DSubpass subpass = renderPass._vSubpasses[desc._subpass];

	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC mspsDesc = {};
	mspsDesc.pRootSignature = _pRootSignature;
	mspsDesc.AS = ToBytecode(nullptr);
	mspsDesc.MS = ToBytecode(nullptr);
	mspsDesc.PS = ToBytecode(nullptr);

	mspsDesc.BlendState.AlphaToCoverageEnable = FALSE;
	mspsDesc.BlendState.IndependentBlendEnable = TRUE;
	FillBlendStateRenderTargets(desc, attachmentCount, mspsDesc.BlendState);

	mspsDesc.SampleMask = UINT_MAX;

	mspsDesc.RasterizerState.FillMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	mspsDesc.RasterizerState.CullMode = ToNativeCullMode(desc._rasterizationState._cullMode);
	mspsDesc.RasterizerState.FrontCounterClockwise = TRUE;
	mspsDesc.RasterizerState.DepthBias = static_cast<INT>(desc._rasterizationState._depthBiasConstantFactor);
	mspsDesc.RasterizerState.DepthBiasClamp = desc._rasterizationState._depthBiasClamp;
	mspsDesc.RasterizerState.SlopeScaledDepthBias = desc._rasterizationState._depthBiasSlopeFactor;
	mspsDesc.RasterizerState.DepthClipEnable = desc._rasterizationState._depthClampEnable;
	mspsDesc.RasterizerState.MultisampleEnable = FALSE;
	mspsDesc.RasterizerState.AntialiasedLineEnable = (desc._colorAttachBlendEqs[0] == VERUS_COLOR_BLEND_ALPHA) ? TRUE : FALSE;
	mspsDesc.RasterizerState.ForcedSampleCount = 0;
	mspsDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	if (subpass._depthStencil._index >= 0)
	{
		mspsDesc.DepthStencilState.DepthEnable = desc._depthTestEnable;
		mspsDesc.DepthStencilState.DepthWriteMask = desc._depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		mspsDesc.DepthStencilState.DepthFunc = ToNativeCompareOp(desc._depthCompareOp);
		mspsDesc.DepthStencilState.StencilEnable = desc._stencilTestEnable;
	}

	mspsDesc.PrimitiveTopologyType = ToNativePrimitiveTopologyType(desc._topology);

	mspsDesc.NumRenderTargets = Utils::Cast32(subpass._vColor.size()); // According to subpass.
	VERUS_RT_ASSERT(mspsDesc.NumRenderTargets == attachmentCount); // These two must match.
	VERUS_U_FOR(i, mspsDesc.NumRenderTargets)
	{
		RP::RcD3DAttachment attachment = renderPass._vAttachments[subpass._vColor[i]._index];
		mspsDesc.RTVFormats[i] = ToNativeFormat(attachment._format, false);
	}
	if (subpass._depthStencil._index >= 0)
	{
		RP::RcD3DAttachment attachment = renderPass._vAttachments[subpass._depthStencil._index];
		mspsDesc.DSVFormat = ToNativeFormat(attachment._format, false);
	}

	mspsDesc.SampleDesc.Count = desc._sampleCount;
	mspsDesc.NodeMask = 0;
	mspsDesc.CachedPSO = { nullptr, 0 };
	mspsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	auto meshStateStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(mspsDesc);
	D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {};
	streamDesc.SizeInBytes = sizeof(meshStateStream);
	streamDesc.pPipelineStateSubobjectStream = &meshStateStream;
	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreatePipelineState(&streamDesc, IID_PPV_ARGS(&_pPipelineState))))
		throw VERUS_RUNTIME_ERROR << "CreatePipelineState(); hr=" << VERUS_HR(hr);
}

D3D12_SHADER_BYTECODE PipelineD3D12::ToBytecode(ID3DBlob* pBlob)
{
	D3D12_SHADER_BYTECODE bytecode = {};
	if (pBlob)
	{
		bytecode.pShaderBytecode = pBlob->GetBufferPointer();
		bytecode.BytecodeLength = pBlob->GetBufferSize();
	}
	return bytecode;
}

void PipelineD3D12::FillBlendStateRenderTargets(RcPipelineDesc desc, int attachmentCount, D3D12_BLEND_DESC& blendDesc)
{
	VERUS_FOR(i, attachmentCount)
	{
		CSZ p = _C(desc._colorAttachWriteMasks[i]);
		while (*p)
		{
			switch (*p)
			{
			case 'r': blendDesc.RenderTarget[i].RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_RED; break;
			case 'g': blendDesc.RenderTarget[i].RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_GREEN; break;
			case 'b': blendDesc.RenderTarget[i].RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_BLUE; break;
			case 'a': blendDesc.RenderTarget[i].RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA; break;
			}
			p++;
		}

		if (desc._colorAttachBlendEqs[i] != "off")
		{
			blendDesc.RenderTarget[i].BlendEnable = TRUE;

			static const D3D12_BLEND bfs[] =
			{
				D3D12_BLEND_ZERO,
				D3D12_BLEND_ONE,
				D3D12_BLEND_INV_DEST_ALPHA,
				D3D12_BLEND_INV_DEST_COLOR,
				D3D12_BLEND_INV_BLEND_FACTOR,
				D3D12_BLEND_INV_SRC_ALPHA,
				D3D12_BLEND_INV_SRC_COLOR,
				D3D12_BLEND_DEST_ALPHA,
				D3D12_BLEND_DEST_COLOR,
				D3D12_BLEND_BLEND_FACTOR,
				D3D12_BLEND_SRC_ALPHA,
				D3D12_BLEND_SRC_ALPHA_SAT,
				D3D12_BLEND_SRC_COLOR
			};
			static const D3D12_BLEND_OP ops[] =
			{
				D3D12_BLEND_OP_ADD,
				D3D12_BLEND_OP_SUBTRACT,
				D3D12_BLEND_OP_REV_SUBTRACT,
				D3D12_BLEND_OP_MIN,
				D3D12_BLEND_OP_MAX
			};

			int colorBlendOp = -1;
			int alphaBlendOp = -1;
			int srcColorBlendFactor = -1;
			int dstColorBlendFactor = -1;
			int srcAlphaBlendFactor = -1;
			int dstAlphaBlendFactor = -1;

			BaseRenderer::SetAlphaBlendHelper(
				_C(desc._colorAttachBlendEqs[i]),
				colorBlendOp,
				alphaBlendOp,
				srcColorBlendFactor,
				dstColorBlendFactor,
				srcAlphaBlendFactor,
				dstAlphaBlendFactor);

			blendDesc.RenderTarget[i].SrcBlend = bfs[srcColorBlendFactor];
			blendDesc.RenderTarget[i].DestBlend = bfs[dstColorBlendFactor];
			blendDesc.RenderTarget[i].BlendOp = ops[colorBlendOp];
			blendDesc.RenderTarget[i].SrcBlendAlpha = bfs[srcAlphaBlendFactor];
			blendDesc.RenderTarget[i].DestBlendAlpha = bfs[dstAlphaBlendFactor];
			blendDesc.RenderTarget[i].BlendOpAlpha = ops[alphaBlendOp];
			blendDesc.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
		}
	}
}
