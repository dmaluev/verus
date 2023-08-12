// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::IO
{
	class Xml
	{
		pugi::xml_document _doc;
		String             _pathname;
		Vector<BYTE>       _vData;

	public:
		Xml();
		Xml(CSZ pathname);
		~Xml();

		Str GetFilename() const { return _C(_pathname); }
		void SetFilename(CSZ name);
		void Load(bool fromCache = false);
		void Save();

		pugi::xml_node GetRoot();
		pugi::xml_node AddElement(CSZ name);

		void Set(CSZ name, CSZ v, bool ifNull = false);
		void Set(CSZ name, int v, bool ifNull = false);
		void Set(CSZ name, float v, bool ifNull = false);
		void Set(CSZ name, bool v, bool ifNull = false);

		CSZ   GetS(CSZ name, CSZ def = nullptr);
		int   GetI(CSZ name, int def = 0);
		float GetF(CSZ name, float def = 0);
		bool  GetB(CSZ name, bool def = false);
	};
	VERUS_TYPEDEFS(Xml);
}
