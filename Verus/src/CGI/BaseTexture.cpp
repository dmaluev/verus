#include "verus.h"

using namespace verus;
using namespace verus::CGI;

// BaseTexture:

BaseTexture::BaseTexture()
{
}

BaseTexture::~BaseTexture()
{
	IO::Async::Cancel(this);
}

bool BaseTexture::IsSRGB() const
{
	return
		!Str::EndsWith(_C(_name), ".NM.dds") &&
		!Str::EndsWith(_C(_name), ".FX.dds") &&
		!Str::EndsWith(_C(_name), ".H.dds");
}

void BaseTexture::LoadDDS(CSZ url, int texturePart)
{
	_name = url;
	IO::Async::I().Load(url, this, IO::Async::TaskDesc(false, false, texturePart));
}

void BaseTexture::LoadDDS(RcBlob blob)
{
	IO::DDSHeader header;
	memcpy(&header, blob._p, sizeof(header));
	if (!header.Validate())
		throw VERUS_RUNTIME_ERROR << "Invalid DDS header: " << _name;
	_part = header.reserved2;

	VERUS_QREF_CONST_SETTINGS;

	if (!header.mipmapCount)
		header.mipmapCount = 1;

	const int maxLod = Math::Max<int>(header.mipmapCount - 4, 0); // Keep 1x1, 2x2, 4x4.
	const int lod = Math::Min(settings._gpuTextureLodLevel, maxLod);

	if (header.IsBC())
	{
		auto SelectFormat = [this](Format unorm, Format srgb)
		{
			return IsSRGB() ? srgb : unorm;
		};

		TextureDesc desc;
		if (header.IsBC1())
			desc._format = SelectFormat(Format::unormBC1, Format::srgbBC1);
		else if (header.IsBC2())
			desc._format = SelectFormat(Format::unormBC2, Format::srgbBC2);
		else if (header.IsBC3())
			desc._format = SelectFormat(Format::unormBC3, Format::srgbBC3);
		desc._width = header.width >> lod;
		desc._height = header.height >> lod;
		desc._mipLevels = header.mipmapCount - lod;

		Init(desc);

		int offset = sizeof(header);
		for (UINT32 i = 0; i < header.mipmapCount; ++i)
		{
			UINT32 w = header.width >> i, h = header.height >> i;
			if (!w) w = 1;
			if (!h) h = 1;
			int levelSize = IO::DDSHeader::ComputeBcLevelSize(w, h, header.IsBC1());
			VERUS_RT_ASSERT(offset + levelSize <= blob._size);

			if (int(i) >= lod)
				UpdateImage(i - lod, blob._p + offset);

			offset += levelSize;
		}
	}
	else // Not DXTn?
	{
		if (header.pixelFormat.flags == IO::DDSHeader::PixelFormatFlags::alpha &&
			header.pixelFormat.rgbBitCount == 8)
		{
			TextureDesc desc;
			desc._format = Format::unormR8;
			desc._width = header.width;
			desc._height = header.height;
			desc._mipLevels = header.mipmapCount;

			Init(desc);

			int offset = sizeof(header);
			for (UINT32 i = 0; i < header.mipmapCount; ++i)
			{
				UINT32 w = header.width >> i, h = header.height >> i;
				if (!w) w = 1;
				if (!h) h = 1;

				UpdateImage(i, blob._p + offset);
				offset += w * h;
			}
		}
		else if (header.pixelFormat.flags == IO::DDSHeader::PixelFormatFlags::rgb &&
			header.pixelFormat.rgbBitCount == 24)
		{
			TextureDesc desc;
			desc._format = IsSRGB() ? Format::srgbR8G8B8A8 : Format::unormR8G8B8A8;
			desc._width = header.width;
			desc._height = header.height;
			desc._mipLevels = header.mipmapCount;

			Init(desc);

			int offset = sizeof(header);
			for (UINT32 i = 0; i < header.mipmapCount; ++i)
			{
				int w = header.width >> i, h = header.height >> i;
				if (!w) w = 1;
				if (!h) h = 1;

				const BYTE* p = blob._p + offset;
				Vector<UINT32> vData;
				vData.resize(w*h);
				VERUS_FOR(y, h)
				{
					VERUS_FOR(x, w)
					{
						const BYTE color[4] = { p[2], p[1], p[0], 255 };
						Utils::CopyColor(vData[y*w + x], color);
						p += 3;
					}
				}
				UpdateImage(i, vData.data());
				offset += w * h * 3;
			}
		}
		else if (header.pixelFormat.flags == (IO::DDSHeader::PixelFormatFlags::rgb | IO::DDSHeader::PixelFormatFlags::alphaPixels) &&
			header.pixelFormat.rgbBitCount == 32)
		{
			TextureDesc desc;
			desc._format = IsSRGB() ? Format::srgbR8G8B8A8 : Format::unormR8G8B8A8;
			desc._width = header.width;
			desc._height = header.height;
			desc._mipLevels = header.mipmapCount;

			Init(desc);

			int offset = sizeof(header);
			for (UINT32 i = 0; i < header.mipmapCount; ++i)
			{
				int w = header.width >> i, h = header.height >> i;
				if (!w) w = 1;
				if (!h) h = 1;

				const BYTE* p = blob._p + offset;
				Vector<UINT32> vData;
				vData.resize(w*h);
				VERUS_FOR(y, h)
				{
					VERUS_FOR(x, w)
					{
						const BYTE color[4] = { p[2], p[1], p[0], p[3] };
						Utils::CopyColor(vData[y*w + x], color);
						p += 4;
					}
				}
				UpdateImage(i, vData.data());
				offset += w * h * 4;
			}
		}
	}
}

