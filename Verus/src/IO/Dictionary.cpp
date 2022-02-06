// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::IO;

// DictionaryValue:

DictionaryValue::DictionaryValue()
{
}

DictionaryValue::DictionaryValue(CSZ value)
{
	Set(value);
}

void DictionaryValue::Set(CSZ value)
{
	_s = value;
	_i = atoi(value);
	_f = static_cast<float>(atof(value));
}

// Dictionary:

Dictionary::Dictionary()
{
}

Dictionary::~Dictionary()
{
	DeleteAll();
}

void Dictionary::LinkParent(PDictionary p)
{
	_pParent = p;
}

void Dictionary::Insert(CSZ key, CSZ value)
{
	if (!key || !strlen(key) || !value)
		return;
	PDictionaryValue p = TStoreValues::Insert(key);
	p->Set(value);
}

void Dictionary::Delete(CSZ key)
{
	TStoreValues::Delete(key);
}

void Dictionary::DeleteAll()
{
	TStoreValues::DeleteAll();
}

CSZ Dictionary::Find(CSZ key, CSZ def) const
{
	VERUS_IF_FOUND_IN(TStoreValues::TMap, TStoreValues::_map, key, it)
		return _C(it->second._s);
	if (_pParent)
		return _pParent->Find(key, def);
	return def;
}

RcString Dictionary::FindSafe(CSZ key) const
{
	VERUS_IF_FOUND_IN(TStoreValues::TMap, TStoreValues::_map, key, it)
		return it->second._s;
	if (_pParent)
		return _pParent->FindSafe(key);
	static String null;
	return null;
}

int Dictionary::FindInt(CSZ key, int def) const
{
	VERUS_IF_FOUND_IN(TStoreValues::TMap, TStoreValues::_map, key, it)
		return it->second._i;
	if (_pParent)
		return _pParent->FindInt(key, def);
	return def;
}

float Dictionary::FindFloat(CSZ key, float def) const
{
	VERUS_IF_FOUND_IN(TStoreValues::TMap, TStoreValues::_map, key, it)
		return it->second._f;
	if (_pParent)
		return _pParent->FindFloat(key, def);
	return def;
}

int Dictionary::GetCount() const
{
	return TStoreValues::GetStoredCount();
}

void Dictionary::ExportAsStrings(Vector<String>& v) const
{
	v.clear();
	v.reserve(TStoreValues::_map.size() * 2);
	VERUS_FOREACH_CONST(TStoreValues::TMap, TStoreValues::_map, it)
	{
		v.push_back(it->first);
		v.push_back(it->second._s);
	}
}

void Dictionary::Serialize(RStream stream)
{
	const BYTE count = static_cast<BYTE>(TStoreValues::_map.size());
	stream << count;
	VERUS_FOREACH_CONST(TStoreValues::TMap, TStoreValues::_map, it)
	{
		stream.WriteString(_C(it->first));
		stream.WriteString(_C(it->second._s));
	}
}

void Dictionary::Deserialize(RStream stream)
{
	DeleteAll();
	char key[Stream::s_bufferSize];
	char value[Stream::s_bufferSize];
	BYTE count;
	stream >> count;
	VERUS_FOR(i, count)
	{
		stream.ReadString(key);
		stream.ReadString(value);
		Insert(key, value);
	}
}

String Dictionary::ToString() const
{
	StringStream ss;
	for (const auto& x : TStoreValues::_map)
		ss << x.first << ": " << x.second._s << VERUS_CRNL;
	return ss.str();
}

void Dictionary::FromString(CSZ text)
{
	DeleteAll();
	CSZ p = text;
	while (*p)
	{
		const size_t end = strcspn(p, VERUS_CRNL);
		const size_t div = strcspn(p, ":");
		if (div < end)
		{
			const String key(p, p + div);
			const String val(p + div + 2, p + end);
			Insert(_C(key), _C(val));
		}
		p += end;
		p += strspn(p, VERUS_CRNL);
	}
}
