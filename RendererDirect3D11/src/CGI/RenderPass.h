// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		namespace RP
		{
			class D3DAttachment
			{
			public:
				Format              _format = Format::unormR8G8B8A8;
				int                 _sampleCount = 1;
				Attachment::LoadOp  _loadOp = Attachment::LoadOp::load;
				Attachment::StoreOp _storeOp = Attachment::StoreOp::store;
				Attachment::LoadOp  _stencilLoadOp = Attachment::LoadOp::dontCare;
				Attachment::StoreOp _stencilStoreOp = Attachment::StoreOp::dontCare;
				int                 _clearSubpassIndex = -1;
			};
			VERUS_TYPEDEFS(D3DAttachment);

			class D3DRef
			{
			public:
				int _index = -1;
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
				bool           _depthStencilReadOnly = false;
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
				Vector<ID3D11RenderTargetView*> _vRTVs;
				ID3D11DepthStencilView* _pDSV = nullptr;
			};
			VERUS_TYPEDEFS(D3DFramebufferSubpass);

			class D3DFramebuffer
			{
			public:
				Vector<D3DFramebufferSubpass> _vSubpasses;
				int                           _width = 0;
				int                           _height = 0;
				int                           _mipLevels = 1;
				CubeMapFace                   _cubeMapFace = CubeMapFace::none;
			};
			VERUS_TYPEDEFS(D3DFramebuffer);
		}
	}
}
