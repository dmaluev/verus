#pragma once

namespace verus
{
	namespace Effects
	{
		class Ssao : public Singleton<Ssao>, public Object
		{
#include "../Shaders/Ssao.inc.hlsl"

			enum TEX
			{
				TEX_GEN_AO,
				TEX_RAND_NORMALS,
				TEX_COUNT
			};

			static UB_SsaoVS s_ubSsaoVS;
			static UB_SsaoFS s_ubSsaoFS;

			CGI::ShaderPwn              _shader;
			CGI::PipelinePwn            _pipe;
			CGI::TexturePwns<TEX_COUNT> _tex;
			CGI::RPHandle               _rph;
			CGI::FBHandle               _fbh;
			CGI::CSHandle               _csh;

		public:
			Ssao();
			~Ssao();

			void Init();
			void InitCmd();
			void Done();

			void OnSwapChainResized();

			void Generate();

			void UpdateRandNormalsTexture();

			CGI::TexturePtr GetTexture() const;
		};
		VERUS_TYPEDEFS(Ssao);
	}
}
