// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI::RP
{
	class D3DAttachment
	{
	public:
		Format                _format = Format::unormR8G8B8A8;
		int                   _sampleCount = 1;
		Attachment::LoadOp    _loadOp = Attachment::LoadOp::load;
		Attachment::StoreOp   _storeOp = Attachment::StoreOp::store;
		Attachment::LoadOp    _stencilLoadOp = Attachment::LoadOp::dontCare;
		Attachment::StoreOp   _stencilStoreOp = Attachment::StoreOp::dontCare;
		D3D12_RESOURCE_STATES _initialState = D3D12_RESOURCE_STATE_COMMON;
		D3D12_RESOURCE_STATES _finalState = D3D12_RESOURCE_STATE_COMMON;
		int                   _clearSubpassIndex = -1;
	};
	VERUS_TYPEDEFS(D3DAttachment);

	class D3DRef
	{
	public:
		int                   _index = -1;
		D3D12_RESOURCE_STATES _state = D3D12_RESOURCE_STATE_COMMON;
	};
	VERUS_TYPEDEFS(D3DRef);

	class D3DSubpass
	{
	public:
		Vector<D3DRef> _vInput;
		Vector<D3DRef> _vColor;
		Vector<D3DRef> _vResolve;
		Vector<int>    _vPreserve;
		D3DRef         _depthStencil;
	};
	VERUS_TYPEDEFS(D3DSubpass);

	class D3DDependency
	{
	public:
	};
	VERUS_TYPEDEFS(D3DDependency);

	class D3DRenderPass
	{
	public:
		Vector<D3DAttachment> _vAttachments;
		Vector<D3DSubpass>    _vSubpasses;
	};
	VERUS_TYPEDEFS(D3DRenderPass);

	class D3DFramebufferSubpass
	{
	public:
		Vector<ID3D12Resource*> _vResources;
		DescriptorHeap          _dhRTVs;
		DescriptorHeap          _dhDSV;
	};
	VERUS_TYPEDEFS(D3DFramebufferSubpass);

	class D3DFramebuffer
	{
	public:
		Vector<ID3D12Resource*>       _vResources;
		Vector<D3DFramebufferSubpass> _vSubpasses;
		int                           _width = 0;
		int                           _height = 0;
		int                           _mipLevels = 1;
		CubeMapFace                   _cubeMapFace = CubeMapFace::none;
	};
	VERUS_TYPEDEFS(D3DFramebuffer);
}
