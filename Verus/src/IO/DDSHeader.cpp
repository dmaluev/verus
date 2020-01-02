#include "verus.h"

using namespace verus;
using namespace verus::IO;

DDSHeader::DDSHeader()
{
	VERUS_CT_ASSERT(128 == sizeof(DDSHeader));
	VERUS_ZERO_MEM(*this);
	memcpy(_magic, "DDS ", 4);
	_size = 124;
	_flags = Flags::caps | Flags::height | Flags::width | Flags::pixelFormat;
	_pixelFormat._size = 32;
	_caps._caps1 = Caps1::texture;
}

bool DDSHeader::Validate() const
{
	const Flags reqFlags = Flags::height | Flags::width;
	return !memcmp(_magic, "DDS ", 4) &&
		124 == _size &&
		static_cast<Flags>(_flags & reqFlags) == reqFlags &&
		32 == _pixelFormat._size;
}

bool DDSHeader::IsDXT10() const
{
	return (_pixelFormat._flags & PixelFormatFlags::fourCC) && (FourCC::dx10 == _pixelFormat._fourCC);
}

bool DDSHeader::Is4BitsBC() const
{
	return (_pixelFormat._flags & PixelFormatFlags::fourCC) && (
		FourCC::dxt1 == _pixelFormat._fourCC ||
		FourCC::bc4u == _pixelFormat._fourCC ||
		FourCC::bc4s == _pixelFormat._fourCC);
}

bool DDSHeader::IsBC1() const
{
	return (_pixelFormat._flags & PixelFormatFlags::fourCC) && (FourCC::dxt1 == _pixelFormat._fourCC);
}

bool DDSHeader::IsBC2() const
{
	return (_pixelFormat._flags & PixelFormatFlags::fourCC) && (FourCC::dxt3 == _pixelFormat._fourCC);
}

bool DDSHeader::IsBC3() const
{
	return (_pixelFormat._flags & PixelFormatFlags::fourCC) && (FourCC::dxt5 == _pixelFormat._fourCC);
}

bool DDSHeader::IsBC() const
{
	return IsBC1() || IsBC2() || IsBC3() || IsDXT10();
}

bool DDSHeader::IsBGRA8() const
{
	return
		(_pixelFormat._flags & PixelFormatFlags::rgb) &&
		(_pixelFormat._flags & PixelFormatFlags::alphaPixels) &&
		(_pixelFormat._rgbBitCount == 32) &&
		(_pixelFormat._rBitMask == 0xFF0000u) &&
		(_pixelFormat._gBitMask == 0xFF00u) &&
		(_pixelFormat._bBitMask == 0xFFu) &&
		(_pixelFormat._rgbAlphaBitMask == 0xFF000000u);
}

bool DDSHeader::IsBGR8() const
{
	return
		(_pixelFormat._flags & PixelFormatFlags::rgb) &&
		!(_pixelFormat._flags & PixelFormatFlags::alphaPixels) &&
		(_pixelFormat._rgbBitCount == 24) &&
		(_pixelFormat._rBitMask == 0xFF0000u) &&
		(_pixelFormat._gBitMask == 0xFF00u) &&
		(_pixelFormat._bBitMask == 0xFFu);
}

int DDSHeader::ComputeBcLevelSize(int w, int h, bool is4Bits)
{
	int w4 = w >> 2;
	int h4 = h >> 2;
	if (!w4) w4 = 1;
	if (!h4) h4 = 1;
	return w4 * h4 * (is4Bits ? 8 : 16);
}

int DDSHeader::ComputeBcPitch(int w, int h, bool is4Bits)
{
	const int size = ComputeBcLevelSize(w, h, is4Bits);
	w = Math::Max(4, w);
	h = Math::Max(4, h);
	return size / h * 4;
}

int DDSHeader::GetPartCount() const
{
	if (_mipMapCount > 0 && IsBC())
	{
		const int partCount = static_cast<int>(_mipMapCount) - 9;
		return (partCount > 0) ? partCount : 1;
	}
	else
		return 1;
}

int DDSHeader::SkipParts(int skipCount)
{
	const int partCount = GetPartCount();
	if (skipCount >= 256)
		skipCount = 8 + partCount - Math::HighestBit(skipCount);
	skipCount = Math::Clamp(skipCount, 0, partCount - 1);
	if (skipCount)
	{
		_mipMapCount -= skipCount;
		_height >>= skipCount;
		_width >>= skipCount;
		if (!_height) _height = 1;
		if (!_width) _width = 1;
		_pitchOrLinearSize = ComputeBcLevelSize(_width, _height, Is4BitsBC());
	}
	_reserved2 = skipCount;
	return skipCount;
}

bool DDSHeaderDXT10::IsBC7() const
{
	return Format::bc7 == _dxgiFormat;
}
