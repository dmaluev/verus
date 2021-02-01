// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Effects
	{
		class Ssr : public Singleton<Ssr>, public Object
		{
#include "../Shaders/Ssr.inc.hlsl"

			static UB_SsrVS s_ubSsrVS;
			static UB_SsrFS s_ubSsrFS;

			CGI::ShaderPwn   _shader;
			CGI::PipelinePwn _pipe;
			CGI::RPHandle    _rph;
			CGI::FBHandle    _fbh;
			CGI::CSHandle    _csh;
			float            _radius = 2.5f;
			float            _depthBias = 0.03f;
			float            _thickness = 0.25f;
			float            _equalizeDist = 12;
			bool             _blur = true;
			bool             _editMode = false;

		public:
			Ssr();
			~Ssr();

			void Init();
			void Done();

			void OnSwapChainResized();

			void Generate();

			bool IsEditMode() const { return _editMode; }
			void ToggleEditMode() { _editMode = !_editMode; }
		};
		VERUS_TYPEDEFS(Ssr);
	}
}
