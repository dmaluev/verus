// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

// F to I:
BYTE   Convert::UnormToUint8(float x) { return BYTE(x * UCHAR_MAX + 0.5f); }
UINT16 Convert::UnormToUint16(float x) { return UINT16(x * USHRT_MAX + 0.5f); }
char   Convert::SnormToSint8(float x) { return char((x >= 0) ? x * SCHAR_MAX + 0.5f : x * SCHAR_MAX - 0.5f); }
short  Convert::SnormToSint16(float x) { return short((x >= 0) ? x * SHRT_MAX + 0.5f : x * SHRT_MAX - 0.5f); }

// I to F:
float Convert::Uint8ToUnorm(BYTE x) { return x * (1.f / UCHAR_MAX); }
float Convert::Uint16ToUnorm(UINT16 x) { return x * (1.f / USHRT_MAX); }
float Convert::Sint8ToSnorm(char x) { if (x == SCHAR_MIN) x = SCHAR_MIN + 1; return x * (1.f / SCHAR_MAX); }
float Convert::Sint16ToSnorm(short x) { if (x == SHRT_MIN) x = SHRT_MIN + 1; return x * (1.f / SHRT_MAX); }

// AoF to AoI:
void    Convert::UnormToUint8(const float* pIn, BYTE* pOut, int count) { VERUS_FOR(i, count) pOut[i] = UnormToUint8(pIn[i]); }
void Convert::UnormToUint16(const float* pIn, UINT16* pOut, int count) { VERUS_FOR(i, count) pOut[i] = UnormToUint16(pIn[i]); }
void    Convert::SnormToSint8(const float* pIn, char* pOut, int count) { VERUS_FOR(i, count) pOut[i] = SnormToSint8(pIn[i]); }
void  Convert::SnormToSint16(const float* pIn, short* pOut, int count) { VERUS_FOR(i, count) pOut[i] = SnormToSint16(pIn[i]); }

// AoI to AoF:
void    Convert::Uint8ToUnorm(const BYTE* pIn, float* pOut, int count) { VERUS_FOR(i, count) pOut[i] = Uint8ToUnorm(pIn[i]); }
void Convert::Uint16ToUnorm(const UINT16* pIn, float* pOut, int count) { VERUS_FOR(i, count) pOut[i] = Uint16ToUnorm(pIn[i]); }
void    Convert::Sint8ToSnorm(const char* pIn, float* pOut, int count) { VERUS_FOR(i, count) pOut[i] = Sint8ToSnorm(pIn[i]); }
void  Convert::Sint16ToSnorm(const short* pIn, float* pOut, int count) { VERUS_FOR(i, count) pOut[i] = Sint16ToSnorm(pIn[i]); }

// I to I:
short Convert::Sint8ToSint16(char x) { return x * SHRT_MAX / SCHAR_MAX; }
char Convert::Sint16ToSint8(short x) { return x * SCHAR_MAX / SHRT_MAX; }
void Convert::Sint8ToSint16(const char* pIn, short* pOut, int count) { VERUS_FOR(i, count) pOut[i] = Sint8ToSint16(pIn[i]); }
void Convert::Sint16ToSint8(const short* pIn, char* pOut, int count) { VERUS_FOR(i, count) pOut[i] = Sint16ToSint8(pIn[i]); }

UINT16 Convert::Uint8x4ToUint4x4(UINT32 in)
{
	int x[4] =
	{
		(in >> 28) & 0xF,
		(in >> 20) & 0xF,
		(in >> 12) & 0xF,
		(in >> 4) & 0xF
	};
	return (x[0] << 12) | (x[1] << 8) | (x[2] << 4) | (x[3]);
}

UINT32 Convert::Uint4x4ToUint8x4(UINT16 in, bool scaleTo255)
{
	int x[4] =
	{
		((in >> 12) & 0xF) << 4,
		((in >> 8) & 0xF) << 4,
		((in >> 4) & 0xF) << 4,
		((in >> 0) & 0xF) << 4
	};
	if (scaleTo255)
	{
		VERUS_FOR(i, 4)
			x[i] = x[i] * 255 / 0xF0;
	}
	return (x[0] << 24) | (x[1] << 16) | (x[2] << 8) | (x[3] << 0);
}

