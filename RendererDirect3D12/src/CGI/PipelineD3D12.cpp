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

	if (desc._compute)
	{
		_compute = true;
		InitCompute(desc);
		return;
	}

	_vertexInputBindingsFilter = desc._vertexInputBindingsFilter;

	RcGeometryD3D12 geo = static_cast<RcGeometryD3D12>(*desc._geometry);
	RcShaderD3D12 shader = static_cast<RcShaderD3D12>(*desc._shader);

	int attachmentCount = 0;
	for (const auto& x : desc._colorAttachBlendEqs)
	{
		if (x.empty())
			break;
		attachmentCount++;
	}

	_pRootSignature = shader.GetD3DRootSignature();
	_topology = ToNativePrimitiveTopology(desc._topology);

	RP::RcD3DRenderPass renderPass = pRendererD3D12->GetRenderPassByID(desc._renderPassID);
	RP::RcD3DSubpass subpass = renderPass._vSubpasses[desc._subpass];

	auto GetStripCutValue = [&geo]()
	{
		return geo.Has32BitIndices() ? D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF;
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.pRootSignature = _pRootSignature;
	gpsDesc.VS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::vs));
	gpsDesc.PS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::fs));
	gpsDesc.DS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::ds));
	gpsDesc.HS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::hs));
	gpsDesc.GS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::gs));
	gpsDesc.StreamOutput = {};

	gpsDesc.BlendState = {};
	gpsDesc.BlendState.AlphaToCoverageEnable = FALSE;
	gpsDesc.BlendState.IndependentBlendEnable = TRUE;
	VERUS_FOR(i, attachmentCount)
	{
		CSZ p = _C(desc._colorAttachWriteMasks[i]);
		while (*p)
		{
			switch (*p)
			{
			case 'r': gpsDesc.BlendState.RenderTarget[i].RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_RED; break;
			case 'g': gpsDesc.BlendState.RenderTarget[i].RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_GREEN; break;
			case 'b': gpsDesc.BlendState.RenderTarget[i].RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_BLUE; break;
			case 'a': gpsDesc.BlendState.RenderTarget[i].RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA; break;
			}
			p++;
		}

		if (desc._colorAttachBlendEqs[i] != "off")
		{
			gpsDesc.BlendState.RenderTarget[i].BlendEnable = TRUE;

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

			gpsDesc.BlendState.RenderTarget[i].SrcBlend = bfs[srcColorBlendFactor];
			gpsDesc.BlendState.RenderTarget[i].DestBlend = bfs[dstColorBlendFactor];
			gpsDesc.BlendState.RenderTarget[i].BlendOp = ops[colorBlendOp];
			gpsDesc.BlendState.RenderTarget[i].SrcBlendAlpha = bfs[srcAlphaBlendFactor];
			gpsDesc.BlendState.RenderTarget[i].DestBlendAlpha = bfs[dstAlphaBlendFactor];
			gpsDesc.BlendState.RenderTarget[i].BlendOpAlpha = ops[alphaBlendOp];
			gpsDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_CLEAR;
		}
	}

	gpsDesc.SampleMask = UINT_MAX;

	gpsDesc.RasterizerState.FillMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	gpsDesc.RasterizerState.CullMode = ToNativeCullMode(desc._rasterizationState._cullMode);
	gpsDesc.RasterizerState.FrontCounterClockwise = TRUE;
	gpsDesc.RasterizerState.DepthBias = static_cast<INT>(desc._rasterizationState._depthBiasConstantFactor);
	gpsDesc.RasterizerState.DepthBiasClamp = desc._rasterizationState._depthBiasClamp;
	gpsDesc.RasterizerState.SlopeScaledDepthBias = desc._rasterizationState._depthBiasSlopeFactor;
	gpsDesc.RasterizerState.DepthClipEnable = desc._rasterizationState._depthClampEnable;
	gpsDesc.RasterizerState.MultisampleEnable = FALSE;
	gpsDesc.RasterizerState.AntialiasedLineEnable = TRUE;
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

	gpsDesc.NumRenderTargets = Utils::Cast32(subpass._vColor.size());
	VERUS_RT_ASSERT(gpsDesc.NumRenderTargets == attachmentCount);
	VERUS_U_FOR(i, gpsDesc.NumRenderTargets)
	{
		const int index = subpass._vColor[i]._index;
		RP::RcD3DAttachment attachment = renderPass._vAttachments[index];
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
		throw VERUS_RUNTIME_ERROR << "CreateGraphicsPipelineState(), hr=" << VERUS_HR(hr);
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
	cpsDesc.CS = ToBytecode(shader.GetD3DBlob(desc._shaderBranch, BaseShader::Stage::cs));
	cpsDesc.NodeMask = 0;
	cpsDesc.CachedPSO = { nullptr, 0 };
	cpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	if (FAILED(hr = pRendererD3D12->GetD3DDevice()->CreateComputePipelineState(&cpsDesc, IID_PPV_ARGS(&_pPipelineState))))
		throw VERUS_RUNTIME_ERROR << "CreateComputePipelineState(), hr=" << VERUS_HR(hr);
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