void BaseTexture::Async_Run(CSZ url, RcBlob blob)
{
	LoadDDS(blob);
}

int BaseTexture::FormatToBytesPerPixel(Format format)
{
	switch (format)
	{
	case Format::unormB4G4R4A4:     return 2;
	case Format::unormB5G6R5:       return 2;

	case Format::unormR8:           return 1;
	case Format::unormR8G8:         return 2;
	case Format::unormR8G8B8A8:     return 4;
	case Format::unormB8G8R8A8:     return 4;
	case Format::srgbR8G8B8A8:      return 4;
	case Format::srgbB8G8R8A8:      return 4;

	case Format::floatR16:          return 2;
	case Format::floatR16G16:       return 4;
	case Format::floatR16G16B16A16: return 8;

	case Format::floatR32:          return 4;
	case Format::floatR32G32:       return 8;
	case Format::floatR32G32B32A32: return 16;

	case Format::unormD16:          return 2;
	case Format::unormD24uintS8:    return 4;
	case Format::floatD32:          return 4;
	}
	return 0;
}

bool BaseTexture::IsBC(Format format)
{
	return format >= Format::unormBC1 && format <= Format::srgbBC3;
}

bool BaseTexture::IsBC1(Format format)
{
	return format == Format::unormBC1 || format == Format::srgbBC1;
}

bool BaseTexture::IsDepthFormat(Format format)
{
	return format >= Format::unormD16 && format <= Format::floatD32;
}

// TexturePtr:

void TexturePtr::Init(RcTextureDesc desc)
{
	VERUS_QREF_RENDERER;
	VERUS_RT_ASSERT(!_p);
	_p = renderer->InsertTexture();
	if (desc._url)
		_p->LoadDDS(desc._url, desc._texturePart);
	else
		_p->Init(desc);
}

void TexturePwn::Done()
{
	if (_p)
	{
		VERUS_QREF_RENDERER;
		renderer->DeleteTexture(_p);
		_p = nullptr;
	}
}

TexturePtr TexturePtr::From(PBaseTexture p)
{
	TexturePtr ptr;
	ptr._p = p;
	return ptr;
}
