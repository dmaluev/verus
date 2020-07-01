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

			static UB_BlurVS s_ubBlurVS;
			static UB_BlurFS s_ubBlurFS;

			CGI::ShaderPwn                _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwn               _tex;
			Handles                       _bloomHandles;
			Handles                       _ssaoHandles;

		public:
			Blur();
			~Blur();

			void Init();
			void Done();

			void OnSwapChainResized();

			void Generate();
			void GenerateForBloom();
			void GenerateForSsao();
			void GenerateForDepthOfField();
			void GenerateForMotionBlur();
		};
		VERUS_TYPEDEFS(Blur);
	}
}
