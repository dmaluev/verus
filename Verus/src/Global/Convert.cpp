#include "verus.h"

using namespace verus;

// F to I:
BYTE   Convert::UnormToUint8(float x) { return BYTE(x*UCHAR_MAX + 0.5f); }
UINT16 Convert::UnormToUint16(float x) { return UINT16(x*USHRT_MAX + 0.5f); }
char   Convert::SnormToSint8(float x) { return char((x >= 0) ? x * SCHAR_MAX + 0.5f : x * SCHAR_MAX - 0.5f); }
short  Convert::SnormToSint16(float x) { return short((x >= 0) ? x * SHRT_MAX + 0.5f : x * SHRT_MAX - 0.5f); }

// I to F:
float Convert::Uint8ToUnorm(BYTE x) { return x * (1.f / UCHAR_MAX); }
float Convert::Uint16ToUnorm(UINT16 x) { return x * (1.f / USHRT_MAX); }
float Convert::Sint8ToSnorm(char x) { if (x == SCHAR_MIN) x = SCHAR_MIN + 1; return x * (1.f / SCHAR_MAX); }
float Convert::Sint16ToSnorm(short x) { if (x == SHRT_MIN) x = SHRT_MIN + 1; return x * (1.f / SHRT_MAX); }

// AoF to AoI:
void    Convert::UnormToUint8(const float* pIn, BYTE* pOut, int num) { VERUS_FOR(i, num) pOut[i] = UnormToUint8(pIn[i]); }
void Convert::UnormToUint16(const float* pIn, UINT16* pOut, int num) { VERUS_FOR(i, num) pOut[i] = UnormToUint16(pIn[i]); }
void    Convert::SnormToSint8(const float* pIn, char* pOut, int num) { VERUS_FOR(i, num) pOut[i] = SnormToSint8(pIn[i]); }
void  Convert::SnormToSint16(const float* pIn, short* pOut, int num) { VERUS_FOR(i, num) pOut[i] = SnormToSint16(pIn[i]); }

// AoI to AoF:
void    Convert::Uint8ToUnorm(const BYTE* pIn, float* pOut, int num) { VERUS_FOR(i, num) pOut[i] = Uint8ToUnorm(pIn[i]); }
void Convert::Uint16ToUnorm(const UINT16* pIn, float* pOut, int num) { VERUS_FOR(i, num) pOut[i] = Uint16ToUnorm(pIn[i]); }
void    Convert::Sint8ToSnorm(const char* pIn, float* pOut, int num) { VERUS_FOR(i, num) pOut[i] = Sint8ToSnorm(pIn[i]); }
void  Convert::Sint16ToSnorm(const short* pIn, float* pOut, int num) { VERUS_FOR(i, num) pOut[i] = Sint16ToSnorm(pIn[i]); }

// I to I:
short Convert::Sint8ToSint16(char x) { return x * SHRT_MAX / SCHAR_MAX; }
char Convert::Sint16ToSint8(short x) { return x * SCHAR_MAX / SHRT_MAX; }
void Convert::Sint8ToSint16(const char* pIn, short* pOut, int num) { VERUS_FOR(i, num) pOut[i] = Sint8ToSint16(pIn[i]); }
void Convert::Sint16ToSint8(const short* pIn, char* pOut, int num) { VERUS_FOR(i, num) pOut[i] = Sint16ToSint8(pIn[i]); }

void Convert::ToDeviceNormal(const char* pIn, char* pOut)
{
	// For UBYTE4 type normal:
	// OpenGL glNormalPointer() only accepts signed bytes (GL_BYTE)
	// Direct3D 9 only accepts unsigned bytes (D3DDECLTYPE_UBYTE4) So it goes.
	//VERUS_QREF_RENDER;
	//if (CGL::RENDERER_DIRECT3D9 == render.GetRenderer())
	//{
	//	VERUS_FOR(i, 3)
	//		pOut[i] = pIn[i] + 127;
	//}
}

UINT32 Convert::ToDeviceColor(UINT32 color)
{
	// OpenGL stores color as RGBA. Direct3D 9 as BGRA.
	// See also GL_EXT_vertex_array_bgra.
	//VERUS_QREF_RENDER;
	//return (CGL::RENDERER_DIRECT3D9 == render.GetRenderer()) ? VERUS_COLOR_TO_D3D(color) : color;
	return 0;
}

float Convert::Byte256ToSFloat(BYTE in)
{
	return float(in)*(1 / 128.f) - 1;
}

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

