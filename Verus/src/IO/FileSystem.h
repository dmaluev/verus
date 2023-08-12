// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::IO
{
	enum class ImageFormat : int
	{
		extension,
		png,
		tga,
		jpg
	};

	class FileSystem : public Singleton<FileSystem>
	{
		typedef Map<String, Vector<BYTE>> TMapCache;

		static CSZ s_dataFolder;
		static CSZ s_shaderPAK;

		TMapCache _mapCache;
		INT64     _cacheSize = 0;

	public:
		struct LoadDesc
		{
			int  _texturePart = 0;
			bool _nullTerm = false;
			bool _mandatory = true;

			LoadDesc(bool nullTerm = false, int texturePart = 0, bool mandatory = true) :
				_nullTerm(nullTerm), _texturePart(texturePart), _mandatory(mandatory) {}
		};
		VERUS_TYPEDEFS(LoadDesc);

		static const int s_querySize = 240;
		static const int s_entrySize = 256;

		FileSystem();
		~FileSystem();

		static size_t FindPosForPAK(CSZ url);
		static void ReadPakHeader(RFile file, UINT32& magic, INT64& entriesOffset, INT64& entriesSize);

		void PreloadCache(CSZ pak, CSZ types[]);
		void PreloadDefaultCache();

		static void LoadResource			/**/(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc = LoadDesc());
		static void LoadResourceFromFile	/**/(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc = LoadDesc());

		void LoadResourceFromCache(CSZ url, Vector<BYTE>& vData, bool mandatory = true);

		VERUS_P(static void LoadResourceFromPAK(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc, RFile file, CSZ pakEntry));

		static void LoadTextureParts(RFile file, CSZ url, int texturePart, Vector<BYTE>& vData);
		static String ConvertFilenameToPassword(CSZ fileEntry);

		static bool FileExist(CSZ url);
		static bool Delete(CSZ pathname);

		// Path & filename:
		static String ConvertAbsolutePathToRelative(RcString path);
		static String ConvertRelativePathToAbsolute(RcString path, bool useProjectDir);
		static String ReplaceFilename(CSZ pathname, CSZ filename);

		// Save data:
		static void SaveImage /**/(CSZ pathname, const void* p, int w, int h, ImageFormat format = ImageFormat::tga, int pixelStride = sizeof(UINT32), int param = 0);
		static void SaveDDS   /**/(CSZ pathname, const void* p, int w, int h, int d = 0);
		static void SaveString/**/(CSZ pathname, CSZ s);
	};
	VERUS_TYPEDEFS(FileSystem);

	class Image : public Object
	{
	public:
		Vector<BYTE> _vData;
		BYTE* _p = nullptr;
		int          _width = 0;
		int          _height = 0;
		int          _pixelStride = 0;

		Image();
		~Image();

		void Init(CSZ url);
		void Done();
	};
	VERUS_TYPEDEFS(Image);
}
