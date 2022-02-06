// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace IO
	{
		struct DDSHeader
		{
			enum class Flags : UINT32
			{
				caps        /**/ = 0x00000001,
				height      /**/ = 0x00000002,
				width       /**/ = 0x00000004,
				pitch       /**/ = 0x00000008,
				pixelFormat /**/ = 0x00001000,
				mipMapCount /**/ = 0x00020000,
				linearSize  /**/ = 0x00080000,
				depth       /**/ = 0x00800000
			};

			enum class PixelFormatFlags : UINT32
			{
				alphaPixels /**/ = 0x00000001,
				alpha       /**/ = 0x00000002,
				fourCC      /**/ = 0x00000004,
				indexed     /**/ = 0x00000020,
				rgb         /**/ = 0x00000040,
				compressed  /**/ = 0x00000080,
				luminance   /**/ = 0x00020000
			};

			enum class Caps1 : UINT32
			{
				complex /**/ = 0x00000008,
				texture /**/ = 0x00001000,
				mipMap  /**/ = 0x00400000
			};

			enum class Caps2 : UINT32
			{
				cubemap          /**/ = 0x00000200,
				cubemapPositiveX /**/ = 0x00000400,
				cubemapNegativeX /**/ = 0x00000800,
				cubemapPositiveY /**/ = 0x00001000,
				cubemapNegativeY /**/ = 0x00002000,
				cubemapPositiveZ /**/ = 0x00004000,
				cubemapNegativeZ /**/ = 0x00008000,
				volume           /**/ = 0x00200000
			};

			enum class FourCC : UINT32
			{
				dxt1 = '1TXD',
				dxt3 = '3TXD',
				dxt5 = '5TXD',
				bc4u = 'U4CB',
				bc4s = 'S4CB',
				bc5u = '2ITA',
				bc5s = 'S5CB',
				dx10 = '01XD'
			};

			struct PixelFormat
			{
				UINT32           _size;
				PixelFormatFlags _flags;
				FourCC           _fourCC;
				UINT32           _rgbBitCount;
				UINT32           _rBitMask;
				UINT32           _gBitMask;
				UINT32           _bBitMask;
				UINT32           _rgbAlphaBitMask;
			};

			struct Caps
			{
				Caps1  _caps1;
				Caps2  _caps2;
				UINT32 _reserved[2];
			};

			char        _magic[4];
			UINT32      _size;
			Flags       _flags;
			UINT32      _height;
			UINT32      _width;
			UINT32      _pitchOrLinearSize;
			UINT32      _depth;
			UINT32      _mipMapCount;
			UINT32      _reserved1[11];
			PixelFormat _pixelFormat;
			Caps        _caps;
			UINT32      _reserved2;

			DDSHeader();
			bool Validate() const;
			bool IsDXT10() const;
			bool Is4BitsBC() const;
			bool IsBC1() const;
			bool IsBC2() const;
			bool IsBC3() const;
			bool IsBC() const;
			bool IsBGRA8() const;
			bool IsBGR8() const;
			static int ComputeBcLevelSize(int w, int h, bool is4Bits);
			static int ComputeBcPitch(int w, int h, bool is4Bits);
			int GetPartCount() const;
			int SkipParts(int skipCount);
		};

		struct DDSHeaderDXT10
		{
			enum class Format : UINT32
			{
				bc4 = 80,
				bc5 = 83,
				bc7 = 98
			};

			enum class Dimension : UINT32
			{
				unknown = 0,
				buffer = 1,
				texture1D = 2,
				texture2D = 3,
				texture3D = 4
			};

			Format    _dxgiFormat;
			Dimension _resourceDimension;
			UINT32    _miscFlag;
			UINT32    _arraySize;
			UINT32    _miscFlags2;

			bool IsBC7() const;
		};
	}
}
