// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
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
		!Str::EndsWith(_C(_name), ".H.dds") &&
		!Str::EndsWith(_C(_name), ".N.dds") &&
		!Str::EndsWith(_C(_name), ".X.dds");
}

Format BaseTexture::ToTextureBcFormat(IO::RcDDSHeader header, IO::RcDDSHeaderDXT10 header10) const
{
	auto SelectFormat = [this](Format unorm, Format srgb)
	{
		return IsSRGB() ? srgb : unorm;
	};
	if (header.IsBC1())
		return SelectFormat(Format::unormBC1, Format::srgbBC1);
	else if (header.IsBC2())
		return SelectFormat(Format::unormBC2, Format::srgbBC2);
	else if (header.IsBC3())
		return SelectFormat(Format::unormBC3, Format::srgbBC3);
	else if (header.IsBC4U() || header.IsBC4S())
		return Format::unormBC4;
	else if (header.IsBC5U() || header.IsBC5S())
		return Format::unormBC5;
	else if (header10.IsBC7())
		return SelectFormat(Format::unormBC7, Format::srgbBC7);
	VERUS_RT_FAIL("DDS format is not BC.");
	return Format::unormR8G8B8A8;
}

void BaseTexture::LoadDDS(CSZ url, int texturePart)
{
	_name = url;
	IO::Async::I().Load(url, this, IO::Async::TaskDesc(false, false, texturePart));
}

void BaseTexture::LoadDDS(CSZ url, RcBlob blob)
{
	_name = url;

	int offset = sizeof(IO::DDSHeader);
	IO::DDSHeader header;
	memcpy(&header, blob._p, sizeof(header));
	if (!header.Validate())
		throw VERUS_RUNTIME_ERROR << "Invalid DDS header: " << _name;
	IO::DDSHeaderDXT10 header10;
	if (header.IsDXT10())
	{
		memcpy(&header10, blob._p + offset, sizeof(header10));
		offset += sizeof(IO::DDSHeaderDXT10);
	}
	_part = header._reserved2;

	VERUS_QREF_CONST_SETTINGS;

	if (!header._mipMapCount)
		header._mipMapCount = 1;

	const int maxLod = Math::Max<int>(header._mipMapCount - 4, 0); // Keep 1x1, 2x2, 4x4.
	const int lod = Math::Min(settings._gpuTextureLodLevel, maxLod);

	if (header.IsBC())
	{
		TextureDesc desc;
		desc._format = ToTextureBcFormat(header, header10);
		desc._width = header._width >> lod;
		desc._height = header._height >> lod;
		desc._mipLevels = header._mipMapCount - lod;
		desc._flags = _desc._flags;
		desc._pSamplerDesc = _desc._pSamplerDesc;

		Init(desc);

		for (UINT32 i = 0; i < header._mipMapCount; ++i)
		{
			UINT32 w = header._width >> i, h = header._height >> i;
			if (!w) w = 1;
			if (!h) h = 1;
			const int levelSize = IO::DDSHeader::ComputeBcLevelSize(w, h, header.Is4BitsBC());
			VERUS_RT_ASSERT(offset + levelSize <= blob._size);

			if (static_cast<int>(i) >= lod)
				UpdateSubresource(blob._p + offset, i - lod);

			offset += levelSize;
		}
	}
	else // Not DXTn?
	{
		if (header._pixelFormat._flags == IO::DDSHeader::PixelFormatFlags::alpha &&
			header._pixelFormat._rgbBitCount == 8)
		{
			TextureDesc desc;
			desc._format = Format::unormR8;
			desc._width = header._width;
			desc._height = header._height;
			desc._mipLevels = header._mipMapCount;
			desc._flags = _desc._flags;
			desc._pSamplerDesc = _desc._pSamplerDesc;

			Init(desc);

			for (UINT32 i = 0; i < header._mipMapCount; ++i)
			{
				UINT32 w = header._width >> i, h = header._height >> i;
				if (!w) w = 1;
				if (!h) h = 1;

				UpdateSubresource(blob._p + offset, i);
				offset += w * h;
			}
		}
		else if (header._pixelFormat._flags == IO::DDSHeader::PixelFormatFlags::rgb &&
			header._pixelFormat._rgbBitCount == 24)
		{
			TextureDesc desc;
			desc._format = IsSRGB() ? Format::srgbR8G8B8A8 : Format::unormR8G8B8A8;
			desc._width = header._width;
			desc._height = header._height;
			desc._mipLevels = header._mipMapCount;
			desc._flags = _desc._flags;
			desc._pSamplerDesc = _desc._pSamplerDesc;

			Init(desc);

			for (UINT32 i = 0; i < header._mipMapCount; ++i)
			{
				int w = header._width >> i, h = header._height >> i;
				if (!w) w = 1;
				if (!h) h = 1;

				const BYTE* p = blob._p + offset;
				Vector<UINT32> vData;
				vData.resize(w * h);
				VERUS_FOR(y, h)
				{
					VERUS_FOR(x, w)
					{
						const BYTE color[4] = { p[2], p[1], p[0], 255 };
						Utils::CopyColor(vData[y * w + x], color);
						p += 3;
					}
				}
				UpdateSubresource(vData.data(), i);
				offset += w * h * 3;
			}
		}
		else if (header._pixelFormat._flags == (IO::DDSHeader::PixelFormatFlags::rgb | IO::DDSHeader::PixelFormatFlags::alphaPixels) &&
			header._pixelFormat._rgbBitCount == 32)
		{
			TextureDesc desc;
			desc._format = IsSRGB() ? Format::srgbR8G8B8A8 : Format::unormR8G8B8A8;
			desc._width = header._width;
			desc._height = header._height;
			desc._mipLevels = header._mipMapCount;
			desc._flags = _desc._flags;
			desc._pSamplerDesc = _desc._pSamplerDesc;

			Init(desc);

			for (UINT32 i = 0; i < header._mipMapCount; ++i)
			{
				int w = header._width >> i, h = header._height >> i;
				if (!w) w = 1;
				if (!h) h = 1;

				const BYTE* p = blob._p + offset;
				Vector<UINT32> vData;
				vData.resize(w * h);
				VERUS_FOR(y, h)
				{
					VERUS_FOR(x, w)
					{
						const BYTE color[4] = { p[2], p[1], p[0], p[3] };
						Utils::CopyColor(vData[y * w + x], color);
						p += 4;
					}
				}
				UpdateSubresource(vData.data(), i);
				offset += w * h * 4;
			}
		}
	}
}

