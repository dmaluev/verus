// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Effects
	{
		class Bloom : public Singleton<Bloom>, public Object
		{
#include "../Shaders/Bloom.inc.hlsl"

			enum PIPE
			{
				PIPE_MAIN,
				PIPE_GOD_RAYS,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_PING,
				TEX_PONG,
				TEX_COUNT
			};

			static UB_BloomVS        s_ubBloomVS;
			static UB_BloomFS        s_ubBloomFS;
			static UB_BloomGodRaysFS s_ubBloomGodRaysFS;

			CGI::ShaderPwn                _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwns<TEX_COUNT>   _tex;
			CGI::TexturePtr               _texAtmoShadow;
			CGI::RPHandle                 _rph;
			CGI::RPHandle                 _rphGodRays;
			CGI::FBHandle                 _fbh;
			CGI::CSHandle                 _csh;
			CGI::CSHandle                 _cshGodRays;
			float                         _colorScale = 0.8f;
			float                         _colorBias = 1.1f;
			float                         _maxDist = 20;
			float                         _sunGloss = 128;
			float                         _wideStrength = 0.2f;
			float                         _sunStrength = 0.3f;
			bool                          _blur = true;
			bool                          _blurGodRays = true;
			bool                          _editMode = false;

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

			bool IsEditMode() const { return _editMode; }
			void ToggleEditMode() { _editMode = !_editMode; }
		};
		VERUS_TYPEDEFS(Bloom);
	}
}
