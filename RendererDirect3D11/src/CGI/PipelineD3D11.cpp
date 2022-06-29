// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;
using namespace verus::CGI;

PipelineD3D11::PipelineD3D11()
{
}

PipelineD3D11::~PipelineD3D11()
{
	Done();
}

void PipelineD3D11::Init(RcPipelineDesc desc)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER_D3D11;
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

	RcGeometryD3D11 geo = static_cast<RcGeometryD3D11>(*desc._geometry);
	RcShaderD3D11 shader = static_cast<RcShaderD3D11>(*desc._shader);

	_topology = ToNativePrimitiveTopology(desc._topology);
	_vertexInputBindingsFilter = desc._vertexInputBindingsFilter;

	RP::RcD3DRenderPass renderPass = pRendererD3D11->GetRenderPass(desc._renderPassHandle);
	RP::RcD3DSubpass subpass = renderPass._vSubpasses[desc._subpass];

	const String name = String(" (") + _C(shader.GetSourceName()) + ", " + desc._shaderBranch + ")";

	ComPtr<ID3DBlob> pBlob;
	ShaderD3D11::RcCompiled compiled = shader.GetCompiled(desc._shaderBranch);
	pBlob = compiled._pBlobs[+BaseShader::Stage::vs];
	if (pBlob && FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &_pVS)))
		throw VERUS_RUNTIME_ERROR << "CreateVertexShader(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pVS.Get(), _C(String("Pipeline.VS") + name));
	pBlob = compiled._pBlobs[+BaseShader::Stage::hs];
	if (pBlob && FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateHullShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &_pHS)))
		throw VERUS_RUNTIME_ERROR << "CreateHullShader(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pHS.Get(), _C(String("Pipeline.HS") + name));
	pBlob = compiled._pBlobs[+BaseShader::Stage::ds];
	if (pBlob && FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateDomainShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &_pDS)))
		throw VERUS_RUNTIME_ERROR << "CreateDomainShader(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pDS.Get(), _C(String("Pipeline.DS") + name));
	pBlob = compiled._pBlobs[+BaseShader::Stage::gs];
	if (pBlob && FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateGeometryShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &_pGS)))
		throw VERUS_RUNTIME_ERROR << "CreateGeometryShader(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pGS.Get(), _C(String("Pipeline.GS") + name));
	pBlob = compiled._pBlobs[+BaseShader::Stage::fs];
	if (pBlob && FAILED(hr = pRendererD3D11->GetD3DDevice()->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &_pPS)))
		throw VERUS_RUNTIME_ERROR << "CreatePixelShader(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pPS.Get(), _C(String("Pipeline.PS") + name));
	pBlob = compiled._pBlobs[+BaseShader::Stage::vs];

	D3D11_BLEND_DESC bDesc = {};
	bDesc.AlphaToCoverageEnable = FALSE;
	bDesc.IndependentBlendEnable = TRUE;
	FillBlendStateRenderTargets(desc, attachmentCount, bDesc);
	if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateBlendState(&bDesc, &_pBlendState)))
		throw VERUS_RUNTIME_ERROR << "CreateBlendState(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pBlendState.Get(), _C(String("Pipeline.BlendState") + name));

	D3D11_RASTERIZER_DESC rDesc = {};
	rDesc.FillMode = ToNativePolygonMode(desc._rasterizationState._polygonMode);
	rDesc.CullMode = ToNativeCullMode(desc._rasterizationState._cullMode);
	rDesc.FrontCounterClockwise = TRUE;
	rDesc.DepthBias = static_cast<INT>(desc._rasterizationState._depthBiasConstantFactor);
	rDesc.DepthBiasClamp = desc._rasterizationState._depthBiasClamp;
	rDesc.SlopeScaledDepthBias = desc._rasterizationState._depthBiasSlopeFactor;
	rDesc.DepthClipEnable = TRUE;
	rDesc.ScissorEnable = TRUE;
	rDesc.MultisampleEnable = FALSE;
	rDesc.AntialiasedLineEnable = (desc._colorAttachBlendEqs[0] == VERUS_COLOR_BLEND_ALPHA) ? TRUE : FALSE;
	if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateRasterizerState(&rDesc, &_pRasterizerState)))
		throw VERUS_RUNTIME_ERROR << "CreateRasterizerState(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pRasterizerState.Get(), _C(String("Pipeline.RasterizerState") + name));

	D3D11_DEPTH_STENCIL_DESC dsDesc = {};
	if (subpass._depthStencil._index >= 0)
	{
		dsDesc.DepthEnable = desc._depthTestEnable;
		dsDesc.DepthWriteMask = desc._depthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = ToNativeCompareOp(desc._depthCompareOp);
		dsDesc.StencilEnable = desc._stencilTestEnable;
	}
	if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateDepthStencilState(&dsDesc, &_pDepthStencilState)))
		throw VERUS_RUNTIME_ERROR << "CreateDepthStencilState(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pDepthStencilState.Get(), _C(String("Pipeline.DepthStencilState") + name));

	Vector<D3D11_INPUT_ELEMENT_DESC> vInputElementDescs;
	geo.GetD3DInputElementDescs(_vertexInputBindingsFilter, vInputElementDescs);
	if (FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateInputLayout(
		vInputElementDescs.data(),
		Utils::Cast32(vInputElementDescs.size()),
		pBlob->GetBufferPointer(),
		pBlob->GetBufferSize(),
		&_pInputLayout)))
		throw VERUS_RUNTIME_ERROR << "CreateInputLayout(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pInputLayout.Get(), _C(String("Pipeline.InputLayout") + name));
}