void BaseTexture::LoadDDSArray(CSZ* urls)
{
	TextureDesc desc;
	desc._arrayLayers = 0;
	CSZ* urlsCount = urls;
	while (*urlsCount)
	{
		desc._arrayLayers++;
		urlsCount++;
	}
	bool init = true;
	int arrayLayer = 0;
	while (*urls)
	{
		_name = *urls;

		Vector<BYTE> vData;
		IO::FileSystem::LoadResource(*urls, vData);

		const BYTE* pData = vData.data();
		const size_t size = vData.size();

		int offset = sizeof(IO::DDSHeader);
		IO::DDSHeader header;
		memcpy(&header, pData, sizeof(header));
		if (!header.Validate())
			throw VERUS_RUNTIME_ERROR << "Invalid DDS header: " << _name;
		IO::DDSHeaderDXT10 header10;
		if (header.IsDXT10())
		{
			memcpy(&header10, pData + offset, sizeof(header10));
			offset += sizeof(IO::DDSHeaderDXT10);
		}

		VERUS_QREF_CONST_SETTINGS;

		if (!header._mipMapCount)
			header._mipMapCount = 1;

		const int maxLod = Math::Max<int>(header._mipMapCount - 4, 0); // Keep 1x1, 2x2, 4x4.
		const int lod = Math::Min(settings._gpuTextureLodLevel, maxLod);

		if (header.IsBC())
		{
			const int w = header._width >> lod;
			const int h = header._height >> lod;
			if (desc._width)
			{
				VERUS_RT_ASSERT(desc._width == w);
			}
			if (desc._height)
			{
				VERUS_RT_ASSERT(desc._height == h);
			}

			desc._format = ToTextureBcFormat(header, header10);
			desc._width = w;
			desc._height = h;
			desc._mipLevels = header._mipMapCount - lod;
			desc._flags = _desc._flags | TextureDesc::Flags::forceArrayTexture;
			desc._pSamplerDesc = _desc._pSamplerDesc;

			if (init)
			{
				init = false;
				Init(desc);
			}

			VERUS_U_FOR(i, header._mipMapCount)
			{
				UINT32 w = header._width >> i;
				UINT32 h = header._height >> i;
				if (!w) w = 1;
				if (!h) h = 1;
				int levelSize = IO::DDSHeader::ComputeBcLevelSize(w, h, header.Is4BitsBC());
				VERUS_RT_ASSERT(offset + levelSize <= size);

				if (static_cast<int>(i) >= lod)
					UpdateSubresource(pData + offset, i - lod, arrayLayer);

				offset += levelSize;
			}
		}

		urls++;
		arrayLayer++;
	}
}

void BaseTexture::Async_WhenLoaded(CSZ url, RcBlob blob)
{
	LoadDDS(url, blob);
}

int BaseTexture::FormatToBytesPerPixel(Format format)
{
	switch (format)
	{
	case Format::unormR10G10B10A2:  return 4;
	case Format::sintR16:           return 2;
	case Format::floatR11G11B10:    return 4;

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

bool BaseTexture::IsSRGBFormat(Format format)
{
	switch (format)
	{
	case Format::srgbR8G8B8A8: return true;
	case Format::srgbB8G8R8A8: return true;
	case Format::srgbBC1:      return true;
	case Format::srgbBC2:      return true;
	case Format::srgbBC3:      return true;
	case Format::srgbBC7:      return true;
	}
	return false;
}

bool BaseTexture::IsBC(Format format)
{
	return format >= Format::unormBC1 && format <= Format::srgbBC7;
}

bool BaseTexture::Is4BitsBC(Format format)
{
	return format == Format::unormBC1 || format == Format::srgbBC1 || format == Format::unormBC4;
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
	_p->SetLoadingFlags(desc._flags);
	_p->SetLoadingSamplerDesc(desc._pSamplerDesc);
	if (desc._flags & TextureDesc::Flags::sync)
	{
		Vector<BYTE> vData;
		IO::FileSystem::LoadResource(desc._url, vData);
		_p->LoadDDS(desc._url, Blob(vData.data(), vData.size()));
	}
	else
	{
		if (desc._url)
			_p->LoadDDS(desc._url, desc._texturePart);
		else if (desc._urls)
			_p->LoadDDSArray(desc._urls);
		else
			_p->Init(desc);
	}
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
