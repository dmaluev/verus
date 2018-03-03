#include "verus.h"

using namespace verus;

WideString Str::Utf8ToWide(RcString utf8)
{
	Vector<wchar_t> vWide;
	utf8::utf8to16(utf8.begin(), utf8::find_invalid(utf8.begin(), utf8.end()), std::back_inserter(vWide));
	return WideString(vWide.begin(), vWide.end());
}

String Str::WideToUtf8(RcWideString wide)
{
	Vector<char> vUtf8;
	utf8::utf16to8(wide.begin(), utf8::find_invalid(wide.begin(), wide.end()), std::back_inserter(vUtf8));
	return String(vUtf8.begin(), vUtf8.end());
}

void Str::Utf8ToWide(CSZ utf8, WSZ wide, int length)
{
#ifdef _WIN32
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, length);
#else
	const WideString s = Utf8ToWide(utf8);
	wcsncpy(wide, s.c_str(), length);
	wide[length - 1] = 0;
#endif
}

void Str::WideToUtf8(CWSZ wide, SZ utf8, int length)
{
#ifdef _WIN32
	WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, length, 0, 0);
#else
	const String s = WideToUtf8(wide);
	strncpy(utf8, s.c_str(), length);
	utf8[length - 1] = 0;
#endif
}

String Str::ResizeUtf8(RcString utf8, int len)
{
	String ret = utf8;
	bool valid = true;
	do
	{
		ret = ret.substr(0, len);
		valid = utf8::is_valid(ret.begin(), ret.end());
		len--;
	} while (len > 0 && !valid);
	return ret;
}

bool Str::StartsWith(CSZ s, CSZ begin, bool caseSensitive)
{
	return caseSensitive ? !strncmp(s, begin, strlen(begin)) : !_strnicmp(s, begin, strlen(begin));
}

bool Str::EndsWith(CSZ s, CSZ end, bool caseSensitive)
{
	const size_t lenString = strlen(s);
	const size_t lenEnd = strlen(end);
	if (lenEnd > lenString)
		return false;
	CSZ test = s + lenString - lenEnd;
	return caseSensitive ? !strcmp(test, end) : !_stricmp(test, end);
}

CSZ Str::Find(CSZ s, CSZ sub, bool caseSensitive)
{
	return caseSensitive ? strstr(s, sub) : StrStrIA(s, sub);
}

void Str::ToLower(SZ s)
{
	while (*s)
	{
		*s = tolower(*s);
		s++;
	}
}

void Str::ToUpper(SZ s)
{
	while (*s)
	{
		*s = toupper(*s);
		s++;
	}
}

int Str::ReplaceAll(RString s, CSZ was, CSZ with)
{
	const size_t len = strlen(was);
	const size_t lenNew = strlen(with);
	size_t pos = s.find(was);
	int num = 0;
	while (pos < 1024 * 1024)
	{
		s.replace(pos, len, with);
		pos = s.find(was, pos + lenNew);
		num++;
	}
	return num;
}

void Str::ReplaceExtension(RString s, CSZ ext)
{
	size_t d = s.rfind(".");
	if (d != String::npos)
	{
		size_t num = s.length() - d;
		s.replace(d, num, ext);
	}
}

void Str::ReplaceFilename(RString s, CSZ name)
{
	size_t d = s.rfind("/");
	if (d != String::npos)
	{
		size_t num = s.length() - d;
		s.replace(d + 1, num, name);
	}
}

String Str::FromInt(int n)
{
	StringStream ss;
	ss << n;
	return ss.str();
}

void Str::Explode(CSZ s, CSZ delimiter, Vector<String>& pieces)
{
	pieces.clear();
	const size_t lenDelim = strlen(delimiter);
	CSZ found = strstr(s, delimiter);
	while (found)
	{
		if (found - s > 0)
			pieces.push_back(String(s, found));
		s = found + lenDelim;
		found = strstr(s, delimiter);
	}
	if (strlen(s) > 0)
		pieces.push_back(s);
}

void Str::Trim(RString s)
{
	auto pos = s.find_last_not_of(VERUS_WHITESPACE);
	if (pos != String::npos)
	{
		s.erase(pos + 1);
		pos = s.find_first_not_of(VERUS_WHITESPACE);
		if (pos != String::npos)
			s.erase(0, pos);
	}
	else
		s.erase(s.begin(), s.end());
}

void Str::TrimVector(Vector<String>& v)
{
	VERUS_WHILE(Vector<String>, v, it)
	{
		Trim(*it);
		if ((*it).empty())
			it = v.erase(it);
		else
			it++;
	}
}

void Str::ReadLines(CSZ s, Vector<String>& lines)
{
	Explode(s, "\n", lines);
	TrimVector(lines);
}