float Convert::SRGBToLinear(float color)
{
	return color < 0.04045f ? color * (1 / 12.92f) : pow((color + 0.055f) * (1 / 1.055f), 2.4f);
}

float Convert::LinearToSRGB(float color)
{
	return color < 0.0031308f ? 12.92f * color : 1.055f * pow(color, 1 / 2.4f) - 0.055f;
}

void Convert::SRGBToLinear(float color[])
{
	VERUS_FOR(i, 3)
		color[i] = SRGBToLinear(color[i]);
}

void Convert::LinearToSRGB(float color[])
{
	VERUS_FOR(i, 3)
		color[i] = LinearToSRGB(color[i]);
}

UINT32 Convert::Color16To32(UINT16 in)
{
	const int b = ((in >> 0) & 0x1F) << 3;
	const int g = ((in >> 5) & 0x3F) << 2;
	const int r = ((in >> 11) & 0x1F) << 3;
	return VERUS_COLOR_RGBA(r, g, b, 255);
}

void Convert::ColorInt32ToFloat(UINT32 in, float* out, bool sRGB)
{
	out[0] = ((in >> 0) & 0xFF) * (1 / 255.f);
	out[1] = ((in >> 8) & 0xFF) * (1 / 255.f);
	out[2] = ((in >> 16) & 0xFF) * (1 / 255.f);
	out[3] = ((in >> 24) & 0xFF) * (1 / 255.f);
	if (sRGB)
	{
		out[0] = SRGBToLinear(out[0]);
		out[1] = SRGBToLinear(out[1]);
		out[2] = SRGBToLinear(out[2]);
	}
}

UINT32 Convert::ColorFloatToInt32(const float* in, bool sRGB)
{
	float out[4];
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	if (sRGB)
	{
		out[0] = LinearToSRGB(out[0]);
		out[1] = LinearToSRGB(out[1]);
		out[2] = LinearToSRGB(out[2]);
	}
	const int r = Math::Clamp(static_cast<int>(out[0] * 255 + 0.5f), 0, 255);
	const int g = Math::Clamp(static_cast<int>(out[1] * 255 + 0.5f), 0, 255);
	const int b = Math::Clamp(static_cast<int>(out[2] * 255 + 0.5f), 0, 255);
	const int a = Math::Clamp(static_cast<int>(out[3] * 255 + 0.5f), 0, 255);
	return VERUS_COLOR_RGBA(r, g, b, a);
}

void Convert::ColorTextToFloat4(CSZ sz, float* out, bool sRGB)
{
	int color[4] = {};
	if (!sz)
	{
	}
	else if (6 == strlen(sz) && !strchr(sz, ' '))
	{
		color[0] = Str::ByteFromHex(sz + 0);
		color[1] = Str::ByteFromHex(sz + 2);
		color[2] = Str::ByteFromHex(sz + 4);
		color[3] = 255;
	}
	else if (8 == strlen(sz) && !strchr(sz, ' '))
	{
		color[0] = Str::ByteFromHex(sz + 0);
		color[1] = Str::ByteFromHex(sz + 2);
		color[2] = Str::ByteFromHex(sz + 4);
		color[3] = Str::ByteFromHex(sz + 6);
	}
	else
	{
		const int count = sscanf(sz, "%d %d %d %d",
			color + 0,
			color + 1,
			color + 2,
			color + 3);
		if (3 == count)
			color[3] = 255;
	}
	out[0] = color[0] * (1 / 255.f);
	out[1] = color[1] * (1 / 255.f);
	out[2] = color[2] * (1 / 255.f);
	out[3] = color[3] * (1 / 255.f);
	if (sRGB)
	{
		out[0] = SRGBToLinear(out[0]);
		out[1] = SRGBToLinear(out[1]);
		out[2] = SRGBToLinear(out[2]);
	}
}

