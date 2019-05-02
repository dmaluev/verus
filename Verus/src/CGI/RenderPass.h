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
				enum class LoadOp
				{
					load,
					clear,
					dontCare
				};

				enum class StoreOp
				{
					store,
					dontCare
				};

				CSZ         _name = nullptr;
				Format      _format = Format::unormR8G8B8A8;
				int         _sampleCount = 1;
				LoadOp      _loadOp = LoadOp::load;
				StoreOp     _storeOp = StoreOp::store;
				ImageLayout _initialLayout = ImageLayout::undefined;
				ImageLayout _finalLayout = ImageLayout::undefined;

				Attachment(CSZ name, Format format, int sampleCount = 1);

				Attachment& LoadOpClear();
				Attachment& LoadOpDontCare();
				Attachment& StoreOpDontCare();
				Attachment& Layout(ImageLayout whenBegins, ImageLayout whenEnds);
				Attachment& FinalLayout(ImageLayout whenEnds);
			};
			VERUS_TYPEDEFS(Attachment);

			class Subpass
			{
			public:
				struct Ref
				{
					CSZ         _name = nullptr;
					ImageLayout _layout = ImageLayout::undefined;

					Ref() {}
				};

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
				Dependency();

				Dependency& Src(CSZ name);
				Dependency& Dst(CSZ name);
			};
			VERUS_TYPEDEFS(Dependency);
		}
	}
}
