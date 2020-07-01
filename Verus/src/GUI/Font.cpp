#include "verus.h"

using namespace verus;
using namespace verus::GUI;

CGI::ShaderPwn Font::s_shader;

Font::UB_FontFS Font::s_ubFontFS;

Font::Font()
{
	VERUS_CT_ASSERT(16 == sizeof(Vertex));
}

Font::~Font()
{
	Done();
}

void Font::InitStatic()
{
	s_shader.Init("[Shaders]:Font.hlsl");
	s_shader->CreateDescriptorSet(0, &s_ubFontFS, sizeof(s_ubFontFS), 100,
		{
			CGI::Sampler::anisoSharp
		}, CGI::ShaderStageFlags::fs);
	s_shader->CreatePipelineLayout();
}

void Font::DoneStatic()
{
	s_shader.Done();
}

void Font::Init(CSZ url)
{
	VERUS_INIT();
	VERUS_QREF_RENDERER;

	CGI::TextureDesc texDesc;
	texDesc._url = url;
	_tex.Init(texDesc);

	String xmlUrl(url);
	Str::ReplaceExtension(xmlUrl, ".xml");
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(_C(xmlUrl), vData, IO::FileSystem::LoadDesc(true));

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size());
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
	pugi::xml_node root = doc.first_child();

	pugi::xml_node commonNode = root.child("common");
	_lineHeight = commonNode.attribute("lineHeight").as_int(_lineHeight);
	_texSize = commonNode.attribute("scaleW").as_int(_texSize);

	pugi::xml_node charsNode = root.child("chars");
	for (auto node : charsNode.children())
	{
		CharInfo ci = {};
		const int id = node.attribute("id").as_int();
		ci._x = node.attribute("x").as_int();
		ci._y = node.attribute("y").as_int();
		ci._w = node.attribute("width").as_int();
		ci._h = node.attribute("height").as_int();
		ci._xoffset = node.attribute("xoffset").as_int();
		ci._yoffset = node.attribute("yoffset").as_int();
		ci._xadvance = node.attribute("xadvance").as_int();
		ci._s = ci._x * SHRT_MAX / _texSize;
		ci._t = ci._y * SHRT_MAX / _texSize;
		ci._sEnd = (ci._x + ci._w) * SHRT_MAX / _texSize;
		ci._tEnd = (ci._y + ci._h) * SHRT_MAX / _texSize;
		_mapCharInfo[id] = ci;
	}

	pugi::xml_node kerningsNode = root.child("kernings");
	for (auto node : kerningsNode.children())
	{
		const int firstChar = node.attribute("first").as_int();
		const int secondChar = node.attribute("second").as_int();
		const int amount = node.attribute("amount").as_int();
		auto it = _mapKerning.find(secondChar);
		if (it != _mapKerning.end())
		{
			RKerning k = it->second;
			k._mapAmount[firstChar] = amount;
		}
		else
		{
			Kerning k;
			k._mapAmount[firstChar] = amount;
			_mapKerning[secondChar] = k;
		}
	}

	CGI::GeometryDesc geoDesc;
	const CGI::VertexInputAttrDesc viaDesc[] =
	{
		{0, offsetof(Vertex, _x),     CGI::ViaType::floats, 2, CGI::ViaUsage::position, 0},
		{0, offsetof(Vertex, _s),     CGI::ViaType::shorts, 2, CGI::ViaUsage::texCoord, 0},
		{0, offsetof(Vertex, _color), CGI::ViaType::ubytes, 4, CGI::ViaUsage::color, 0},
		CGI::VertexInputAttrDesc::End()
	};
	geoDesc._pVertexInputAttrDesc = viaDesc;
	const int strides[] = { sizeof(Vertex), 0 };
	geoDesc._pStrides = strides;
	_dynBuffer.Init(geoDesc, 0x10000);
	_dynBuffer.CreateVertexBuffer();

	CGI::PipelineDesc pipeDesc(_dynBuffer, s_shader, "#", renderer.GetRenderPassHandle_AutoWithDepth());
	pipeDesc._colorAttachBlendEqs[0] = VERUS_COLOR_BLEND_ALPHA;
	pipeDesc.DisableDepthTest();
	_pipe.Init(pipeDesc);
}

void Font::Done()
{
	s_shader->FreeDescriptorSet(_csh);
	VERUS_DONE(Font);
}

void Font::ResetDynamicBuffer()
{
	_dynBuffer.Reset();
}

