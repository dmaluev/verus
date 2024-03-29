// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Effects
{
	class Blur : public Singleton<Blur>, public Object
	{
#include "../Shaders/Blur.inc.hlsl"

		enum PIPE
		{
			PIPE_U,
			PIPE_V,

			PIPE_VSM_U,
			PIPE_VSM_V,

			PIPE_SSAO,

			PIPE_RESOLVE_DITHERING,
			PIPE_SHARPEN,

			PIPE_DOF_U,
			PIPE_DOF_V,

			PIPE_BLOOM_U,
			PIPE_BLOOM_V,
			PIPE_BLOOM_LIGHT_SHAFTS_U,
			PIPE_BLOOM_LIGHT_SHAFTS_V,

			PIPE_AA,
			PIPE_AA_OFF,
			PIPE_MOTION_BLUR,
			PIPE_MOTION_BLUR_OFF,

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

		CGI::RPHandle                 _rphVsmU;
		CGI::RPHandle                 _rphVsmV;

		CGI::RPHandle                 _rphSsao;
		CGI::FBHandle                 _fbhSsao;
		CGI::CSHandle                 _cshSsao;

		Handles                       _rdsHandles;
		CGI::CSHandle                 _cshRdsExtra;

		Handles                       _dofHandles;
		CGI::CSHandle                 _cshDofExtra;

		Handles                       _bloomHandles;

		CGI::RPHandle                 _rphAntiAliasing;
		CGI::FBHandle                 _fbhAntiAliasing;
		CGI::CSHandle                 _cshAntiAliasing;
		CGI::CSHandle                 _cshAntiAliasingExtra;

		CGI::RPHandle                 _rphMotionBlur;
		CGI::FBHandle                 _fbhMotionBlur;
		CGI::CSHandle                 _cshMotionBlur;
		CGI::CSHandle                 _cshMotionBlurExtra;

		float                         _dofFocusDist = 10;
		float                         _dofRadius = 0.005f;
		float                         _bloomRadius = 0.02f;
		float                         _bloomLightShaftsRadius = 0.002f;
		bool                          _enableDepthOfField = false;

	public:
		Blur();
		~Blur();

		void Init();
		void Done();

		void OnSwapChainResized();

		CGI::ShaderPtr GetShader() const { return _shader; }

		void GenerateForVSM(CGI::FBHandle fbhU, CGI::FBHandle fbhV, CGI::CSHandle cshU, CGI::CSHandle cshV,
			RcVector4 rc, RcVector4 zNearFarEx);
		void GenerateForSsao();
		void GenerateForResolveDitheringAndSharpen();
		void GenerateForDepthOfField();
		void GenerateForBloom(bool forLightShafts);
		void GenerateForAntiAliasing();
		void GenerateForMotionBlur();

		void UpdateUniformBuffer(float radius, int sampleCount, int texSize = 0, float samplesPerPixel = 1, int maxSamples = 61);

		CGI::RPHandle GetRenderPassHandleForVsmU() const { return _rphVsmU; }
		CGI::RPHandle GetRenderPassHandleForVsmV() const { return _rphVsmV; }

		bool IsDepthOfFieldEnabled() const { return _enableDepthOfField; }
		void EnableDepthOfField(bool b) { _enableDepthOfField = b; }
		float GetDofFocusDistance() const { return _dofFocusDist; }
		void SetDofFocusDistance(float dist) { _dofFocusDist = dist; }
		float GetDofRadius() const { return _dofRadius; }
		void SetDofRadius(float radius) { _dofRadius = radius; }
	};
	VERUS_TYPEDEFS(Blur);
}