UINT32 Convert::ColorTextToInt32(CSZ sz)
{
	int color[4] = {};
	if (6 == strlen(sz) && !strchr(sz, ' '))
	{
		color[0] = Str::ByteFromHex(sz + 0);
		color[1] = Str::ByteFromHex(sz + 2);
		color[2] = Str::ByteFromHex(sz + 4);
		color[3] = 255;
	}
	else if (8 == strlen(sz) && !strchr(sz, ' '))
	{
		color[0] = Str::ByteFromHex(sz + 0);
		color[1] = Str::ByteFromHex(sz + 2);
		color[2] = Str::ByteFromHex(sz + 4);
		color[3] = Str::ByteFromHex(sz + 6);
	}
	else
	{
		sscanf(sz, "%d %d %d %d",
			color + 0,
			color + 1,
			color + 2,
			color + 3);
	}
	return VERUS_COLOR_RGBA(color[0], color[1], color[2], color[3]);
}

UINT16 Convert::QuantizeFloat(float f, float mn, float mx)
{
	const float range = mx - mn;
	return UINT16(Math::Clamp<float>((f - mn) / range * USHRT_MAX + 0.5f, 0, USHRT_MAX));
}

float Convert::DequantizeFloat(UINT16 i, float mn, float mx)
{
	const float range = mx - mn;
	return float(i) / USHRT_MAX * range + mn;
}

BYTE Convert::QuantizeFloatToByte(float f, float mn, float mx)
{
	const float range = mx - mn;
	return BYTE(Math::Clamp<float>((f - mn) / range * UCHAR_MAX + 0.5f, 0, UCHAR_MAX));
}

float Convert::DequantizeFloatFromByte(BYTE i, float mn, float mx)
{
	const float range = mx - mn;
	return float(i) / UCHAR_MAX * range + mn;
}

String Convert::ToBase64(const Vector<BYTE>& vBin)
{
	Vector<char> vBase64(vBin.size() * 2);
	base64_encodestate state;
	base64_init_encodestate(&state);
	const int len = base64_encode_block(reinterpret_cast<CSZ>(vBin.data()), Utils::Cast32(vBin.size()), vBase64.data(), &state);
	const int len2 = base64_encode_blockend(&vBase64[len], &state);
	vBase64.resize(len + len2);
	return String(vBase64.begin(), vBase64.end());
}

Vector<BYTE> Convert::ToBinFromBase64(CSZ base64)
{
	Vector<BYTE> vBin(strlen(base64));
	base64_decodestate state;
	base64_init_decodestate(&state);
	const int len = base64_decode_block(base64, Utils::Cast32(strlen(base64)), reinterpret_cast<SZ>(vBin.data()), &state);
	vBin.resize(len);
	return vBin;
}

String Convert::ByteToHex(BYTE b)
{
	static const char hexval[] = "0123456789ABCDEF";
	char ret[3];
	ret[0] = hexval[(b >> 4) & 0xF];
	ret[1] = hexval[(b >> 0) & 0xF];
	ret[2] = 0;
	return ret;
}

String Convert::ToHex(const Vector<BYTE>& vBin)
{
	static const char hexval[] = "0123456789ABCDEF";
	const int count = Utils::Cast32(vBin.size());
	Vector<char> vHex(count * 2);
	VERUS_FOR(i, count)
	{
		vHex[(i << 1) + 0] = hexval[(vBin[i] >> 4) & 0xF];
		vHex[(i << 1) + 1] = hexval[(vBin[i] >> 0) & 0xF];
	}
	return String(vHex.begin(), vHex.end());
}

String Convert::ToHex(UINT32 color)
{
	static const char hexval[] = "0123456789ABCDEF";
	char hex[8];
	VERUS_FOR(i, 4)
	{
		hex[(i << 1) + 0] = hexval[((color >> (i << 3)) >> 4) & 0xF];
		hex[(i << 1) + 1] = hexval[((color >> (i << 3)) >> 0) & 0xF];
	}
	return String(hex, hex + 8);
}

