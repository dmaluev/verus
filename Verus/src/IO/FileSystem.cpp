// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::IO;

CSZ FileSystem::s_dataFolder = "/Data/";
CSZ FileSystem::s_shaderPAK = "[Shaders]:";

FileSystem::FileSystem()
{
}

FileSystem::~FileSystem()
{
}

size_t FileSystem::FindPosForPAK(CSZ url)
{
	if (!url)
		return String::npos;
	if (*url != '[')
		return String::npos;
	const char* pEnd = strstr(url, "]:");
	if (!pEnd)
		return String::npos;
	const size_t at = pEnd - url;
	return at;
}

void FileSystem::ReadPakHeader(RFile file, UINT32& magic, INT64& entriesOffset, INT64& entriesSize)
{
	file >> magic;
	if (magic != 'KAP2')
		throw VERUS_RUNTIME_ERROR << "ReadPakHeader(), Invalid magic number in PAK";

	file >> entriesOffset;
	file >> entriesSize;
	if (entriesSize % s_entrySize)
		throw VERUS_RUNTIME_ERROR << "ReadPakHeader(), Invalid size of entries in PAK";
}

void FileSystem::PreloadCache(CSZ pak, CSZ types[])
{
	StringStream ss;
	ss << _C(Utils::I().GetModulePath()) << s_dataFolder << pak;
	File file;
	if (!file.Open(_C(ss.str())))
		return;

	UINT32 magic;
	INT64 entriesOffset, entriesSize;
	ReadPakHeader(file, magic, entriesOffset, entriesSize);

	char value[s_querySize];

	auto LoadThisFile = [types](CSZ value)
	{
		CSZ* p = types;
		while (*p)
		{
			if (Str::EndsWith(value, *p))
				return true;
			p++;
		}
		return false;
	};

	char fileEntry[s_entrySize] = {};
	INT64 entriesCount = entriesSize / s_entrySize;
	INT64 pakDataOffset, pakDataSize;
	file.Seek(entriesOffset, SEEK_SET);
	while (entriesCount)
	{
		file >> fileEntry;

		strcpy(value, fileEntry);
		Str::CyrillicToUppercase(value);

		if (LoadThisFile(value))
		{
			memcpy(&pakDataOffset, fileEntry + s_querySize, sizeof(INT64));
			memcpy(&pakDataSize, fileEntry + s_querySize + sizeof(INT64), sizeof(INT64));

			const String password = ConvertFilenameToPassword(fileEntry);

			Vector<BYTE> vCip, vZip, vData;
			const INT64 entryOffset = file.GetPosition();
			file.Seek(pakDataOffset, SEEK_SET); // [unzipped size][encrypted data].
			INT64 size;
			file >> size;
			vData.resize(size + 1); // For null-terminated string.
			vCip.resize(pakDataSize - sizeof(INT64));
			vZip.resize(pakDataSize - sizeof(INT64));
			file.Read(vCip.data(), vCip.size());
			Security::CipherRC4::Decrypt(password, vCip, vZip);
			uLongf destLen = Utils::Cast32(size);
			const int ret = uncompress(vData.data(), &destLen, vZip.data(), Utils::Cast32(vZip.size()));
			if (ret != Z_OK)
				throw VERUS_RUNTIME_ERROR << "uncompress(), " << ret;

			_cacheSize += vData.size();
			String key("[");
			key += pak;
			Str::ReplaceExtension(key, "]:");
			key += fileEntry;
			_mapCache[key] = std::move(vData);

			file.Seek(entryOffset, SEEK_SET); // Go back to entries.
		}

		entriesCount--;
	}
}

void FileSystem::PreloadDefaultCache()
{
	CSZ types[] =
	{
		".primary",
		".rig",
		".txt",
		".xml",
		".xmt",
		".xwa",
		".xwx",
		nullptr
	};
	PreloadCache("Misc.pak", types);
	PreloadCache("Models.pak", types);
	PreloadCache("Textures.pak", types);
}

