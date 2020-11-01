// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		namespace RP
		{
			class Attachment
			{
			public:
				enum class LoadOp : int
				{
					load,
					clear,
					dontCare
				};

				enum class StoreOp : int
				{
					store,
					dontCare
				};

				CSZ         _name = nullptr;
				Format      _format = Format::unormR8G8B8A8;
				int         _sampleCount = 1;
				LoadOp      _loadOp = LoadOp::load;
				StoreOp     _storeOp = StoreOp::store;
				LoadOp      _stencilLoadOp = LoadOp::dontCare;
				StoreOp     _stencilStoreOp = StoreOp::dontCare;
				ImageLayout _initialLayout = ImageLayout::undefined;
				ImageLayout _finalLayout = ImageLayout::undefined;

				Attachment(CSZ name, Format format, int sampleCount = 1);

				Attachment& SetLoadOp(LoadOp op);
				Attachment& LoadOpClear();
				Attachment& LoadOpDontCare();
				Attachment& SetStoreOp(StoreOp op);
				Attachment& StoreOpDontCare();
				Attachment& Layout(ImageLayout whenBegins, ImageLayout whenEnds);
				Attachment& Layout(ImageLayout both);
			};
			VERUS_TYPEDEFS(Attachment);

			class Ref
			{
			public:
				CSZ         _name = nullptr;
				ImageLayout _layout = ImageLayout::undefined;

				Ref() = default;
				Ref(CSZ name, ImageLayout layout) : _name(name), _layout(layout) {}
			};
			VERUS_TYPEDEFS(Ref);

			class Subpass
			{
			public:
				CSZ                        _name = nullptr;
				std::initializer_list<Ref> _ilInput;
				std::initializer_list<Ref> _ilColor;
				std::initializer_list<Ref> _ilResolve;
				std::initializer_list<Ref> _ilPreserve;
				Ref                        _depthStencil;

				Subpass(CSZ name);

				Subpass& Input(std::initializer_list<Ref> il);
				Subpass& Color(std::initializer_list<Ref> il);
				Subpass& Resolve(std::initializer_list<Ref> il);
				Subpass& Preserve(std::initializer_list<Ref> il);
				Subpass& DepthStencil(Ref r);
			};
			VERUS_TYPEDEFS(Subpass);

			class Dependency
			{
			public:
				CSZ _srcSubpass = nullptr;
				CSZ _dstSubpass = nullptr;
				int _mode = 0;

				Dependency(CSZ src = nullptr, CSZ dst = nullptr);

				Dependency& Mode(int mode);
			};
			VERUS_TYPEDEFS(Dependency);
		}
	}
}