Vector<BYTE> Convert::ToBinFromHex(CSZ hex)
{
	Vector<BYTE> vBin;
	const size_t len = strlen(hex);
	if (0 == len || (len & 0x1))
		return vBin;
	const int lenBin = Utils::Cast32(len / 2);
	vBin.resize(lenBin);
	VERUS_FOR(i, lenBin)
	{
		const char hi = hex[(i << 1)];
		const char lo = hex[(i << 1) + 1];

		if /**/ (hi >= '0' && hi <= '9') vBin[i] = ((hi - '0')) << 4;
		else if (hi >= 'a' && hi <= 'f') vBin[i] = ((hi - 'a') + 10) << 4;
		else if (hi >= 'A' && hi <= 'F') vBin[i] = ((hi - 'A') + 10) << 4;

		if /**/ (lo >= '0' && lo <= '9') vBin[i] |= (lo - '0');
		else if (lo >= 'a' && lo <= 'f') vBin[i] |= (lo - 'a') + 10;
		else if (lo >= 'A' && lo <= 'F') vBin[i] |= (lo - 'A') + 10;
	}
	return vBin;
}

Vector<BYTE> Convert::ToMd5(const Vector<BYTE>& vBin)
{
	MD5Hash md5;
	md5.update(vBin.data(), Utils::Cast32(vBin.size()));
	md5.finalize();

	Vector<BYTE> vOut = ToBinFromHex(_C(md5.hexdigest()));
	return vOut;
}

String Convert::ToMd5String(const Vector<BYTE>& vBin)
{
	return ToHex(ToMd5(vBin));
}

void Convert::Test()
{
	const float e = 1e-6f;

	BYTE dataUint8[] = { 0, 127, 255 };
	UINT16 dataUint16[] = { 0, 127, 65535, 65534 };
	char dataSint8[] = { 64, -127, -128 };
	short dataSint16[] = { 32767, -32766, -32768, -32767 };
	float dataFloat[4];

	Uint8ToUnorm(dataUint8, dataFloat, 3);
	UnormToUint8(dataFloat, dataUint8, 3);
	Uint16ToUnorm(dataUint16, dataFloat, 4);
	UnormToUint16(dataFloat, dataUint16, 4);
	Sint8ToSnorm(dataSint8, dataFloat, 3);
	SnormToSint8(dataFloat, dataSint8, 3);
	Sint16ToSnorm(dataSint16, dataFloat, 4);
	SnormToSint16(dataFloat, dataSint16, 4);

	BYTE colorIn[20];
	VERUS_FOR(i, 16)
		colorIn[i] = i;
	colorIn[16] = 64;
	colorIn[17] = 128;
	colorIn[18] = 192;
	colorIn[19] = 255;
	float colorOut[20];
	VERUS_FOR(i, 20)
		colorOut[i] = SRGBToLinear(colorIn[i] * (1 / 255.f));
	BYTE colorTest[20];
	VERUS_FOR(i, 20)
		colorTest[i] = static_cast<BYTE>(LinearToSRGB(colorOut[i]) * 255 + 0.5f);
	VERUS_FOR(i, 20)
	{
		if (i < 19)
		{
			const float d = colorOut[i + 1] - colorOut[i];
			if (i < 10)
				VERUS_RT_ASSERT(glm::epsilonEqual(d, colorOut[1], e));
			else
				VERUS_RT_ASSERT(glm::epsilonNotEqual(d, colorOut[1], e));
		}
		VERUS_RT_ASSERT(colorIn[i] == colorTest[i]);
	}

	Vector<BYTE> vBin(16);

	Random random;
	random.NextArray(vBin);

	const String base64 = ToBase64(vBin);
	const Vector<BYTE> vBinBase64 = ToBinFromBase64(_C(base64));

	const String hex = ToHex(vBin);
	const Vector<BYTE> vBinHex = ToBinFromHex(_C(hex));

	VERUS_RT_ASSERT(std::equal(vBin.begin(), vBin.end(), vBinHex.begin()));
	VERUS_RT_ASSERT(std::equal(vBin.begin(), vBin.end(), vBinBase64.begin()));

	CSZ txt = "Hello World 1! Hello World 2! Hello World 3!";
	vBin.assign(txt, txt + strlen(txt));
	const Vector<BYTE> vMd5 = ToMd5(vBin);
	const String md5 = ToMd5String(vBin);
	VERUS_RT_ASSERT(md5 == "C84AAFD3D09E719514977BF3624F4D85");

	const double someNum = fast_atof("1.097");
	VERUS_RT_ASSERT(1.097 == someNum);
}