void FileSystem::LoadResource(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc)
{
	String pakPathname, pakEntry;
	const size_t pakPos = FindPosForPAK(url);
	if (pakPos != String::npos) // "[Foo]:Bar.ext" format -> in PAK file:
	{
		String strUrl(url);
		StringStream ss;
		ss << _C(Utils::I().GetModulePath()) << s_dataFolder;
		ss << strUrl.substr(1, pakPos - 1) << ".pak";
		const String name = strUrl.substr(pakPos + 2);
		const WideString wide = Str::Utf8ToWide(name);
		pakPathname = ss.str();
		pakEntry = Str::CyrillicWideToAnsi(_C(wide));
	}

	if (pakPathname.empty() || pakEntry.empty()) // System file name?
		return LoadResourceFromFile(url, vData, desc);
	File pakFile;
	if (!pakFile.Open(_C(pakPathname))) // PAK not found? Try system file.
		return LoadResourceFromFile(url, vData, desc);

	LoadResourceFromPAK(url, vData, desc, pakFile, _C(pakEntry));
}

void FileSystem::LoadResourceFromFile(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc)
{
	String strUrl(url), projectPathname(url);
	const bool shader = Str::StartsWith(url, s_shaderPAK);
	const size_t pakPos = FindPosForPAK(url);
	if (pakPos != String::npos)
	{
		if (shader)
		{
			strUrl.replace(0, strlen(s_shaderPAK), "/");
			projectPathname = String(_C(Utils::I().GetProjectPath())) + strUrl;
			StringStream ss;
			ss << _C(Utils::I().GetShaderPath()) << strUrl;
			strUrl = ss.str();
		}
		else
		{
			String folder = strUrl.substr(1, pakPos - 1);
			const String name = strUrl.substr(pakPos + 2);
			projectPathname = String(_C(Utils::I().GetProjectPath())) + s_dataFolder + folder + "/" + name;
			StringStream ss;
			ss << _C(Utils::I().GetModulePath()) << s_dataFolder << folder << "/" << name;
			strUrl = ss.str();
		}
	}

	File file;
	if (file.Open(_C(strUrl)) || file.Open(_C(projectPathname)))
	{
		const INT64 size = file.GetSize();
		if (size >= 0)
		{
			if (Str::EndsWith(url, ".dds", false))
			{
				LoadTextureParts(file, url, desc._texturePart, vData);
			}
			else
			{
				const size_t vsize = desc._nullTerm ? static_cast<size_t>(size) + 1 : static_cast<size_t>(size);
				vData.resize(vsize);
				file.Read(vData.data(), size);
			}
		}
	}
	else if (desc._mandatory)
		throw VERUS_RUNTIME_ERROR << "LoadResourceFromFile(), File not found: " << url << " (" << strUrl << ")";
}

void FileSystem::LoadResourceFromCache(CSZ url, Vector<BYTE>& vData, bool mandatory)
{
	if (_mapCache.empty()) // Cache not ready?
		return LoadResourceFromFile(url, vData, LoadDesc(true, 0, mandatory));

	auto it = _mapCache.find(url);
	if (it != _mapCache.end())
		vData = it->second;
	else if (mandatory)
		throw VERUS_RUNTIME_ERROR << "LoadResourceFromCache(), File not found in cache: " << url;
}

