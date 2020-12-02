// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	//! This class converts different data types.
	class Convert
	{
	public:
		// Single floating-point to integer:
		static   BYTE UnormToUint8(float x);
		static UINT16 UnormToUint16(float x);
		static   char SnormToSint8(float x);
		static  short SnormToSint16(float x);

		// Single integer to Floating-point:
		static float Uint8ToUnorm(BYTE x);
		static float Uint16ToUnorm(UINT16 x);
		static float Sint8ToSnorm(char x);
		static float Sint16ToSnorm(short x);

		// Array of floating-points to integers:
		static void UnormToUint8(const float* pIn, BYTE* pOut, int count);
		static void UnormToUint16(const float* pIn, UINT16* pOut, int count);
		static void SnormToSint8(const float* pIn, char* pOut, int count);
		static void SnormToSint16(const float* pIn, short* pOut, int count);

		// Array of integers to floating-points:
		static void Uint8ToUnorm(const BYTE* pIn, float* pOut, int count);
		static void Uint16ToUnorm(const UINT16* pIn, float* pOut, int count);
		static void Sint8ToSnorm(const char* pIn, float* pOut, int count);
		static void Sint16ToSnorm(const short* pIn, float* pOut, int count);

		// Integers to integers:
		static short Sint8ToSint16(char x);
		static char Sint16ToSint8(short x);
		static void Sint8ToSint16(const char* pIn, short* pOut, int count);
		static void Sint16ToSint8(const short* pIn, char* pOut, int count);

		// 4 bits per channel:
		static UINT16 Uint8x4ToUint4x4(UINT32 in);
		static UINT32 Uint4x4ToUint8x4(UINT16 in, bool scaleTo255 = false);

		// Colors:
		static float SRGBToLinear(float color);
		static float LinearToSRGB(float color);
		static void SRGBToLinear(float color[]);
		static void LinearToSRGB(float color[]);
		static UINT32 Color16To32(UINT16 in);
		static   void ColorInt32ToFloat(UINT32 in, float* out, bool sRGB = true);
		static UINT32 ColorFloatToInt32(const float* in, bool sRGB = true);
		static   void ColorTextToFloat4(CSZ sz, float* out, bool sRGB = true);
		static UINT32 ColorTextToInt32(CSZ sz);

		// Floating-point quantization:
		static         UINT16 QuantizeFloat(float f, float mn, float mx);
		static       float DequantizeFloat(UINT16 i, float mn, float mx);
		static     BYTE QuantizeFloatToByte(float f, float mn, float mx);
		static float DequantizeFloatFromByte(BYTE i, float mn, float mx);

		// Base64:
		static String ToBase64(const Vector<BYTE>& vBin);
		static Vector<BYTE> ToBinFromBase64(CSZ base64);

		// Hexadecimal:
		static String ByteToHex(BYTE b);
		static String ToHex(const Vector<BYTE>& vBin);
		static String ToHex(UINT32 color);
		static Vector<BYTE> ToBinFromHex(CSZ hex);

		// MD5:
		static Vector<BYTE> ToMd5(const Vector<BYTE>& vBin);
		static String ToMd5String(const Vector<BYTE>& vBin);

		static void Test();
	};
}
