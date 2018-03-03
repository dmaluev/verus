#pragma once

namespace verus
{
	namespace IO
	{
		class Xml
		{
			tinyxml2::XMLDocument _doc;
			String                _pathName;

		public:
			Xml();
			Xml(CSZ pathName);
			~Xml();

			Str GetFilename() const { return _C(_pathName); }
			void SetFilename(CSZ name);
			void Load(bool fromCache = false);
			void Save();

			tinyxml2::XMLElement* GetRoot();
			tinyxml2::XMLElement* AddElement(CSZ name);

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
}