BYTE Str::ByteFromHex(CSZ s)
{
	BYTE ret = 0;
	if (strlen(s) >= 2)
	{
		const char hi = s[0];
		const char lo = s[1];

		if /**/ (hi >= '0' && hi <= '9') ret = ((hi - '0')) << 4;
		else if (hi >= 'a' && hi <= 'f') ret = ((hi - 'a') + 10) << 4;
		else if (hi >= 'A' && hi <= 'F') ret = ((hi - 'A') + 10) << 4;

		if /**/ (lo >= '0' && lo <= '9') ret |= (lo - '0');
		else if (lo >= 'a' && lo <= 'f') ret |= (lo - 'a') + 10;
		else if (lo >= 'A' && lo <= 'F') ret |= (lo - 'A') + 10;
	}
	return ret;
}

String Str::GetDate(bool andTime)
{
	time_t t;
	time(&t);
	tm* pTM = localtime(&t);
	char buffer[40];
	strftime(buffer, sizeof(buffer), andTime ? "%Y-%m-%d %H-%M-%S" : "%Y-%m-%d", pTM);
	return buffer;
}

glm::vec2 Str::FromStringVec2(CSZ s)
{
	glm::vec2 v;
	sscanf(s, "%f %f", &v.x, &v.y);
	return v;
}

glm::vec3 Str::FromStringVec3(CSZ s)
{
	glm::vec3 v;
	sscanf(s, "%f %f %f", &v.x, &v.y, &v.z);
	return v;
}

glm::vec4 Str::FromStringVec4(CSZ s)
{
	glm::vec4 v;
	sscanf(s, "%f %f %f %f", &v.x, &v.y, &v.z, &v.w);
	return v;
}

String Str::ToString(int i)
{
	char buffer[20];
	sprintf_s(buffer, "%d", i);
	return buffer;
}

String Str::ToString(float f)
{
	char buffer[20];
	sprintf_s(buffer, "%g", f);
	return buffer;
}

String Str::ToString(const glm::vec2& v)
{
	char buffer[40];
	sprintf_s(buffer, "%g %g", v.x, v.y);
	return buffer;
}

String Str::ToString(const glm::vec3& v)
{
	char buffer[60];
	sprintf_s(buffer, "%g %g %g", v.x, v.y, v.z);
	return buffer;
}

String Str::ToString(const glm::vec4& v)
{
	char buffer[80];
	sprintf_s(buffer, "%g %g %g %g", v.x, v.y, v.z, v.w);
	return buffer;
}

glm::vec4 Str::FromColorString(CSZ sz, bool sRGB)
{
	float rgba[4];
	Convert::ColorTextToFloat4(sz, rgba, sRGB);
	return glm::make_vec4(rgba);
}

String Str::ToColorString(const glm::vec4& v, bool sRGB)
{
	const UINT32 color = Convert::ColorFloatToInt32(glm::value_ptr(v), sRGB);
	char txt[20];
	sprintf_s(txt, "%02X%02X%02X%02X",
		(color >> 0) & 0xFF,
		(color >> 8) & 0xFF,
		(color >> 16) & 0xFF,
		(color >> 24) & 0xFF);
	return txt;
}

String Str::XmlEscape(CSZ s)
{
	StringStream ss;
	const int len = Utils::Cast32(strlen(s));
	VERUS_FOR(i, len)
	{
		switch (s[i])
		{
		case '&':	ss << "&amp;"; break;
		case '<':	ss << "&lt;"; break;
		case '>':	ss << "&gt;"; break;
		case '\"':	ss << "&quot;"; break;
		case '\'':	ss << "&apos;"; break;
		default:	ss << s[i];
		}
	}
	return ss.str();
}

String Str::XmlUnescape(CSZ s)
{
	StringStream ss;
	const int len = Utils::Cast32(strlen(s));
	VERUS_FOR(i, len)
	{
		if (s[i] == '&')
		{
			if /**/ (!strncmp(s + i, "&amp;", 5)) { ss << '&'; i += 4; }
			else if (!strncmp(s + i, "&lt;", 4)) { ss << '<'; i += 3; }
			else if (!strncmp(s + i, "&gt;", 4)) { ss << '>'; i += 3; }
			else if (!strncmp(s + i, "&quot;", 6)) { ss << '\"'; i += 5; }
			else if (!strncmp(s + i, "&apos;", 6)) { ss << '\''; i += 5; }
		}
		else
			ss << s[i];
	}
	return ss.str();
}

String Str::UrlEncode(CSZ s)
{
	String ret;
	ret.reserve(strlen(s) * 3);
	while (*s)
	{
		if ((*s >= 'A' && *s <= 'Z') ||
			(*s >= 'a' && *s <= 'z') ||
			(*s >= '0' && *s <= '9'))
		{
			ret.append(1, *s);
		}
		else
		{
			ret += "%";
			ret += Convert::ByteToHex(*s);
		}
		s++;
	}
	return ret;
}

