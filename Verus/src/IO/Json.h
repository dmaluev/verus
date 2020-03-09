#pragma once

namespace verus
{
	namespace IO
	{
		class Json
		{
			nlohmann::json _json;
			String         _pathname;

		public:
			Json();
			Json(CSZ pathname);
			~Json();

			Str GetFilename() const { return _C(_pathname); }
			void SetFilename(CSZ name);
			void Load(bool fromCache = false);
			void Save();

			void Set(CSZ name, CSZ v, bool ifNull = false);
			void Set(CSZ name, int v, bool ifNull = false);
			void Set(CSZ name, float v, bool ifNull = false);
			void Set(CSZ name, bool v, bool ifNull = false);

			CSZ   GetS(CSZ name, CSZ def = nullptr);
			int   GetI(CSZ name, int def = 0);
			float GetF(CSZ name, float def = 0);
			bool  GetB(CSZ name, bool def = false);
		};
		VERUS_TYPEDEFS(Json);
	}
}
