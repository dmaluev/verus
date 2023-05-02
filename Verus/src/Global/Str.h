// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Str
	{
	public:
		CSZ _sz;

		Str(CSZ sz) : _sz(sz) {}
		void operator=(CSZ sz) { _sz = sz; }

		bool operator==(const Str& that) const { return !strcmp(_sz, that._sz); }
		bool operator!=(const Str& that) const { return 0 != strcmp(_sz, that._sz); }
		bool operator<(const Str& that) const { return strcmp(_sz, that._sz) < 0; }

		size_t Size()   const { return strlen(_sz) + 1; }
		size_t Length() const { return strlen(_sz); }
		bool IsEmpty()  const { return !_sz || !(*_sz); }

		CSZ Find(CSZ sz) const { return strstr(_sz, sz); }

		CSZ c_str() const { return _sz; }

		// UTF8 <-> Wide:
		static WideString Utf8ToWide(RcString utf8);
		static String     WideToUtf8(RcWideString wide);
		static void Utf8ToWide(CSZ utf8, WSZ wide, int length);
		static void WideToUtf8(CWSZ wide, SZ utf8, int length);

		static String ResizeUtf8(RcString utf8, int len);

		static bool StartsWith(CSZ s, CSZ begin, bool caseSensitive = true);
		static bool EndsWith(CSZ s, CSZ end, bool caseSensitive = true);
		static CSZ Find(CSZ s, CSZ sub, bool caseSensitive = true);
		static void ToLower(SZ s);
		static void ToUpper(SZ s);
		static int ReplaceAll(RString s, CSZ was, CSZ with);
		static String FromInt(int n);
		static void Explode(CSZ s, CSZ delimiter, Vector<String>& pieces);
		static void Trim(RString s);
		static void TrimVector(Vector<String>& v);
		static void ReadLines(CSZ s, Vector<String>& lines);
		static BYTE ByteFromHex(CSZ s);
		static String GetDate(bool andTime = true);

		// File system:
		static String GetPath(CSZ pathname);
		static String GetFilename(CSZ pathname);
		static String GetExtension(CSZ pathname, bool toLower = true);
		static void ReplaceFilename(RString pathname, CSZ filename);
		static void ReplaceExtension(RString pathname, CSZ ext);
		static String ToPakFriendlyUrl(CSZ url);

		// From/To string:
		static glm::vec2 FromStringVec2(CSZ s);
		static glm::vec3 FromStringVec3(CSZ s);
		static glm::vec4 FromStringVec4(CSZ s);
		static String ToString(int i);
		static String ToString(float f);
		static String ToString(const glm::vec2& v, bool brackets = false);
		static String ToString(const glm::vec3& v, bool brackets = false);
		static String ToString(const glm::vec4& v, bool brackets = false);
		static glm::vec4 FromColorString(CSZ sz, bool sRGB = true);
		static String ToColorString(const glm::vec4& v, bool sRGB = true, bool hexPrefix = false);

		// XML:
		static String XmlEscape(CSZ s);
		static String XmlUnescape(CSZ s);

		// Web:
		static String UrlEncode(CSZ s);

		// Cyrillic:
		static String FixCyrillicX(CSZ s, bool trans);
		static String CyrillicWideToAnsi(CWSZ s);
		static bool IsMultiLang(CSZ s);
		static void CyrillicToUppercase(SZ s);

		static void Test();
	};

	class WideStr
	{
	public:
		CWSZ _sz;

		WideStr(CWSZ sz) : _sz(sz) {}
		void operator=(CWSZ sz) { _sz = sz; }

		bool operator==(const WideStr& that) const { return !wcscmp(_sz, that._sz); }
		bool operator!=(const WideStr& that) const { return 0 != wcscmp(_sz, that._sz); }

		size_t Size()   const { return (wcslen(_sz) + 1) << 1; }
		size_t Length() const { return wcslen(_sz); }

		CWSZ Find(CWSZ sz) const { return wcsstr(_sz, sz); }

		CWSZ c_str() const { return _sz; }
	};
}