void FileSystem::LoadResourceFromPAK(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc, RFile file, CSZ pakEntry)
{
	UINT32 magic;
	INT64 entriesOffset, entriesSize;
	ReadPakHeader(file, magic, entriesOffset, entriesSize);

	char query[s_querySize];
	char value[s_querySize];
	strcpy(query, pakEntry);
	Str::CyrillicToUppercase(query);

	bool found = false;
	char fileEntry[s_entrySize] = {};
	INT64 entriesCount = entriesSize / s_entrySize;
	INT64 pakDataOffset, pakDataSize;
	file.Seek(entriesOffset, SEEK_SET);
	while (entriesCount)
	{
		file >> fileEntry;

		strcpy(value, fileEntry);
		Str::CyrillicToUppercase(value);

		if (!_stricmp(query, value))
		{
			memcpy(&pakDataOffset, fileEntry + s_querySize, sizeof(INT64));
			memcpy(&pakDataSize, fileEntry + s_querySize + sizeof(INT64), sizeof(INT64));
			found = true;
			break;
		}
		entriesCount--;
	}
	if (!found) // Resource is not in PAK file?
	{
		if (Str::EndsWith(pakEntry, ".primary"))
			return;
		return LoadResourceFromFile(url, vData, desc);
	}

	const String password = ConvertFilenameToPassword(fileEntry);

	Vector<BYTE> vCip, vZip;
	file.Seek(pakDataOffset, SEEK_SET);
	if (Str::EndsWith(query, ".dds", false))
	{
		const int maxParts = 8;
		INT64 partEntries[maxParts * 3] = {};

		int headerSize = sizeof(DDSHeader);
		DDSHeader header;
		file >> header;
		if (!header.Validate())
			throw VERUS_RUNTIME_ERROR << "LoadResourceFromPAK(), Invalid DDS header: " << url;
		DDSHeaderDXT10 header10;
		if (header.IsDXT10())
		{
			file >> header10;
			headerSize += sizeof(DDSHeaderDXT10);
		}
		const int partCount = header.GetPartCount();
		const int skipPartCount = header.SkipParts(desc._texturePart);

		if (partCount > maxParts)
			throw VERUS_RUNTIME_ERROR << "LoadResourceFromPAK(), Too many parts in PAK";

		INT64 totalSize = headerSize;
		VERUS_FOR(part, partCount)
		{
			file >> partEntries[part * 3 + 0];
			file >> partEntries[part * 3 + 1];
			file >> partEntries[part * 3 + 2];
			if (part >= skipPartCount)
				totalSize += partEntries[part * 3 + 1];
		}

		// Generate new DDS inside vData:
		vData.resize(totalSize);
		memcpy(vData.data(), &header, sizeof(header));
		if (header.IsDXT10())
			memcpy(&vData[sizeof(DDSHeader)], &header10, sizeof(header10));

		INT64 dataPos = headerSize;
		VERUS_FOR(part, partCount)
		{
			const INT64 partOffset = partEntries[part * 3 + 0];
			const INT64 partSize = partEntries[part * 3 + 1];
			const INT64 partZipSize = partEntries[part * 3 + 2];
			INT64 size = 0;
			if (part >= skipPartCount)
			{
				file.Seek(pakDataOffset + partOffset, SEEK_SET);
				file >> size;
				if (size != partSize)
					throw VERUS_RUNTIME_ERROR << "LoadResourceFromPAK(), Invalid size in PAK";
				vCip.resize(partZipSize - sizeof(INT64));
				vZip.resize(partZipSize - sizeof(INT64));
				file.Read(vCip.data(), vCip.size());
				Security::CipherRC4::Decrypt(password, vCip, vZip);
				uLongf destLen = Utils::Cast32(size);
				const int ret = uncompress(vData.data() + dataPos, &destLen, vZip.data(), Utils::Cast32(vZip.size()));
				if (ret != Z_OK)
					throw VERUS_RUNTIME_ERROR << "uncompress(), " << ret;
			}
			dataPos += size;
		}
	}
	else
	{
		INT64 size;
		file >> size;
		const INT64 vsize = desc._nullTerm ? size + 1 : size;
		vData.resize(vsize);
		vCip.resize(pakDataSize - sizeof(INT64));
		vZip.resize(pakDataSize - sizeof(INT64));
		file.Read(vCip.data(), vCip.size());
		Security::CipherRC4::Decrypt(password, vCip, vZip);
		uLongf destLen = Utils::Cast32(size);
		const int ret = uncompress(vData.data(), &destLen, vZip.data(), Utils::Cast32(vZip.size()));
		if (ret != Z_OK)
			throw VERUS_RUNTIME_ERROR << "uncompress(), " << ret;
	}
}

