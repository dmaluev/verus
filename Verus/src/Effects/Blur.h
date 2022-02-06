// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
				PIPE_U,
				PIPE_V,
				PIPE_BLOOM_U,
				PIPE_BLOOM_V,
				PIPE_DOF_U,
				PIPE_DOF_V,
				PIPE_SSAO_U,
				PIPE_SSAO_V,
				PIPE_SSR_U,
				PIPE_SSR_V,
				PIPE_AA,
				PIPE_MOTION_BLUR,
				PIPE_COUNT
			};

			struct Handles
			{
				CGI::RPHandle _rphU;
				CGI::RPHandle _rphV;
				CGI::FBHandle _fbhU;
				CGI::FBHandle _fbhV;
				CGI::CSHandle _cshU;
				CGI::CSHandle _cshV;

				void FreeDescriptorSets(CGI::ShaderPtr shader);
				void DeleteFramebuffers();
				void DeleteRenderPasses();
			};

			static UB_BlurVS      s_ubBlurVS;
			static UB_BlurFS      s_ubBlurFS;
			static UB_ExtraBlurFS s_ubExtraBlurFS;

			CGI::ShaderPwn                _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwn               _tex;
			Handles                       _bloomHandles;
			Handles                       _dofHandles;
			Handles                       _ssaoHandles;
			Handles                       _ssrHandles;
			CGI::CSHandle                 _cshDofExtra;
			CGI::CSHandle                 _cshSsrExtra;
			CGI::RPHandle                 _rphAntiAliasing;
			CGI::FBHandle                 _fbhAntiAliasing;
			CGI::CSHandle                 _cshAntiAliasing;
			CGI::CSHandle                 _cshAntiAliasingExtra;
			CGI::RPHandle                 _rphMotionBlur;
			CGI::FBHandle                 _fbhMotionBlur;
			CGI::CSHandle                 _cshMotionBlur;
			CGI::CSHandle                 _cshMotionBlurExtra;
			float                         _bloomRadius = 0.015f;
			float                         _bloomLightShaftsRadius = 0.004f;
			float                         _dofFocusDist = 10;
			float                         _dofBlurStrength = 0.2f;
			float                         _ssrRadius = 0.025f;
			bool                          _enableDepthOfField = false;

		public:
			Blur();
			~Blur();

			void Init();
			void Done();

			void OnSwapChainResized();

			void Generate();
			void GenerateForBloom(bool forLightShafts);
			void GenerateForDepthOfField();
			void GenerateForSsao();
			void GenerateForSsr();
			void GenerateForAntiAliasing();
			void GenerateForMotionBlur();

			void UpdateUniformBuffer(float radius, int sampleCount, int texSize = 0, float samplesPerPixel = 1, int maxSamples = 61);

			bool IsDepthOfFieldEnabled() const { return _enableDepthOfField; }
			void EnableDepthOfField(bool b) { _enableDepthOfField = b; }
			float GetDofFocusDistance() const { return _dofFocusDist; }
			void SetDofFocusDistance(float dist) { _dofFocusDist = dist; }
			float GetDofBlurStrength() const { return _dofBlurStrength; }
			void SetDofBlurStrength(float strength) { _dofBlurStrength = strength; }
		};
		VERUS_TYPEDEFS(Blur);
	}
}
