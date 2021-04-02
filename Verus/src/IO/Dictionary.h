// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace IO
	{
		class DictionaryValue
		{
		public:
			String _s;
			int    _i = 0;
			float  _f = 0;

			DictionaryValue();
			DictionaryValue(CSZ value);
			void Set(CSZ value);

			void AddRef() {}
			bool Done() { return true; }
		};
		VERUS_TYPEDEFS(DictionaryValue);

		typedef StoreUnique<String, DictionaryValue> TStoreValues;
		class Dictionary : private TStoreValues
		{
			Dictionary* _pParent = nullptr;

		public:
			Dictionary();
			~Dictionary();

			void LinkParent(Dictionary* p);

			void Insert(CSZ name, CSZ value);
			void Delete(CSZ name);
			void DeleteAll();

			CSZ      Find(CSZ name, CSZ def = nullptr) const;
			RcString FindSafe(CSZ name) const;
			int      FindInt(CSZ name, int def = 0) const;
			float    FindFloat(CSZ name, float def = 0) const;

			int GetCount() const;

			void ExportAsStrings(Vector<String>& v) const;

			void Serialize(RStream stream);
			void Deserialize(RStream stream);

			String ToString() const;
			void FromString(CSZ text);

			template<typename T>
			void ForEach(const T& fn)
			{
				for (const auto& x : TStoreValues::_map)
				{
					if (Continue::no == fn(_C(x.first), _C(x.second._s)))
						return;
				}
			}
		};
		VERUS_TYPEDEFS(Dictionary);
	}
}