void FileSystem::LoadTextureParts(RFile file, CSZ url, int texturePart, Vector<BYTE>& vData)
{
	int headerSize = sizeof(DDSHeader);
	DDSHeader header;
	file >> header;
	if (!header.Validate())
		throw VERUS_RUNTIME_ERROR << "LoadTextureParts(), Invalid DDS header: " << url;
	DDSHeaderDXT10 header10;
	if (header.IsDXT10())
	{
		file >> header10;
		headerSize += sizeof(DDSHeaderDXT10);
	}

	const int maxW = header._width;
	const int maxH = header._height;

	const int partCount = header.GetPartCount();
	const int skipPartCount = header.SkipParts(texturePart);
	const int partCheckCount = partCount - 1;

	size_t skipSize = 0;
	VERUS_FOR(part, partCheckCount)
	{
		const int w = maxW >> part;
		const int h = maxH >> part;
		const size_t partSize = DDSHeader::ComputeBcLevelSize(w, h, header.Is4BitsBC());
		if (part < skipPartCount)
			skipSize += partSize;
	}

	const INT64 size = file.GetSize();
	vData.resize(size - skipSize);
	memcpy(vData.data(), &header, sizeof(header));
	if (header.IsDXT10())
		memcpy(vData.data() + sizeof(header), &header10, sizeof(header10));
	file.Seek(skipSize, SEEK_CUR);
	file.Read(vData.data() + headerSize, size - headerSize - skipSize);
}

String FileSystem::ConvertFilenameToPassword(CSZ fileEntry)
{
	String password;
	password.resize(strlen(fileEntry));
	for (size_t i = 0; i < password.size(); ++i)
		password[i] = ~(((fileEntry[i] >> 4) & 0x0F) | ((fileEntry[i] << 4) & 0xF0));
	return password;
}

bool FileSystem::FileExist(CSZ url)
{
	String pathname(url), pakPathname, projectPathname;
	const size_t pakPos = FindPosForPAK(url);
	if (pakPos != String::npos)
	{
		const String dataFolder(s_dataFolder);
		const String pakFilename = pathname.substr(1, pakPos - 1);

		pakPathname = _C(Utils::I().GetModulePath()) + dataFolder + pakFilename + ".pak";

		pathname = pakFilename + '/' + pathname.substr(pakPos + 2);
		projectPathname = _C(Utils::I().GetProjectPath()) + dataFolder + pathname;

		pathname = _C(Utils::I().GetModulePath()) + dataFolder + pathname;
	}
	File file;
	if (file.Open(_C(pathname))) // Normal filename:
		return true;
	if (file.Open(_C(pakPathname))) // PAK filename:
		return true;
	if (file.Open(_C(projectPathname))) // File in another project dir:
		return true;
	return false;
}

bool FileSystem::Delete(CSZ pathname)
{
#ifdef _WIN32
	const WideString ws = Str::Utf8ToWide(pathname);
	return !_wremove(_C(ws));
#else
	return !remove(pathname);
#endif
}

String FileSystem::ConvertAbsolutePathToRelative(RcString path)
{
	String s;
	const size_t data = path.rfind("\\Data\\");
	if (data != String::npos)
	{
		s = '[' + path.substr(data + 6);
		const size_t colon = s.find('\\');
		if (colon != String::npos)
			s.replace(colon, 1, "]:");
	}
	Str::ReplaceAll(s, "\\", "/");
	return s;
}

String FileSystem::ConvertRelativePathToAbsolute(RcString path, bool useProjectDir)
{
	VERUS_QREF_UTILS;
	const String dataFolder(s_dataFolder);
	String systemPath = path.substr(1);
	const size_t colon = systemPath.find(':');
	if (colon != String::npos && colon > 0)
		systemPath = systemPath.substr(0, colon - 1) + '/' + systemPath.substr(colon + 1);
	if (useProjectDir && !utils.GetProjectPath().IsEmpty())
		systemPath = _C(utils.GetProjectPath()) + dataFolder + systemPath;
	else
		systemPath = _C(utils.GetModulePath()) + dataFolder + systemPath;
	return systemPath;
}

String FileSystem::ReplaceFilename(CSZ pathname, CSZ filename)
{
	CSZ slash = strrchr(pathname, '\\');
	if (!slash)
		slash = strrchr(pathname, '/');
	if (slash)
	{
		const size_t pathSize = slash - pathname;
		const size_t size = pathSize + 1 + strlen(filename) + 1;
		Vector<char> name;
		name.resize(size);
		strncpy(name.data(), pathname, pathSize);
		name[pathSize] = '/';
		strcat(name.data(), filename);
		return name.data();
	}
	return filename;
}