UINT32 Convert::Uint4x4ToUint8x4(UINT16 in)
{
	int x[4] =
	{
		(in >> 12) & 0xF,
		(in >> 8) & 0xF,
		(in >> 4) & 0xF,
		(in >> 0) & 0xF
	};
	return (x[0] << 28) | (x[1] << 20) | (x[2] << 12) | (x[3] << 4);
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
	const float gamma = sRGB ? 2.2f : 1.f;
	out[0] = pow(float(int((in >> 0) & 0xFF))*(1 / 255.f), gamma);
	out[1] = pow(float(int((in >> 8) & 0xFF))*(1 / 255.f), gamma);
	out[2] = pow(float(int((in >> 16) & 0xFF))*(1 / 255.f), gamma);
	out[3] = float(int((in >> 24) & 0xFF))*(1 / 255.f);
}

UINT32 Convert::ColorFloatToInt32(const float* in, bool sRGB)
{
	const float gamma = sRGB ? 1 / 2.2f : 1.f;
	const int r = Math::Clamp(int(pow(in[0], gamma)*255.5f), 0, 255);
	const int g = Math::Clamp(int(pow(in[1], gamma)*255.5f), 0, 255);
	const int b = Math::Clamp(int(pow(in[2], gamma)*255.5f), 0, 255);
	const int a = Math::Clamp(int(in[3] * 255.5f), 0, 255);
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
		const int num = sscanf(sz, "%d %d %d %d",
			color + 0,
			color + 1,
			color + 2,
			color + 3);
		if (3 == num)
			color[3] = 255;
	}
	const float gamma = sRGB ? 2.2f : 1.f;
	out[0] = pow(float(color[0])*(1 / 255.f), gamma);
	out[1] = pow(float(color[1])*(1 / 255.f), gamma);
	out[2] = pow(float(color[2])*(1 / 255.f), gamma);
	out[3] = float(color[3])*(1 / 255.f);
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

void Convert::ToCorrectNormal(const char* in, char* out)
{
	// For UBYTE4 type normal:
	// OpenGL glNormalPointer() only accepts signed bytes (GL_BYTE)
	// Direct3D only accepts unsigned bytes (D3DDECLTYPE_UBYTE4) So it goes.
	//VERUS_QREF_RENDER;
	//if (CGL::RENDERER_OPENGL != render.GetRenderer())
	//{
	//	VERUS_FOR(i, 3)
	//		out[i] = in[i] + 125;
	//}
}

UINT32 Convert::ToCorrectColor(UINT32 in)
{
	// OpenGL stores color as RGBA. Direct3D 9 as BGRA.
	// See also GL_EXT_vertex_array_bgra.
	//VERUS_QREF_RENDER;
	//return (CGL::RENDERER_DIRECT3D9 == render.GetRenderer()) ? VERUS_COLOR_TO_D3D(in) : in;
	return 0;
}

void Convert::ByteToChar3(const BYTE* in, char* out)
{
	VERUS_FOR(i, 3)
		out[i] = int(in[i]) - 125;
}

void Convert::ByteToShort3(const BYTE* in, short* out)
{
	VERUS_FOR(i, 3)
		out[i] = (int(in[i]) << 8) - 32000;
}

UINT16 Convert::QuantizeFloat(float f, float mn, float mx)
{
	const float range = mx - mn;
	return UINT16(Math::Clamp((f - mn) / range * USHRT_MAX, 0.f, float(USHRT_MAX)));
}

float Convert::DequantizeFloat(UINT16 i, float mn, float mx)
{
	const float range = mx - mn;
	return float(i) / USHRT_MAX * range + mn;
}

BYTE Convert::QuantizeFloatToByte(float f, float mn, float mx)
{
	const float range = mx - mn;
	return BYTE(Math::Clamp((f - mn) / range * UCHAR_MAX, 0.f, float(UCHAR_MAX)));
}

float Convert::DequantizeFloatFromByte(BYTE i, float mn, float mx)
{
	const float range = mx - mn;
	return float(i) / UCHAR_MAX * range + mn;
}

String Convert::ToBase64(const Vector<BYTE>& vBin)
{
	Vector<char> vBase64;
	vBase64.resize(vBin.size() * 2);
	base64_encodestate state;
	base64_init_encodestate(&state);
	const int len = base64_encode_block(reinterpret_cast<CSZ>(vBin.data()), Utils::Cast32(vBin.size()), vBase64.data(), &state);
	const int len2 = base64_encode_blockend(&vBase64[len], &state);
	vBase64.resize(len + len2);
	return String(vBase64.begin(), vBase64.end());
}

Vector<BYTE> Convert::ToBinFromBase64(CSZ base64)
{
	Vector<BYTE> vBin;
	vBin.resize(strlen(base64));
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
	const int num = Utils::Cast32(vBin.size());
	Vector<char> vHex;
	vHex.resize(num * 2);
	VERUS_FOR(i, num)
	{
		vHex[(i << 1) + 0] = hexval[(vBin[i] >> 4) & 0xF];
		vHex[(i << 1) + 1] = hexval[(vBin[i] >> 0) & 0xF];
	}
	return String(vHex.begin(), vHex.end());
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

	Vector<BYTE> vBin;
	vBin.resize(16);

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
}