String Str::FixCyrillicX(CSZ s, bool trans)
{
	String ret;

#ifdef _WIN32

	while (*s)
	{
		if (*s == '\\')
		{
			s = strchr(s + 1, '\\');
			if (!s)
				break;
			else
				s++;
		}

		if (!(*s))
			break;

		if (*s == ' ' && trans)
		{
			ret += '_';
		}
		else if (*s > 0 && *s < 128)
		{
			ret += *s;
		}
		else
		{
			if (trans)
			{
				switch (*s)
				{
				case 'А':
				case 'а': ret += 'a'; break;
				case 'Б':
				case 'б': ret += 'b'; break;
				case 'В':
				case 'в': ret += 'v'; break;
				case 'Г':
				case 'г': ret += 'g'; break;
				case 'Д':
				case 'д': ret += 'd'; break;
				case 'Е':
				case 'е': ret += 'e'; break;
				case 'Ё':
				case 'ё': ret += 'e'; break;
				case 'Ж':
				case 'ж': ret += "zh"; break;
				case 'З':
				case 'з': ret += 'z'; break;
				case 'И':
				case 'и': ret += 'i'; break;
				case 'Й':
				case 'й': ret += "i"; break;

				case 'К':
				case 'к': ret += 'k'; break;
				case 'Л':
				case 'л': ret += 'l'; break;

				case 'М':
				case 'м': ret += 'm'; break;
				case 'Н':
				case 'н': ret += 'n'; break;
				case 'О':
				case 'о': ret += 'o'; break;
				case 'П':
				case 'п': ret += 'p'; break;
				case 'Р':
				case 'р': ret += 'r'; break;
				case 'С':
				case 'с': ret += 's'; break;
				case 'Т':
				case 'т': ret += 't'; break;

				case 'У':
				case 'у': ret += 'u'; break;
				case 'Ф':
				case 'ф': ret += 'f'; break;
				case 'Х':
				case 'х': ret += 'h'; break;
				case 'Ц':
				case 'ц': ret += 'c'; break;
				case 'Ч':
				case 'ч': ret += "ch"; break;
				case 'Ш':
				case 'ш': ret += "sh"; break;
				case 'Щ':
				case 'щ': ret += "sh"; break;
				case 'Ъ':
				case 'ъ': ret += ""; break;
				case 'Ы':
				case 'ы': ret += 'q'; break;
				case 'Ь':
				case 'ь': ret += ""; break;
				case 'Э':
				case 'э': ret += 'e'; break;
				case 'Ю':
				case 'ю': ret += "yu"; break;
				case 'Я':
				case 'я': ret += "ya"; break;
				}
			}
			else
			{
				ret += *s;
			}
		}
		s++;
	}

	if (!trans)
	{
		char bufferA[256];
		wchar_t buffer[256];
		MultiByteToWideChar(1251, 0, ret.c_str(), -1, buffer, 256);
		WideCharToMultiByte(CP_UTF8, 0, buffer, -1, bufferA, 256, 0, 0);
		ret = bufferA;
	}

#endif

	return ret;
}

String Str::CyrillicWideToAnsi(CWSZ s)
{
	String ansi;
	ansi.reserve(wcslen(s));
	while (*s)
	{
		if (*s < 128)
		{
			ansi += char(*s);
		}
		else if (*s >= 0x410 && *s < 0x450)
		{
			ansi += char(*s - 0x410 + 0xC0);
		}
		else if (*s == 0x401)
		{
			ansi += char(0xA8);
		}
		else if (*s == 0x451)
		{
			ansi += char(0xB8);
		}
		else
		{
			ansi += '?';
		}
		s++;
	}
	return ansi;
}

bool Str::IsMultiLang(CSZ s)
{
	while (*s)
	{
		if (int(*s) >= 128)
			return true;
		s++;
	}
	return false;
}

void Str::CyrillicToUppercase(SZ s)
{
	while (*s)
	{
		if (*s >= 'а' && *s <= 'я')
			*s = *s - ('а' - 'А');
		if (*s == 'ё')
			*s = 'Ё';
		s++;
	}
}

void Str::Test()
{
	String utf8Text = Str::WideToUtf8(L"Test");
	WideString wideText = Str::Utf8ToWide(utf8Text);

	VERUS_RT_ASSERT(Str::StartsWith("Foobar!", "Foo"));
	VERUS_RT_ASSERT(!Str::StartsWith("Foobar!", "Bar"));
	VERUS_RT_ASSERT(Str::EndsWith("Foobar!", "bar!"));
	VERUS_RT_ASSERT(!Str::EndsWith("Foobar!", "Foo"));

	char makeLower[] = "HelloWorld!";
	Str::ToLower(makeLower);
	VERUS_RT_ASSERT(!strcmp(makeLower, "helloworld!"));

	String replace = "Use Windows. New Windows is good!";
	ReplaceAll(replace, "Windows", "Linux");
	VERUS_RT_ASSERT(replace == "Use Linux. New Linux is good!");

	String five = Str::FromInt(5);
	VERUS_RT_ASSERT(five == "5");

	Vector<String> exRes;
	Str::Explode("Hello|World|Foo|Bar||!|0|1|", "|", exRes);
	Str::Explode("YoFooYoBarYoYoHelloYoWorld", "Yo", exRes);

	String trimMe = "  Trim Me   ";
	Str::Trim(trimMe);
	VERUS_RT_ASSERT(trimMe == "Trim Me");

	Vector<String> lines;
	Str::ReadLines("Line1\r\nLine2\nLine3\n", lines);
	VERUS_RT_ASSERT(lines.size() == 3);
}
