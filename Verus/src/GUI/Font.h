// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::GUI
{
	// Module for drawing texture-based fonts.
	// Use AngelCode Bitmap Font Generator to create required files.
	// Geometry uses -32767 to 32767 short texture coordinates.
	class Font : public Object
	{
	public:
#include "../Shaders/Font.inc.hlsl"

		struct CharInfo
		{
			int   _x;
			int   _y;
			int   _w;
			int   _h;
			int   _xoffset;
			int   _yoffset;
			int   _xadvance;
			short _s;
			short _t;
			short _sEnd;
			short _tEnd;
		};
		VERUS_TYPEDEFS(CharInfo);

		struct Kerning
		{
			Map<int, int> _mapAmount;
		};
		VERUS_TYPEDEFS(Kerning);

		struct Vertex
		{
			float _x, _y;
			short _s, _t;
			BYTE  _color[4];
		};

		typedef Map<int, CharInfo> TMapCharInfo;
		typedef Map<int, Kerning> TMapKerning;

	private:
		static CGI::ShaderPwn s_shader;
		static UB_FontVS      s_ubFontVS;
		static UB_FontFS      s_ubFontFS;

		CGI::DynamicBuffer<Vertex> _dynBuffer;
		CGI::PipelinePwn           _pipe;
		CGI::TexturePwn            _tex;
		CGI::CSHandle              _csh;
		TMapCharInfo               _mapCharInfo;
		TMapKerning                _mapKerning;
		int                        _lineHeight = 0;
		int                        _texSize = 0;
		UINT32                     _overrideColor = 0;

	public:
		struct DrawDesc
		{
			CWSZ   _text = nullptr;
			float  _x = 0;
			float  _y = 0;
			float  _w = 1;
			float  _h = 1;
			int    _skippedLineCount = 0;
			float  _scale = 1;
			bool   _center = false;
			bool   _preserveAspectRatio = false;
			UINT32 _colorFont = VERUS_COLOR_WHITE;
		};
		VERUS_TYPEDEFS(DrawDesc);

		Font();
		~Font();

		static void InitStatic();
		static void DoneStatic();

		void Init(CSZ url);
		void Done();

		void ResetDynamicBuffer();

		int GetLineHeight() const { return _lineHeight; }

		void Draw(RcDrawDesc dd);
		float DrawWord(CWSZ word, int wordLen, float xoffset, float yoffset, bool onlyCalcWidth, UINT32 color, float xScale, float yScale);
		int GetTextWidth(CWSZ text, int textLen = -1);

		static float ToFloatX(int size, float scale);
		static float ToFloatY(int size, float scale);
	};
	VERUS_TYPEDEFS(Font);
}
