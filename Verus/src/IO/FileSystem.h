#pragma once

namespace verus
{
	namespace IO
	{
		class FileSystem : public Singleton<FileSystem>
		{
			typedef Map<String, Vector<BYTE>> TMapCache;

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

			enum { querySize = 240, entrySize = 256 };

			FileSystem();
			~FileSystem();

			static size_t FindColonForPAK(CSZ url);
			static void ReadHeaderPAK(RFile file, UINT32& magic, INT64& entriesOffset, INT64& entriesSize);

			void PreloadCache(CSZ pak, CSZ types[]);

			static void LoadResource			/**/(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc = LoadDesc());
			static void LoadResourceFromFile	/**/(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc = LoadDesc());

			void LoadResourceFromCache(CSZ url, Vector<BYTE>& vData, bool mandatory = true);

			VERUS_P(static void LoadResourceFromPAK(CSZ url, Vector<BYTE>& vData, RcLoadDesc desc, RFile file, CSZ pakEntry));

			static void LoadTextureParts(RFile file, CSZ url, int texturePart, Vector<BYTE>& vData);
			static String ConvertFilenameToPassword(CSZ fileEntry);

			static bool FileExist(CSZ url);
			static bool Delete(CSZ pathName);

			// Path & filename:
			static String ConvertAbsolutePathToRelative(RcString path);
			static String ConvertRelativePathToAbsolute(RcString path, bool useProjectDir);
			static String ReplaceFilename(CSZ pathName, CSZ filename);

			// Save data:
			//static void SaveImage	/**/(CSZ pathName, const UINT32* p, int w, int h, bool upsideDown = false, ILenum type = IL_PSD);
			static void SaveDDS		/**/(CSZ pathName, const UINT32* p, int w, int h, int d = 0);
			static void SaveString	/**/(CSZ pathName, CSZ s);
		};
		VERUS_TYPEDEFS(FileSystem);

		class Image : public Object
		{
		public:
			Vector<BYTE> _v;
			BYTE*        _p = nullptr;
			//ILuint     _name = 0;
			int          _width = 0;
			int          _height = 0;
			int          _bytesPerPixel = 0;

			Image();
			~Image();

			void Init(CSZ url);
			void Done();
		};
		VERUS_TYPEDEFS(Image);
	}
}