void PipelineD3D11::Done()
{
	_pInputLayout.Reset();
	_pDepthStencilState.Reset();
	_pRasterizerState.Reset();
	_pBlendState.Reset();
	_pCS.Reset();
	_pPS.Reset();
	_pGS.Reset();
	_pDS.Reset();
	_pHS.Reset();
	_pVS.Reset();

	VERUS_DONE(PipelineD3D11);
}

void PipelineD3D11::InitCompute(RcPipelineDesc desc)
{
	VERUS_QREF_RENDERER_D3D11;
	HRESULT hr = 0;

	RcShaderD3D11 shader = static_cast<RcShaderD3D11>(*desc._shader);

	const String name = String(" (") + _C(shader.GetSourceName()) + ", " + desc._shaderBranch + ")";

	ComPtr<ID3DBlob> pBlob;
	ShaderD3D11::RcCompiled compiled = shader.GetCompiled(desc._shaderBranch);
	pBlob = compiled._pBlobs[+BaseShader::Stage::cs];
	if (pBlob && FAILED(hr = pRendererD3D11->GetD3DDevice()->CreateComputeShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &_pCS)))
		throw VERUS_RUNTIME_ERROR << "CreateComputeShader(); hr=" << VERUS_HR(hr);
	SetDebugObjectName(_pCS.Get(), _C(String("Pipeline.CS") + name));
}

void PipelineD3D11::FillBlendStateRenderTargets(RcPipelineDesc desc, int attachmentCount, D3D11_BLEND_DESC& blendDesc)
{
	VERUS_FOR(i, attachmentCount)
	{
		CSZ p = _C(desc._colorAttachWriteMasks[i]);
		while (*p)
		{
			switch (*p)
			{
			case 'r': blendDesc.RenderTarget[i].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_RED; break;
			case 'g': blendDesc.RenderTarget[i].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_GREEN; break;
			case 'b': blendDesc.RenderTarget[i].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_BLUE; break;
			case 'a': blendDesc.RenderTarget[i].RenderTargetWriteMask |= D3D11_COLOR_WRITE_ENABLE_ALPHA; break;
			}
			p++;
		}

		if (desc._colorAttachBlendEqs[i] != "off")
		{
			blendDesc.RenderTarget[i].BlendEnable = TRUE;

			static const D3D11_BLEND bfs[] =
			{
				D3D11_BLEND_ZERO,
				D3D11_BLEND_ONE,
				D3D11_BLEND_INV_DEST_ALPHA,
				D3D11_BLEND_INV_DEST_COLOR,
				D3D11_BLEND_INV_BLEND_FACTOR,
				D3D11_BLEND_INV_SRC_ALPHA,
				D3D11_BLEND_INV_SRC_COLOR,
				D3D11_BLEND_DEST_ALPHA,
				D3D11_BLEND_DEST_COLOR,
				D3D11_BLEND_BLEND_FACTOR,
				D3D11_BLEND_SRC_ALPHA,
				D3D11_BLEND_SRC_ALPHA_SAT,
				D3D11_BLEND_SRC_COLOR
			};
			static const D3D11_BLEND_OP ops[] =
			{
				D3D11_BLEND_OP_ADD,
				D3D11_BLEND_OP_SUBTRACT,
				D3D11_BLEND_OP_REV_SUBTRACT,
				D3D11_BLEND_OP_MIN,
				D3D11_BLEND_OP_MAX
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
		}
	}
}
