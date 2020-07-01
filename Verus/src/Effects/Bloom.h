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

			static UB_BloomVS s_ubBloomVS;
			static UB_BloomFS s_ubBloomFS;

			CGI::ShaderPwn              _shader;
			CGI::PipelinePwn            _pipe;
			CGI::TexturePwns<TEX_COUNT> _tex;
			CGI::RPHandle               _rph;
			CGI::FBHandle               _fbh;
			CGI::CSHandle               _csh;

		public:
			Bloom();
			~Bloom();

			void Init();
			void Done();

			void OnSwapChainResized();

			void Generate();

			CGI::TexturePtr GetTexture() const;
			CGI::TexturePtr GetPongTexture() const;
		};
		VERUS_TYPEDEFS(Bloom);
	}
}
