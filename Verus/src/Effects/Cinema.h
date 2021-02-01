// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Effects
	{
		class Cinema : public Singleton<Cinema>, public Object
		{
#include "../Shaders/Cinema.inc.hlsl"

			static UB_CinemaVS s_ubCinemaVS;
			static UB_CinemaFS s_ubCinemaFS;

			CGI::ShaderPwn   _shader;
			CGI::PipelinePwn _pipe;
			CGI::TexturePwn  _texFilmGrain;
			CGI::CSHandle    _csh;
			float            _uOffset = 0;
			float            _vOffset = 0;
			float            _brightness = 1;
			float            _flickerStrength = 0.01f;
			float            _noiseStrength = 0.15f;
			bool             _editMode = false;

		public:
			Cinema();
			~Cinema();

			void Init();
			void Done();

			void Draw();

			bool IsEditMode() const { return _editMode; }
			void ToggleEditMode() { _editMode = !_editMode; }
		};
		VERUS_TYPEDEFS(Cinema);
	}
}
