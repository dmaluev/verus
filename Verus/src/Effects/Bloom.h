// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Effects
	{
		class Bloom : public Singleton<Bloom>, public Object
		{
#include "../Shaders/Bloom.inc.hlsl"

			enum TEX
			{
				TEX_PING,
				TEX_PONG,
				TEX_COUNT
			};

			static UB_BloomVS        s_ubBloomVS;
			static UB_BloomFS        s_ubBloomFS;
			static UB_BloomGodRaysFS s_ubBloomGodRaysFS;

			CGI::ShaderPwn              _shader;
			CGI::PipelinePwn            _pipe;
			CGI::TexturePwns<TEX_COUNT> _tex;
			CGI::TexturePtr             _texAtmoShadow;
			CGI::RPHandle               _rph;
			CGI::FBHandle               _fbh;
			CGI::CSHandle               _csh;
			CGI::CSHandle               _cshGodRays;

		public:
			Bloom();
			~Bloom();

			void Init();
			void InitByAtmosphere(CGI::TexturePtr texShadow);
			void Done();

			void OnSwapChainResized();

			void Generate();

			CGI::TexturePtr GetTexture() const;
			CGI::TexturePtr GetPongTexture() const;
		};
		VERUS_TYPEDEFS(Bloom);
	}
}
