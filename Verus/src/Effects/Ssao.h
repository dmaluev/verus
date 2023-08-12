// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::Effects
{
	class Ssao : public Singleton<Ssao>, public Object
	{
#include "../Shaders/Ssao.inc.hlsl"

		static UB_SsaoVS s_ubSsaoVS;
		static UB_SsaoFS s_ubSsaoFS;

		CGI::ShaderPwn   _shader;
		CGI::PipelinePwn _pipe;
		CGI::TexturePwn  _texRandNormals;
		CGI::RPHandle    _rph;
		CGI::FBHandle    _fbh;
		CGI::CSHandle    _csh;
		float            _smallRad = 0.02f;
		float            _largeRad = 0.05f;
		float            _weightScale = 10;
		float            _weightBias = 2.5f;
		bool             _blur = true;
		bool             _editMode = false;

	public:
		Ssao();
		~Ssao();

		void Init();
		void InitCmd();
		void Done();

		void OnSwapChainResized();

		void Generate();

		void UpdateRandNormalsTexture();

		bool IsEditMode() const { return _editMode; }
		void ToggleEditMode() { _editMode = !_editMode; }
	};
	VERUS_TYPEDEFS(Ssao);
}
