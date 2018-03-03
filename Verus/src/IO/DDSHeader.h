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
				mipmapCount /**/ = 0x00020000,
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
				mipmap  /**/ = 0x00400000
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
				dxt2 = '2TXD',
				dxt3 = '3TXD',
				dxt4 = '4TXD',
				dxt5 = '5TXD'
			};

			struct PixelFormat
			{
				UINT32           size;
				PixelFormatFlags flags;
				FourCC           fourCC;
				UINT32           rgbBitCount;
				UINT32           rBitMask;
				UINT32           gBitMask;
				UINT32           bBitMask;
				UINT32           rgbAlphaBitMask;
			};

			struct Caps
			{
				Caps1  caps1;
				Caps2  caps2;
				UINT32 reserved[2];
			};

			char        magic[4];
			UINT32      size;
			Flags       flags;
			UINT32      height;
			UINT32      width;
			UINT32      pitchOrLinearSize;
			UINT32      depth;
			UINT32      mipmapCount;
			UINT32      reserved1[11];
			PixelFormat pixelFormat;
			Caps        caps;
			UINT32      reserved2;

			DDSHeader();
			bool Validate() const;
			bool IsDxt1() const;
			bool IsDxt3() const;
			bool IsDxt5() const;
			bool IsDxt() const;
			bool IsBGRA8() const;
			bool IsBGR8() const;
			static int ComputeDxtLevelSize(int w, int h, bool dxt1 = true);
			int GetNumParts() const;
			int SkipParts(int numSkip);
		};
	}
}
