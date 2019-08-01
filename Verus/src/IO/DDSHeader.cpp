#include "verus.h"

using namespace verus;
using namespace verus::IO;

DDSHeader::DDSHeader()
{
	VERUS_CT_ASSERT(128 == sizeof(DDSHeader));
	VERUS_ZERO_MEM(*this);
	memcpy(magic, "DDS ", 4);
	size = 124;
	flags = Flags::caps | Flags::height | Flags::width | Flags::pixelFormat;
	pixelFormat.size = 32;
	caps.caps1 = Caps1::texture;
}

bool DDSHeader::Validate() const
{
	const Flags reqFlags = Flags::caps | Flags::height | Flags::width | Flags::pixelFormat;
	return !memcmp(magic, "DDS ", 4) &&
		124 == size &&
		static_cast<Flags>(flags & reqFlags) == reqFlags &&
		32 == pixelFormat.size &&
		(caps.caps1 & Caps1::texture);
}

bool DDSHeader::IsBC1() const
{
	return (pixelFormat.flags & PixelFormatFlags::fourCC) && (pixelFormat.fourCC == FourCC::dxt1);
}

bool DDSHeader::IsBC2() const
{
	return (pixelFormat.flags & PixelFormatFlags::fourCC) && (pixelFormat.fourCC == FourCC::dxt3);
}

bool DDSHeader::IsBC3() const
{
	return (pixelFormat.flags & PixelFormatFlags::fourCC) && (pixelFormat.fourCC == FourCC::dxt5);
}

bool DDSHeader::IsBC() const
{
	return IsBC1() || IsBC2() || IsBC3();
}

bool DDSHeader::IsBGRA8() const
{
	return
		(pixelFormat.flags & PixelFormatFlags::rgb) &&
		(pixelFormat.flags & PixelFormatFlags::alphaPixels) &&
		(pixelFormat.rgbBitCount == 32) &&
		(pixelFormat.rBitMask == 0xFF0000u) &&
		(pixelFormat.gBitMask == 0xFF00u) &&
		(pixelFormat.bBitMask == 0xFFu) &&
		(pixelFormat.rgbAlphaBitMask == 0xFF000000u);
}

bool DDSHeader::IsBGR8() const
{
	return
		(pixelFormat.flags & PixelFormatFlags::rgb) &&
		!(pixelFormat.flags & PixelFormatFlags::alphaPixels) &&
		(pixelFormat.rgbBitCount == 24) &&
		(pixelFormat.rBitMask == 0xFF0000u) &&
		(pixelFormat.gBitMask == 0xFF00u) &&
		(pixelFormat.bBitMask == 0xFFu);
}

int DDSHeader::ComputeBcLevelSize(int w, int h, bool bc1)
{
	int w4 = w >> 2;
	int h4 = h >> 2;
	if (!w4) w4 = 1;
	if (!h4) h4 = 1;
	return w4 * h4*(bc1 ? 8 : 16);
}

int DDSHeader::ComputeBcPitch(int w, int h, bool bc1)
{
	const int size = IO::DDSHeader::ComputeBcLevelSize(w, h, bc1);
	w = Math::Max(4, w);
	h = Math::Max(4, h);
	return size / h * 4;
}

int DDSHeader::GetNumParts() const
{
	if (mipmapCount > 0 && IsBC())
	{
		const int numParts = int(mipmapCount) - 9;
		return (numParts > 0) ? numParts : 1;
	}
	else
		return 1;
}

int DDSHeader::SkipParts(int numSkip)
{
	const int numParts = GetNumParts();
	if (numSkip >= 256)
		numSkip = 8 + numParts - Math::HighestBit(numSkip);
	numSkip = Math::Clamp(numSkip, 0, numParts - 1);
	if (numSkip)
	{
		mipmapCount -= numSkip;
		height >>= numSkip;
		width >>= numSkip;
		if (!height) height = 1;
		if (!width) width = 1;
		pitchOrLinearSize = ComputeBcLevelSize(width, height, IsBC1());
	}
	reserved2 = numSkip;
	return numSkip;
}
