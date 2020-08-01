#pragma once

namespace verus
{
	namespace Effects
	{
		class Blur : public Singleton<Blur>, public Object
		{
#include "../Shaders/Blur.inc.hlsl"

			enum PIPE
			{
				PIPE_H,
				PIPE_V,
				PIPE_BLOOM_H,
				PIPE_BLOOM_V,
				PIPE_SSAO_H,
				PIPE_SSAO_V,
				PIPE_AA,
				PIPE_MOTION_BLUR,
				PIPE_COUNT
			};

			struct Handles
			{
				CGI::RPHandle _rphH;
				CGI::RPHandle _rphV;
				CGI::FBHandle _fbhH;
				CGI::FBHandle _fbhV;
				CGI::CSHandle _cshH;
				CGI::CSHandle _cshV;

				void FreeDescriptorSets(CGI::ShaderPtr shader);
				void DeleteFramebuffers();
			};

			static UB_BlurVS      s_ubBlurVS;
			static UB_BlurFS      s_ubBlurFS;
			static UB_ExtraBlurFS s_ubExtraBlurFS;

			CGI::ShaderPwn                _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwn               _tex;
			Handles                       _bloomHandles;
			Handles                       _ssaoHandles;
			CGI::RPHandle                 _rphAntiAliasing;
			CGI::FBHandle                 _fbhAntiAliasing;
			CGI::CSHandle                 _cshAntiAliasing;
			CGI::CSHandle                 _cshAntiAliasingExtra;
			CGI::RPHandle                 _rphMotionBlur;
			CGI::FBHandle                 _fbhMotionBlur;
			CGI::CSHandle                 _cshMotionBlur;
			CGI::CSHandle                 _cshMotionBlurExtra;

		public:
			Blur();
			~Blur();

			void Init();
			void Done();

			void OnSwapChainResized();

			void Generate();
			void GenerateForBloom();
			void GenerateForSsao();
			void GenerateForAntiAliasing();
			void GenerateForMotionBlur();
		};
		VERUS_TYPEDEFS(Blur);
	}
}
