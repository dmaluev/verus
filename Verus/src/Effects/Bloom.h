// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
				PIPE_LIGHT_SHAFTS,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_PING,
				TEX_PONG,
				TEX_COUNT
			};

			static UB_BloomVS            s_ubBloomVS;
			static UB_BloomFS            s_ubBloomFS;
			static UB_BloomLightShaftsFS s_ubBloomLightShaftsFS;

			CGI::ShaderPwn                _shader;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwns<TEX_COUNT>   _tex;
			CGI::TexturePtr               _texAtmoShadow;
			CGI::RPHandle                 _rph;
			CGI::RPHandle                 _rphLightShafts;
			CGI::FBHandle                 _fbh;
			CGI::CSHandle                 _csh;
			CGI::CSHandle                 _cshLightShafts;
			float                         _colorScale = 0.7f;
			float                         _colorBias = 1.4f;
			float                         _maxDist = 20;
			float                         _sunGloss = 128;
			float                         _wideStrength = 0.15f;
			float                         _sunStrength = 0.35f;
			bool                          _blur = true;
			bool                          _blurLightShafts = true;
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