void Font::Draw(RcDrawDesc dd)
{
	VERUS_QREF_VM;
	VERUS_QREF_RENDERER;

	if (!_csh.IsSet())
	{
		if (_tex->IsLoaded())
			_csh = s_shader->BindDescriptorSetTextures(0, { _tex });
		else
			return;
	}

	const wchar_t wrapChars[] = L" \t\r\n-";
	CWSZ text = dd._text;
	int lineCount = -dd._skippedLineCount;
	const float lineHeight = ToFloatY(_lineHeight, dd._scale);
	const float whitespaceWidth = ToFloatX(_mapCharInfo[' ']._xadvance, dd._scale);
	const float yLimit = dd._y + dd._h - lineHeight;
	float xoffset = dd._x;
	float yoffset = dd._y;
	if (dd._center)
	{
		const float textWidth = ToFloatX(GetTextWidth(dd._text), dd._scale);
		xoffset = xoffset + (dd._w - textWidth) * 0.5f;
	}
	const float xoffsetInit = xoffset;

	auto cb = renderer.GetCommandBuffer();

	cb->BindVertexBuffers(_dynBuffer);
	cb->BindPipeline(_pipe);

	s_shader->BeginBindDescriptors();
	cb->BindDescriptors(s_shader, 0, _csh);
	_dynBuffer.Begin();

	// Draw chars:
	while (*text)
	{
		const float spaceLeft = (dd._x + dd._w) - xoffset;

		int wordLen = wcscspn(text, wrapChars); // First occurrence of wrap chars.

		if (!wordLen && L'-' == *text)
			wordLen = 1; // Don't throw away hyphen.

		if (wordLen > 0) // Add word:
		{
			if (L'-' != *text && L'-' == text[wordLen])
				wordLen++; // Also include hyphen, but not 2 in a row.

			const float wordWidth = ToFloatX(GetTextWidth(text, wordLen), dd._scale);
			if (wordWidth > spaceLeft) // Word is too big; new line:
			{
				// 1) goto new line:
				lineCount++;
				xoffset = xoffsetInit;
				if (lineCount > 0)
					yoffset += lineHeight;

				if (yoffset >= yLimit)
					break; // No more vertical space.

				// 2) draw the word:
				xoffset += DrawWord(text, wordLen, xoffset, yoffset, lineCount < 0, dd._colorFont, dd._scale);
			}
			else // Word fits in:
			{
				// Draw the word:
				xoffset += DrawWord(text, wordLen, xoffset, yoffset, lineCount < 0, dd._colorFont, dd._scale);
			}

			text += wordLen; // Next char.
		}
		else // Deal with wrap char:
		{
			if (L'\n' == *text) // New line:
			{
				lineCount++;
				xoffset = xoffsetInit;
				if (lineCount > 0)
					yoffset += lineHeight;
			}
			else // Add space:
			{
				xoffset += whitespaceWidth;
			}
			text++; // Next char.
		}

		if (yoffset >= yLimit && lineCount)
			break; // No more vertical space.
	}

	_dynBuffer.End();
	s_shader->EndBindDescriptors();
}

float Font::DrawWord(CWSZ word, int wordLen, float xoffset, float yoffset, bool onlyCalcWidth, UINT32 color, float scale)
{
	const float xbegin = xoffset;

	VERUS_FOR(i, wordLen)
	{
		VERUS_IF_FOUND_IN(TMapCharInfo, _mapCharInfo, word[i], itCi)
		{
			RcCharInfo ci = itCi->second;
			int kerning = 0;
			if (i > 0)
			{
				VERUS_IF_FOUND_IN(TMapKerning, _mapKerning, word[i], itK)
				{
					RcKerning k = itK->second;
					auto it = k._mapAmount.find(word[i - 1]);
					if (it != k._mapAmount.end())
						kerning = it->second;
				}
			}

			if (!onlyCalcWidth)
			{
				const float xFloat = xoffset + ToFloatX(ci._xoffset + kerning, scale);
				const float yFloat = yoffset + ToFloatY(ci._yoffset, scale);
				const float xFloatEnd = xFloat + ToFloatX(ci._w, scale);
				const float yFloatEnd = yFloat + ToFloatY(ci._h, scale);

				const float xScreen = xFloat * 2 - 1;
				const float yScreen = yFloat * -2 + 1;
				const float xScreenEnd = xFloatEnd * 2 - 1;
				const float yScreenEnd = yFloatEnd * -2 + 1;

				Vertex a0, a1, b0, b1;

				// A0:
				a0._x = xScreen;
				a0._y = yScreen;
				a0._s = ci._s;
				a0._t = ci._t;
				Utils::CopyColor(a0._color, color);
				// A1:
				a1._x = xScreenEnd;
				a1._y = yScreen;
				a1._s = ci._sEnd;
				a1._t = ci._t;
				Utils::CopyColor(a1._color, color);
				// B0:
				b0._x = xScreen;
				b0._y = yScreenEnd;
				b0._s = ci._s;
				b0._t = ci._tEnd;
				Utils::CopyColor(b0._color, color);
				// B1:
				b1._x = xScreenEnd;
				b1._y = yScreenEnd;
				b1._s = ci._sEnd;
				b1._t = ci._tEnd;
				Utils::CopyColor(b1._color, color);

				_dynBuffer.AddQuad(a0, a1, b0, b1);
			}

			xoffset += ToFloatX(ci._xadvance + kerning, scale);
		}
	}
	return xoffset - xbegin;
}

int Font::GetTextWidth(CWSZ text, int textLen)
{
	const int len = (textLen >= 0) ? textLen : wcslen(text);
	int width = 0;
	VERUS_FOR(i, len)
	{
		VERUS_IF_FOUND_IN(TMapCharInfo, _mapCharInfo, text[i], itCi)
		{
			RcCharInfo ci = itCi->second;
			int kerning = 0;
			if (i > 0)
			{
				VERUS_IF_FOUND_IN(TMapKerning, _mapKerning, text[i], itK)
				{
					RcKerning k = itK->second;
					auto it = k._mapAmount.find(text[i - 1]);
					if (it != k._mapAmount.end())
						kerning = it->second;
				}
			}
			width += ci._xadvance + kerning;
		}
	}
	return width;
}

float Font::ToFloatX(int size, float scale)
{
	return size * (1 / 1920.f) * scale;
}

float Font::ToFloatY(int size, float scale)
{
	return size * (1 / 1080.f) * scale;
}