void FileSystem::SaveImage(CSZ pathname, const UINT32* p, int w, int h, bool upsideDown, ILenum type)
{
	struct RAII
	{
		ILuint name;
		RAII() : name(0) {}
		~RAII()
		{
			if (name)
				ilDeleteImages(1, &name);
		}
	} raii;

	Vector<UINT32> vFlip;
	if (!upsideDown)
	{
		vFlip.resize(w * h);
		VERUS_FOR(i, h)
			memcpy(&vFlip[i * w], &p[(h - i - 1) * w], w * 4);
		p = vFlip.data();
	}

	raii.name = ilGenImage();
	ilBindImage(raii.name);
	ilTexImage(w, h, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, (void*)p);
	Vector<BYTE> v;
	v.resize(w * h * 8);
	ilSaveL(type, v.data(), Utils::Cast32(v.size()));
	const ILuint sizeOut = ilGetLumpPos();
	IO::File file;
	if (file.Open(pathname, "wb"))
	{
		file.Write(v.data(), sizeOut);
		file.Close();
	}
	else
		throw VERUS_RUNTIME_ERROR << "SaveImage(), Open()";
}

void FileSystem::SaveDDS(CSZ pathname, const UINT32* p, int w, int h, int d)
{
	IO::File file;
	if (!file.Open(pathname, "wb"))
		throw VERUS_RUNTIME_ERROR << "SaveDDS(), Open()";

	DDSHeader header;
	header._flags |= DDSHeader::Flags::linearSize;
	header._height = h;
	header._width = w;
	header._pitchOrLinearSize = w * h * 4 * Math::Max(1, abs(d));
	header._depth = abs(d);
	header._pixelFormat._flags |= DDSHeader::PixelFormatFlags::alphaPixels | DDSHeader::PixelFormatFlags::rgb;
	header._pixelFormat._rgbBitCount = 32;
	header._pixelFormat._rBitMask = 0x00FF0000;
	header._pixelFormat._gBitMask = 0x0000FF00;
	header._pixelFormat._bBitMask = 0x000000FF;
	header._pixelFormat._rgbAlphaBitMask = 0xFF000000;
	if (d > 0)
	{
		header._flags |= DDSHeader::Flags::depth;
		header._caps._caps1 = DDSHeader::Caps1::complex;
		header._caps._caps2 = DDSHeader::Caps2::volume;
	}
	else if (-6 == d)
	{
		header._caps._caps1 = DDSHeader::Caps1::complex;
		header._caps._caps2 = DDSHeader::Caps2::cubemap |
			DDSHeader::Caps2::cubemapPositiveX |
			DDSHeader::Caps2::cubemapNegativeX |
			DDSHeader::Caps2::cubemapPositiveY |
			DDSHeader::Caps2::cubemapNegativeY |
			DDSHeader::Caps2::cubemapPositiveZ |
			DDSHeader::Caps2::cubemapNegativeZ;
	}
	file.Write(&header, sizeof(header));
	file.Write(p, header._pitchOrLinearSize);
}

void FileSystem::SaveString(CSZ pathname, CSZ s)
{
	File file;
	if (file.Open(pathname, "wb"))
		file.Write(s, strlen(s));
	else
		throw VERUS_RUNTIME_ERROR << "Create(SaveString)";
}

// Image:

Image::Image()
{
}

Image::~Image()
{
	Done();
}

void Image::Init(CSZ url)
{
	VERUS_INIT();
	FileSystem::LoadResourceFromFile(url, _vData);
	ilGenImages(1, &_name);
	ilBindImage(_name);
	ilLoadL(IL_TYPE_UNKNOWN, _vData.data(), Utils::Cast32(_vData.size()));
	_width = ilGetInteger(IL_IMAGE_WIDTH);
	_height = ilGetInteger(IL_IMAGE_HEIGHT);
	_bytesPerPixel = ilGetInteger(IL_IMAGE_BYTES_PER_PIXEL);
	_p = ilGetData();
}

void Image::Done()
{
	if (_name)
	{
		ilBindImage(_name);
		ilDeleteImages(1, &_name);
		_name = 0;
	}
	_vData.clear();
	VERUS_DONE(Image);
}
