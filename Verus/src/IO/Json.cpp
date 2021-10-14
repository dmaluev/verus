// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::IO;

Json::Json()
{
}

Json::Json(CSZ pathname) : _pathname(pathname)
{
}

Json::~Json()
{
}

void Json::SetFilename(CSZ name)
{
	String pathname;
	CSZ pSlash = strchr(name, '/');
	if (!pSlash)
	{
		pathname = String(_C(Utils::I().GetWritablePath())) + name;
		name = _C(pathname);
	}
	_pathname = name;
}

void Json::Load(bool fromCache)
{
	Vector<BYTE> vData;
	if (fromCache)
	{
		FileSystem::I().LoadResourceFromCache(_C(_pathname), vData);
		_json = nlohmann::json::parse(reinterpret_cast<CSZ>(vData.data()));
	}
	else
	{
		StringStream ss;
		ss << "Load(); url=" << _pathname;
		VERUS_LOG_INFO(_C(ss.str()));

		const size_t pakPos = FileSystem::FindPosForPAK(_C(_pathname));
		if (pakPos != String::npos)
		{
			FileSystem::LoadResource(_C(_pathname), vData, FileSystem::LoadDesc(true));
			_json = nlohmann::json::parse(reinterpret_cast<CSZ>(vData.data()));
		}
		else
		{
			File file;
			if (file.Open(_C(_pathname)))
			{
				file.ReadAll(vData, true);
				_json = nlohmann::json::parse(reinterpret_cast<CSZ>(vData.data()));
			}
		}
	}
}

void Json::Save()
{
	StringStream ss;
	ss << "Save(); url=" << _pathname;
	File file;
	if (file.Open(_C(_pathname), "w"))
	{
		const String s = _json.dump(1, '\t');
		file.WriteText(_C(s));
		ss << ", OK";
	}
	VERUS_LOG_INFO(_C(ss.str()));
}

void Json::Clear()
{
	_json.clear();
}

void Json::Set(CSZ name, CSZ v, bool ifNull)
{
	if (ifNull && _json.find(name) != _json.end())
		return;
	_json[name] = v;
}

void Json::Set(CSZ name, int v, bool ifNull)
{
	if (ifNull && _json.find(name) != _json.end())
		return;
	_json[name] = v;
}

void Json::Set(CSZ name, float v, bool ifNull)
{
	if (ifNull && _json.find(name) != _json.end())
		return;
	_json[name] = v;
}

void Json::Set(CSZ name, bool v, bool ifNull)
{
	if (ifNull && _json.find(name) != _json.end())
		return;
	_json[name] = v;
}

CSZ Json::GetS(CSZ name, CSZ def)
{
	if (_json.find(name) == _json.end())
		return def;
	return _json[name].get_ref<const nlohmann::json::string_t&>().c_str();
}

int Json::GetI(CSZ name, int def)
{
	if (_json.find(name) == _json.end())
		return def;
	return _json.value(name, def);
}

float Json::GetF(CSZ name, float def)
{
	if (_json.find(name) == _json.end())
		return def;
	return _json.value(name, def);
}

bool Json::GetB(CSZ name, bool def)
{
	if (_json.find(name) == _json.end())
		return def;
	return _json.value(name, def);
}
